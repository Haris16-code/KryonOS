#pragma once

#include <Arduino.h>

class WebManager {
public:
    // Try reading wifi.txt and connecting to WiFi. Returns true if connected.
    // Also starts the Async Web Server if connected.
    static bool init();
    
    // Check if WiFi is active
    static bool isActive();
    
    // Get the current IP address as string
    static String getIPAddress();
};
