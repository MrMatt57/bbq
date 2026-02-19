#pragma once

inline float celsiusToFahrenheit(float c) { return c * 9.0f / 5.0f + 32.0f; }
inline float fahrenheitToCelsius(float f) { return (f - 32.0f) * 5.0f / 9.0f; }
