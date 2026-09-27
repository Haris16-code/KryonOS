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
};
