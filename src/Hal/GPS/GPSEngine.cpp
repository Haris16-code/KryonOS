#include "GPSEngine.h"
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <sys/time.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../Boards/Board.h"
#include "../../FileSystem/FileSystem.h"
#include <ArduinoJson.h>

#ifndef BOARD_POWERON
#define BOARD_POWERON 10
#endif

#ifndef BOARD_GPS_RX_PIN
#define BOARD_GPS_RX_PIN 44
#endif

#ifndef BOARD_GPS_TX_PIN
#define BOARD_GPS_TX_PIN 43
#endif

#ifndef BOARD_GPS_DEFAULT_BAUD
#define BOARD_GPS_DEFAULT_BAUD 38400
#endif

// Candidate baud rates:
// 38400: Factory default for LilyGO T-Deck Plus u-blox MIA-M10Q
// 9600: Factory default for LilyGO T-Deck Plus Quectel L76K (AT6558R)
// 115200: High-speed fallback for units configured with custom firmwares
static const uint32_t CANDIDATE_BAUDS[] = { 38400, 9600, 115200 };
static const int NUM_CANDIDATE_BAUDS = sizeof(CANDIDATE_BAUDS) / sizeof(CANDIDATE_BAUDS[0]);

static HardwareSerial s_serialGPS(1);
static TinyGPSPlus s_tinyGps;

// Custom listeners for satellite constellation visibility in view (GSV sentences)
static TinyGPSCustom s_satGpgsv(s_tinyGps, "GPGSV", 3);
static TinyGPSCustom s_satGlgsv(s_tinyGps, "GLGSV", 3);
static TinyGPSCustom s_satGagsv(s_tinyGps, "GAGSV", 3);
static TinyGPSCustom s_satGbgsv(s_tinyGps, "GBGSV", 3);
static TinyGPSCustom s_satBdgsv(s_tinyGps, "BDGSV", 3);
static TinyGPSCustom s_satGngsv(s_tinyGps, "GNGSV", 3);

bool GPSEngine::s_initialized = false;
bool GPSEngine::s_masterEnabled = true;
bool GPSEngine::s_hardwareRunning = false;
TaskHandle_t GPSEngine::s_taskHandle = nullptr;
SemaphoreHandle_t GPSEngine::s_mutex = nullptr;

uint32_t GPSEngine::s_currentBaud = BOARD_GPS_DEFAULT_BAUD;
bool GPSEngine::s_baudLocked = false;
int GPSEngine::s_candidateIdx = 0;
uint32_t GPSEngine::s_probeStartTime = 0;
uint32_t GPSEngine::s_probeStartPassedChecksum = 0;
static uint32_t s_lastSentenceMs = 0;
static uint32_t s_searchStartTime = 0;

std::map<String, bool> GPSEngine::s_activeHolds;
GPSLocationData GPSEngine::s_latestLocation = {0};
GPSLocationData GPSEngine::s_lastKnownLocation = {0};
GPSStatusData GPSEngine::s_latestStatus = {0};
GPSTimeData GPSEngine::s_latestTime = {0};

static const char* GPS_SETTINGS_FILE = "/local/system/gps_settings.json";
static const char* GPS_LAST_FIX_FILE = "/local/system/gps_last_loc.json";

// Direct multi-constellation NMEA line parser state (supports GP, GN, BD, GB, GL, GA, QZ and GGA, RMC, GLL, GNS, GSA, GSV, VTG)
static char s_nmeaLineBuf[160];
static uint16_t s_nmeaLineLen = 0;

static char s_recentNmea[4][96] = {{0}};
static uint8_t s_recentNmeaCount = 0;

static char s_detectedChipModel[24] = "Detecting...";
static bool s_chipIdentified = false;

static double s_directLat = 0.0;
static double s_directLon = 0.0;
static double s_directAlt = 0.0;
static double s_directSpeedKnots = 0.0;
static double s_directSpeedKmh = 0.0;
static double s_directCourse = 0.0;
static double s_directHdop = 99.9;
static double s_directVdop = 99.9;
static uint16_t s_directSatsUsed = 0;
static uint16_t s_directSatsVisible = 0;
static uint16_t s_directSatsWithSnr = 0;
static double s_directAvgSnr = 0.0;

static uint16_t s_gsvSnrCount = 0;
static uint32_t s_gsvSnrSum = 0;
static uint32_t s_directPassedChecksum = 0;

static uint32_t s_directFixMillis = 0;
static bool s_directHasCoords = false;
static uint8_t s_gsaFixMode = 0; // 1=none, 2=2D, 3=3D

static uint16_t s_directYear = 0;
static uint8_t s_directMonth = 0;
static uint8_t s_directDay = 0;
static uint8_t s_directHour = 0;
static uint8_t s_directMin = 0;
static uint8_t s_directSec = 0;
static uint8_t s_directCentisec = 0;
static bool s_directTimeValid = false;
static bool s_directDateValid = false;

static int hexCharVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool verifyNmeaChecksum(const char* line, size_t len) {
    if (len < 6 || line[0] != '$') return false;
    const char* star = strchr(line, '*');
    if (!star || (size_t)(star - line) + 3 > len) return false;
    uint8_t calc = 0;
    for (const char* p = line + 1; p < star; ++p) {
        calc ^= (uint8_t)(*p);
    }
    int hi = hexCharVal(star[1]);
    int lo = hexCharVal(star[2]);
    if (hi < 0 || lo < 0) return false;
    uint8_t expected = (uint8_t)((hi << 4) | lo);
    return calc == expected;
}

// Extract N-th comma-separated field (0 = sentence header e.g. "$GNRMC")
static bool getNmeaField(const char* line, int fieldIndex, char* out, size_t maxLen) {
    if (!out || maxLen == 0) return false;
    out[0] = '\0';
    int currentField = 0;
    const char* start = line;
    for (const char* p = line; ; ++p) {
        if (*p == ',' || *p == '*' || *p == '\0' || *p == '\r' || *p == '\n') {
            if (currentField == fieldIndex) {
                size_t copyLen = (size_t)(p - start);
                if (copyLen >= maxLen) copyLen = maxLen - 1;
                memcpy(out, start, copyLen);
                out[copyLen] = '\0';
                return true;
            }
            if (*p == '*' || *p == '\0' || *p == '\r' || *p == '\n') {
                break;
            }
            currentField++;
            start = p + 1;
        }
    }
    return false;
}

