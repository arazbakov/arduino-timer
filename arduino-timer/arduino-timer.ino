#include <Keypad_I2C.h>
#include <Keypad.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>

const int screenRows = 2;
const int screenCols = 16;

#define SCREEN_RST_PIN 8
#define SCREEN_ENABLE_PIN 7
#define SCREEN_D4_PIN 6
#define SCREEN_D5_PIN 5
#define SCREEN_D6_PIN 4
#define SCREEN_D7_PIN 3

#define KEYPAD_I2CADDR 0x38

#define LOAD_PIN 2

const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 4; 

char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte keypadRowPins[KEYPAD_ROWS] = {0, 1, 2, 3};
byte keypadColPins[KEYPAD_COLS] = {4, 5, 6, 7};

char lastPressedKey = '\0';

enum TimerState { MAIN_MENU, EDIT, RUNNING, WARMING_UP };
TimerState currentState;


LiquidCrystal lcd(SCREEN_RST_PIN, SCREEN_ENABLE_PIN, SCREEN_D4_PIN, SCREEN_D5_PIN, SCREEN_D6_PIN, SCREEN_D7_PIN);
Keypad_I2C keypad = Keypad_I2C( makeKeymap(keys), keypadRowPins, keypadColPins, KEYPAD_ROWS, KEYPAD_COLS, KEYPAD_I2CADDR );

unsigned int scrollPosition = 0;
unsigned long scrollLastTime = 0;
int scrollDelay = 650;

void switchState(TimerState state) {
  currentState = state;
  switch(currentState) {
    case MAIN_MENU:
      mainMenuSetup();
      break;
    case EDIT:
      editSetup();
      break;
    case WARMING_UP:
      warmingUpSetup();
      break;
    case RUNNING:
      runningSetup();
      break;  
  }
}

void EEPROMWriteInt(int p_address, int p_value)
{
  byte lowByte = ((p_value >> 0) & 0xFF);
  byte highByte = ((p_value >> 8) & 0xFF);
  EEPROM.write(p_address, lowByte);
  EEPROM.write(p_address + 1, highByte);
}

//This function will read a 2 byte integer from the eeprom at the specified address and address + 1
unsigned int EEPROMReadInt(int p_address)
{
  byte lowByte = EEPROM.read(p_address);
  byte highByte = EEPROM.read(p_address + 1);
  return ((lowByte << 0) & 0xFF) + ((highByte << 8) & 0xFF00);
}

unsigned int defaultPresetValue = 60;

unsigned int presets[10] = {defaultPresetValue, defaultPresetValue, defaultPresetValue, defaultPresetValue, defaultPresetValue, defaultPresetValue, defaultPresetValue, defaultPresetValue, defaultPresetValue, defaultPresetValue};

void savePreset(int presetNumber) {
  int resultAddress = 2 + presetNumber * 2;
  EEPROMWriteInt(resultAddress, presets[presetNumber]);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  keypad.begin();
  pinMode(LOAD_PIN, OUTPUT);
  keypad.addEventListener(keypadEvent);
  lcd.begin(screenCols, screenRows);

  byte editSymbolS[8] = {
    0b00000,
    0b00000,
    0b01110,
    0b10000,
    0b01110,
    0b00001,
    0b11110,
    0b11111
  };

  byte editSymbolWithZero[8] = {
    0b01110,
    0b10001,
    0b10011,
    0b10101,
    0b11001,
    0b10001,
    0b01110,
    0b11111
  };

  lcd.createChar(0, editSymbolS);
  lcd.createChar(1, editSymbolWithZero);
  
  // load presets from eeeprom:
  if (EEPROMReadInt(0) == 42) {
    // it's correct data, read presets:
    for (int i = 0; i < 10; ++i) {
      presets[i] = EEPROMReadInt(2 + i * 2);
    }
  } else {
    EEPROMWriteInt(0, 42);
    for (int i = 0; i < 10; ++i) {
      savePreset(i);
    }
  }
  
  switchState(MAIN_MENU);
}

void loop() {
  // put your main code here, to run repeatedly:
  char key = keypad.getKey();
  switch(currentState) {
    case MAIN_MENU:
      mainMenuLoop();
      break;
    case EDIT:
      editLoop();
      break;
    case RUNNING:
      runningLoop();
      break;  
    case WARMING_UP:
      warmingUpLoop();
      break;
  }
}

