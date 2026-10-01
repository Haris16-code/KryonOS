#pragma once

#include <Arduino.h>

class WebManager {
public:
    static bool init();
    static bool startServer();
    static void stopServer();
    static bool isServerRunning();
    static bool isActive();
    static String getIPAddress();

    // Authentication Credential Management (stored in NVS)
    static String getAdminUsername();
    static String getAdminPassword();
    static void setAdminCredentials(const String& username, const String& password);
};