// Convert NMEA DDMM.MMMMM / DDDMM.MMMMM to decimal degrees
static bool parseNmeaCoord(const char* valStr, const char* dirStr, double& outDeg) {
    if (!valStr || !dirStr || valStr[0] == '\0' || dirStr[0] == '\0') return false;
    const char* dot = strchr(valStr, '.');
    if (!dot || (dot - valStr) < 3) return false;

    double raw = atof(valStr);
    if (raw <= 0.0) return false;

    int degDigits = (int)(dot - valStr) - 2;
    char degBuf[8] = {0};
    if (degDigits <= 0 || degDigits >= (int)sizeof(degBuf)) return false;
    memcpy(degBuf, valStr, degDigits);
    degBuf[degDigits] = '\0';

    int deg = atoi(degBuf);
    double mins = atof(valStr + degDigits);
    if (mins < 0.0 || mins >= 60.0) return false;

    double dec = (double)deg + (mins / 60.0);
    if (dirStr[0] == 'S' || dirStr[0] == 's' || dirStr[0] == 'W' || dirStr[0] == 'w') {
        dec = -dec;
    } else if (dirStr[0] != 'N' && dirStr[0] != 'n' && dirStr[0] != 'E' && dirStr[0] != 'e') {
        return false;
    }

    if (fabs(dec) < 0.0001 || fabs(dec) > 180.0) return false;
    outDeg = dec;
    return true;
}

static void parseNmeaTimeField(const char* timeStr) {
    if (!timeStr || strlen(timeStr) < 6) return;
    char buf[3] = {0};
    buf[0] = timeStr[0]; buf[1] = timeStr[1];
    int hh = atoi(buf);
    buf[0] = timeStr[2]; buf[1] = timeStr[3];
    int mm = atoi(buf);
    buf[0] = timeStr[4]; buf[1] = timeStr[5];
    int ss = atoi(buf);
    if (hh >= 0 && hh < 24 && mm >= 0 && mm < 60 && ss >= 0 && ss < 60) {
        s_directHour = (uint8_t)hh;
        s_directMin = (uint8_t)mm;
        s_directSec = (uint8_t)ss;
        const char* dot = strchr(timeStr, '.');
        if (dot && dot[1] >= '0' && dot[1] <= '9') {
            int cs = (dot[1] - '0') * 10;
            if (dot[2] >= '0' && dot[2] <= '9') cs += (dot[2] - '0');
            s_directCentisec = (uint8_t)cs;
        }
        s_directTimeValid = true;
    }
}

static void parseNmeaDateField(const char* dateStr) {
    if (!dateStr || strlen(dateStr) < 6) return;
    char buf[3] = {0};
    buf[0] = dateStr[0]; buf[1] = dateStr[1];
    int dd = atoi(buf);
    buf[0] = dateStr[2]; buf[1] = dateStr[3];
    int mo = atoi(buf);
    buf[0] = dateStr[4]; buf[1] = dateStr[5];
    int yy = atoi(buf);
    if (dd >= 1 && dd <= 31 && mo >= 1 && mo <= 12 && yy >= 20) {
        s_directDay = (uint8_t)dd;
        s_directMonth = (uint8_t)mo;
        s_directYear = (uint16_t)(2000 + yy);
        s_directDateValid = true;
    }
}

static void recordRecentNmea(const char* line, const char* type3) {
    int slot = 3;
    if (strcmp(type3, "RMC") == 0 || strcmp(type3, "GNS") == 0) slot = 0;
    else if (strcmp(type3, "GGA") == 0 || strcmp(type3, "GLL") == 0) slot = 1;
    else if (strcmp(type3, "GSA") == 0 || strcmp(type3, "VTG") == 0) slot = 2;
    else if (strcmp(type3, "GSV") == 0) slot = 3;

    strncpy(s_recentNmea[slot], line, sizeof(s_recentNmea[slot]) - 1);
    s_recentNmea[slot][sizeof(s_recentNmea[slot]) - 1] = '\0';

    uint8_t count = 0;
    for (int i = 0; i < 4; ++i) {
        if (s_recentNmea[i][0] != '\0') count++;
    }
    s_recentNmeaCount = count;
}

static void sendChipSpecificOptimization() {
    if (strcmp(s_detectedChipModel, "Quectel L76K") == 0) {
        // Quectel L76K / AT6558: Enable GPS + BeiDou + GLONASS + QZSS concurrent tracking
        s_serialGPS.print("$PCAS04,7*1E\r\n");
        // Enable full NMEA output: GGA, GLL, GSA, GSV, RMC, VTG, ZDA, ANT
        s_serialGPS.print("$PCAS03,1,1,1,1,1,1,1,1,0,0,,,0,0*02\r\n");
        s_serialGPS.flush();
        Serial.println("[GPS] Quectel L76K optimized: multi-constellation (GPS+BDS+GLONASS) enabled.");
    } else if (strcmp(s_detectedChipModel, "u-blox MIA-M10Q") == 0) {
        // u-blox MIA-M10Q: Enable standard sentences
        s_serialGPS.print("$PUBX,40,RMC,0,1,0,0,0,0*46\r\n");
        s_serialGPS.print("$PUBX,40,GGA,0,1,0,0,0,0*5B\r\n");
        s_serialGPS.print("$PUBX,40,GSA,0,1,0,0,0,0*4F\r\n");
        s_serialGPS.print("$PUBX,40,GSV,0,1,0,0,0,0*58\r\n");
        s_serialGPS.print("$PUBX,40,GLL,0,1,0,0,0,0*5C\r\n");
        s_serialGPS.flush();
        Serial.println("[GPS] u-blox MIA-M10Q optimized: NMEA 4.10 enabled.");
    }
}

