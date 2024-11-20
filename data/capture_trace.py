import socket
from pwn import *
import json
import numpy as np
from time import sleep
from tqdm.auto import tqdm

class Scope:
    def __init__(self, ip, port):
        self.r = remote(ip, port)
        
        print("Cleaning up errors queue")
        while True:
            self.r.sendline(b":SYSTEM:ERRor?")
            error = self.r.recvline().decode().strip()
            if error == "0,\"No error\"":
                break
        
        print("Cleaned up errors queue")
        
        scope = self.send_and_recv("*IDN?")
        print(f"Oscilloscope: {scope}")
        self.r.sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 100 * 1024 * 1024)
        
        
    def send(self, cmd):
        if isinstance(cmd, str):
            cmd = cmd.encode()
        print("CMD: ", cmd)
            
        self.r.sendline(cmd)
        self.r.sendline(b":SYSTEM:ERRor?")
        error = self.r.recvline().decode().strip()
        if error != "0,\"No error\"":
            print("ERROR: ", error)
            exit(1)
        
    def send_and_recv(self, cmd):
        if isinstance(cmd, str):
            cmd = cmd.encode()
        print("CMD: ", cmd)
        
        self.r.sendline(cmd)
        res = self.r.recvline().decode().strip()
        print("RES: ", res)
        
        self.r.sendline(b":SYSTEM:ERRor?")
        error = self.r.recvline().decode().strip()
        if error != "0,\"No error\"":
            print("ERROR: ", error)
            exit(1)
        
        return res
        
    def preamble(self):
        return dict(zip([
            "format",
            "type",
            "points",
            "count",
            "xincrement",
            "xorigin",
            "xreference",
            "yincrement",
            "yorigin",
            "yreference",
        ],
        [float(x) for x in self.send_and_recv(":WAV:PRE?").split(",")]))
        
    def wait_for_trigger(self):
        while True:
            status = self.send_and_recv(":TRIGger:STATus?")
            if status == "STOP":
                print("Trigger received!")
                break
            sleep(0.1)
            
    def wait_for_data_ready(self, n_points_expected):
        with tqdm(total=n_points_expected, unit='points') as pb:
            last = 0
            while True:
                res = int(self.send_and_recv(":WAV:POINTS?"))
                pb.update(res - last)
                last = res
                if res == n_points_expected:
                    print(f"Data ready: {res} points")
                    break
                sleep(0.1)

    def setup_channel(self, channel, probe=10, scale=0.05, offset=-2.97, coupling="DC", display=True):
        """Setup individual channel with verification"""
        cmds = [
            f":CHAN{channel}:DISPLAY {'ON' if display else 'OFF'}",
            f":CHAN{channel}:COUPLING {coupling}",
            f":CHAN{channel}:BWLimit OFF",
            f":CHAN{channel}:PROB {probe}",
            f":CHAN{channel}:SCALE {scale}",
            f":CHAN{channel}:OFFSET {offset}",
        ]
        
        for cmd in cmds:
            self.send(cmd)
            # Verify the setting was applied
            param = cmd.split()[0].split(":")[2]  # Extract parameter name
            verify_cmd = f":CHAN{channel}:{param}?"
            result = self.send_and_recv(verify_cmd)
            print(f"Verified {verify_cmd}: {result}")
            
# 50M for 1 channel
# 25M for 2 channels
# 10M for 3 or 4 channels
n = 25_000_000
# 1.25G for 1 channel
# 625M for 2 channels
# 312.5M for 3 or 4 channels
sample_rate = 625000000.0
time_range = n / sample_rate
print(f"Time range: {time_range}")

setup_cmds = [
    f":TIMEBASE:SCALE {10e-6:f}",
    f":TIMEBASE:OFFSET {-50e-6:f}",
    
    ":ACQuire:TYPE NORMal",  # Full command names
    f":ACQuire:MDEPth {n}".encode(), # Some scopes need full number
    
    ":TRIGger:MODE EDGE",
    ":TRIGger:SWEep SINGLE",
    ":TRIGger:EDGE:SLOPe POSitive", 
    ":TRIGger:EDGE:SOURce CHAN2",
    ":TRIGger:EDGE:LEVel 1", # Trigger on 1V
    
    ":WAVeform:MODE RAW",
    ":WAVeform:FORMat WORD", 
    f":WAVeform:POINts {n}".encode(),
    ":WAVeform:STARt 1",
    f":WAVeform:STOP {n}".encode(),
    
]

scope = Scope("192.168.69.4", 5555)

scope.send("*RST")
scope.send(":STOP")

for setup_cmd in setup_cmds:
    scope.send(setup_cmd)

scope.setup_channel(1, probe=10, scale=0.05, offset=-2.97)
#scope.setup_channel(2, probe=10, scale=0.05, offset=-2.97)
scope.setup_channel(2, probe=10, scale=1, offset=0)
    
# Start acquisition and wait for trigger
print("Waiting for trigger...")
scope.send(":SINGLE")

scope.wait_for_trigger()

#scope.wait_for_data_ready(n)
        
for chan in [1, 2]:
    preamble = scope.preamble()
    print(f"Preamble: {preamble}")

    print(f"TimeRange {preamble['xincrement'] * preamble['points']}s")

    with open(f"preamble_{chan}.json", "w") as f:
        json.dump(preamble, f, indent=4)

    scope.send(f":WAV:SOURCE CHAN{chan}")
    
    scope.r.sendline(b":WAV:DATA?")
    # IEEE Block
    header = scope.r.recvn(2).decode()
    assert header.startswith("#")
    header_len = int(header[1:2])
    print(f"Header length: {header_len}")
    data_len = int(scope.r.recvn(header_len))
    print(f"Data length: {data_len}")
    
    with open(f"dump_{chan}.bin", "wb") as f:
        with tqdm(total=data_len, unit='B', unit_scale=True) as pb:
            while data_len > 0:
                buffer = scope.r.recv(data_len)
                f.write(buffer)
                data_len -= len(buffer)
                pb.update(len(buffer))
    print("Residual: ", scope.r.recvline())
    sleep(0.5)
        
    