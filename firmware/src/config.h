#pragma once

// --- Pin Assignments (WT32-SC01 Plus Extension Connector) ---
#define PIN_SDA         10
#define PIN_SCL         11
#define PIN_FAN_PWM     12
#define PIN_SERVO       13
#define PIN_BUZZER      14
#define PIN_SPARE       21

// --- ADC Channels (ADS1115) ---
#define ADC_CHANNEL_PIT   0
#define ADC_CHANNEL_MEAT1 1
#define ADC_CHANNEL_MEAT2 2
#define ADC_CHANNEL_SPARE 3

// --- ADS1115 ---
#define ADS1115_ADDR    0x48
#ifndef NATIVE_BUILD
#define ADS1115_GAIN    GAIN_ONE    // +/- 4.096V range
#endif

// --- Voltage Divider ---
#define REFERENCE_RESISTANCE  10000.0   // 10K ohm reference resistor
#define ADC_MAX_VOLTAGE       4.096     // ADS1115 at GAIN_ONE
#define ADC_MAX_VALUE         32767     // 16-bit signed

// --- Steinhart-Hart Coefficients (Thermoworks Pro-Series) ---
#define THERM_A  7.3431401e-04
#define THERM_B  2.1574370e-04
#define THERM_C  9.5156860e-08

// --- PID Defaults ---
#define PID_KP          4.0
#define PID_KI          0.02
#define PID_KD          5.0
#define PID_SAMPLE_MS   4000    // Compute every 4 seconds
#define PID_OUTPUT_MIN  0.0
#define PID_OUTPUT_MAX  100.0

// --- Fan Control ---
#define FAN_PWM_FREQ       25000   // 25 kHz
#define FAN_PWM_CHANNEL    0
#define FAN_PWM_RESOLUTION 8       // 8-bit (0-255)
#define FAN_KICKSTART_PCT  75      // Kick-start at 75%
#define FAN_KICKSTART_MS   500     // for 500ms
#define FAN_MIN_SPEED      15      // Minimum sustained speed %
#define FAN_LONGPULSE_THRESHOLD 10 // Below 10%, use long-pulse mode
#define FAN_LONGPULSE_CYCLE_MS 10000 // 10-second cycle

// --- Servo (Damper) ---
#define SERVO_MIN_US    544     // 0 degrees
#define SERVO_MAX_US    2400    // 180 degrees
#define DAMPER_CLOSED   0       // Servo angle for closed
#define DAMPER_OPEN     90      // Servo angle for full open

// --- Split-Range Fan+Damper ---
#define FAN_ON_THRESHOLD   30   // Default fan-on threshold (runtime value from configManager)

// --- Temperature Reading ---
#define TEMP_SAMPLE_INTERVAL_MS  1000   // Read probes every 1 second
#define TEMP_AVG_SAMPLES         4      // Average 4 readings
#define TEMP_EMA_ALPHA           0.2    // EMA smoothing factor (lower = smoother, less derivative noise)

// --- Lid-Open Detection ---
#define LID_OPEN_DROP_PCT   6    // 6% drop below setpoint triggers lid-open
#define LID_OPEN_RECOVER_PCT 2   // Recovered when within 2% of setpoint

// --- Cook Session ---
#define SESSION_BUFFER_SIZE     600     // RAM buffer samples
#define SESSION_SAMPLE_INTERVAL 5000    // 5 seconds between data points
#define SESSION_FLUSH_INTERVAL  60000   // Flush to LittleFS every 60 seconds
#define SESSION_FILE_PATH       "/session.dat"

// --- Config ---
#define CONFIG_FILE_PATH  "/config.json"

// --- Web Server ---
#define WEB_PORT          80
#define WS_PATH           "/ws"
#define WS_MAX_CLIENTS    4
#define WS_SEND_INTERVAL  1500   // Send data every 1.5 seconds

// --- Alarms ---
#define ALARM_PIT_BAND_DEFAULT  15.0    // +/- 15F
#define ALARM_BUZZER_FREQ       2000    // 2kHz tone
#define ALARM_BUZZER_DURATION   500     // 500ms beep
#define ALARM_BUZZER_PAUSE      500     // 500ms pause between beeps

// --- Error Detection ---
#define ERROR_PROBE_OPEN_THRESHOLD   32000  // ADC value indicating open circuit
#define ERROR_PROBE_SHORT_THRESHOLD  100    // ADC value indicating short
#define ERROR_FIREOUT_RATE           2.0    // Degrees F per minute decline
#define ERROR_FIREOUT_DURATION_MS    600000 // 10 minutes of decline

// --- Display ---
#define DISPLAY_WIDTH   480
#define DISPLAY_HEIGHT  320

// --- Misc ---
#define WIFI_HOSTNAME       "bbq"
#define MDNS_HOSTNAME       "bbq"
#define AP_SSID             "BBQ-Setup"
#define AP_PASSWORD         "bbqsetup"
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION    "dev"
#endif
