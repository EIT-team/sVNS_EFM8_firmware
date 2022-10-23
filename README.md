# EFM8SB1_QFN24_stim_v2

EFM8SB1 firmware for the 15-channel selective Vagus Nerve Stimulator. 

Created for the QFN24 package of the microcontroller and wired according to the EAGLE project (see sVNS_16chan_EAGLE repository). 

To flash the firmware,
EFM8SB1 programmer is needed. We used this one: https://uk.rs-online.com/web/p/debuggers-in-circuit-emulators/7570297

Please check the pinout of the programmer and the pinout of the stimulator and connect corresponding GND, RSTB/C2CK clock and C2D data pins. 

1) Install Simplicity Studio. Install all corresponding drivers.
2) Build the fetched project. 
3) Connect +3V power to the power pins.
4) Connect GND, C2CK, C2D pins from the 8-bit debugger to the stimulator.
5) Press "Run" in the Simplicity Studio 5 IDE to begin debugging. Press "stop" and disconnect the debugger.
6) Leave the +3V connection for testing or use any sufficiently powerful NFC source to power the stimulator. We used Adafruit PN532 and custom code (see the sVNS_PN532 repo)
for communication and powering the stimulator.

Note: 16 channels in the file names refer to the number of the output demultiplexer states.
