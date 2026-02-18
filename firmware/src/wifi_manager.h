#pragma once

#include "config.h"

#ifndef NATIVE_BUILD
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#endif

/// Manages Wi-Fi connectivity: attempts STA connection using stored
/// credentials via WiFiManager captive portal, falls back to AP mode
/// for initial setup, and handles mDNS registration.
class WifiManager {
public:
    WifiManager();

    /// Attempt STA connection with optional credentials; fall back to AP mode.
    /// If ssid/password are provided, they are set as WiFiManager defaults.
    /// If empty, WiFiManager uses its saved credentials or starts AP.
    /// Call once from setup().
    void begin(const char* ssid = nullptr, const char* password = nullptr);

    /// Monitor connection health and reconnect if needed. Call every loop().
    void update();

    /// Whether the device is connected to a Wi-Fi network (STA mode).
    bool isConnected() const;

    /// Whether the device is running its own AP (setup mode).
    bool isAPMode() const;

    /// Current IP address as a human-readable string.
    String getIPAddress() const;

    /// SSID of the connected network (STA), AP name (AP mode), or empty string.
    String getSSID() const;

    /// Signal strength in dBm (STA mode only).
    int getRSSI() const;

    /// Returns the QR code data string for the AP setup network.
    /// Format: WIFI:T:WPA;S:BBQ-Setup;P:bbqsetup;;
    String getAPQRCodeData() const;

    /// Disconnect from Wi-Fi and stop auto-reconnect.
    /// User must call reconnect() or startAP() to resume.
    void disconnect();

    /// Trigger a reconnection attempt.
    void reconnect();

    /// Switch to AP mode (e.g., user-triggered from the UI).
    void startAP();

private:
    /// Start mDNS responder.
    void setupMDNS();

    /// Internal reconnect logic with backoff.
    void attemptReconnect();

    bool          _apMode;              // Currently in AP mode?
    bool          _connected;           // STA connected?
    uint8_t       _reconnectAttempts;   // Current reconnect attempts
    uint8_t       _bootFailCount;       // Boot-time connection failures
    unsigned long _lastReconnectMs;     // Last reconnect attempt timestamp
    unsigned long _reconnectIntervalMs; // Current backoff interval
    bool          _mdnsStarted;         // mDNS registered?

    static const uint8_t  MAX_BOOT_FAILURES    = 3;
    static const uint8_t  MAX_RECONNECT_ATTEMPTS = 20;
    static const unsigned long RECONNECT_BASE_MS = 5000;
    static const unsigned long RECONNECT_MAX_MS  = 60000;
    static const unsigned long CONNECTION_CHECK_MS = 10000;

    unsigned long _lastConnectionCheckMs;

#ifndef NATIVE_BUILD
    WiFiManager   _wifiManager;
#endif
};
