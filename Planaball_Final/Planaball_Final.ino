#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>
#include <Servo.h>

// --- SERVO SETUP ---
Servo servos[4];                           // Array of 4 independent Servo objects
const int servoPins[4] = {9, 10, 11, 12};  // Digital pins mapped to servos 1-4

// --- RELAY (SOLENOID) SETUP ---
const int relayPins[4] = {44, 45, 46, 47};             // Digital pins mapped to relays 1-4
bool solenoidActive[4] = {false, false, false, false}; // Tracks which solenoids are currently energized
unsigned long solenoidStartTime[4] = {0, 0, 0, 0};     // Timestamps for independent 3-second relay shutoff

// --- SEQUENCE STATE MACHINE TIMERS ---
int sequenceState[4] = {0, 0, 0, 0};           // Current physical animation step (0=Idle, 1=Climb, 2=Pause, 3=Return)
unsigned long stateStartTime[4] = {0, 0, 0, 0}; // Timestamps to manage non-blocking state transitions

// --- BUTTONS ---
const int btnCheck = 42;    // Global hardware override/test pin
const int switchPin = 30;   // Pin to cycle active UI screen
const int modePin = 31;     // Pin to toggle UI edit modes (Arm/Hr/Min)
const int plusPin = 32;     // Pin to increment time values
const int minusPin = 33;    // Pin to decrement time values

bool lastSw = HIGH, lastMd = HIGH, lastPl = HIGH, lastMi = HIGH; // Tracks previous logic levels for falling-edge detection
unsigned long plusPressTime = 0, lastPlusInc = 0;                // Timestamps to detect holding the plus button
unsigned long minusPressTime = 0, lastMinusInc = 0;              // Timestamps to detect holding the minus button
const int longPressDelay = 500, fastScrollRate = 100;            // Thresholds (in ms) to trigger rapid scrolling

// --- UI & ALARM STATE ---
int activeScreen = -1;                               // Tracks which screen the buttons control (-1 = no screen selected)
int editState[4] = {0, 0, 0, 0};                     // Tracks UI state per screen (0=View/Arm, 1=Edit Hour, 2=Edit Minute)
bool needsUpdate[4] = {true, true, true, true};      // Flags to indicate an OLED buffer needs to be redrawn
bool alarmArmed[4] = {false, false, false, false};   // Tracks whether the hardware sequence will trigger at the set time

// --- RTC & SCREENS ---
RTC_DS3231 rtc;
int lastMinute = -1; // Tracks current minute to force screen updates when the real-world minute changes

