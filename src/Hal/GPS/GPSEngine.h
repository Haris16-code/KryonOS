#ifndef GPS_ENGINE_H
#define GPS_ENGINE_H

#include <Arduino.h>
#include <vector>
#include <map>

struct GPSLocationData {
    double latitude;
    double longitude;
    double altitudeMeters;
    double speedKmh;
    double speedKnots;
    double courseDeg;
    double accuracyMeters;
    uint64_t timestampUtcMs;
    uint32_t lastFixMillis;
    bool isValid;
};

struct GPSStatusData {
    bool enabled;
    bool hasFix;
    char fixType[8]; // "none", "2D", "3D"
    char chipModel[24]; // "u-blox MIA-M10Q", "Quectel L76K", "GNSS Receiver"
    uint16_t satellitesTracked;
    uint16_t satellitesVisible;
    uint16_t satellitesWithSignal;
    double avgSnr;
    double hdop;
    double vdop;
    uint32_t ageMs;
    uint32_t sentencesParsed;
    uint32_t checksumErrors;
    uint32_t charsProcessed;
    uint32_t currentBaud;
    bool baudLocked;
    uint32_t searchTimeSec;
    char recentNmea[4][96];
    uint8_t recentNmeaCount;
};

struct GPSTimeData {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t centisecond;
    uint64_t epochMs;
    bool isValid;
};

class GPSEngine {
public:
    static void init();
    static bool isAvailable();
    static bool isMasterEnabled();
    static void setMasterEnabled(bool en);
    static bool isRunning();

    // App reference counting & power management
    static bool acquireHold(const String& pkg);
    static bool releaseHold(const String& pkg);
    static void releaseAllHolds(const String& pkg);
    static int getActiveHoldCount();

    // Data queries (Thread-safe)
    static bool hasFix();
    static bool getLocation(GPSLocationData& out);
    static bool getLastKnownLocation(GPSLocationData& out);
    static bool getStatus(GPSStatusData& out);
    static bool getTime(GPSTimeData& out);
    static bool syncSystemTime();

    // Native C++ Fast Geodesy
    static double distanceBetween(double lat1, double lon1, double lat2, double lon2);
    static double courseTo(double lat1, double lon1, double lat2, double lon2);
    static bool isInsideCircle(double lat, double lon, double cLat, double cLon, double radiusMeters);

    // Configuration & Telemetry
    static bool setRate(uint8_t rateHz);
    static uint32_t getBaud();
    static bool isBaudLocked();

    // Background Worker
    static void gpsTaskWorker(void* param);

private:
    static bool startHardware();
    static void stopHardware();
    static void updateCacheFromParser();

    static bool s_initialized;
    static bool s_masterEnabled;
    static bool s_hardwareRunning;
    static TaskHandle_t s_taskHandle;
    static SemaphoreHandle_t s_mutex;

    static uint32_t s_currentBaud;
    static bool s_baudLocked;
    static int s_candidateIdx;
    static uint32_t s_probeStartTime;
    static uint32_t s_probeStartPassedChecksum;

    static std::map<String, bool> s_activeHolds;
    static GPSLocationData s_latestLocation;
    static GPSLocationData s_lastKnownLocation;
    static GPSStatusData s_latestStatus;
    static GPSTimeData s_latestTime;
};

#endif // GPS_ENGINE_H

