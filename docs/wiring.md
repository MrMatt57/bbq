# Wiring

All components on the carrier perfboard are connected with short soldered traces. External connections use wire runs to panel-mount jacks and the display board.

## Wiring Diagram

```
Panel-mount DC barrel jack (12V in)
  │
  └─► Carrier Board 12V rail
       ├─► MP1584EN buck converter IN → OUT (set to 5.0V)
       │     ├─► WT32-SC01 Plus 5V (via extension cable)
       │     └─► Servo VCC
       │
       ├─► IRLZ44N MOSFET drain
       │     └─► Fan +12V (fan GND to MOSFET source)
       │         Gate ◄── GPIO12 (with 10K pulldown to GND)
       │
       └─► GND rail (common ground)

WT32-SC01 Plus Extension Connector (8-pin ribbon cable)
  GPIO10 (SDA) ──► ADS1115 SDA (on carrier board)
  GPIO11 (SCL) ──► ADS1115 SCL (on carrier board)
  GPIO12       ──► IRLZ44N gate (on carrier board)
  GPIO13       ──► Servo signal (wire to servo)
  GPIO14       ──► Buzzer (on carrier board)
  3.3V         ──► ADS1115 VDD + voltage divider supply
  5V           ──► From MP1584EN output
  GND          ──► Common ground rail

Probe voltage dividers (on carrier board, x3)
  3.3V ──[10K 1% resistor]──┬──► ADS1115 AINx
                             │
                       [0.1uF cap]
                             │
    Panel-mount 2.5mm jack ──┴──► GND
    (tip=signal, sleeve=GND, polarity doesn't matter)
```

## Pin Assignments

| GPIO | Function |
|------|----------|
| 10   | I2C SDA (ADS1115) |
| 11   | I2C SCL (ADS1115) |
| 12   | Fan PWM (via MOSFET) |
| 13   | Servo PWM |
| 14   | Buzzer |
| 21   | Spare |

## Carrier Board Layout (50x70mm)

```
┌──────────────────────────────────────────────────┐
│  MP1584EN        ADS1115 (socketed)              │
│  buck conv       ┌─────────┐                     │
│  ┌──────┐        │ A0 A1 A2│ A3(spare)           │
│  │12V→5V│        └─────────┘                     │
│  └──────┘                                        │
│                  R1  R2  R3   (10K 1% resistors)  │
│  IRLZ44N        C1  C2  C3   (0.1uF caps)       │
│  ┌──┐  10K                                       │
│  │FET│  pulldown  [BUZZER]                       │
│  └──┘                                            │
│                                                  │
│  Connectors: [12V in] [Fan] [Servo] [WT32 ext]  │
│              [Probe1] [Probe2] [Probe3]          │
└──────────────────────────────────────────────────┘
```

Components on the carrier board (~20 solder joints total):
- MP1584EN mini buck converter (12V→5V, 22x17mm module)
- ADS1115 breakout (pin-header socketed for replacement)
- IRLZ44N MOSFET (TO-220, logic-level, fully on at 3.3V for <1A loads) + 10K gate pulldown resistor
- 3x 10K 1% metal film resistors (voltage dividers)
- 3x 0.1uF ceramic capacitors (ADC input filters)
- Piezo buzzer (soldered directly)
- Power distribution traces (12V, 5V, 3.3V, GND rails)

Connections off the carrier board (short wire runs):
- 8-pin ribbon to WT32-SC01 Plus extension connector (2.0mm pitch, cable included with board)
- Wire pairs to panel-mount 2.5mm probe jacks (soldered to jack lugs)
- Wire pair to panel-mount DC barrel jack
- 2-pin to fan (JST-XH or direct solder)
- 3-pin to servo (standard servo connector)