// Screen Initialization
U8G2_SSD1306_128X64_NONAME_F_HW_I2C s1(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C s2(U8G2_R0, 23, 22, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C s3(U8G2_R0, 25, 24, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C s4(U8G2_R0, 27, 26, U8X8_PIN_NONE);

int alarmH[4] = {12, 6, 18, 9};  // Configured hours at initialisation for Timers 1-4
int alarmM[4] = {0, 30, 45, 15}; // Configured minutes at initialisation for Timers 1-4

void setup() {
  rtc.begin();
  
  pinMode(btnCheck, INPUT_PULLUP);
  pinMode(switchPin, INPUT_PULLUP);
  pinMode(modePin, INPUT_PULLUP);
  pinMode(plusPin, INPUT_PULLUP);
  pinMode(minusPin, INPUT_PULLUP);
  
  // Initialize hardware states for all 4 channels
  for(int i = 0; i < 4; i++) {
    servos[i].attach(servoPins[i]);
    servos[i].write(90); 
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW); 
  }
  
  s1.begin(); s2.begin(); s3.begin(); s4.begin();
}

void loop() {
  DateTime now = rtc.now();
  bool checkPressed = (digitalRead(btnCheck) == LOW); 

  // --- 1. HARDWARE LOGIC ---
  // Process the sequence and hardware output for each of the 4 timers sequentially
  for (int i = 0; i < 4; i++) {
    
    int trigMin = (alarmM[i] == 0) ? 59 : alarmM[i] - 1;
    int trigHr = (alarmM[i] == 0) ? (alarmH[i] == 0 ? 23 : alarmH[i] - 1) : alarmH[i];
    
    // Check if current time falls within the 3-second window prior to the configured alarm time
    bool timeToStart = (now.hour() == trigHr && now.minute() == trigMin && now.second() >= 55 && now.second() <= 57);
    bool startSequence = alarmArmed[i] && timeToStart && (sequenceState[i] == 0);

    // Initiate the sequence if armed, time matches, and it is currently idle
    if (startSequence) {
      sequenceState[i] = 1;
      stateStartTime[i] = millis();
    }

    // Process state transitions based on elapsed time
    if (sequenceState[i] == 1) { 
      // Transition to State 2 after 5000ms
      if (millis() - stateStartTime[i] >= 5000) {
        sequenceState[i] = 2; 
        stateStartTime[i] = millis();
        solenoidActive[i] = true; 
        solenoidStartTime[i] = millis();
      }
    } 
    else if (sequenceState[i] == 2) { 
      // Transition to State 3 after 2000ms
      if (millis() - stateStartTime[i] >= 2000) {
        sequenceState[i] = 3; 
        stateStartTime[i] = millis();
      }
    }
    else if (sequenceState[i] == 3) { 
      // Return to Idle (State 0) after 4750ms and reset armed status
      if (millis() - stateStartTime[i] >= 4750) {
        sequenceState[i] = 0; 
        alarmArmed[i] = false; 
        needsUpdate[i] = true; 
      }
    }

    // Deactivate solenoid once its independent 3000ms active window passes
    if (solenoidActive[i] && (millis() - solenoidStartTime[i] >= 3000)) {
      solenoidActive[i] = false;
    }

    // Apply manual override to hardware if the check button is currently held
    if (checkPressed) {
      servos[i].write(180); 
      digitalWrite(relayPins[i], HIGH); 
    } else {
      // Set servo angles based on current sequence state
      if (sequenceState[i] == 1) servos[i].write(180);
      else if (sequenceState[i] == 2) servos[i].write(90);
      else if (sequenceState[i] == 3) servos[i].write(0);
      else servos[i].write(90);

      digitalWrite(relayPins[i], solenoidActive[i] ? HIGH : LOW);
    }
  }

  // --- 2. BUTTONS & UI STATE ---
  
  // Cycle the active screen target on button press (falling edge detection)
  bool currentSw = digitalRead(switchPin);
  if (currentSw == LOW && lastSw == HIGH) {
    activeScreen++;
    // Wrap active screen index back to -1 (none selected) if it exceeds 3
    if (activeScreen > 3) activeScreen = -1; 
    // Reset edit views and force redraws for all screens when selection changes
    for(int i = 0; i < 4; i++) { editState[i] = 0; needsUpdate[i] = true; } 
  }
  lastSw = currentSw;

  // Process UI edit buttons only if a specific screen is currently selected
  if (activeScreen != -1) {
    
    // Toggle edit modes on mode button press (falling edge)
    bool currentMd = digitalRead(modePin);
    if (currentMd == LOW && lastMd == HIGH) { 
      // Automatically disarm if entering edit mode, re-arm if exiting edit mode
      if (editState[activeScreen] == 0) alarmArmed[activeScreen] = false; 
      else if (editState[activeScreen] == 2) alarmArmed[activeScreen] = true;  
      
      editState[activeScreen] = (editState[activeScreen] + 1) % 3; 
      needsUpdate[activeScreen] = true; 
    }
    lastMd = currentMd;

    // Handle plus button for incrementing time
    bool currentPl = digitalRead(plusPin);
    if (currentPl == LOW) {
      // Handle initial single press
      if (lastPl == HIGH) { 
        if (editState[activeScreen] == 1) alarmH[activeScreen] = (alarmH[activeScreen] + 1) % 24;
        if (editState[activeScreen] == 2) alarmM[activeScreen] = (alarmM[activeScreen] + 1) % 60;
        needsUpdate[activeScreen] = true; plusPressTime = millis(); lastPlusInc = millis();
      } 
      // Handle continuous scrolling if button is held past the delay threshold
      else if (millis() - plusPressTime > longPressDelay && millis() - lastPlusInc > fastScrollRate) {
        if (editState[activeScreen] == 1) alarmH[activeScreen] = (alarmH[activeScreen] + 1) % 24;
        if (editState[activeScreen] == 2) alarmM[activeScreen] = (alarmM[activeScreen] + 1) % 60;
        needsUpdate[activeScreen] = true; lastPlusInc = millis();
      }
    }
    lastPl = currentPl;

    // Handle minus button for decrementing time
    bool currentMi = digitalRead(minusPin);
    if (currentMi == LOW) {
      // Handle initial single press
      if (lastMi == HIGH) { 
        if (editState[activeScreen] == 1) alarmH[activeScreen] = (alarmH[activeScreen] + 23) % 24;
        if (editState[activeScreen] == 2) alarmM[activeScreen] = (alarmM[activeScreen] + 59) % 60;
        needsUpdate[activeScreen] = true; minusPressTime = millis(); lastMinusInc = millis();
      } 
      // Handle continuous scrolling if button is held past the delay threshold
      else if (millis() - minusPressTime > longPressDelay && millis() - lastMinusInc > fastScrollRate) {
        if (editState[activeScreen] == 1) alarmH[activeScreen] = (alarmH[activeScreen] + 23) % 24;
        if (editState[activeScreen] == 2) alarmM[activeScreen] = (alarmM[activeScreen] + 59) % 60;
        needsUpdate[activeScreen] = true; lastMinusInc = millis();
      }
    }
    lastMi = currentMi;
  } else {
    lastMd = digitalRead(modePin); lastPl = digitalRead(plusPin); lastMi = digitalRead(minusPin);
  }

  // Flag all screens for redraw if the RTC minute changes to keep "NOW" clock synced
  if (now.minute() != lastMinute) {
    lastMinute = now.minute();
    // Mark all screens as needing an update
    for(int i = 0; i < 4; i++) needsUpdate[i] = true;
  }

  // --- 3. STAGGERED UI REFRESH ---
  static int updateIdx = 0;
  // Draw only the currently queued screen if it requires an update, avoiding loop block
  if (needsUpdate[updateIdx]) {
    if (updateIdx == 0) updateUI(&s1, "TIMER 1", 0, now.hour(), now.minute());
    else if (updateIdx == 1) updateUI(&s2, "TIMER 2", 1, now.hour(), now.minute());
    else if (updateIdx == 2) updateUI(&s3, "TIMER 3", 2, now.hour(), now.minute());
    else if (updateIdx == 3) updateUI(&s4, "TIMER 4", 3, now.hour(), now.minute());
    needsUpdate[updateIdx] = false; 
  }
  updateIdx = (updateIdx + 1) % 4;

  delay(10); 
}

void updateUI(U8G2 *s, const char* label, int id, int ch, int cm) {
  s->clearBuffer();
  
  // Draw bounding frame if this is the actively selected screen
  if (id == activeScreen) s->drawFrame(0, 0, 128, 64); 
  
  // Draw clock icon if alarm is armed and not currently being edited
  if (alarmArmed[id] && editState[id] == 0) { 
    s->drawCircle(115, 10, 6); s->drawLine(115, 10, 115, 5); s->drawLine(115, 10, 118, 10);   
  }
  
  s->setDrawColor(1); s->setFont(u8g2_font_helvB08_tr); s->drawStr(4, 12, label); 
  s->setFont(u8g2_font_helvB18_tr);
  
  // Highlight behind hours if currently in hour edit mode
  if (editState[id] == 1) { s->drawBox(18, 20, 35, 25); s->setDrawColor(0); } 
  s->setCursor(20, 42); 
  // Add leading zero to hours if under 10
  if (alarmH[id] < 10) s->print("0"); 
  s->print(alarmH[id]);
  
  s->setDrawColor(1); s->drawStr(55, 40, ":"); 
  
  // Highlight minutes if currently in minute edit mode
  if (editState[id] == 2) { s->drawBox(68, 20, 35, 25); s->setDrawColor(0); } 
  s->setCursor(70, 42); 
  // Add leading zero to minutes if under 10
  if (alarmM[id] < 10) s->print("0"); 
  s->print(alarmM[id]);
  
  s->setDrawColor(1); s->setFont(u8g2_font_helvB08_tr); s->setCursor(15, 60); 
  s->print("NOW: "); 
  // Add leading zero to current time hours if under 10
  if (ch < 10) s->print("0"); 
  s->print(ch);
  s->print(":"); 
  // Add leading zero to current time minutes if under 10
  if (cm < 10) s->print("0"); 
  s->print(cm);
  
  s->sendBuffer();
}