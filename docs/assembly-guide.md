# Pit Claw BBQ Controller - Assembly Guide

Step-by-step instructions for building the Pit Claw temperature controller carrier board, wiring it to the WT32-SC01 Plus display, and assembling the enclosure.

## Safety

- **Soldering iron**: Work in a ventilated area. The iron tip is 300-400C. Use a stand and keep the tip away from skin, clothing, and plastic.
- **Lead-free solder recommended**: Wash hands after soldering.
- **12V power**: The barrel jack input is low-voltage DC (12V, ~2A max). No mains voltage is involved, but avoid shorting 12V to 3.3V components.
- **ESD awareness**: The ESP32-S3 and ADS1115 are sensitive to static. Touch a grounded metal surface before handling bare PCBs.

## Tools Required

| Tool | Notes |
|------|-------|
| Soldering iron (adjustable temp) | 300-350C for through-hole, 230C for heat-set inserts |
| Solder (0.8mm rosin core) | Lead-free recommended |
| Wire strippers | For 22-26 AWG hookup wire |
| Flush cutters | Trimming leads and wire |
| Multimeter | Voltage and continuity testing (essential) |
| Screwdrivers (Phillips #1, #0) | M3 and M2 screws |
| Helping hands / PCB holder | Makes perfboard work much easier |
| Heat-shrink tubing + heat gun | Insulating solder joints on wire runs |
| Hot glue gun (optional) | Strain relief for wires |
| 22-26 AWG stranded hookup wire | Red, black, and 2-3 signal colors. ~1m of each |

## Bill of Materials

### Carrier Board Components

| Ref | Qty | Component | Specs | Notes |
|-----|-----|-----------|-------|-------|
| U1 | 1 | MP1584EN buck converter module | 22x17mm, adj output | Set to 5.0V with trimpot before installing |
| U2 | 1 | ADS1115 breakout board | 16-bit I2C ADC | Socket it (use pin headers) for easy replacement |
| Q1 | 1 | IRLZ44N N-channel MOSFET | TO-220, logic-level | Fully on at 3.3V gate drive |
| R1-R3 | 3 | 10K resistor (1% metal film) | 1/4W, axial | Probe voltage dividers |
| R4 | 1 | 10K resistor | 1/4W, axial | MOSFET gate pulldown |
| C1-C3 | 3 | 0.1uF ceramic capacitor | Axial or radial | ADC input noise filters |
| BZ1 | 1 | Piezo buzzer | 5V passive or active | Soldered directly to board |
| - | 1 | 50x70mm perfboard | 2.54mm pitch, double-sided | The carrier board itself |
| - | 2 | Pin header strips (female) | 2.54mm, for ADS1115 socket | Cut to length for the ADS1115 breakout |
| - | 1 | 8-pin connector / header | 2.0mm pitch | Mates with WT32-SC01 Plus extension connector |

### Panel-Mount Connectors

| Qty | Component | Notes |
|-----|-----------|-------|
| 3 | 2.5mm mono panel-mount jacks | Thermoworks probe connectors (tip + sleeve) |
| 1 | DC barrel jack (panel-mount) | 5.5x2.1mm, 12V input |

### External Components

| Qty | Component | Notes |
|-----|-----------|-------|
| 1 | WT32-SC01 Plus | ESP32-S3 with 3.5" 480x320 capacitive touchscreen |
| 1 | 5015 12V blower fan | 50x50x15mm centrifugal blower |
| 1 | MG90S servo | Standard micro servo for damper control |
| 1-3 | Thermoworks Pro-Series probes | NTC thermistor, 2.5mm mono plug |
| 1 | 12V DC power supply | 2A minimum, barrel plug 5.5x2.1mm |
| 1 | 8-pin ribbon cable (2.0mm) | Included with WT32-SC01 Plus board |

### Enclosure Hardware

| Qty | Component | Notes |
|-----|-----------|-------|
| 4 | M3x8mm screws | Join bezel to rear shell |
| 4 | M3 heat-set threaded inserts | Optional (screws self-tap into PETG) |
| 4 | M3x6mm screws | Mount WT32-SC01 Plus to bezel standoffs |
| 4 | M3x6mm screws | Mount carrier board to shell standoffs (optional) |

---

## Schematic

See the full wiring schematic: [schematic.svg](schematic.svg)

The circuit has five functional sections:

1. **Power supply** - 12V barrel jack through MP1584EN buck converter to 5V rail
2. **Probe voltage dividers** - 3x identical circuits: 3.3V -> 10K resistor -> junction -> NTC probe -> GND, with 0.1uF filter cap at each junction
3. **ADS1115 ADC** - Reads the three probe voltage divider junctions on A0-A2, communicates to ESP32 via I2C
4. **Fan driver** - IRLZ44N MOSFET switches 12V to the blower fan, gate driven by GPIO12 PWM with 10K pulldown
5. **Servo & buzzer** - Direct GPIO connections (GPIO13 servo signal, GPIO14 buzzer)

### Simplified Circuit Diagram

```
                           3.3V (from WT32)
                             |
                     ┌───────┼───────┐
                     |       |       |
                   [10K]   [10K]   [10K]    R1, R2, R3
                     |       |       |
              ┌──────┤───────┤───────┤
              |      |       |       |
            [0.1uF] [0.1uF] [0.1uF]        C1, C2, C3
              |      |       |       |
             GND    GND     GND     |
                     |       |       |
                    NTC     NTC     NTC     Probe thermistors
                     |       |       |      (via 2.5mm jacks)
                    GND     GND     GND

              A0─────┘   A1─┘   A2──┘
              ┌──────────────────────┐
              |      ADS1115        |
              |   (I2C: 0x48)       |
              └──┬───────┬──────────┘
                SDA     SCL
                 |       |
              GPIO10  GPIO11        WT32-SC01 Plus


  12V ────┬──[MP1584EN]──── 5V ────── WT32 5V pin
          |                           Servo VCC
          |
          ├──── Fan (+)
          |       |
          |     Fan (-)
          |       |
          |    Drain
          |   ┌──┤ IRLZ44N (Q1)
          |   |  Gate──────── GPIO12
          |   |  |
          |  [10K] R4 (pulldown)
          |   |  |
          |  GND Source
          |       |
          └──────GND


  GPIO13 ──── Servo Signal (orange wire)
  GPIO14 ──── Buzzer (+)
  GND    ──── Buzzer (-), Servo GND (brown wire)
```

---

## Assembly Instructions

### Overview

The build has 8 phases. Each phase ends with a checkpoint you can verify with a multimeter before moving on. This catches wiring mistakes early when they're easy to fix.

```
Phase 1: Power section (buck converter)
Phase 2: Probe voltage dividers (resistors + caps)
Phase 3: ADS1115 socket
Phase 4: Fan driver (MOSFET circuit)
Phase 5: Buzzer + connectors
Phase 6: Panel-mount components
Phase 7: Wiring harness (ribbon cable + external connections)
Phase 8: Enclosure assembly + final test
```

---

### Phase 1: Power Section

**Goal**: Mount the MP1584EN buck converter module and establish the 12V input and 5V output rails on the perfboard.

> **Before you begin**: Connect 12V to the MP1584EN module **off the perfboard** and adjust the trimpot with a small screwdriver until the output reads **5.0V** on your multimeter. This prevents accidental over-voltage to the ESP32.

#### Steps

1. **Plan your layout.** Place the MP1584EN module in the upper-left area of the 50x70mm perfboard. The module is 22x17mm and has 4 pins (IN+, IN-, OUT+, OUT-). Leave room to the right for the ADS1115 and voltage dividers.

   > ![Photo placeholder](images/phase1-layout-planning.jpg)
   > *Photo: Perfboard with component placement marked (use a marker or tape to outline positions)*

2. **Solder the MP1584EN module.** Insert the 4 pins through the perfboard and solder from the back side. The module should sit flush against the board.

   > ![Photo placeholder](images/phase1-buck-converter-soldered.jpg)
   > *Photo: MP1584EN soldered to perfboard, showing pin positions*

3. **Create power rails.** Using bare wire or solder bridges on the back of the perfboard:
   - Run a **12V trace** (red wire) from the MP1584EN IN+ pad toward the board edge where the barrel jack wires will connect. Also extend it toward the fan driver area (lower-left).
   - Run a **5V trace** (orange wire) from the MP1584EN OUT+ pad toward the right side of the board (where the WT32 and servo connections will go).
   - Run a **GND trace** (black wire) across the bottom of the board, connecting MP1584EN IN- and OUT- together. This is your common ground rail.

   > ![Photo placeholder](images/phase1-power-rails.jpg)
   > *Photo: Back of perfboard showing power rail traces/wires*

4. **Solder barrel jack wires.** Solder two ~10cm wires to the perfboard power input pads:
   - Red wire for 12V (will connect to barrel jack tip/center pin)
   - Black wire for GND (will connect to barrel jack sleeve)
   - Leave the other ends free for now; they'll connect to the panel-mount barrel jack in Phase 6.

#### Checkpoint 1

- [ ] With the barrel jack wires temporarily connected to a 12V supply, measure:
  - 12V between the IN+ pad and GND: **~12V**
  - 5V between the OUT+ pad and GND: **5.0V** (within 4.9-5.1V)
- [ ] Disconnect power before proceeding.

---

### Phase 2: Probe Voltage Dividers

**Goal**: Install the three 10K reference resistors and 0.1uF filter capacitors that form the voltage divider circuits for the temperature probes.

#### How the Probe Circuit Works

Each probe is an NTC (negative temperature coefficient) thermistor. It forms a voltage divider with a fixed 10K resistor:

```
3.3V ──[10K resistor]──┬──[NTC probe]── GND
                        |
                     ADC input
                        |
                     [0.1uF]
                        |
                       GND
```

The ADS1115 reads the voltage at the junction. As the probe heats up, its resistance drops, and the voltage at the junction changes. The firmware converts this voltage to temperature using the Steinhart-Hart equation.

#### Steps

1. **Install R1, R2, R3 (10K 1% resistors).** Place three 10K metal film resistors vertically in a row on the perfboard, to the right of the buck converter. Space them about 10mm apart. One lead of each connects to a 3.3V rail trace; the other lead goes to the junction point.

   > ![Photo placeholder](images/phase2-resistors-placed.jpg)
   > *Photo: Three 10K resistors installed on perfboard*

2. **Install C1, C2, C3 (0.1uF capacitors).** Place each capacitor near its corresponding resistor. One lead connects to the same junction point as the resistor; the other lead connects to the GND rail.

   > ![Photo placeholder](images/phase2-caps-placed.jpg)
   > *Photo: Filter capacitors installed next to resistors*

3. **Create junction traces.** On the back of the perfboard, solder short traces or wire jumpers connecting each resistor-capacitor junction to a pad that will later wire to the ADS1115 analog input (A0, A1, A2).

4. **Run probe jack wires.** From each junction, also run a trace/pad for the wire that will go to the panel-mount 2.5mm probe jack. The other side of the probe jack connects to GND.

   > ![Photo placeholder](images/phase2-divider-wiring.jpg)
   > *Photo: Back of board showing voltage divider wiring*

5. **Create a 3.3V bus.** Run a trace connecting the top of all three 10K resistors together. This 3.3V bus will later connect to the WT32-SC01 Plus 3.3V output.

   > ![Photo placeholder](images/phase2-3v3-bus.jpg)
   > *Photo: 3.3V bus trace connecting all three dividers*

#### Checkpoint 2

- [ ] Continuity test: Each resistor's top lead connects to the 3.3V bus (all three should be connected together).
- [ ] Continuity test: Each capacitor's bottom lead connects to the GND rail.
- [ ] Each junction point (resistor bottom + cap top) is **isolated** from the other two junctions (no shorts between A0, A1, A2 points).
- [ ] Resistance from each junction to GND: **approximately 10K ohms** (reading through the resistor to the 3.3V bus, which is floating).

---

### Phase 3: ADS1115 Socket

**Goal**: Install female pin headers as a socket so the ADS1115 breakout can be inserted and removed without desoldering.

#### Steps

1. **Cut female pin headers.** The ADS1115 breakout board typically has 10 pins (some boards vary). Cut your female header strips to match your specific ADS1115 breakout board's pin layout.

   > ![Photo placeholder](images/phase3-headers-cut.jpg)
   > *Photo: Female pin headers cut to size next to ADS1115 breakout*

2. **Position the socket.** Place the headers on the perfboard to the right of the voltage dividers. Insert the ADS1115 breakout into the headers to verify alignment, then remove the breakout and solder the headers.

   > ![Photo placeholder](images/phase3-socket-soldered.jpg)
   > *Photo: Female header socket soldered to perfboard*

3. **Wire the ADS1115 connections.** On the back of the perfboard, solder wires/traces from the socket pins to:

   | ADS1115 Pin | Connects To |
   |-------------|-------------|
   | VDD | 3.3V bus (same as probe resistors) |
   | GND | GND rail |
   | SDA | Pad for I2C SDA wire (to GPIO10) |
   | SCL | Pad for I2C SCL wire (to GPIO11) |
   | A0 | Probe 1 (Pit) junction |
   | A1 | Probe 2 (Meat 1) junction |
   | A2 | Probe 3 (Meat 2) junction |
   | A3 | Leave unconnected (spare) |
   | ADDR | Connect to GND (sets I2C address 0x48) |

   > ![Photo placeholder](images/phase3-ads1115-wiring.jpg)
   > *Photo: Back of board showing ADS1115 socket wiring*

4. **Insert the ADS1115 breakout.** Gently press the breakout board into the socket. It should seat firmly but be removable.

   > ![Photo placeholder](images/phase3-ads1115-installed.jpg)
   > *Photo: ADS1115 breakout seated in socket*

#### Checkpoint 3

- [ ] ADS1115 VDD pin has continuity to the 3.3V bus.
- [ ] ADS1115 GND pin has continuity to the GND rail.
- [ ] ADS1115 ADDR pin has continuity to GND.
- [ ] A0, A1, A2 each have continuity to their respective probe junction points (and NOT to each other).
- [ ] SDA and SCL pads are isolated (no continuity between them).

---

### Phase 4: Fan Driver (MOSFET Circuit)

**Goal**: Install the IRLZ44N MOSFET and gate pulldown resistor to switch the 12V blower fan via PWM from GPIO12.

#### How the Fan Driver Works

```
        12V
         |
      Fan (+)
         |
      Fan (-)
         |
       Drain
     ┌───┤
     |   | IRLZ44N
     |  Gate ──── GPIO12 (PWM)
     |   |
    [10K] R4 (gate pulldown)
     |   |
    GND Source
          |
         GND
```

The MOSFET acts as a switch. When GPIO12 goes high (3.3V), the MOSFET turns on and current flows through the fan. The 10K pulldown resistor ensures the gate stays low (fan off) when GPIO12 is not actively driving it (e.g., during boot).

#### Steps

1. **Install Q1 (IRLZ44N MOSFET).** The IRLZ44N is in a TO-220 package with three legs: Gate, Drain, Source (left to right, facing the label). Place it in the lower-left area of the perfboard with the metal tab facing the board edge (for heat dissipation).

   > ![Photo placeholder](images/phase4-mosfet-placed.jpg)
   > *Photo: IRLZ44N positioned on perfboard, pin labels visible*

2. **Solder the MOSFET pins.** Solder all three legs from the back. The pins are:
   - **Gate** (left): Will connect to GPIO12 via signal wire
   - **Drain** (center): Will connect to the fan's negative wire
   - **Source** (right): Connects to GND rail

3. **Install R4 (10K gate pulldown resistor).** Solder a 10K resistor between the Gate pin and GND. This keeps the gate pulled low when GPIO12 is floating (during ESP32 boot).

   > ![Photo placeholder](images/phase4-gate-pulldown.jpg)
   > *Photo: 10K resistor connecting MOSFET gate to GND*

4. **Wire the Source to GND.** Connect the Source pin to the GND rail with a trace or wire on the back of the board.

5. **Wire the Drain.** Connect the Drain pin to a pad where the fan's negative wire will attach. Also run a trace from a nearby pad to the 12V rail for the fan's positive wire.

6. **Create fan connection pads.** Create two pads near the board edge for the fan wires:
   - Fan (+): Connected to 12V rail
   - Fan (-): Connected to MOSFET Drain

   > ![Photo placeholder](images/phase4-fan-pads.jpg)
   > *Photo: Fan connection pads on perfboard edge*

7. **Wire the Gate signal.** Run a wire/trace from the Gate to a pad that will connect to GPIO12 through the ribbon cable.

#### Checkpoint 4

- [ ] MOSFET Source to GND: **continuity** (0 ohms).
- [ ] MOSFET Gate to GND: **~10K ohms** (through the pulldown resistor).
- [ ] MOSFET Drain to GND: **no continuity** (MOSFET is off; should read open/very high resistance).
- [ ] Fan (+) pad to 12V rail: **continuity**.
- [ ] Gate pad is isolated from Drain and Source (no accidental shorts).

---

### Phase 5: Buzzer and Remaining Connections

**Goal**: Install the piezo buzzer and create all remaining connection pads for the ribbon cable.

#### Steps

1. **Install BZ1 (piezo buzzer).** Solder the buzzer to the perfboard. Note the polarity:
   - **Positive (+)**: Connects to a pad for GPIO14
   - **Negative (-)**: Connects to GND rail

   > ![Photo placeholder](images/phase5-buzzer-installed.jpg)
   > *Photo: Piezo buzzer soldered to perfboard*

2. **Create servo connection pads.** The servo has three wires:
   - **Orange** (signal): Pad for GPIO13
   - **Red** (VCC): Pad connected to 5V rail
   - **Brown** (GND): Pad connected to GND rail

   > ![Photo placeholder](images/phase5-servo-pads.jpg)
   > *Photo: Servo connection pads labeled on perfboard*

3. **Create the ribbon cable header.** Create a row of pads (or solder a pin header) for the 8-pin ribbon cable connection. The pin mapping is:

   | Ribbon Pin | Signal | Perfboard Connection |
   |------------|--------|---------------------|
   | 1 | GPIO10 (SDA) | ADS1115 SDA pad |
   | 2 | GPIO11 (SCL) | ADS1115 SCL pad |
   | 3 | GPIO12 (Fan PWM) | MOSFET Gate pad |
   | 4 | GPIO13 (Servo PWM) | Servo signal pad |
   | 5 | GPIO14 (Buzzer) | Buzzer (+) pad |
   | 6 | 3.3V | 3.3V bus (probe dividers) |
   | 7 | 5V | 5V rail (from buck converter) |
   | 8 | GND | GND rail |

   > ![Photo placeholder](images/phase5-ribbon-header.jpg)
   > *Photo: Ribbon cable header/pads on perfboard edge*

4. **Final perfboard wiring.** Verify all traces on the back of the board connect correctly. Use your multimeter to check every connection in the table above.

   > ![Photo placeholder](images/phase5-board-back-complete.jpg)
   > *Photo: Complete back of perfboard showing all traces and wiring*

   > ![Photo placeholder](images/phase5-board-front-complete.jpg)
   > *Photo: Complete front of perfboard with all components installed*

#### Checkpoint 5

- [ ] Buzzer (+) to GPIO14 pad: **continuity**.
- [ ] Buzzer (-) to GND rail: **continuity**.
- [ ] Servo signal pad to GPIO13 pad: **continuity**.
- [ ] Servo VCC pad to 5V rail: **continuity**.
- [ ] Every ribbon cable pad connects to its intended destination and nothing else.
- [ ] **Full board continuity audit** - check for shorts between:
  - 12V and GND: **no continuity**
  - 5V and GND: **no continuity**
  - 3.3V and GND: **no continuity**
  - 12V and 5V: **no continuity**
  - All signal lines isolated from each other

---

### Phase 6: Panel-Mount Components

**Goal**: Install the probe jacks and barrel jack into the 3D-printed enclosure rear shell, and wire them to the carrier board.

#### Steps

1. **Thread the 2.5mm probe jacks.** Insert the three 2.5mm mono panel-mount jacks through the probe holes in the rear shell bottom edge. Secure each with its mounting nut from inside.

   > ![Photo placeholder](images/phase6-probe-jacks-mounted.jpg)
   > *Photo: Three probe jacks installed in rear shell, viewed from outside*

2. **Thread the DC barrel jack.** Insert the panel-mount barrel jack through its hole on the rear shell. Secure with its mounting nut.

   > ![Photo placeholder](images/phase6-barrel-jack-mounted.jpg)
   > *Photo: Barrel jack installed in rear shell*

3. **Wire the probe jacks.** For each 2.5mm mono jack, solder two wires (~8cm each):
   - **Tip lug**: Connect to the corresponding probe junction pad on the carrier board (Probe 1 = A0 junction, Probe 2 = A1 junction, Probe 3 = A2 junction)
   - **Sleeve lug**: Connect to the GND rail on the carrier board

   > ![Photo placeholder](images/phase6-probe-jack-wiring.jpg)
   > *Photo: Close-up of solder connections on a probe jack lug*

4. **Wire the barrel jack.** Solder the pre-cut wires from Phase 1 to the barrel jack:
   - **Center/tip pin**: Red wire (12V from carrier board)
   - **Sleeve pin**: Black wire (GND from carrier board)

   > ![Photo placeholder](images/phase6-barrel-jack-wiring.jpg)
   > *Photo: Barrel jack wired to carrier board power input*

#### Checkpoint 6

- [ ] With a probe plugged into each jack, check continuity from the jack's tip lug to the corresponding junction pad on the carrier board.
- [ ] Each jack's sleeve lug has continuity to the carrier board GND rail.
- [ ] Barrel jack center pin has continuity to the carrier board 12V input.
- [ ] Barrel jack sleeve has continuity to the carrier board GND rail.

---

### Phase 7: Wiring Harness

**Goal**: Connect the carrier board to the WT32-SC01 Plus via the ribbon cable, and attach the fan and servo leads.

#### Steps

1. **Connect the ribbon cable.** Plug the 8-pin ribbon cable (included with the WT32-SC01 Plus) between:
   - The WT32-SC01 Plus extension connector
   - The ribbon cable header/pads on the carrier board

   > **Important**: Verify pin 1 orientation. The WT32-SC01 Plus extension connector has a pin 1 indicator. Match it to pin 1 on your carrier board header (GPIO10/SDA).

   > ![Photo placeholder](images/phase7-ribbon-cable-connected.jpg)
   > *Photo: Ribbon cable connecting carrier board to WT32-SC01 Plus*

2. **Connect the fan.** Solder or connect the 5015 blower fan's wires to the fan pads on the carrier board:
   - Red wire to Fan (+) pad (12V)
   - Black wire to Fan (-) pad (MOSFET drain)

   Route the fan wire through the right-side cable slot in the rear shell.

   > ![Photo placeholder](images/phase7-fan-connected.jpg)
   > *Photo: Fan wires connected to carrier board*

3. **Connect the servo.** Plug the servo connector into the servo pads (or solder if using direct wiring):
   - Orange wire to servo signal pad (GPIO13)
   - Red wire to servo VCC pad (5V)
   - Brown wire to servo GND pad

   Route the servo wire through the right-side cable slot in the rear shell.

   > ![Photo placeholder](images/phase7-servo-connected.jpg)
   > *Photo: Servo connector wired to carrier board*

#### Checkpoint 7 - Power-On Test

This is the first time you'll power everything up together. **Double-check all connections before applying power.**

1. **Pre-power checklist:**
   - [ ] 12V and GND are not shorted (check barrel jack with multimeter)
   - [ ] 5V and GND are not shorted
   - [ ] No loose wires or solder blobs that could cause shorts
   - [ ] Ribbon cable is properly oriented (pin 1 matches)

2. **Apply power.** Connect the 12V power supply to the barrel jack.

3. **Verify voltages:**
   - [ ] 5V rail: **5.0V** (measure at WT32 5V pin or servo VCC pad)
   - [ ] 3.3V rail: **3.3V** (measure at 3.3V bus or ADS1115 VDD)
   - [ ] WT32-SC01 Plus display turns on and shows the boot screen or setup wizard

4. **Verify I2C (probe readings):**
   - [ ] If firmware is flashed, the touchscreen should show temperature readings. Plug in a probe and verify you see a temperature value (room temp, roughly 70-75F / 21-24C).
   - [ ] If no firmware yet, this will be verified after flashing.

5. **Verify fan:**
   - [ ] If firmware is flashed and setup wizard runs, the hardware test step should spin the fan briefly.
   - [ ] Manual test: Briefly touch a jumper wire from the MOSFET gate to 3.3V. The fan should spin. **Remove the jumper quickly** (this is full speed with no PWM control).

6. **Verify servo:**
   - [ ] The setup wizard hardware test should sweep the servo.

7. **Verify buzzer:**
   - [ ] The setup wizard hardware test should beep the buzzer.

---

### Phase 8: Enclosure Assembly

**Goal**: Mount everything in the 3D-printed enclosure and close it up.

#### Steps

1. **Install heat-set inserts (optional).** If using threaded inserts, press M3 heat-set inserts into the 4 screw bosses on the rear shell using a soldering iron set to 230C. Let cool completely before proceeding.

   > ![Photo placeholder](images/phase8-heat-set-inserts.jpg)
   > *Photo: Heat-set inserts being pressed into rear shell screw bosses*

2. **Mount the WT32-SC01 Plus in the front bezel.** Place the display face-down into the bezel. The glass sits against the bezel lip. Secure with 4x M3x6mm screws through the PCB mounting holes.

   > ![Photo placeholder](images/phase8-display-mounted.jpg)
   > *Photo: WT32-SC01 Plus mounted in front bezel, viewed from back*

3. **Mount the carrier board in the rear shell.** Place the carrier board onto the standoffs in the rear shell. Optionally secure with M3x6mm screws.

   > ![Photo placeholder](images/phase8-carrier-mounted.jpg)
   > *Photo: Carrier board seated on rear shell standoffs*

4. **Route all wires.** Ensure wires are neatly routed and won't get pinched when closing the enclosure:
   - Ribbon cable from carrier board to WT32 extension connector
   - Fan and servo wires through the right-side cable slots
   - Probe jack and barrel jack wires between shell panel and carrier board

   > ![Photo placeholder](images/phase8-wire-routing.jpg)
   > *Photo: All wires neatly routed inside the enclosure*

5. **Close the enclosure.** Mate the front bezel to the rear shell. Align the screw bosses and secure with 4x M3x8mm screws.

   > ![Photo placeholder](images/phase8-enclosure-closed.jpg)
   > *Photo: Fully assembled and closed enclosure*

6. **Attach the kickstand (optional).** Press the kickstand hinge tab into the slot on the rear of the shell. It should click in and swing freely.

   > ![Photo placeholder](images/phase8-kickstand.jpg)
   > *Photo: Kickstand attached and deployed*

---

## First Boot & Firmware

### Flashing Firmware (First Time - USB Required)

1. Connect the WT32-SC01 Plus to your computer via USB-C.
2. Build and flash the firmware:
   ```bash
   pio run -e wt32_sc01_plus --target upload
   ```
3. Upload the web UI files to LittleFS:
   ```bash
   pio run -e wt32_sc01_plus --target uploadfs
   ```
4. The setup wizard should appear on the touchscreen.

### Setup Wizard

The touchscreen will walk you through:
1. **Welcome** - Project name and version
2. **Units** - Choose Fahrenheit or Celsius
3. **Wi-Fi** - Scan the QR code with your phone to connect to the `BBQ-Setup` AP, then enter your Wi-Fi credentials in the captive portal
4. **Probe check** - Plug in probes and verify live readings
5. **Hardware test** - Fan spins, servo sweeps, buzzer beeps
6. **Done** - Dashboard appears

### Future Firmware Updates (OTA)

After initial USB flash, update wirelessly:
1. Navigate to `http://bbq.local/update` in your browser
2. Upload the new `.bin` file
3. Device reboots with updated firmware (config and session data preserved)

---

## Troubleshooting

### No Display / Nothing Happens on Power-On

- Check 5V rail with multimeter (should be 5.0V)
- Check that the MP1584EN trimpot is set correctly
- Verify ribbon cable orientation (pin 1 alignment)
- Try powering the WT32 directly via its USB-C port to isolate power issues

### Probe Reads "---" (Disconnected)

- Check continuity from the probe jack tip lug to the ADS1115 analog input
- Check that the 10K resistor in that probe's voltage divider is connected to 3.3V
- Try a different probe in the same jack
- Try the same probe in a different jack to isolate jack vs. probe vs. ADC channel

### Probe Reads "ERR" (Shorted)

- The probe wires may be shorted. Check the 2.5mm jack for solder bridges
- Check for shorts between the junction point and GND

### Probe Temperature is Wrong

- Verify the 10K reference resistor is actually 10K (1% tolerance = 9.9K to 10.1K)
- Check that the correct Steinhart-Hart coefficients are configured for your probe type
- Use the calibration offset in settings (ice water = 32F/0C, boiling water = 212F/100C adjusted for altitude)

### Fan Doesn't Spin

- Check 12V at the fan (+) pad
- Check that the MOSFET gate is receiving the PWM signal (put multimeter on AC voltage mode on the gate pin while the firmware is commanding fan output)
- Manually test: briefly touch gate to 3.3V. If the fan spins, the MOSFET circuit is fine and the issue is the signal from GPIO12
- Check the 10K gate pulldown - if it's shorted or too low value, the gate may not reach threshold

### Servo Doesn't Move

- Check 5V at the servo VCC pad
- Check that GPIO13 is connected to the servo signal wire (orange)
- Try the servo with a known-good signal source (e.g., Arduino servo sweep example)

### Buzzer Doesn't Sound

- Check polarity (+ to GPIO14, - to GND)
- Check that GPIO14 is connected through the ribbon cable
- Some passive buzzers need a specific frequency drive. Active buzzers just need DC.

### Wi-Fi Won't Connect

- During setup: the `BBQ-Setup` AP should appear. If not, factory reset (hold touchscreen 10 seconds during boot splash).
- After setup: verify SSID and password in config. Factory reset and re-run wizard if needed.
- Check that your router is 2.4 GHz (ESP32 does not support 5 GHz).

### I2C Errors / ADS1115 Not Found

- Check SDA and SCL wiring (GPIO10 = SDA, GPIO11 = SCL; don't swap them)
- Check that ADS1115 ADDR pin is connected to GND (address 0x48)
- Check that the ADS1115 breakout is fully seated in its socket
- Run an I2C scanner sketch to verify the device is detected

---

## Pin Reference

Quick reference for all connections between the carrier board and WT32-SC01 Plus:

| WT32 GPIO | Function | Carrier Board Destination |
|-----------|----------|---------------------------|
| GPIO10 | I2C SDA | ADS1115 SDA pin |
| GPIO11 | I2C SCL | ADS1115 SCL pin |
| GPIO12 | Fan PWM | IRLZ44N Gate (via R4 pulldown) |
| GPIO13 | Servo PWM | MG90S servo signal (orange wire) |
| GPIO14 | Buzzer | Piezo buzzer (+) |
| 3.3V | Power | ADS1115 VDD, probe voltage dividers |
| 5V | Power (input) | MP1584EN OUT+, servo VCC |
| GND | Ground | Common ground rail (everything) |

---

## Photo Checklist

As you build, take photos at these key moments. They'll be useful for others following this guide and for troubleshooting if something isn't working. Replace the placeholder images above with your actual build photos.

- [ ] `phase1-layout-planning.jpg` - Component placement marked on perfboard
- [ ] `phase1-buck-converter-soldered.jpg` - MP1584EN installed
- [ ] `phase1-power-rails.jpg` - Power rail traces on back of board
- [ ] `phase2-resistors-placed.jpg` - Three 10K resistors installed
- [ ] `phase2-caps-placed.jpg` - 0.1uF capacitors installed
- [ ] `phase2-divider-wiring.jpg` - Voltage divider wiring on back
- [ ] `phase2-3v3-bus.jpg` - 3.3V bus connecting dividers
- [ ] `phase3-headers-cut.jpg` - Female headers for ADS1115 socket
- [ ] `phase3-socket-soldered.jpg` - Socket installed on perfboard
- [ ] `phase3-ads1115-wiring.jpg` - ADS1115 socket wiring on back
- [ ] `phase3-ads1115-installed.jpg` - ADS1115 breakout seated in socket
- [ ] `phase4-mosfet-placed.jpg` - IRLZ44N on perfboard
- [ ] `phase4-gate-pulldown.jpg` - 10K gate pulldown resistor
- [ ] `phase4-fan-pads.jpg` - Fan connection pads
- [ ] `phase5-buzzer-installed.jpg` - Piezo buzzer soldered
- [ ] `phase5-servo-pads.jpg` - Servo connection pads
- [ ] `phase5-ribbon-header.jpg` - Ribbon cable header on perfboard
- [ ] `phase5-board-back-complete.jpg` - Complete back of perfboard
- [ ] `phase5-board-front-complete.jpg` - Complete front of perfboard
- [ ] `phase6-probe-jacks-mounted.jpg` - Probe jacks in rear shell
- [ ] `phase6-barrel-jack-mounted.jpg` - Barrel jack in rear shell
- [ ] `phase6-probe-jack-wiring.jpg` - Probe jack solder connections
- [ ] `phase6-barrel-jack-wiring.jpg` - Barrel jack wiring
- [ ] `phase7-ribbon-cable-connected.jpg` - Ribbon cable connection
- [ ] `phase7-fan-connected.jpg` - Fan wires on carrier board
- [ ] `phase7-servo-connected.jpg` - Servo wiring
- [ ] `phase8-heat-set-inserts.jpg` - Heat-set insert installation
- [ ] `phase8-display-mounted.jpg` - Display in front bezel
- [ ] `phase8-carrier-mounted.jpg` - Carrier board in rear shell
- [ ] `phase8-wire-routing.jpg` - Wire routing inside enclosure
- [ ] `phase8-enclosure-closed.jpg` - Fully assembled enclosure
- [ ] `phase8-kickstand.jpg` - Kickstand attached
