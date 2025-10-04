# ESP32-MOZA-SteeringWheel-Emulator
This is a project of reverse engineering of MOZA Steering wheel. Based on MikeSzklarz / Arduino-Moza-Emulator.
## 1.Before all
Special thanks to MikeSzklarz's [project](https://github.com/MikeSzklarz/Arduino-Moza-Emulator), without his work there's no way I could do this.

I have **not** complete this project yet, today is Oct 4 2025. This program now can only communicate with MOZA Wheelbase R9 emulated as an ES Wheel in sleep mode, when I turn on the working mode, it lost the connection.

## 2.About the hardware and software
### 2.1 Hardware
I am using esp32s3 wroom 1 as the controller, pin 40 as SCL and pin 39 as SDA. And using tape to stick the cables on the wheelbase (I know this is unstable but it just works for now.)
![hardware.jpg](/hardware.jpg)


### 2.2 Software
