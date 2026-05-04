/**
 * ChristmasQuest
 *
 * An RFID-based game for Arduino Mega 2560 R3.
 * Present the correct RFID fob to the RC522 reader to activate the onboard LED.
 * A 2004A LCD (via I2C) displays status messages and a 12-button keypad allows control.
 *
 * Hardware:
 *   - Arduino Mega 2560 R3
 *   - MFRC522 RFID reader
 *   - 2004A LCD display via I2C (address 0x27)
 *   - 12-button keypad (3×4 matrix, 7-pin)
 *
 * Libraries required:
 *   - MFRC522 by Miguel Balboa (miguelbalboa)
 *   - LiquidCrystal_I2C by Frank de Brabander
 *   - Keypad by Mark Stanley, Alexander Brevig
 *
 * See README.md for wiring details.
 */

#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <Keypad.h>

// ── Pin Definitions ──────────────────────────────────────────────────────────

// RFID-RC522 (SPI – Arduino Mega hardware SPI pins: MOSI=51, MISO=50, SCK=52)
#define RST_PIN   9   // Configurable reset pin
#define SS_PIN    53  // Slave-select (SS/SDA on RC522), Mega hardware SS

// Onboard LED
#define LED_PIN   13

// ── Keypad (3×4 matrix, 7-pin) ───────────────────────────────────────────────
//
// Standard layout:
//   1  2  3
//   4  5  6
//   7  8  9
//   *  0  #
//
// Wiring (left-to-right pinout: R1 R2 R3 R4 C1 C2 C3):
//   R1 → D2,  R2 → D3,  R3 → D4,  R4 → D5
//   C1 → D6,  C2 → D7,  C3 → D8

static const byte KP_ROWS = 4;
static const byte KP_COLS = 3;

static char keys[KP_ROWS][KP_COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};

static byte rowPins[KP_ROWS] = { 2, 3, 4, 5 };   // R1 R2 R3 R4
static byte colPins[KP_COLS] = { 6, 7, 8 };       // C1 C2 C3

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, KP_ROWS, KP_COLS);

// LED auto-reset timeout: the LED turns off automatically after this many ms.
// The timer resets each time the LED is re-activated within the window.
#define LED_AUTO_RESET_MS 5000UL

// ── Object Instances ─────────────────────────────────────────────────────────

// LCD: 20 columns × 4 rows at I2C address 0x27
// If your display uses address 0x3F, change the first argument below.
LiquidCrystal_I2C lcd(0x27, 20, 4);

// RFID reader
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ── Correct Fob UID ──────────────────────────────────────────────────────────
//
// This is the fallback UID used when no fob has been stored in EEPROM yet.
// To set the correct fob at runtime, press Key 4 on the keypad and scan a fob.
// The new UID is saved to EEPROM and survives power cycles.
//
byte correctUID[]   = { 0xDE, 0xAD, 0xBE, 0xEF };
byte correctUIDSize = 4;

// ── EEPROM Layout ─────────────────────────────────────────────────────────────
//
// Address 0 : sentinel byte (EEPROM_SENTINEL = 0xC5 means "valid UID stored")
// Address 1 : UID size in bytes (typically 4 or 7)
// Address 2+ : UID bytes
//
#define EEPROM_SENTINEL     0xC5
#define EEPROM_ADDR_SENTINEL  0
#define EEPROM_ADDR_SIZE      1
#define EEPROM_ADDR_UID       2
#define EEPROM_MAX_UID_SIZE   10  // MFRC522 UIDs are at most 10 bytes

// ── State ────────────────────────────────────────────────────────────────────

bool ledActive = false;
bool storeMode = false;   // when true, the next scanned fob is saved as correct

// Timestamp (millis()) of the last LED activation; used for the auto-reset timer.
unsigned long ledActivatedAt = 0;

// Non-blocking display timeout.
// Use start-time + duration (subtraction-based) to be safe at millis() rollover.
unsigned long showHomeScheduledAt = 0;
unsigned long showHomeDuration    = 0;

// ── Helper: display the idle / home screen ───────────────────────────────────

