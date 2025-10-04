# ESP32-MOZA-SteeringWheel-Emulator
This is a project of reverse engineering of MOZA Steering wheel. Based on MikeSzklarz / Arduino-Moza-Emulator.
## 1.Before all
Special thanks to MikeSzklarz's [project](https://github.com/MikeSzklarz/Arduino-Moza-Emulator), without his work there's no way I could do this.

I have **not** complete this project yet, today is Oct 4 2025. This program now can only communicate with MOZA Wheelbase R9 emulated as an ES Wheel in sleep mode, when I turn on the working mode, it lost the connection.

## 2.About the hardware and software
### 2.1 Hardware
I am using esp32s3 wroom 1 as the controller, pin 40 as SCL and pin 39 as SDA. And using tape to stick the cables on the wheelbase (I know this is unstable but it just works for now.) as shown below.
![hardware.jpg](/hardware.jpg)
### 2.2 Software
As you can see in the list, the main code is i2c_basic_example_main.c. This code was build in ESP-IDF v5.5.

I may not completely understand MikeSzklarz's code(poor at C++), the main idea of this program is: Firstly, initial i2c slave, `0x09` as slave address, and other settings. Secondly, set 2 callbacks('request' and 'receive') and pass the parameters to RTOS, in these 2 callbacks, when it is 'receive', I read the value and set a flag, when it is 'request', I reply the message to i2c master as the flag shows. Lastly, the rtos procedure is just print the value.

Right now I just want to emulate the esp32s3 as an ES wheel, no button input yet.

## 3.Problems
When I turn the wheelbase to sleep mode, as a few seconds, the moza pit house shows that the es wheel connected. When I turn the wheelbase to work mode, everything disappear. 
![MOZA PIT HOUSE.png](/MOZA_PIT_HOUSE.png)

I donnot have the i2c contents sent by wheelbase, as I can recall, when it is in sleep mode, there are only 2 kinds of messages, `0xFC` and `0xDD`, base on MikeSzklarz's work, this is an asking of wheen type and button status. I replied `0x04` and `0x00` seperately. Sometimes there are some `0xED` and I did not reply. There is almost an reply asking from the master for every message.

When I turn to work mode, there are regular `0xFC`, `0xF9` messages sent to the slave, but there is no slave reply signal, maybe after 4 or 5 messages, a master reply request. I try to replay all messages `0x04`, because I want to just emulate it as ES Wheel. But no responding at all. After a few times receiving, the messages change to `0xFF` and other things.

## 4.Furthermore
The reason I want to do this is because MOZA's steering wheel is very pricy. Far away from my paygrade. And another reason is, I saw FSR wheel has ABS info settings, which means if I emulate an FSR wheel, I can have ABS working info, I also can send this value to my DIY active brake paddles. The FOC for motor control is another project I am working on, this motor could be the active force for the brake paddle.
