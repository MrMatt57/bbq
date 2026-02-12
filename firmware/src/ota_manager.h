#pragma once

#include "config.h"

#ifndef NATIVE_BUILD
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#endif

/// Manages over-the-air firmware updates via AsyncElegantOTA.
/// Registers the /update endpoint on the existing AsyncWebServer.
class OtaManager {
public:
    OtaManager();

    /// Register OTA routes on the given web server.
    /// Call once from setup() after web server is created and WiFi is up.
    void begin(AsyncWebServer* server);

    /// Service OTA events. Call every loop().
    /// Mostly a no-op since ElegantOTA handles updates asynchronously,
    /// but keeps the interface consistent with other managers.
    void update();

    /// Whether an OTA update is currently in progress.
    bool isUpdating() const;

    /// Progress of current update as a percentage (0-100).
    uint8_t getProgress() const;

private:
    bool     _updating;
    uint8_t  _progress;
    bool     _initialized;

#ifndef NATIVE_BUILD
    AsyncWebServer* _server;
#endif
};