void showHome() {
  showHomeDuration = 0;   // cancel any pending auto-return
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Christmas Quest!  ");
  if (storeMode) {
    lcd.setCursor(0, 1);
    lcd.print("STORE MODE: scan fob");
    lcd.setCursor(0, 2);
    lcd.print("Key4 again to cancel");
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Scan your fob...    ");
  }
  lcd.setCursor(0, 3);
  lcd.print(ledActive ? "LED: ON             " : "LED: OFF            ");
}

// Schedule an automatic return to the home screen after durationMs.
// Uses subtraction-based comparison so it is safe at millis() rollover.
void scheduleHome(unsigned long durationMs) {
  showHomeScheduledAt = millis();
  showHomeDuration    = durationMs;
}

// ── EEPROM helpers ────────────────────────────────────────────────────────────

// Save the current correctUID / correctUIDSize to EEPROM.
void saveUIDToEEPROM() {
  EEPROM.update(EEPROM_ADDR_SENTINEL, EEPROM_SENTINEL);
  EEPROM.update(EEPROM_ADDR_SIZE, correctUIDSize);
  for (byte i = 0; i < correctUIDSize; i++) {
    EEPROM.update(EEPROM_ADDR_UID + i, correctUID[i]);
  }
}

// Load a UID from EEPROM into correctUID / correctUIDSize.
// Returns true if a valid UID was found, false otherwise.
bool loadUIDFromEEPROM() {
  if (EEPROM.read(EEPROM_ADDR_SENTINEL) != EEPROM_SENTINEL) return false;
  byte size = EEPROM.read(EEPROM_ADDR_SIZE);
  if (size == 0 || size > EEPROM_MAX_UID_SIZE) return false;
  correctUIDSize = size;
  for (byte i = 0; i < correctUIDSize; i++) {
    correctUID[i] = EEPROM.read(EEPROM_ADDR_UID + i);
  }
  return true;
}

// ── Helper: print a UID to Serial ────────────────────────────────────────────

void printUID(byte *uid, byte uidSize) {
  for (byte i = 0; i < uidSize; i++) {
    if (i > 0) Serial.print(" ");
    if (uid[i] < 0x10) Serial.print("0");
    Serial.print(uid[i], HEX);
  }
  Serial.println();
}

// ── Helper: check if the scanned UID matches the correct one ─────────────────

bool isCorrectFob() {
  if (mfrc522.uid.size != correctUIDSize) return false;
  for (byte i = 0; i < correctUIDSize; i++) {
    if (mfrc522.uid.uidByte[i] != correctUID[i]) return false;
  }
  return true;
}

// ── Helper: read a keypad key and map it to an action index ──────────────────
//
// Returns the action index (0–3) for keys '1'–'4', or -1 for any other key.
// Debouncing is handled internally by the Keypad library.

int8_t readButtonPress() {
  char key = keypad.getKey();
  switch (key) {
    case '1': return 0;   // Show instructions
    case '2': return 1;   // Reset LED
    case '3': return 2;   // Show LED status
    case '4': return 3;   // Store fob (enter/cancel store mode)
    default:  return -1;
  }
}

// ── RFID Responses ───────────────────────────────────────────────────────────

void handleCorrectFob() {
  ledActive = true;
  ledActivatedAt = millis();   // (re-)start the 5-second auto-reset timer
  digitalWrite(LED_PIN, HIGH);

  Serial.println(F("Correct fob! LED activated."));

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("** ACCESS GRANTED **");
  lcd.setCursor(0, 1);
  lcd.print("Correct fob!        ");
  lcd.setCursor(0, 2);
  lcd.print("Onboard LED: ON     ");
  scheduleHome(3000);
}

void handleWrongFob() {
  Serial.print(F("Wrong fob! UID: "));
  printUID(mfrc522.uid.uidByte, mfrc522.uid.size);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("** ACCESS DENIED ** ");
  lcd.setCursor(0, 1);
  lcd.print("Wrong fob!          ");
  lcd.setCursor(0, 2);
  lcd.print("See Serial for UID. ");
  scheduleHome(2000);
}