int currentPreset = 1;


void clearRow(int row) {
  lcd.setCursor(0, row);
  for (int i = 0; i < screenCols; ++i) {
    lcd.print(' ');
  }
}

void initScroll() {
  clearRow(1);
  scrollLastTime = 0;
  scrollPosition = 0;
}

void processScroll(char* text, int sizeOfText) {
  unsigned long currentTime = millis();
  if (scrollLastTime == 0 || currentTime - scrollLastTime > scrollDelay){
    lcd.setCursor(0, 1);
    int textLength = sizeOfText - 1;
    for (int i = 0; i < min(screenCols, textLength); ++i) {
      lcd.print(text[(scrollPosition + i)%textLength]);
    }
    scrollLastTime = millis();
    if (textLength > screenCols) {
      ++scrollPosition;
      scrollPosition = scrollPosition % textLength;
    }
  }
}

void printChosenPreset() {
  clearRow(0);
  lcd.setCursor(0, 0);
  lcd.print("Preset ");
  lcd.print(currentPreset);
  lcd.print(": ");
  lcd.print(presets[currentPreset]);
  lcd.print("s");
}

void mainMenuSetup() {
  lastPressedKey = '\0';
  digitalWrite(LOAD_PIN, LOW);
  lcd.clear();
  printChosenPreset();
  initScroll();
}

char mainMenuScrollText[] = "A-Start B-Edit ";

void mainMenuLoop() {
  if (lastPressedKey != '\0') {
    char key = lastPressedKey;
    lastPressedKey = '\0';
    if (key >= '0' && key <= '9') {
      // switch preset:
      currentPreset = key - '0';
      printChosenPreset();
    } else {
      switch (key) {
        case 'A':
          // start
          switchState(WARMING_UP);
          return;
        case 'B':
          // edit
          switchState(EDIT);
          return;
      }
    }
  }

  processScroll(mainMenuScrollText, sizeof(mainMenuScrollText));  
}

unsigned int editedPresetValue = 0;

int blinkPeriod = 1000;
unsigned long lastBlinkTime = 0;
bool cursorBlinkState = true;

void printPresetEditState() {
  clearRow(0);
  
  lcd.setCursor(0, 0);
  lcd.print(">Preset ");
  lcd.print(currentPreset);
  lcd.print(": ");
  if (editedPresetValue <= 0) {
    if (cursorBlinkState) {
      lcd.write(byte(1));
    } else {
      lcd.print(editedPresetValue);
    }
    lcd.print("s");
  } else {
    lcd.print(editedPresetValue);
    if (cursorBlinkState && editedPresetValue <= 999) {
      lcd.write(byte(0));
    } else {
      lcd.print("s");
    }
  }
}

void editSetup() {
  lastPressedKey = '\0';
  // load:
  editedPresetValue = presets[currentPreset];
  lcd.clear();
  printPresetEditState();
  initScroll();
  lastBlinkTime = millis();
}

char editScrollText[] = " A-Save B-Discard C-Backspace D-Default";

void editLoop() {
  if (lastPressedKey != '\0') {
    char key = lastPressedKey;
    lastPressedKey = '\0';
    if (key >= '0' && key <= '9') {
      if (editedPresetValue <= 999) {
        unsigned int val = key - '0';
        editedPresetValue = editedPresetValue * 10 + val;
        cursorBlinkState = true;
        lastBlinkTime = millis();
        printPresetEditState();
      }
    } else {
      switch (key) {
        case 'A':
          // save
          presets[currentPreset] = editedPresetValue;
          savePreset(currentPreset);
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print(" Preset ");
          lcd.print(currentPreset);
          lcd.print(" saved");
          delay(2000);
          switchState(MAIN_MENU);
          return;
        case 'B':
          // cancel
          switchState(MAIN_MENU);
          return;
        case 'C':
          // backspace
          if (editedPresetValue > 0) {
            editedPresetValue /= 10;
          }
          cursorBlinkState = true;
          lastBlinkTime = millis();
          printPresetEditState();
          return;
        case 'D':
          // default
          presets[currentPreset] = defaultPresetValue;
          savePreset(currentPreset);
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Preset ");
          lcd.print(currentPreset);
          lcd.print(" reset");
          lcd.setCursor(0,1);
          lcd.print("to default ");
          lcd.print(defaultPresetValue);
          lcd.print("s");
          delay(3000);
          switchState(MAIN_MENU);
          return;
      }
    }
  }

  processScroll(editScrollText, sizeof(editScrollText));  
  unsigned long currentTime = millis();
  
  if (currentTime - lastBlinkTime > blinkPeriod) {
    cursorBlinkState = !cursorBlinkState;
    printPresetEditState();
    lastBlinkTime = millis();
  }
}

