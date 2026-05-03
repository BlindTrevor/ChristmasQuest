# ChristmasQuest

An RFID-based game for the **Arduino Mega 2560 R3**.  
Present the correct RFID fob to the RC522 reader and the onboard LED lights up!  
Status messages are shown on a 2004A LCD (I2C) and four buttons provide in-game controls.

---

## Hardware Required

| Component | Notes |
|-----------|-------|
| Arduino Mega 2560 R3 | Main microcontroller |
| RFID-RC522 module | SPI interface |
| RFID fob / card | One designated as the "correct" key |
| 2004A LCD (20×4) with I2C backpack | Address `0x27` (or `0x3F`) |
| 4 × push button | Momentary, normally open |
| 4 × 10 kΩ resistor | Pull-up (not needed if using `INPUT_PULLUP`) |
| Jumper wires | — |

---

## Wiring

### RFID-RC522 → Arduino Mega

| RC522 Pin | Mega Pin | Notes |
|-----------|----------|-------|
| SDA (SS)  | 53       | Hardware SS |
| SCK       | 52       | Hardware SPI |
| MOSI      | 51       | Hardware SPI |
| MISO      | 50       | Hardware SPI |
| IRQ       | —        | Not used |
| GND       | GND      | |
| RST       | 9        | Configurable |
| 3.3V      | 3.3V     | **Do not connect to 5 V** |

### 2004A LCD (I2C backpack) → Arduino Mega

| LCD Pin | Mega Pin |
|---------|----------|
| GND     | GND      |
| VCC     | 5V       |
| SDA     | 20       |
| SCL     | 21       |

### Buttons → Arduino Mega

Connect each button between the listed pin and **GND** (internal pull-up resistors are enabled in the sketch).

| Button | Mega Pin | Function |
|--------|----------|----------|
| BTN 1  | 2        | Show instructions |
| BTN 2  | 3        | Reset (turn off LED) |
| BTN 3  | 4        | Show LED status |
| BTN 4  | 5        | Store fob (enter/cancel store mode) |

---

## Libraries

Install the following libraries via the Arduino IDE Library Manager  
(**Sketch → Include Library → Manage Libraries…**):

| Library | Author |
|---------|--------|
| `MFRC522` | Miguel Balboa (miguelbalboa) |
| `LiquidCrystal_I2C` | Frank de Brabander |

> `EEPROM` is built into the Arduino core — no separate installation needed.

---

## Storing a Fob at Runtime

The correct fob's UID is saved in the Mega's EEPROM and loaded automatically on every boot. You can change it at any time **without recompiling**:

1. Press **Button 4** — the LCD shows `STORE MODE: scan fob`.  
2. Hold your chosen fob against the reader.  
3. The LCD confirms `** FOB STORED **` and Serial Monitor prints the new UID.  
4. Press **Button 4** again *before* scanning to cancel store mode.

The stored UID survives power cycles. On first boot (no fob stored yet) the sketch falls back to the `correctUID[]` array compiled into the sketch.

---

## First-Time Setup: Finding Your Fob's UID

You no longer need to edit the sketch to set the correct fob — just use Button 4 as described above.

If you prefer to hard-code the UID as the compile-time default, you can still scan your fob and read the UID from the Serial Monitor (9600 baud), then edit `correctUID[]` near the top of the sketch:

```cpp
byte correctUID[]   = { 0xAB, 0xCD, 0xEF, 0x12 };  // ← your bytes here
byte correctUIDSize = 4;
```

---

## How to Play

- **Scan the correct fob** → `ACCESS GRANTED`, onboard LED turns **ON**.  
- **Scan the wrong fob** → `ACCESS DENIED`, UID is printed to Serial Monitor.  
- **Button 1** – Display instructions on the LCD.  
- **Button 2** – Reset / turn the LED off.  
- **Button 3** – Show current LED status.  
- **Button 4** – Enter store mode (scan next fob to save it as the correct one; press again to cancel).

---

## Serial Monitor Output

```
ChristmasQuest ready.
Loaded stored fob UID: AB CD EF 12
Correct fob! LED activated.
Wrong fob! UID: 11 22 33 44
Store mode: scan the fob you want to register.
Fob stored! New UID: AB CD EF 12
```