static void processDirectNmeaSentence(const char* line, size_t len) {
    if (!verifyNmeaChecksum(line, len)) return;
    s_directPassedChecksum++;

    // Chip identification heuristics
    if (!s_chipIdentified) {
        if (strncmp(line, "$BD", 3) == 0 || strncmp(line, "$PCAS", 5) == 0 || strstr(line, "AT6558") != nullptr || strstr(line, "Quectel") != nullptr) {
            strncpy(s_detectedChipModel, "Quectel L76K", sizeof(s_detectedChipModel));
            s_chipIdentified = true;
            Serial.printf("[GPS] Detected GNSS Chip: %s\n", s_detectedChipModel);
            sendChipSpecificOptimization();
        } else if (strncmp(line, "$PUBX", 5) == 0 || strstr(line, "u-blox") != nullptr || strstr(line, "M10") != nullptr || strncmp(line + 3, "GNS", 3) == 0) {
            strncpy(s_detectedChipModel, "u-blox MIA-M10Q", sizeof(s_detectedChipModel));
            s_chipIdentified = true;
            Serial.printf("[GPS] Detected GNSS Chip: %s\n", s_detectedChipModel);
            sendChipSpecificOptimization();
        }
    }

    char hdr[16] = {0};
    if (!getNmeaField(line, 0, hdr, sizeof(hdr))) return;
    size_t hdrLen = strlen(hdr);
    if (hdrLen < 6) return;

    const char* type3 = hdr + (hdrLen - 3);
    recordRecentNmea(line, type3);

    char f1[24], f2[24], f3[24], f4[24], f5[24], f6[24], f7[24], f8[24], f9[24];

    if (strcmp(type3, "RMC") == 0) {
        getNmeaField(line, 1, f1, sizeof(f1)); // time
        getNmeaField(line, 2, f2, sizeof(f2)); // status ('A' = valid fix, 'V' = void/warning)
        getNmeaField(line, 3, f3, sizeof(f3)); // lat
        getNmeaField(line, 4, f4, sizeof(f4)); // N/S
        getNmeaField(line, 5, f5, sizeof(f5)); // lon
        getNmeaField(line, 6, f6, sizeof(f6)); // E/W
        getNmeaField(line, 7, f7, sizeof(f7)); // speed knots
        getNmeaField(line, 8, f8, sizeof(f8)); // course
        getNmeaField(line, 9, f9, sizeof(f9)); // date

        parseNmeaTimeField(f1);
        parseNmeaDateField(f9);

        if (f2[0] == 'A') {
            double lat = 0.0, lon = 0.0;
            if (parseNmeaCoord(f3, f4, lat) && parseNmeaCoord(f5, f6, lon)) {
                s_directLat = lat;
                s_directLon = lon;
                s_directFixMillis = millis();
                s_directHasCoords = true;

                if (f7[0] != '\0') {
                    s_directSpeedKnots = atof(f7);
                    s_directSpeedKmh = s_directSpeedKnots * 1.852;
                }
                if (f8[0] != '\0') {
                    s_directCourse = atof(f8);
                }
            }
        }
    } else if (strcmp(type3, "GGA") == 0) {
        getNmeaField(line, 1, f1, sizeof(f1)); // time
        getNmeaField(line, 2, f2, sizeof(f2)); // lat
        getNmeaField(line, 3, f3, sizeof(f3)); // N/S
        getNmeaField(line, 4, f4, sizeof(f4)); // lon
        getNmeaField(line, 5, f5, sizeof(f5)); // E/W
        getNmeaField(line, 6, f6, sizeof(f6)); // quality (0=invalid, 1=GPS, 2=DGPS, 4=RTK)
        getNmeaField(line, 7, f7, sizeof(f7)); // numSV
        getNmeaField(line, 8, f8, sizeof(f8)); // hdop
        getNmeaField(line, 9, f9, sizeof(f9)); // alt

        parseNmeaTimeField(f1);

        if (f7[0] != '\0') {
            int sv = atoi(f7);
            if (sv >= 0) s_directSatsUsed = (uint16_t)sv;
        }
        if (f8[0] != '\0') {
            double h = atof(f8);
            if (h > 0.0 && h < 99.9) s_directHdop = h;
        }
        if (f9[0] != '\0') {
            s_directAlt = atof(f9);
        }

        if (f6[0] >= '1' && f6[0] <= '6') {
            double lat = 0.0, lon = 0.0;
            if (parseNmeaCoord(f2, f3, lat) && parseNmeaCoord(f4, f5, lon)) {
                s_directLat = lat;
                s_directLon = lon;
                s_directFixMillis = millis();
                s_directHasCoords = true;
            }
        }
    } else if (strcmp(type3, "GNS") == 0) {
        getNmeaField(line, 1, f1, sizeof(f1)); // time
        getNmeaField(line, 2, f2, sizeof(f2)); // lat
        getNmeaField(line, 3, f3, sizeof(f3)); // N/S
        getNmeaField(line, 4, f4, sizeof(f4)); // lon
        getNmeaField(line, 5, f5, sizeof(f5)); // E/W
        getNmeaField(line, 6, f6, sizeof(f6)); // mode
        getNmeaField(line, 7, f7, sizeof(f7)); // numSV
        getNmeaField(line, 8, f8, sizeof(f8)); // hdop
        getNmeaField(line, 9, f9, sizeof(f9)); // alt

        parseNmeaTimeField(f1);

        if (f7[0] != '\0') {
            int sv = atoi(f7);
            if (sv >= 0) s_directSatsUsed = (uint16_t)sv;
        }
        if (f8[0] != '\0') {
            double h = atof(f8);
            if (h > 0.0 && h < 99.9) s_directHdop = h;
        }
        if (f9[0] != '\0') {
            s_directAlt = atof(f9);
        }

        if (f6[0] != 'N' && f6[0] != '\0') {
            double lat = 0.0, lon = 0.0;
            if (parseNmeaCoord(f2, f3, lat) && parseNmeaCoord(f4, f5, lon)) {
                s_directLat = lat;
                s_directLon = lon;
                s_directFixMillis = millis();
                s_directHasCoords = true;
            }
        }
    } else if (strcmp(type3, "GLL") == 0) {
        getNmeaField(line, 1, f1, sizeof(f1)); // lat
        getNmeaField(line, 2, f2, sizeof(f2)); // N/S
        getNmeaField(line, 3, f3, sizeof(f3)); // lon
        getNmeaField(line, 4, f4, sizeof(f4)); // E/W
        getNmeaField(line, 5, f5, sizeof(f5)); // time
        getNmeaField(line, 6, f6, sizeof(f6)); // status

        parseNmeaTimeField(f5);

        if (f6[0] == 'A') {
            double lat = 0.0, lon = 0.0;
            if (parseNmeaCoord(f1, f2, lat) && parseNmeaCoord(f3, f4, lon)) {
                s_directLat = lat;
                s_directLon = lon;
                s_directFixMillis = millis();
                s_directHasCoords = true;
            }
        }
    } else if (strcmp(type3, "GSA") == 0) {
        getNmeaField(line, 2, f2, sizeof(f2));
        if (f2[0] >= '1' && f2[0] <= '3') {
            s_gsaFixMode = (uint8_t)(f2[0] - '0');
        }
        uint16_t gsaSats = 0;
        for (int idx = 3; idx <= 14; ++idx) {
            if (getNmeaField(line, idx, f3, sizeof(f3)) && f3[0] != '\0') {
                gsaSats++;
            }
        }
        if (gsaSats > 0) {
            s_directSatsUsed = gsaSats;
        }
        if (getNmeaField(line, 16, f6, sizeof(f6)) && f6[0] != '\0') {
            double h = atof(f6);
            if (h > 0.0 && h < 99.9) s_directHdop = h;
        }
        if (getNmeaField(line, 17, f7, sizeof(f7)) && f7[0] != '\0') {
            double v = atof(f7);
            if (v > 0.0 && v < 99.9) s_directVdop = v;
        }
    } else if (strcmp(type3, "GSV") == 0) {
        getNmeaField(line, 1, f1, sizeof(f1)); // numMsg
        getNmeaField(line, 2, f2, sizeof(f2)); // msgNum
        getNmeaField(line, 3, f3, sizeof(f3)); // numSV
        int numMsg = atoi(f1);
        int msgNum = atoi(f2);
        int numSv = atoi(f3);

        char talker[3] = { hdr[1], hdr[2], '\0' };
        static int s_gsvCurrentSatsInView = 0;

        if (msgNum == 1) {
            if (strcmp(talker, "GP") == 0 || strcmp(talker, "GN") == 0) {
                s_gsvSnrCount = 0;
                s_gsvSnrSum = 0;
                s_gsvCurrentSatsInView = 0;
            }
            s_gsvCurrentSatsInView += numSv;
        }

        const int snrFields[4] = { 7, 11, 15, 19 };
        for (int i = 0; i < 4; ++i) {
            if (getNmeaField(line, snrFields[i], f4, sizeof(f4)) && f4[0] != '\0') {
                int snr = atoi(f4);
                if (snr > 0) {
                    s_gsvSnrCount++;
                    s_gsvSnrSum += (uint32_t)snr;
                }
            }
        }

        if (s_gsvCurrentSatsInView > 0) {
            s_directSatsVisible = (uint16_t)s_gsvCurrentSatsInView;
        } else if (numSv > 0 && numSv > s_directSatsVisible) {
            s_directSatsVisible = (uint16_t)numSv;
        }

        s_directSatsWithSnr = s_gsvSnrCount;
        if (s_gsvSnrCount > 0) {
            s_directAvgSnr = (double)s_gsvSnrSum / (double)s_gsvSnrCount;
        }
    } else if (strcmp(type3, "VTG") == 0) {
        if (getNmeaField(line, 1, f1, sizeof(f1)) && f1[0] != '\0') {
            s_directCourse = atof(f1);
        }
        if (getNmeaField(line, 5, f5, sizeof(f5)) && f5[0] != '\0') {
            s_directSpeedKnots = atof(f5);
        }
        if (getNmeaField(line, 7, f7, sizeof(f7)) && f7[0] != '\0') {
            s_directSpeedKmh = atof(f7);
        }
    }
}

