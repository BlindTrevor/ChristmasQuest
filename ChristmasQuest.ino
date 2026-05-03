/**
 * ChristmasQuest
 *
 * An RFID-based game for Arduino Mega 2560 R3.
 * Present the correct RFID fob to the RC522 reader to activate the onboard LED.
 * A 2004A LCD (via I2C) displays status messages and 4 buttons allow control.
 *
 * Hardware:
 *   - Arduino Mega 2560 R3
 *   - MFRC522 RFID reader
 *   - 2004A LCD display via I2C (address 0x27)
 *   - 4 push buttons
 *
 * Libraries required:
 *   - MFRC522 by Miguel Balboa (miguelbalboa)
 *   - LiquidCrystal_I2C by Frank de Brabander
 *
 * See README.md for wiring details.
 */

#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ── Pin Definitions ──────────────────────────────────────────────────────────

// RFID-RC522 (SPI – Arduino Mega hardware SPI pins: MOSI=51, MISO=50, SCK=52)
#define RST_PIN   9   // Configurable reset pin
#define SS_PIN    53  // Slave-select (SS/SDA on RC522), Mega hardware SS

// Onboard LED
#define LED_PIN   13

// Buttons (INPUT_PULLUP – connect each button between pin and GND)
static const uint8_t BTN_PINS[] = { 2, 3, 4, 5 };
static const uint8_t NUM_BUTTONS = 4;

// Debounce period in milliseconds
#define DEBOUNCE_MS 50UL

// ── Object Instances ─────────────────────────────────────────────────────────

// LCD: 20 columns × 4 rows at I2C address 0x27
// If your display uses address 0x3F, change the first argument below.
LiquidCrystal_I2C lcd(0x27, 20, 4);

// RFID reader
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ── Correct Fob UID ──────────────────────────────────────────────────────────
//
// Replace these bytes with the UID of your own RFID fob.
// Upload the sketch and scan your fob once – the UID is printed to the
// Serial Monitor (Tools → Serial Monitor, 9600 baud).  Then copy the
// printed bytes here, recompile, and upload again.
//
byte correctUID[]   = { 0xDE, 0xAD, 0xBE, 0xEF };
byte correctUIDSize = 4;

// ── State ────────────────────────────────────────────────────────────────────

bool ledActive = false;

// Non-blocking display timeout.
// Use start-time + duration (subtraction-based) to be safe at millis() rollover.
unsigned long showHomeScheduledAt = 0;
unsigned long showHomeDuration    = 0;

// Per-button debounce: timestamp of the last confirmed press.
unsigned long lastBtnPressMs[NUM_BUTTONS] = { 0, 0, 0, 0 };

// ── Helper: display the idle / home screen ───────────────────────────────────

void showHome() {
  showHomeDuration = 0;   // cancel any pending auto-return
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Christmas Quest!  ");
  lcd.setCursor(0, 1);
  lcd.print("Scan your fob...    ");
  lcd.setCursor(0, 3);
  lcd.print(ledActive ? "LED: ON             " : "LED: OFF            ");
}

// Schedule an automatic return to the home screen after durationMs.
// Uses subtraction-based comparison so it is safe at millis() rollover.
void scheduleHome(unsigned long durationMs) {
  showHomeScheduledAt = millis();
  showHomeDuration    = durationMs;
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

// ── Helper: non-blocking button debounce ─────────────────────────────────────
//
// Returns the index (0-3) of the first newly-pressed button, or -1 if none.
// All button states are sampled first; only the lowest-indexed press is acted on.

int8_t readButtonPress() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    if (digitalRead(BTN_PINS[i]) == LOW) {
      if (now - lastBtnPressMs[i] > DEBOUNCE_MS) {
        lastBtnPressMs[i] = now;
        return (int8_t)i;
      }
    }
  }
  return -1;
}

// ── RFID Responses ───────────────────────────────────────────────────────────

void handleCorrectFob() {
  ledActive = true;
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

void handleButtonUIDHelper() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UID Helper:         ");
  lcd.setCursor(0, 1);
  lcd.print("Scan any fob now.   ");
  lcd.setCursor(0, 2);
  lcd.print("UID -> Serial port. ");
  scheduleHome(2000);
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

  // Buttons (internal pull-up; connect button between pin and GND)
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BTN_PINS[i], INPUT_PULLUP);
  }

  Serial.println(F("ChristmasQuest ready."));
  Serial.println(F("Scan a fob to begin."));

  showHome();
}

// ── loop() ───────────────────────────────────────────────────────────────────

void loop() {
  // ── Non-blocking display timeout ──────────────────────────────────────────
  if (showHomeDuration != 0 && (millis() - showHomeScheduledAt) >= showHomeDuration) {
    showHome();
  }

  // ── Button handling (non-blocking debounce; one press per iteration) ───────
  int8_t btn = readButtonPress();
  if      (btn == 0) handleButtonInstructions();
  else if (btn == 1) handleButtonReset();
  else if (btn == 2) handleButtonStatus();
  else if (btn == 3) handleButtonUIDHelper();

  // ── RFID handling ──────────────────────────────────────────────────────────
  // Wait for a new card
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  // Read the card serial number
  if (!mfrc522.PICC_ReadCardSerial())   return;

  if (isCorrectFob()) {
    handleCorrectFob();
  } else {
    handleWrongFob();
  }

  // Halt the card and stop encryption to allow a new read
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
