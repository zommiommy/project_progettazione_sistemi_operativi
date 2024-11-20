# Project for Progettazione di Sistemi Operativi

This is all the code and data used for the ECDH side-channel attack project.

## Structure
- `data/`: contains the data used for the attack and the capture and print scripts.
- `manuals`: are the relevant manuals for the board and scope.
- `old`: contains old code for 256-bit finite fields ECDH.
- `tcl`: is the folder from `openocd` needed for uploading the code to the board.
- everything else is a standard CubeMX project with most code in `Core/Src/main.c`.

## Building
To build the project you need `cmake`, `ninja`, `arm-none-eabi-gcc`, `openocd`, and `tio`.

```shell
mkdir build
cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
ninja
sudo openocd -f ../tcl/interface/stlink.cfg -f ../tcl/target/stm32f4x.cfg -c "program ./examnew.elf verify reset exit"
```

Then to connect to the board and see the output:

```shell
sudo tio -b 115200 /dev/ttyUSB0
```