static void sendGnssWakeAndEnableSentences() {
    s_serialGPS.write("\r\n");

    // General probes to trigger manufacturer response
    s_serialGPS.print("$PCAS00*01\r\n");
    s_serialGPS.print("$PUBX,00*33\r\n");

    // u-blox enablement
    s_serialGPS.print("$PUBX,40,RMC,0,1,0,0,0,0*46\r\n");
    s_serialGPS.print("$PUBX,40,GGA,0,1,0,0,0,0*5B\r\n");
    s_serialGPS.print("$PUBX,40,GSA,0,1,0,0,0,0*4F\r\n");
    s_serialGPS.print("$PUBX,40,GSV,0,1,0,0,0,0*58\r\n");
    s_serialGPS.print("$PUBX,40,GLL,0,1,0,0,0,0*5C\r\n");

    // MTK / Quectel enablement
    s_serialGPS.print("$PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n");
    s_serialGPS.print("$PCAS04,7*1E\r\n");
    s_serialGPS.print("$PCAS03,1,1,1,1,1,1,1,1,0,0,,,0,0*02\r\n");

    s_serialGPS.flush();
}

void GPSEngine::init() {
    if (s_initialized) return;

    // Ensure peripheral power rail is asserted
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);

    if (!s_mutex) {
        s_mutex = xSemaphoreCreateRecursiveMutex();
    }

    // Load persisted Master Switch state and Baud rate if present
    if (FileSystem::exists(GPS_SETTINGS_FILE)) {
        String json = FileSystem::readTextFile(GPS_SETTINGS_FILE);
        JsonDocument doc;
        if (deserializeJson(doc, json) == DeserializationError::Ok) {
            if (doc["masterEnabled"].is<bool>()) {
                s_masterEnabled = doc["masterEnabled"].as<bool>();
            }
            if (doc["baud"].is<uint32_t>()) {
                uint32_t b = doc["baud"].as<uint32_t>();
                if (b == 9600 || b == 38400 || b == 115200) {
                    s_currentBaud = b;
                    s_baudLocked = true;
                    for (int i = 0; i < NUM_CANDIDATE_BAUDS; ++i) {
                        if (CANDIDATE_BAUDS[i] == b) {
                            s_candidateIdx = i;
                            break;
                        }
                    }
                }
            }
        }
    }

    // Load last persisted location into s_lastKnownLocation if available
    if (FileSystem::exists(GPS_LAST_FIX_FILE)) {
        String locJson = FileSystem::readTextFile(GPS_LAST_FIX_FILE);
        JsonDocument locDoc;
        if (deserializeJson(locDoc, locJson) == DeserializationError::Ok) {
            double lat = locDoc["lat"] | 0.0;
            double lng = locDoc["lng"] | 0.0;
            if (fabs(lat) > 0.0001 && fabs(lng) > 0.0001) {
                s_lastKnownLocation.latitude = lat;
                s_lastKnownLocation.longitude = lng;
                s_lastKnownLocation.altitudeMeters = locDoc["altitude"] | 0.0;
                s_lastKnownLocation.speedKmh = locDoc["speed"] | 0.0;
                s_lastKnownLocation.speedKnots = locDoc["speedKnots"] | 0.0;
                s_lastKnownLocation.courseDeg = locDoc["course"] | 0.0;
                s_lastKnownLocation.accuracyMeters = locDoc["accuracy"] | 2.5;
                s_lastKnownLocation.timestampUtcMs = locDoc["timestamp"] | 0ULL;
                s_lastKnownLocation.isValid = true;
            }
        }
    }

    // Default status initialization
    s_latestStatus.enabled = false;
    s_latestStatus.hasFix = false;
    strncpy(s_latestStatus.fixType, "none", sizeof(s_latestStatus.fixType));
    strncpy(s_latestStatus.chipModel, "Detecting...", sizeof(s_latestStatus.chipModel));
    s_latestStatus.recentNmeaCount = 0;
    s_latestStatus.satellitesWithSignal = 0;
    s_latestStatus.avgSnr = 0.0;
    s_latestStatus.searchTimeSec = 0;

    // Start hardware immediately on OS boot if Master Switch is enabled so ephemeris & satellite lock stay hot continuously!
    if (s_masterEnabled && isAvailable()) {
        startHardware();
    }

    // Launch Core 0 FreeRTOS Worker Task
    xTaskCreatePinnedToCore(
        gpsTaskWorker,
        "gpsTask",
        6144,
        nullptr,
        2, // Priority 2
        &s_taskHandle,
        0  // Pinned to Core 0
    );

    s_initialized = true;
}

