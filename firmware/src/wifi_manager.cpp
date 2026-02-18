#include "wifi_manager.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

WifiManager::WifiManager()
    : _apMode(false)
    , _connected(false)
    , _reconnectAttempts(0)
    , _bootFailCount(0)
    , _lastReconnectMs(0)
    , _reconnectIntervalMs(RECONNECT_BASE_MS)
    , _mdnsStarted(false)
    , _lastConnectionCheckMs(0)
{
}

void WifiManager::begin(const char* ssid, const char* password) {
#ifndef NATIVE_BUILD
    Serial.println("[WIFI] Initializing WiFi manager...");

    // Set hostname before connecting
    WiFi.setHostname(WIFI_HOSTNAME);

    // Configure WiFiManager
    _wifiManager.setConfigPortalTimeout(180);  // 3 minute timeout on portal
    _wifiManager.setConnectTimeout(15);         // 15 second connect timeout
    _wifiManager.setDebugOutput(true);

    // If explicit credentials are provided, try connecting with them first
    if (ssid != nullptr && ssid[0] != '\0') {
        Serial.printf("[WIFI] Attempting connection to '%s'...\n", ssid);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);

        // Wait up to 15 seconds for connection
        unsigned long startMs = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startMs < 15000) {
            delay(250);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            _connected = true;
            _apMode = false;
            Serial.printf("[WIFI] Connected to '%s', IP: %s\n",
                          ssid, WiFi.localIP().toString().c_str());
            setupMDNS();
            return;
        }

        Serial.printf("[WIFI] Failed to connect to '%s'\n", ssid);
        _bootFailCount++;
    }

    // Try WiFiManager autoConnect (uses saved credentials)
    // Attempt up to MAX_BOOT_FAILURES times before falling back to AP
    while (_bootFailCount < MAX_BOOT_FAILURES) {
        Serial.printf("[WIFI] Auto-connect attempt %d/%d...\n",
                      _bootFailCount + 1, MAX_BOOT_FAILURES);

        // autoConnect returns true if connected, false if portal timed out
        // It will use saved credentials first, then open portal if they fail
        WiFi.mode(WIFI_STA);

        // Try a simple reconnect with saved credentials
        WiFi.begin();
        unsigned long startMs = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startMs < 15000) {
            delay(250);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            _connected = true;
            _apMode = false;
            Serial.printf("[WIFI] Connected! IP: %s\n",
                          WiFi.localIP().toString().c_str());
            setupMDNS();
            return;
        }

        _bootFailCount++;
        Serial.printf("[WIFI] Connection attempt failed (%d/%d)\n",
                      _bootFailCount, MAX_BOOT_FAILURES);
    }

    // All attempts failed -- fall back to AP captive portal
    Serial.println("[WIFI] All connection attempts failed, starting AP mode...");
    startAP();
#endif
}

void WifiManager::update() {
#ifndef NATIVE_BUILD
    unsigned long now = millis();

    // If we are in AP mode, let WiFiManager handle portal clients.
    // WiFiManager's process() services the captive portal in non-blocking mode.
    if (_apMode) {
        _wifiManager.process();

        // Check if a client has connected us to a network via the portal
        if (WiFi.status() == WL_CONNECTED) {
            _apMode = false;
            _connected = true;
            _reconnectAttempts = 0;
            _reconnectIntervalMs = RECONNECT_BASE_MS;
            Serial.printf("[WIFI] Connected via portal! IP: %s\n",
                          WiFi.localIP().toString().c_str());
            setupMDNS();
        }
        return;
    }

    // Station mode: periodic connection health check
    if (now - _lastConnectionCheckMs < CONNECTION_CHECK_MS) {
        return;
    }
    _lastConnectionCheckMs = now;

    bool currentlyConnected = (WiFi.status() == WL_CONNECTED);

    if (currentlyConnected && !_connected) {
        // Just reconnected
        _connected = true;
        _reconnectAttempts = 0;
        _reconnectIntervalMs = RECONNECT_BASE_MS;
        Serial.printf("[WIFI] Reconnected! IP: %s, RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        if (!_mdnsStarted) {
            setupMDNS();
        }
    }
    else if (!currentlyConnected && _connected) {
        // Just lost connection
        _connected = false;
        Serial.println("[WIFI] Connection lost, will attempt reconnection...");
    }

    // If disconnected, attempt reconnect with exponential backoff
    if (!_connected) {
        attemptReconnect();
    }
#endif
}

