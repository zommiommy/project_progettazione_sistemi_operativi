from pwn import *
import numpy as np
import json
from tqdm.auto import tqdm
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter, ScalarFormatter
from scipy import signal

def load_data(channel):

    with open(f"preamble_{channel}.json", "r") as f:
        preabmle = json.load(f)

    print("Loaded preamble ", preabmle)

    data = np.fromfile(f"dump_{channel}.bin", dtype=np.dtype('<u2')).astype(np.float64)
    data = data[len(data)//2:]
    print("RawMean: ", np.mean(data))
    print("RawMax : ", np.max(data))
    print("RawMin : ", np.min(data))
    print("Loaded")

    parsed = ((data + preabmle["yorigin"]) * preabmle["yincrement"])

    print("Parsed")

    print("Mean: ", np.mean(parsed))
    print("Max : ", np.max(parsed))
    print("Min : ", np.min(parsed))
    print("Trace length: ",  len(data) * preabmle["xincrement"])

    times = np.linspace(0, len(data) * preabmle["xincrement"], len(data), dtype=np.float64)
    
    return times, parsed, preabmle

x1, y1, p1 = load_data(1)
x2, y2, p2 = load_data(2)
#x3, y3, p3 = load_data(3)

def filter_data(data, cutoff_freq, btype='lowpass'):
    sample_rate = 1/p1["xincrement"]
    nyquist_freq = sample_rate / 2
    normalized_cutoff_freq = cutoff_freq / nyquist_freq

    # Design the high-pass Butterworth filter
    b, a = signal.butter(3, normalized_cutoff_freq, btype=btype)

    # Apply the filter
    return signal.filtfilt(b, a, data - np.mean(data))

filtered_data = y1
filtered_data = filter_data(filtered_data, 2e8, 'lowpass')
filtered_data = filter_data(filtered_data , 1000, 'highpass')

fig, axs = plt.subplots(2, 1, figsize=(20, 10))
axs[0].plot(x1, filtered_data, label="Difference")
#axs.plot(x1, y1, label="Channel 1")
axs[1].plot(x2, y2, label="Channel 2")
#axs[1].plot(x3, y3, label="Channel 3")

axs[0].set(ylabel="Voltage (V)", xlabel="Time (s)")
axs[0].legend()
#axs[1].set(ylabel="Voltage (V)", xlabel="Time (s)")
#axs[1].legend()

def format_time_ticks(x, pos):
    if x == 0:
        return "0s"
    if abs(x) >= 1:
        return f"{x:.3f}s"
    elif abs(x) >= 1e-3:
        return f"{x*1e3:.3f}ms"
    elif abs(x) >= 1e-6:
        return f"{x*1e6:.3f}μs"
    else:
        return f"{x*1e9:.3f}ns"

def format_ticks(x, pos):
    if x == 0:
        return "0"
    if abs(x) >= 1e3:
        return f"{x*1e-3:.3f}k"
    elif abs(x) >= 1:
        return f"{x:.3f}"
    elif abs(x) >= 1e-3:
        return f"{x*1e3:.3f}m"
    else:
        return f"{x*1e6:.3f}μ"
    
def format_voltage_ticks(x, pos):
    x = format_ticks(x, pos)
    return f"{x:}V"

def format_current_ticks(x, pos):
    x = format_ticks(x, pos)
    return f"{x:}A"

# Apply to your existing plot
axs[0].xaxis.set_major_formatter(FuncFormatter(format_time_ticks))
axs[0].yaxis.set_major_formatter(FuncFormatter(format_voltage_ticks))
# Apply to your existing plot
#axs[1].xaxis.set_major_formatter(FuncFormatter(format_time_ticks))
#axs[1].yaxis.set_major_formatter(FuncFormatter(format_voltage_ticks))
plt.show()