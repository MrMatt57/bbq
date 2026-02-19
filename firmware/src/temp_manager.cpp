#include "temp_manager.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

// ADC channel mapping: probe index -> ADS1115 channel
const uint8_t TempManager::_adcChannels[NUM_PROBES] = {
    ADC_CHANNEL_PIT,
    ADC_CHANNEL_MEAT1,
    ADC_CHANNEL_MEAT2
};

TempManager::TempManager()
    : _emaAlpha(TEMP_EMA_ALPHA)
    , _useFahrenheit(true)
    , _lastSampleMs(0)
{
    for (uint8_t i = 0; i < NUM_PROBES; i++) {
        _rawADC[i] = 0;
        _filteredTempC[i] = 0.0f;
        _status[i] = ProbeStatus::OPEN_CIRCUIT;
        _firstReading[i] = true;
        // ProbeConfig default-initialized with THERM_A/B/C and offset 0
    }
}

bool TempManager::begin() {
#ifndef NATIVE_BUILD
    Wire.begin(PIN_SDA, PIN_SCL);

    if (!_ads.begin(ADS1115_ADDR, &Wire)) {
        Serial.println("[TEMP] ADS1115 not found at 0x48!");
        return false;
    }

    // Set gain to GAIN_ONE (+/- 4.096V range)
    _ads.setGain(GAIN_ONE);

    Serial.println("[TEMP] ADS1115 initialized OK.");
#endif
    _lastSampleMs = 0;
    return true;
}

void TempManager::update() {
#ifndef NATIVE_BUILD
    unsigned long now = millis();
    if (now - _lastSampleMs < TEMP_SAMPLE_INTERVAL_MS) {
        return;  // Not time to sample yet
    }
    _lastSampleMs = now;

    for (uint8_t i = 0; i < NUM_PROBES; i++) {
        // Read raw ADC value from ADS1115 single-ended
        int16_t raw = _ads.readADC_SingleEnded(_adcChannels[i]);
        _rawADC[i] = raw;

        // Check for probe errors
        if (raw >= ERROR_PROBE_OPEN_THRESHOLD) {
            _status[i] = ProbeStatus::OPEN_CIRCUIT;
            _firstReading[i] = true;  // Reset EMA on reconnect
            continue;
        }
        if (raw <= ERROR_PROBE_SHORT_THRESHOLD) {
            _status[i] = ProbeStatus::SHORT_CIRCUIT;
            _firstReading[i] = true;
            continue;
        }

        // Convert ADC to resistance
        float resistance = adcToResistance(raw);
        if (resistance <= 0.0f) {
            _status[i] = ProbeStatus::SHORT_CIRCUIT;
            _firstReading[i] = true;
            continue;
        }

        // Convert resistance to temperature in Celsius
        float tempC = resistanceToTempC(resistance, _probeConfig[i]);

        // Apply calibration offset
        tempC += _probeConfig[i].offset;

        // Apply EMA filter
        if (_firstReading[i]) {
            _filteredTempC[i] = tempC;
            _firstReading[i] = false;
        } else {
            _filteredTempC[i] = _emaAlpha * tempC + (1.0f - _emaAlpha) * _filteredTempC[i];
        }

        _status[i] = ProbeStatus::OK;
    }
#endif
}

float TempManager::getTemp(uint8_t probe) const {
    if (probe >= NUM_PROBES) return 0.0f;
    if (_status[probe] != ProbeStatus::OK) return 0.0f;

    if (_useFahrenheit) {
        return cToF(_filteredTempC[probe]);
    }
    return _filteredTempC[probe];
}

float TempManager::getTempC(uint8_t probe) const {
    if (probe >= NUM_PROBES) return 0.0f;
    if (_status[probe] != ProbeStatus::OK) return 0.0f;
    return _filteredTempC[probe];
}

bool TempManager::isConnected(uint8_t probe) const {
    if (probe >= NUM_PROBES) return false;
    return _status[probe] == ProbeStatus::OK;
}

ProbeStatus TempManager::getStatus(uint8_t probe) const {
    if (probe >= NUM_PROBES) return ProbeStatus::OPEN_CIRCUIT;
    return _status[probe];
}

int16_t TempManager::getRawADC(uint8_t probe) const {
    if (probe >= NUM_PROBES) return 0;
    return _rawADC[probe];
}

void TempManager::setEMAAlpha(float alpha) {
    if (alpha > 0.0f && alpha <= 1.0f) {
        _emaAlpha = alpha;
    }
}

void TempManager::setOffset(uint8_t probe, float offset) {
    if (probe < NUM_PROBES) {
        _probeConfig[probe].offset = offset;
    }
}

void TempManager::setCoefficients(uint8_t probe, float a, float b, float c) {
    if (probe < NUM_PROBES) {
        _probeConfig[probe].a = a;
        _probeConfig[probe].b = b;
        _probeConfig[probe].c = c;
    }
}

void TempManager::setUseFahrenheit(bool useF) {
    _useFahrenheit = useF;
}

float TempManager::adcToResistance(int16_t raw) const {
    // Voltage divider: Vout = Vref * R_therm / (R_ref + R_therm)
    // ADC value proportional to voltage: raw / ADC_MAX = Vout / Vref
    // Solving for R_therm:
    //   R_therm = R_ref * raw / (ADC_MAX - raw)
    // But the standard NTC voltage divider with pullup:
    //   Vout = Vcc * R_ref / (R_ref + R_therm)
    //   raw / ADC_MAX = R_ref / (R_ref + R_therm)
    //   R_therm = R_ref * (ADC_MAX / raw - 1)
    if (raw <= 0) return 0.0f;
    return REFERENCE_RESISTANCE * ((float)ADC_MAX_VALUE / (float)raw - 1.0f);
}

float TempManager::resistanceToTempC(float resistance, const ProbeConfig& cfg) const {
    // Steinhart-Hart equation:
    // 1/T = A + B * ln(R) + C * (ln(R))^3
    // T is in Kelvin
    float lnR = logf(resistance);
    float lnR3 = lnR * lnR * lnR;
    float invT = cfg.a + cfg.b * lnR + cfg.c * lnR3;

    if (invT == 0.0f) return 0.0f;

    float tempK = 1.0f / invT;
    float tempC = tempK - 273.15f;
    return tempC;
}