bool GPSEngine::isAvailable() {
    return hasGPS();
}

bool GPSEngine::isMasterEnabled() {
    return s_masterEnabled;
}

void GPSEngine::setMasterEnabled(bool en) {
    if (s_mutex) xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    s_masterEnabled = en;

    // Persist
    JsonDocument doc;
    doc["masterEnabled"] = s_masterEnabled;
    String out;
    serializeJson(doc, out);
    FileSystem::writeTextFile(GPS_SETTINGS_FILE, out.c_str());

    if (!s_masterEnabled && s_hardwareRunning) {
        stopHardware();
    } else if (s_masterEnabled && !s_hardwareRunning) {
        startHardware();
    }
    if (s_mutex) xSemaphoreGiveRecursive(s_mutex);
}

bool GPSEngine::isRunning() {
    return s_hardwareRunning;
}

int GPSEngine::getActiveHoldCount() {
    int count = 0;
    for (const auto& kv : s_activeHolds) {
        if (kv.second) count++;
    }
    return count;
}

bool GPSEngine::acquireHold(const String& pkg) {
    if (!isAvailable()) return false;
    if (!s_masterEnabled) return false;

    if (s_mutex) xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    s_activeHolds[pkg] = true;

    if (!s_hardwareRunning) {
        startHardware();
    }
    if (s_mutex) xSemaphoreGiveRecursive(s_mutex);
    return true;
}

bool GPSEngine::releaseHold(const String& pkg) {
    if (s_mutex) xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    s_activeHolds[pkg] = false;

    if (!s_masterEnabled && s_hardwareRunning) {
        stopHardware();
    }
    if (s_mutex) xSemaphoreGiveRecursive(s_mutex);
    return true;
}

void GPSEngine::releaseAllHolds(const String& pkg) {
    releaseHold(pkg);
}

bool GPSEngine::startHardware() {
    if (s_hardwareRunning) return true;
    if (!isAvailable()) return false;

    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);

    pinMode(BOARD_GPS_RX_PIN, INPUT_PULLUP);
    pinMode(BOARD_GPS_TX_PIN, OUTPUT);

    s_serialGPS.setRxBufferSize(4096);
    s_serialGPS.begin(s_currentBaud, SERIAL_8N1, BOARD_GPS_RX_PIN, BOARD_GPS_TX_PIN);

    sendGnssWakeAndEnableSentences();

    uint32_t now = millis();
    s_probeStartTime = now;
    s_lastSentenceMs = now;
    s_searchStartTime = now;
    s_probeStartPassedChecksum = s_tinyGps.passedChecksum() + s_directPassedChecksum;

    s_hardwareRunning = true;
    s_latestStatus.enabled = true;
    Serial.printf("[GPS] Hardware started at %u baud (pins RX:%d, TX:%d, PWR:%d)\n", s_currentBaud, BOARD_GPS_RX_PIN, BOARD_GPS_TX_PIN, BOARD_POWERON);
    return true;
}