// Called when storeMode is active and a fob is scanned.
void handleStoreFob() {
  byte size = mfrc522.uid.size;
  if (size == 0 || size > EEPROM_MAX_UID_SIZE) {
    Serial.println(F("Store failed: invalid UID size."));
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Store FAILED:       ");
    lcd.setCursor(0, 1);
    lcd.print("Invalid UID size.   ");
    storeMode = false;
    scheduleHome(2000);
    return;
  }

  correctUIDSize = size;
  for (byte i = 0; i < size; i++) {
    correctUID[i] = mfrc522.uid.uidByte[i];
  }
  saveUIDToEEPROM();
  storeMode = false;

  Serial.print(F("Fob stored! New UID: "));
  printUID(correctUID, correctUIDSize);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("** FOB STORED **    ");
  lcd.setCursor(0, 1);
  lcd.print("New fob saved!      ");
  lcd.setCursor(0, 2);
  lcd.print("Scan it to activate.");
  scheduleHome(3000);
}

// ── Button Handlers ───────────────────────────────────────────────────────────

void handleButtonInstructions() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("How to play:        ");
  lcd.setCursor(0, 1);
  lcd.print("Scan the right fob  ");
  lcd.setCursor(0, 2);
  lcd.print("to light the LED!   ");
  lcd.setCursor(0, 3);
  lcd.print("Wrong fob = denied. ");
  scheduleHome(3000);
}

void handleButtonReset() {
  ledActive = false;
  ledActivatedAt = 0;   // clear the auto-reset timer
  digitalWrite(LED_PIN, LOW);
  Serial.println(F("LED reset."));
  showHome();
}

void handleButtonStatus() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Current Status:     ");
  lcd.setCursor(0, 1);
  lcd.print(ledActive ? "LED: ON - Activated!" : "LED: OFF - Inactive ");
  lcd.setCursor(0, 3);
  lcd.print("RFID reader: ready  ");
  scheduleHome(2500);
}

void handleButtonStoreFob() {
  if (storeMode) {
    // Second press cancels store mode
    storeMode = false;
    Serial.println(F("Store mode cancelled."));
    showHome();
    return;
  }
  storeMode = true;
  Serial.println(F("Store mode: scan the fob you want to register."));
  showHome();
}

// ── setup() ──────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(9600);

  // SPI & RFID
  SPI.begin();
  mfrc522.PCD_Init();

  // I2C & LCD
  Wire.begin();
  lcd.init();
  lcd.backlight();

  // Onboard LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Keypad pins are configured by the Keypad library automatically.

  Serial.println(F("ChristmasQuest ready."));

  if (loadUIDFromEEPROM()) {
    Serial.print(F("Loaded stored fob UID: "));
    printUID(correctUID, correctUIDSize);
  } else {
    Serial.println(F("No stored fob found. Using default UID."));
    Serial.println(F("Press Key4 and scan a fob to store one."));
  }

  showHome();
}

// ── loop() ───────────────────────────────────────────────────────────────────

void loop() {
  // ── Non-blocking display timeout ──────────────────────────────────────────
  if (showHomeDuration != 0 && (millis() - showHomeScheduledAt) >= showHomeDuration) {
    showHome();
  }

  // ── LED auto-reset after LED_AUTO_RESET_MS (non-blocking) ─────────────────
  if (ledActive && ledActivatedAt != 0 && (millis() - ledActivatedAt) >= LED_AUTO_RESET_MS) {
    ledActive = false;
    ledActivatedAt = 0;
    digitalWrite(LED_PIN, LOW);
    Serial.println(F("LED auto-reset after 5 s."));
    showHome();
  }

  // ── Keypad handling (one key per iteration) ────────────────────────────────
  int8_t btn = readButtonPress();
  if      (btn == 0) handleButtonInstructions();
  else if (btn == 1) handleButtonReset();
  else if (btn == 2) handleButtonStatus();
  else if (btn == 3) handleButtonStoreFob();

  // ── RFID handling ──────────────────────────────────────────────────────────
  // Wait for a new card
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  // Read the card serial number
  if (!mfrc522.PICC_ReadCardSerial())   return;

  if (storeMode) {
    handleStoreFob();
  } else if (isCorrectFob()) {
    handleCorrectFob();
  } else {
    handleWrongFob();
  }

  // Halt the card and stop encryption to allow a new read
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
