# ChristmasQuest

An RFID-based game for the **Arduino Mega 2560 R3**.  
Present the correct RFID fob to the RC522 reader and the onboard LED lights up!  
Status messages are shown on a 2004A LCD (I2C) and a 12-button keypad provides in-game controls.

---

## Hardware Required

| Component | Notes |
|-----------|-------|
| Arduino Mega 2560 R3 | Main microcontroller |
| RFID-RC522 module | SPI interface |
| RFID fob / card | One designated as the "correct" key |
| 2004A LCD (20×4) with I2C backpack | Address `0x27` (or `0x3F`) |
| 12-button keypad (3×4) | 7-pin membrane or mechanical keypad |
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

### Keypad (3×4 matrix) → Arduino Mega

The keypad has 7 pins: 4 row pins (R1–R4) and 3 column pins (C1–C3).  
Connect each pin to a separate digital I/O pin on the Mega.  
The Keypad library configures `INPUT_PULLUP` automatically — no external resistors are needed.

Standard keypad pinout (left to right; verify against your keypad's datasheet):

```
Pin:  1    2    3    4    5    6    7
      R1   R2   R3   R4   C1   C2   C3
```

| Keypad Pin | Signal | Mega Pin |
|------------|--------|----------|
| 1 (R1)     | Row 1  | D2       |
| 2 (R2)     | Row 2  | D3       |
| 3 (R3)     | Row 3  | D4       |
| 4 (R4)     | Row 4  | D5       |
| 5 (C1)     | Col 1  | D6       |
| 6 (C2)     | Col 2  | D7       |
| 7 (C3)     | Col 3  | D8       |

Keypad button layout and functions:

```
[ 1 ] [ 2 ] [ 3 ]    1 → Show key guide / instructions
[ 4 ] [ 5 ] [ 6 ]    2 → Reset (turn off LED immediately)
[ 7 ] [ 8 ] [ 9 ]    3 → Show current LED status
[ * ] [ 0 ] [ # ]    4 → Program a fob (enter store mode; press again to cancel)
                     5–9, *, 0, # → used for name entry (see below)
```

The home screen always shows these shortcuts on rows 3–4, so you never need to remember them.

---

## Libraries

Install the following libraries via the Arduino IDE Library Manager  
(**Sketch → Include Library → Manage Libraries…**):

| Library | Author |
|---------|--------|
| `MFRC522` | Miguel Balboa (miguelbalboa) |
| `LiquidCrystal_I2C` | Frank de Brabander |
| `Keypad` | Mark Stanley, Alexander Brevig |

> `EEPROM` is built into the Arduino core — no separate installation needed.

---

## Storing a Fob at Runtime

The correct fob's UID — and an optional name — are saved in the Mega's EEPROM and loaded automatically on every boot. You can change them at any time **without recompiling**:

1. Press **Key `4`** — the LCD shows `STORE: scan fob now`.
2. Hold your chosen fob against the reader.  
   The LCD switches to **name-entry mode**.
3. Type a name using **Nokia-style multi-tap** (see table below), then press **`#`** to save.  
   Press `#` immediately to save without a name.
4. The LCD confirms `** FOB STORED **`.

To cancel at the **scan step** (before a fob is read), press **Key `4`** again.  
To cancel during **name entry**, delete all characters with `*` then press `*` once more.

### Name-entry key map

| Key | Characters (press repeatedly to cycle) |
|-----|----------------------------------------|
| `2` | A → B → C → 2 → A … |
| `3` | D → E → F → 3 → D … |
| `4` | G → H → I → 4 → G … |
| `5` | J → K → L → 5 → J … |
| `6` | M → N → O → 6 → M … |
| `7` | P → Q → R → S → 7 → P … |
| `8` | T → U → V → 8 → T … |
| `9` | W → X → Y → Z → 9 → W … |
| `0` | (space) → 0 → (space) … |
| `1` | . → , → ! → ? → - → 1 → . … |
| `*` | Backspace (delete last character) |
| `#` | Confirm and save |

A character is committed automatically after **0.8 s** of inactivity, or instantly when you press a different key — just like old Nokia phones.

When the correct fob is scanned, the LCD shows `Welcome, NAME!` if a name was saved.

The stored UID and name survive power cycles. On first boot (no fob stored yet) the sketch falls back to the `correctUID[]` array compiled into the sketch.

---

## First-Time Setup: Finding Your Fob's UID

You no longer need to edit the sketch to set the correct fob — just use Key `4` as described above.

If you prefer to hard-code the UID as the compile-time default, you can still scan your fob and read the UID from the Serial Monitor (9600 baud), then edit `correctUID[]` near the top of the sketch:

```cpp
byte correctUID[]   = { 0xAB, 0xCD, 0xEF, 0x12 };  // ← your bytes here
byte correctUIDSize = 4;
```

---

## How to Play

The home screen always reminds you what each key does (rows 3–4).

- **Scan the correct fob** → `ACCESS GRANTED` + `Welcome, NAME!`, onboard LED turns **ON**.  
- **Scan the wrong fob** → `ACCESS DENIED`, UID is printed to Serial Monitor.  
- **Key `1`** – Show the key guide on the LCD.  
- **Key `2`** – Reset / turn the LED off immediately.  
- **Key `3`** – Show current LED status.  
- **Key `4`** – Enter store mode to program a new fob (see *Storing a Fob* above).

> **LED auto-reset:** The LED turns off automatically **5 seconds** after it is activated. Scanning the correct fob again while the LED is on restarts the 5-second timer. Key `2` still resets the LED immediately at any time.

---

## Serial Monitor Output

```
ChristmasQuest ready.
Loaded stored fob UID: AB CD EF 12
Correct fob! LED activated.
Wrong fob! UID: 11 22 33 44
Store mode: scan the fob you want to register.
Fob scanned. Enter name then press #.
Fob stored! Name: ALICE
UID: AB CD EF 12
LED auto-reset after 5 s.
```