#include "ota_manager.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <Update.h>
#endif

OtaManager::OtaManager()
    : _updating(false)
    , _progress(0)
    , _initialized(false)
#ifndef NATIVE_BUILD
    , _server(nullptr)
#endif
{
}

void OtaManager::begin(AsyncWebServer* server) {
#ifndef NATIVE_BUILD
    if (server == nullptr) {
        Serial.println("[OTA] Error: null server pointer, OTA not initialized.");
        return;
    }

    _server = server;

    // Set up ElegantOTA callbacks for progress tracking
    AsyncElegantOTA.onStart([this]() {
        _updating = true;
        _progress = 0;
        Serial.println("[OTA] Update started.");
    });

    AsyncElegantOTA.onProgress([this](size_t current, size_t total) {
        if (total > 0) {
            _progress = (uint8_t)((current * 100) / total);
        }
        // Print progress every 10%
        static uint8_t lastPrinted = 255;
        uint8_t decile = _progress / 10;
        if (decile != lastPrinted) {
            lastPrinted = decile;
            Serial.printf("[OTA] Progress: %u%% (%u / %u bytes)\n",
                          _progress, (unsigned)current, (unsigned)total);
        }
    });

    AsyncElegantOTA.onEnd([this](bool success) {
        _updating = false;
        if (success) {
            Serial.println("[OTA] Update successful! Rebooting...");
            delay(500);
            ESP.restart();
        } else {
            Serial.println("[OTA] Update failed.");
            _progress = 0;
        }
    });

    // Register the /update endpoint on the existing web server
    AsyncElegantOTA.begin(_server);

    _initialized = true;
    Serial.println("[OTA] ElegantOTA initialized at /update");
#endif
}

void OtaManager::update() {
#ifndef NATIVE_BUILD
    // AsyncElegantOTA handles everything asynchronously via the web server.
    // This method is provided for interface consistency with other managers.
    // ElegantOTA v2.x uses AsyncElegantOTA.loop() -- call it if available.
    AsyncElegantOTA.loop();
#endif
}

bool OtaManager::isUpdating() const {
    return _updating;
}

uint8_t OtaManager::getProgress() const {
    return _progress;
}