int warmUpTimerSeconds = 4;
int warmUpTimerValue;
unsigned long warmUpTimerLastSwitch;

void warmingUpSetup() {
  lastPressedKey = '\0';
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Warming Up");
  lcd.setCursor(5, 1);
  lcd.print("B-Stop");
  warmUpTimerValue = warmUpTimerSeconds;
  warmUpTimerLastSwitch = millis();
}

void warmingUpLoop() {
  if (lastPressedKey != '\0') {
    char key = lastPressedKey;
    lastPressedKey = '\0';
    if (key == 'B') {
      lcd.clear();
      lcd.setCursor(4,0);
      lcd.print("STOPPED");
      delay(2000);
      switchState(MAIN_MENU);
      return;
    }
  }
  
  if (millis() - warmUpTimerLastSwitch > 1000) {
    warmUpTimerLastSwitch = millis();
    --warmUpTimerValue;
    if (warmUpTimerValue <= 0) {
      switchState(RUNNING);
      return;
    }
    clearRow(0);
    lcd.setCursor(8, 0);
    lcd.print(warmUpTimerValue);
  }
}

unsigned int runningCounter;
unsigned long lastCounterSwitchTime;
bool paused = false;
unsigned long pauseCurrentTimeDelta;

char runningModeText[] = "A-Pause B-Stop";

void printRunningCounter() {
  clearRow(0);
  lcd.setCursor(0, 0);
  lcd.print("Left: ");
  lcd.print(runningCounter);
  lcd.print("s");
}

void runningSetup() {
  lastPressedKey = '\0';
  digitalWrite(LOAD_PIN, HIGH);
  runningCounter = presets[currentPreset];
  lastCounterSwitchTime = millis();
  pauseCurrentTimeDelta = 0;
  paused = false;
  
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(runningModeText);

  printRunningCounter();
}

void runningLoop() {
  if (!paused) {
    unsigned long currentTime = millis();
    if (currentTime - lastCounterSwitchTime > 1000) {
      --runningCounter;
      lastCounterSwitchTime = currentTime;
      if (runningCounter <= 0) {
        digitalWrite(LOAD_PIN, LOW);
        lcd.clear();
        lcd.setCursor(6, 0);
        lcd.print("DONE");
        delay(2000);
        switchState(MAIN_MENU);
        return;
      }
      printRunningCounter();
    }
  }

  if (lastPressedKey != '\0') {
      char key = lastPressedKey;
      lastPressedKey = '\0';
      switch (key) {
        case 'A':
          // pause or resume
          if (!paused) {
            pauseCurrentTimeDelta = millis() - lastCounterSwitchTime;
            paused = true;
            digitalWrite(LOAD_PIN, LOW);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Paused (");
            lcd.print(runningCounter);
            lcd.print("s)");
            lcd.setCursor(0, 1);
            lcd.print("A-Resume B-Stop");
          } else {
            paused = false;
            lastCounterSwitchTime = millis() - pauseCurrentTimeDelta;
            digitalWrite(LOAD_PIN, HIGH);
            lcd.clear();
            printRunningCounter();
            lcd.setCursor(0, 1);
            lcd.print(runningModeText);
          }
          return;
        case 'B':
          // stop
          digitalWrite(LOAD_PIN, LOW);
          lcd.clear();
          lcd.setCursor(4,0);
          lcd.print("STOPPED");
          delay(2000);
          switchState(MAIN_MENU);
          return;
      }
    }
}

void keypadEvent(KeypadEvent key){
  switch (keypad.getState()){
    case PRESSED:
      
      break;
    case RELEASED:
      lastPressedKey = key;
      break;
    case HOLD:
      
      break;
  }
}