bool WifiManager::isConnected() const {
#ifndef NATIVE_BUILD
    return _connected && (WiFi.status() == WL_CONNECTED);
#else
    return false;
#endif
}

bool WifiManager::isAPMode() const {
    return _apMode;
}

String WifiManager::getIPAddress() const {
#ifndef NATIVE_BUILD
    if (_apMode) {
        return WiFi.softAPIP().toString();
    }
    if (_connected) {
        return WiFi.localIP().toString();
    }
#endif
    return String("0.0.0.0");
}

String WifiManager::getSSID() const {
#ifndef NATIVE_BUILD
    if (_apMode) {
        return String(AP_SSID);
    }
    if (_connected) {
        return WiFi.SSID();
    }
#endif
    return String("");
}

int WifiManager::getRSSI() const {
#ifndef NATIVE_BUILD
    if (_connected && !_apMode) {
        return WiFi.RSSI();
    }
#endif
    return 0;
}

String WifiManager::getAPQRCodeData() const {
    // Standard WiFi QR code format
    return String("WIFI:T:WPA;S:" AP_SSID ";P:" AP_PASSWORD ";;");
}

void WifiManager::disconnect() {
#ifndef NATIVE_BUILD
    Serial.println("[WIFI] Manual disconnect requested.");
    _connected = false;
    _apMode = false;
    _reconnectAttempts = MAX_RECONNECT_ATTEMPTS; // Prevent auto-reconnect
    WiFi.disconnect(true);
#endif
}

void WifiManager::reconnect() {
#ifndef NATIVE_BUILD
    Serial.println("[WIFI] Manual reconnect requested.");
    _reconnectAttempts = 0;
    _reconnectIntervalMs = RECONNECT_BASE_MS;
    _connected = false;

    WiFi.disconnect();
    delay(100);
    WiFi.reconnect();
#endif
}

void WifiManager::startAP() {
#ifndef NATIVE_BUILD
    Serial.println("[WIFI] Starting AP mode for configuration...");
    Serial.printf("[WIFI] AP SSID: %s, Password: %s\n", AP_SSID, AP_PASSWORD);

    _apMode = true;
    _connected = false;

    // Stop any existing connection
    WiFi.disconnect(true);
    delay(100);

    // Start WiFiManager in non-blocking AP mode
    _wifiManager.setConfigPortalBlocking(false);
    _wifiManager.startConfigPortal(AP_SSID, AP_PASSWORD);

    Serial.printf("[WIFI] AP started. IP: %s\n",
                  WiFi.softAPIP().toString().c_str());
    Serial.printf("[WIFI] QR Code data: %s\n", getAPQRCodeData().c_str());
#endif
}

void WifiManager::setupMDNS() {
#ifndef NATIVE_BUILD
    if (_mdnsStarted) {
        MDNS.end();
    }

    if (MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", WEB_PORT);
        _mdnsStarted = true;
        Serial.printf("[WIFI] mDNS started: http://%s.local\n", MDNS_HOSTNAME);
    } else {
        Serial.println("[WIFI] mDNS failed to start!");
        _mdnsStarted = false;
    }
#endif
}

void WifiManager::attemptReconnect() {
#ifndef NATIVE_BUILD
    unsigned long now = millis();

    // Respect backoff interval
    if (now - _lastReconnectMs < _reconnectIntervalMs) {
        return;
    }
    _lastReconnectMs = now;

    if (_reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
        Serial.println("[WIFI] Max reconnect attempts reached, switching to AP mode.");
        startAP();
        return;
    }

    _reconnectAttempts++;
    Serial.printf("[WIFI] Reconnect attempt %d/%d (backoff: %lu ms)...\n",
                  _reconnectAttempts, MAX_RECONNECT_ATTEMPTS, _reconnectIntervalMs);

    WiFi.reconnect();

    // Exponential backoff: double the interval, up to the max
    _reconnectIntervalMs = min(_reconnectIntervalMs * 2, RECONNECT_MAX_MS);
#endif
}
