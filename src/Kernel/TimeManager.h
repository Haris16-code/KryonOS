#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <time.h>

class TimeManager {
public:
    static void init();
    static void syncNTP();
    static void setManualTime(int year, int month, int day, int hour, int minute);
    static void setTimezone(String tzOffset);
    static void setTimeFormat(bool use24h);
    static void setNTPEnabled(bool enabled);
    
    // Getters
    static String getFormattedTime();
    static String getFormattedDate();
    static int getYear();
    static int getMonth();
    static int getDay();
    static int getSeconds();
    
    // Preferences state
    static String currentTimezone;
    static bool use24hFormat;
    static bool ntpEnabled;

private:
    static void savePreferences();
    static void loadPreferences();
};

#endif // TIME_MANAGER_H
