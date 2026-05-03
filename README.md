# Planaball
A productivity tool that makes organisation and time-blocking a stress-free and playful experience!

### Libraries Used
* [U8g2](https://docs.arduino.cc/libraries/u8g2/): Monochrome graphics for OLED screens
* [RTClib](https://docs.arduino.cc/libraries/rtclib/): Interfacing with the RTC (Real-Time Clock) module
* [Servo](https://docs.arduino.cc/libraries/servo/): Control and coordinate servo motors
* [Wire](https://docs.arduino.cc/language-reference/en/functions/communication/wire/): Manages I2C (Inter-Integrated Circuit i.e. wiring set-up) protocols for hardware communication
___
### Set-up Instructions
**Required: Arduino Mega 2560**
#### - Install Software Dependencies
1. Open the Arduino IDE.
2. Go to **Sketch** > **Include Library** > **Manage Libraries**.
3. Search for and install **U8g2** (by oliver) and **RTClib** (by Adafruit). Note: **Wire** and **Servo** are built-in and do not require installation.

#### - Connect Hardware (Wiring)
**WARNING: Do not power 4 servos and 4 solenoids directly from the Arduino's 5V pin, as this may fry the board. Use a dedicated external power supply (we used battery packs) for the motors and relays, and connect its ground to the Arduino's ground**

1. Pushbuttons: Wire one leg of each button to the pin and the other leg to GND (no external resistors needed)
* Test Button: Pin 42
* Switch Screen: Pin 30
* Toggle Mode: Pin 31
* Plus (+): Pin 32
* Minus (-): Pin 33

2. Servos 1, 2, 3, 4 to Pins 9, 10, 11, 12, respectively.

3. Relays / Solenoids 1, 2, 3, 4 to Pins 44, 45, 46, 47, respectively.

4. RTC Module
* VCC: 5V
* GND: GND
* SDA: Pin 20
* SCL: Pin 21

5. OLED Screens: Wire the VCC and GND for all screens to the power rails accordingly. Next, wire the data pins as follows:
* Screen 1: SDA to Pin 20, SCL to Pin 21
* Screen 2: SDA to Pin 22, SCL to Pin 23
* Screen 3: SDA to Pin 24, SCL to Pin 25
* Screen 4: SDA to Pin 26, SCL to Pin 27

#### - Upload Code
Plug the Arduino Mega into your computer via USB.
   * Go to **Tools** > **Board** and select **Arduino Mega or Mega 2560**.
   * Go to **Tools** > **Port** and select the active COM port.
   * Click the **Upload** arrow.
___

### Usage and Interaction

This Tangible UI (TUI) is intended to be open to the creative interpretation of the user! As the creators, we wanted each visual chip + ball combination to represent one task. Set an alarm on the timer associated with that servo for when you plan to start the task. The visual chip slowly begins to rise around 5 seconds (this will be a longer duration in future iterations) before the alarm and the ball drops at the set time. The ball is meant to be played with as you work on your task, as they were specifically selected due to their stress relieving designs. Once you are done with your task or your next visual chip begins to rise, place the ball back in its slot and loop the rubber band over the solenoid lock to 'close' the door. 

___
#### Credits
This project was developed end-to-end for CS320: Tangible User Interfaces at Wellesley College by Amanda Cheng, Nicole Lei, and Ananya Ganesh under the guidance of Professor Orit Shaer, Nina Davalos, and Jennifer Long. 

Google Gemini was used as a supplementary tool for writing code and mapping hardware connections.


