#include "TimeManager.h"
#include "../File System/FileSystem.h"
#include <esp_sntp.h>

String TimeManager::currentTimezone = "UTC0";
bool TimeManager::use24hFormat = true;
bool TimeManager::ntpEnabled = true;

static const char* PREF_FILE = "/local/config_time.txt";

void TimeManager::loadPreferences() {
    if (FileSystem::exists(PREF_FILE)) {
        String content = FileSystem::readTextFile(PREF_FILE);
        // Format: TZ|24H|NTP (e.g. "UTC-5|1|1")
        int sep1 = content.indexOf('|');
        int sep2 = content.indexOf('|', sep1 + 1);
        if (sep1 != -1 && sep2 != -1) {
            currentTimezone = content.substring(0, sep1);
            use24hFormat = (content.substring(sep1 + 1, sep2) == "1");
            ntpEnabled = (content.substring(sep2 + 1) == "1");
        }
    }
}

void TimeManager::savePreferences() {
    String content = currentTimezone + "|" + (use24hFormat ? "1" : "0") + "|" + (ntpEnabled ? "1" : "0");
    FileSystem::writeTextFile(PREF_FILE, content.c_str());
}

void TimeManager::init() {
    loadPreferences();
    setenv("TZ", currentTimezone.c_str(), 1);
    tzset();
    // Defer esp_sntp_init() until syncNTP() to prevent LwIP assertions
}

void TimeManager::syncNTP() {
    if (!ntpEnabled) return;
    // Configure and start SNTP only when WiFi is active
    esp_sntp_stop();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.nist.gov");
    esp_sntp_init();
}

void TimeManager::setManualTime(int year, int month, int day, int hour, int minute) {
    struct tm t;
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = 0;
    t.tm_isdst = -1;
    
    time_t epoch = mktime(&t);
    
    struct timeval now = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&now, NULL);
}

void TimeManager::setTimezone(String tzOffset) {
    currentTimezone = tzOffset;
    setenv("TZ", currentTimezone.c_str(), 1);
    tzset();
    savePreferences();
}

void TimeManager::setTimeFormat(bool use24h) {
    use24hFormat = use24h;
    savePreferences();
}

void TimeManager::setNTPEnabled(bool enabled) {
    ntpEnabled = enabled;
    savePreferences();
    if (enabled) syncNTP();
}

String TimeManager::getFormattedTime() {
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char buffer[16];
    if (use24hFormat) {
        snprintf(buffer, sizeof(buffer), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    } else {
        int h = timeinfo.tm_hour % 12;
        if (h == 0) h = 12;
        const char* ampm = (timeinfo.tm_hour >= 12) ? "PM" : "AM";
        snprintf(buffer, sizeof(buffer), "%02d:%02d %s", h, timeinfo.tm_min, ampm);
    }
    return String(buffer);
}

String TimeManager::getFormattedDate() {
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char buffer[16];
    // DD/MM/YYYY format
    snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
    return String(buffer);
}

int TimeManager::getYear() {
    time_t now; time(&now);
    struct tm timeinfo; localtime_r(&now, &timeinfo);
    return timeinfo.tm_year + 1900;
}

int TimeManager::getMonth() {
    time_t now; time(&now);
    struct tm timeinfo; localtime_r(&now, &timeinfo);
    return timeinfo.tm_mon + 1;
}

int TimeManager::getDay() {
    time_t now; time(&now);
    struct tm timeinfo; localtime_r(&now, &timeinfo);
    return timeinfo.tm_mday;
}

int TimeManager::getSeconds() {
    time_t now; time(&now);
    struct tm timeinfo; localtime_r(&now, &timeinfo);
    return timeinfo.tm_sec;
}