void GPSEngine::stopHardware() {
    if (!s_hardwareRunning) return;

    s_serialGPS.flush();
    s_serialGPS.end();
    s_hardwareRunning = false;
    s_latestStatus.enabled = false;
    s_latestStatus.hasFix = false;
    strncpy(s_latestStatus.fixType, "none", sizeof(s_latestStatus.fixType));
    Serial.println("[GPS] Hardware stopped.");
}

void GPSEngine::gpsTaskWorker(void* param) {
    while (true) {
        if (s_hardwareRunning) {
            bool gotBytes = false;
            while (s_serialGPS.available() > 0) {
                gotBytes = true;
                char c = (char)s_serialGPS.read();
                s_tinyGps.encode(c);

                if (c == '$') {
                    s_nmeaLineBuf[0] = '$';
                    s_nmeaLineLen = 1;
                } else if (s_nmeaLineLen > 0) {
                    if (c == '\r' || c == '\n') {
                        s_nmeaLineBuf[s_nmeaLineLen] = '\0';
                        if (s_nmeaLineLen >= 6) {
                            processDirectNmeaSentence(s_nmeaLineBuf, s_nmeaLineLen);
                        }
                        s_nmeaLineLen = 0;
                    } else if (s_nmeaLineLen < sizeof(s_nmeaLineBuf) - 1) {
                        s_nmeaLineBuf[s_nmeaLineLen++] = c;
                    } else {
                        s_nmeaLineLen = 0;
                    }
                }
            }

            // Kernel Auto-Baud Detection between 38400 (u-blox MIA-M10Q), 9600 (Quectel L76K), and 115200
            uint32_t totalPassed = s_tinyGps.passedChecksum() + s_directPassedChecksum;
            if (!s_baudLocked) {
                if (totalPassed > s_probeStartPassedChecksum) {
                    s_baudLocked = true;
                    s_lastSentenceMs = millis();
                    setGpsHardwareConfirmed(true);
                    sendGnssWakeAndEnableSentences();
                    sendChipSpecificOptimization();
                    Serial.printf("[GPS] Auto-baud LOCKED: %u baud (%u sentences verified) -> Hardware model confirmed: T-Deck Plus\n", s_currentBaud, totalPassed);

                    // Persist locked baud for fast subsequent boots
                    JsonDocument doc;
                    doc["masterEnabled"] = s_masterEnabled;
                    doc["baud"] = s_currentBaud;
                    String out;
                    serializeJson(doc, out);
                    FileSystem::writeTextFile(GPS_SETTINGS_FILE, out.c_str());
                } else if (millis() - s_probeStartTime > 3500) {
                    // Try next candidate baud rate (dwell 3.5s to ensure full 1Hz NMEA bursts are caught)
                    s_candidateIdx = (s_candidateIdx + 1) % NUM_CANDIDATE_BAUDS;
                    s_currentBaud = CANDIDATE_BAUDS[s_candidateIdx];
                    Serial.printf("[GPS] Auto-baud scanning next candidate: %u baud\n", s_currentBaud);

                    s_serialGPS.flush();
                    s_serialGPS.end();
                    vTaskDelay(pdMS_TO_TICKS(50));

                    s_serialGPS.setRxBufferSize(4096);
                    s_serialGPS.begin(s_currentBaud, SERIAL_8N1, BOARD_GPS_RX_PIN, BOARD_GPS_TX_PIN);

                    uint32_t now = millis();
                    s_probeStartTime = now;
                    s_lastSentenceMs = now;
                    s_probeStartPassedChecksum = s_tinyGps.passedChecksum() + s_directPassedChecksum;
                }
            } else {
                // Heartbeat check: keep track of last received sentence time
                static uint32_t s_lastPassedCount = 0;
                if (totalPassed > s_lastPassedCount) {
                    s_lastPassedCount = totalPassed;
                    s_lastSentenceMs = millis();
                } else if (millis() - s_lastSentenceMs > 60000 && s_tinyGps.charsProcessed() == 0) {
                    // Only unlock if 60s elapsed AND zero bytes were ever processed
                    s_baudLocked = false;
                    s_probeStartTime = millis();
                    s_probeStartPassedChecksum = totalPassed;
                    Serial.println("[GPS] Complete silence on serial line for 60s; checking baud.");
                }
            }

            updateCacheFromParser();

            if (!gotBytes) {
                vTaskDelay(pdMS_TO_TICKS(20));
            } else {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void GPSEngine::updateCacheFromParser() {
    if (!s_mutex) return;
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);

    uint32_t now = millis();

    bool tinyValid = s_tinyGps.location.isValid() && (fabs(s_tinyGps.location.lat()) > 0.0001 || fabs(s_tinyGps.location.lng()) > 0.0001);
    uint32_t tinyAge = tinyValid ? s_tinyGps.location.age() : 999999;

    bool directValid = s_directHasCoords && (fabs(s_directLat) > 0.0001 || fabs(s_directLon) > 0.0001);
    uint32_t directAge = directValid ? (now - s_directFixMillis) : 999999;

    uint32_t bestAge = (directAge < tinyAge) ? directAge : tinyAge;
    bool hasRecentFix = (tinyValid && tinyAge < 30000) || (directValid && directAge < 30000);

    uint16_t visCount = 0;
    if (s_satGngsv.isValid() && s_satGngsv.age() < 6000) {
        visCount = (uint16_t)atoi(s_satGngsv.value());
    } else {
        if (s_satGpgsv.isValid() && s_satGpgsv.age() < 6000) visCount += (uint16_t)atoi(s_satGpgsv.value());
        if (s_satGlgsv.isValid() && s_satGlgsv.age() < 6000) visCount += (uint16_t)atoi(s_satGlgsv.value());
        if (s_satGagsv.isValid() && s_satGagsv.age() < 6000) visCount += (uint16_t)atoi(s_satGagsv.value());
        if (s_satGbgsv.isValid() && s_satGbgsv.age() < 6000) visCount += (uint16_t)atoi(s_satGbgsv.value());
        if (s_satBdgsv.isValid() && s_satBdgsv.age() < 6000) visCount += (uint16_t)atoi(s_satBdgsv.value());
    }
    if (s_directSatsVisible > visCount) {
        visCount = s_directSatsVisible;
    }

    uint16_t trackedCount = s_tinyGps.satellites.isValid() ? (uint16_t)s_tinyGps.satellites.value() : 0;
    if (s_directSatsUsed > trackedCount) {
        trackedCount = s_directSatsUsed;
    }
    if (trackedCount == 0 && s_directSatsWithSnr > 0) {
        trackedCount = s_directSatsWithSnr;
    }
    if (visCount < trackedCount) {
        visCount = trackedCount;
    }

    double hdopVal = (s_tinyGps.hdop.isValid() && s_tinyGps.hdop.hdop() > 0.0 && s_tinyGps.hdop.hdop() < 99.0)
                     ? s_tinyGps.hdop.hdop()
                     : s_directHdop;
    double vdopVal = (s_directVdop > 0.0 && s_directVdop < 99.0)
                     ? s_directVdop
                     : (hdopVal < 90.0 ? hdopVal * 1.2 : 99.9);

    s_latestStatus.enabled = s_hardwareRunning;
    s_latestStatus.hasFix = hasRecentFix;
    strncpy(s_latestStatus.chipModel, s_detectedChipModel, sizeof(s_latestStatus.chipModel));
    s_latestStatus.satellitesTracked = trackedCount;
    s_latestStatus.satellitesVisible = visCount;
    s_latestStatus.satellitesWithSignal = s_directSatsWithSnr;
    s_latestStatus.avgSnr = s_directAvgSnr;
    s_latestStatus.hdop = hdopVal;
    s_latestStatus.vdop = vdopVal;
    s_latestStatus.ageMs = bestAge;
    s_latestStatus.sentencesParsed = s_tinyGps.passedChecksum() + s_directPassedChecksum;
    s_latestStatus.checksumErrors = s_tinyGps.failedChecksum();
    s_latestStatus.charsProcessed = s_tinyGps.charsProcessed();
    s_latestStatus.currentBaud = s_currentBaud;
    s_latestStatus.baudLocked = s_baudLocked;
    s_latestStatus.searchTimeSec = s_searchStartTime > 0 ? ((now - s_searchStartTime) / 1000) : 0;

    // Copy latest real NMEA lines into status
    uint8_t outIdx = 0;
    for (int i = 0; i < 4; ++i) {
        if (s_recentNmea[i][0] != '\0') {
            strncpy(s_latestStatus.recentNmea[outIdx], s_recentNmea[i], sizeof(s_latestStatus.recentNmea[outIdx]) - 1);
            s_latestStatus.recentNmea[outIdx][sizeof(s_latestStatus.recentNmea[outIdx]) - 1] = '\0';
            outIdx++;
        }
    }
    s_latestStatus.recentNmeaCount = outIdx;

    // Update Time from TinyGPSPlus or Direct NMEA parser
    bool timeUpdated = false;
    if (s_tinyGps.date.isValid() && s_tinyGps.time.isValid() && s_tinyGps.date.year() >= 2024) {
        s_latestTime.year = s_tinyGps.date.year();
        s_latestTime.month = s_tinyGps.date.month();
        s_latestTime.day = s_tinyGps.date.day();
        s_latestTime.hour = s_tinyGps.time.hour();
        s_latestTime.minute = s_tinyGps.time.minute();
        s_latestTime.second = s_tinyGps.time.second();
        s_latestTime.centisecond = s_tinyGps.time.centisecond();
        timeUpdated = true;
    } else if (s_directTimeValid) {
        s_latestTime.year = s_directDateValid ? s_directYear : 2026;
        s_latestTime.month = s_directDateValid ? s_directMonth : 1;
        s_latestTime.day = s_directDateValid ? s_directDay : 1;
        s_latestTime.hour = s_directHour;
        s_latestTime.minute = s_directMin;
        s_latestTime.second = s_directSec;
        s_latestTime.centisecond = s_directCentisec;
        timeUpdated = s_directDateValid;
    }

    if (timeUpdated) {
        struct tm tm_utc = {0};
        tm_utc.tm_year = s_latestTime.year - 1900;
        tm_utc.tm_mon = s_latestTime.month - 1;
        tm_utc.tm_mday = s_latestTime.day;
        tm_utc.tm_hour = s_latestTime.hour;
        tm_utc.tm_min = s_latestTime.minute;
        tm_utc.tm_sec = s_latestTime.second;
        tm_utc.tm_isdst = 0;
        time_t ep_utc = mktime(&tm_utc);
        s_latestTime.epochMs = ((uint64_t)ep_utc * 1000ULL) + (s_latestTime.centisecond * 10);
        s_latestTime.isValid = true;
    }

    if (hasRecentFix) {
        if (s_gsaFixMode == 3 || s_latestStatus.satellitesTracked >= 4) {
            strncpy(s_latestStatus.fixType, "3D", sizeof(s_latestStatus.fixType));
        } else {
            strncpy(s_latestStatus.fixType, "2D", sizeof(s_latestStatus.fixType));
        }

        if (directValid && directAge <= tinyAge) {
            s_latestLocation.latitude = s_directLat;
            s_latestLocation.longitude = s_directLon;
        } else {
            s_latestLocation.latitude = s_tinyGps.location.lat();
            s_latestLocation.longitude = s_tinyGps.location.lng();
        }

        double rawSpeedKmh = (s_tinyGps.speed.isValid() && s_tinyGps.speed.kmph() > 0.0) ? s_tinyGps.speed.kmph() : s_directSpeedKmh;
        double rawSpeedKnots = (s_tinyGps.speed.isValid() && s_tinyGps.speed.knots() > 0.0) ? s_tinyGps.speed.knots() : s_directSpeedKnots;

        // Stationary Speed Deadband Filter:
        // Atmospheric/multipath GPS Doppler noise typically jitters between 0.5 to 3.0 km/h when standing still.
        // If speed is below 3.0 km/h (~0.83 m/s), snap to absolute 0.0.
        if (rawSpeedKmh < 3.0) {
            rawSpeedKmh = 0.0;
            rawSpeedKnots = 0.0;
        }

        s_latestLocation.altitudeMeters = s_tinyGps.altitude.isValid() ? s_tinyGps.altitude.meters() : s_directAlt;
        s_latestLocation.speedKmh = rawSpeedKmh;
        s_latestLocation.speedKnots = rawSpeedKnots;
        s_latestLocation.courseDeg = s_tinyGps.course.isValid() ? s_tinyGps.course.deg() : s_directCourse;
        double effHdop = (s_latestStatus.hdop > 0.0 && s_latestStatus.hdop < 50.0) ? s_latestStatus.hdop : 1.5;
        s_latestLocation.accuracyMeters = effHdop * 2.5;
        s_latestLocation.lastFixMillis = now;
        s_latestLocation.isValid = true;

        if (s_latestTime.isValid) {
            s_latestLocation.timestampUtcMs = s_latestTime.epochMs;
        }

        s_lastKnownLocation = s_latestLocation;

        // Persist valid fix to flash every 30 seconds
        static uint32_t s_lastFixPersistMs = 0;
        if (now - s_lastFixPersistMs > 30000) {
            s_lastFixPersistMs = now;
            JsonDocument locDoc;
            locDoc["lat"] = s_lastKnownLocation.latitude;
            locDoc["lng"] = s_lastKnownLocation.longitude;
            locDoc["altitude"] = s_lastKnownLocation.altitudeMeters;
            locDoc["speed"] = s_lastKnownLocation.speedKmh;
            locDoc["speedKnots"] = s_lastKnownLocation.speedKnots;
            locDoc["course"] = s_lastKnownLocation.courseDeg;
            locDoc["accuracy"] = s_lastKnownLocation.accuracyMeters;
            locDoc["timestamp"] = s_lastKnownLocation.timestampUtcMs;
            String locOut;
            serializeJson(locDoc, locOut);
            FileSystem::writeTextFile(GPS_LAST_FIX_FILE, locOut.c_str());
        }
    } else {
        strncpy(s_latestStatus.fixType, "none", sizeof(s_latestStatus.fixType));
        if (!directValid && !tinyValid) {
            s_latestLocation.isValid = false;
            s_latestLocation.speedKmh = 0.0;
            s_latestLocation.speedKnots = 0.0;
        }
    }

    xSemaphoreGiveRecursive(s_mutex);
}

bool GPSEngine::hasFix() {
    if (!s_mutex) return false;
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    bool fix = s_latestStatus.hasFix;
    xSemaphoreGiveRecursive(s_mutex);
    return fix;
}

bool GPSEngine::getLocation(GPSLocationData& out) {
    if (!s_mutex) return false;
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    bool ok = s_latestLocation.isValid;
    if (ok) {
        out = s_latestLocation;
    }
    xSemaphoreGiveRecursive(s_mutex);
    return ok;
}

bool GPSEngine::getLastKnownLocation(GPSLocationData& out) {
    if (!s_mutex) return false;
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    bool ok = s_lastKnownLocation.isValid;
    if (ok) {
        out = s_lastKnownLocation;
    }
    xSemaphoreGiveRecursive(s_mutex);
    return ok;
}

bool GPSEngine::getStatus(GPSStatusData& out) {
    if (!s_mutex) return false;
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    out = s_latestStatus;
    xSemaphoreGiveRecursive(s_mutex);
    return true;
}

bool GPSEngine::getTime(GPSTimeData& out) {
    if (!s_mutex) return false;
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    bool ok = s_latestTime.isValid;
    if (ok) {
        out = s_latestTime;
    }
    xSemaphoreGiveRecursive(s_mutex);
    return ok;
}

bool GPSEngine::syncSystemTime() {
    GPSTimeData td;
    if (!getTime(td) || !td.isValid) return false;

    struct tm t = {0};
    t.tm_year = td.year - 1900;
    t.tm_mon = td.month - 1;
    t.tm_mday = td.day;
    t.tm_hour = td.hour;
    t.tm_min = td.minute;
    t.tm_sec = td.second;
    t.tm_isdst = 0;

    time_t epoch = mktime(&t);
    struct timeval tv = { .tv_sec = epoch, .tv_usec = (suseconds_t)(td.centisecond * 10000) };
    settimeofday(&tv, nullptr);
    return true;
}

double GPSEngine::distanceBetween(double lat1, double lon1, double lat2, double lon2) {
    return TinyGPSPlus::distanceBetween(lat1, lon1, lat2, lon2);
}

double GPSEngine::courseTo(double lat1, double lon1, double lat2, double lon2) {
    return TinyGPSPlus::courseTo(lat1, lon1, lat2, lon2);
}

bool GPSEngine::isInsideCircle(double lat, double lon, double cLat, double cLon, double radiusMeters) {
    double dist = TinyGPSPlus::distanceBetween(lat, lon, cLat, cLon);
    return dist <= radiusMeters;
}

bool GPSEngine::setRate(uint8_t rateHz) {
    if (!s_hardwareRunning) return false;
    if (rateHz == 1) {
        s_serialGPS.print("$PMTK220,1000*1F\r\n"); // 1000ms = 1Hz
        s_serialGPS.print("$PCAS02,1000*2E\r\n");
    } else if (rateHz == 5) {
        s_serialGPS.print("$PMTK220,200*2C\r\n");  // 200ms = 5Hz
        s_serialGPS.print("$PCAS02,200*1D\r\n");
    } else if (rateHz == 10) {
        s_serialGPS.print("$PMTK220,100*2F\r\n");  // 100ms = 10Hz
        s_serialGPS.print("$PCAS02,100*1E\r\n");
    } else {
        return false;
    }
    s_serialGPS.flush();
    return true;
}

uint32_t GPSEngine::getBaud() {
    return s_currentBaud;
}

bool GPSEngine::isBaudLocked() {
    return s_baudLocked;
}
