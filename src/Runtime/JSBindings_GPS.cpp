#include "JSBindings.h"
#include "../Hal/GPS/GPSEngine.h"
#include <ArduinoJson.h>
#include <vector>

struct GPSWatchSubscription {
    int id;
    uint32_t minIntervalMs;
    double minDistanceMeters;
    uint32_t lastTriggerMs;
    double lastLat;
    double lastLon;
    bool hasPreviousFix;
};

static std::vector<GPSWatchSubscription> s_gpsWatchers;
static int s_nextWatchId = 1;

static void pushLocationObject(duk_context *ctx, const GPSLocationData& loc) {
    duk_push_object(ctx);
    duk_push_number(ctx, loc.latitude);
    duk_put_prop_string(ctx, -2, "lat");
    duk_push_number(ctx, loc.longitude);
    duk_put_prop_string(ctx, -2, "lng");
    duk_push_number(ctx, loc.altitudeMeters);
    duk_put_prop_string(ctx, -2, "altitude");
    duk_push_number(ctx, loc.speedKmh);
    duk_put_prop_string(ctx, -2, "speed");
    duk_push_number(ctx, loc.speedKnots);
    duk_put_prop_string(ctx, -2, "speedKnots");
    duk_push_number(ctx, loc.courseDeg);
    duk_put_prop_string(ctx, -2, "course");
    duk_push_number(ctx, loc.accuracyMeters);
    duk_put_prop_string(ctx, -2, "accuracy");
    duk_push_number(ctx, (double)loc.timestampUtcMs);
    duk_put_prop_string(ctx, -2, "timestamp");
}

static void pushStatusObject(duk_context *ctx, const GPSStatusData& st) {
    duk_push_object(ctx);
    duk_push_boolean(ctx, st.enabled);
    duk_put_prop_string(ctx, -2, "enabled");
    duk_push_boolean(ctx, st.hasFix);
    duk_put_prop_string(ctx, -2, "hasFix");
    duk_push_string(ctx, st.fixType);
    duk_put_prop_string(ctx, -2, "fixType");
    duk_push_string(ctx, st.chipModel);
    duk_put_prop_string(ctx, -2, "chipModel");
    duk_push_int(ctx, st.satellitesTracked);
    duk_put_prop_string(ctx, -2, "satellitesTracked");
    duk_push_int(ctx, st.satellitesVisible);
    duk_put_prop_string(ctx, -2, "satellitesVisible");
    duk_push_int(ctx, st.satellitesWithSignal);
    duk_put_prop_string(ctx, -2, "satellitesWithSignal");
    duk_push_number(ctx, st.avgSnr);
    duk_put_prop_string(ctx, -2, "avgSnr");
    duk_push_number(ctx, st.hdop);
    duk_put_prop_string(ctx, -2, "hdop");
    duk_push_number(ctx, st.vdop);
    duk_put_prop_string(ctx, -2, "vdop");
    duk_push_uint(ctx, st.ageMs);
    duk_put_prop_string(ctx, -2, "ageMs");
    duk_push_uint(ctx, st.sentencesParsed);
    duk_put_prop_string(ctx, -2, "sentencesParsed");
    duk_push_uint(ctx, st.checksumErrors);
    duk_put_prop_string(ctx, -2, "checksumErrors");
    duk_push_uint(ctx, st.charsProcessed);
    duk_put_prop_string(ctx, -2, "charsProcessed");
    duk_push_uint(ctx, st.currentBaud);
    duk_put_prop_string(ctx, -2, "currentBaud");
    duk_push_boolean(ctx, st.baudLocked);
    duk_put_prop_string(ctx, -2, "baudLocked");
    duk_push_uint(ctx, st.searchTimeSec);
    duk_put_prop_string(ctx, -2, "searchTimeSec");

    // Push real hardware NMEA sentences received from UART
    duk_push_array(ctx);
    for (uint8_t i = 0; i < st.recentNmeaCount && i < 4; ++i) {
        duk_push_string(ctx, st.recentNmea[i]);
        duk_put_prop_index(ctx, -2, i);
    }
    duk_put_prop_string(ctx, -2, "rawNmea");
}

static void pushTimeObject(duk_context *ctx, const GPSTimeData& td) {
    duk_push_object(ctx);
    duk_push_int(ctx, td.year);
    duk_put_prop_string(ctx, -2, "year");
    duk_push_int(ctx, td.month);
    duk_put_prop_string(ctx, -2, "month");
    duk_push_int(ctx, td.day);
    duk_put_prop_string(ctx, -2, "day");
    duk_push_int(ctx, td.hour);
    duk_put_prop_string(ctx, -2, "hour");
    duk_push_int(ctx, td.minute);
    duk_put_prop_string(ctx, -2, "minute");
    duk_push_int(ctx, td.second);
    duk_put_prop_string(ctx, -2, "second");
    duk_push_int(ctx, td.centisecond);
    duk_put_prop_string(ctx, -2, "centisecond");
    duk_push_number(ctx, (double)td.epochMs);
    duk_put_prop_string(ctx, -2, "epochMs");
}

// ----------------------------------------------------
// 1. Hardware Status & Lifecycle
// ----------------------------------------------------

static duk_ret_t js_gps_isAvailable(duk_context *ctx) {
    duk_push_boolean(ctx, GPSEngine::isAvailable());
    return 1;
}

static duk_ret_t js_gps_isEnabled(duk_context *ctx) {
    duk_push_boolean(ctx, GPSEngine::isMasterEnabled() && GPSEngine::isRunning());
    return 1;
}

static duk_ret_t js_gps_enable(duk_context *ctx) {
    if (!GPSEngine::isAvailable()) {
        duk_push_boolean(ctx, false);
        return 1;
    }

    if (!JSBindings::checkLocationPermission()) {
        duk_push_boolean(ctx, false);
        return 1;
    }

    if (!GPSEngine::isMasterEnabled()) {
        if (!JSBindings::promptEnableGPSMasterSwitch()) {
            duk_push_boolean(ctx, false);
            return 1;
        }
    }

    String pkg = JSBindings::getAppPackageName();
    bool res = GPSEngine::acquireHold(pkg);
    duk_push_boolean(ctx, res);
    return 1;
}

static duk_ret_t js_gps_disable(duk_context *ctx) {
    String pkg = JSBindings::getAppPackageName();
    bool res = GPSEngine::releaseHold(pkg);
    duk_push_boolean(ctx, res);
    return 1;
}

static duk_ret_t js_gps_requestPermission(duk_context *ctx) {
    bool granted = JSBindings::checkLocationPermission();
    duk_push_string(ctx, granted ? "granted" : "denied");
    return 1;
}

// ----------------------------------------------------
// 2. Fix Status & Constellation Metrics
// ----------------------------------------------------

static duk_ret_t js_gps_hasFix(duk_context *ctx) {
    if (!JSBindings::checkLocationPermission()) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    duk_push_boolean(ctx, GPSEngine::hasFix());
    return 1;
}

static duk_ret_t js_gps_getStatus(duk_context *ctx) {
    if (!JSBindings::checkLocationPermission()) {
        duk_push_null(ctx);
        return 1;
    }

    GPSStatusData st;
    if (GPSEngine::getStatus(st)) {
        pushStatusObject(ctx, st);
    } else {
        duk_push_null(ctx);
    }
    return 1;
}

// ----------------------------------------------------
// 3. Coordinate & Navigation Data
// ----------------------------------------------------

static duk_ret_t js_gps_getLocation(duk_context *ctx) {
    if (!JSBindings::checkLocationPermission()) {
        duk_push_null(ctx);
        return 1;
    }

    GPSLocationData loc;
    if (GPSEngine::getLocation(loc)) {
        pushLocationObject(ctx, loc);
    } else {
        duk_push_null(ctx);
    }
    return 1;
}

static duk_ret_t js_gps_getLastKnownLocation(duk_context *ctx) {
    if (!JSBindings::checkLocationPermission()) {
        duk_push_null(ctx);
        return 1;
    }

    GPSLocationData loc;
    if (GPSEngine::getLastKnownLocation(loc)) {
        pushLocationObject(ctx, loc);
    } else {
        duk_push_null(ctx);
    }
    return 1;
}

// ----------------------------------------------------
// 4. Offline RTC & System Clock Synchronization
// ----------------------------------------------------

static duk_ret_t js_gps_getTime(duk_context *ctx) {
    GPSTimeData td;
    if (GPSEngine::getTime(td)) {
        pushTimeObject(ctx, td);
    } else {
        duk_push_null(ctx);
    }
    return 1;
}

static duk_ret_t js_gps_syncSystemTime(duk_context *ctx) {
    bool ok = GPSEngine::syncSystemTime();
    duk_push_boolean(ctx, ok);
    return 1;
}

// ----------------------------------------------------
// 5. Event-Driven Streaming (watchPosition)
// ----------------------------------------------------

static duk_ret_t js_gps_watchPosition(duk_context *ctx) {
    if (!duk_is_function(ctx, 0)) {
        duk_error(ctx, DUK_ERR_TYPE_ERROR, "watchPosition requires a callback function");
        return 0;
    }

    if (!JSBindings::checkLocationPermission()) {
        duk_push_int(ctx, -1);
        return 1;
    }

    if (!GPSEngine::isMasterEnabled()) {
        if (!JSBindings::promptEnableGPSMasterSwitch()) {
            duk_push_int(ctx, -1);
            return 1;
        }
    }

    // Acquire GPS hold for this application
    String pkg = JSBindings::getAppPackageName();
    GPSEngine::acquireHold(pkg);

    uint32_t minInterval = 1000;
    double minDistance = 0.0;

    if (duk_is_object(ctx, 1)) {
        duk_get_prop_string(ctx, 1, "minIntervalMs");
        if (duk_is_number(ctx, -1)) {
            minInterval = (uint32_t)duk_get_uint(ctx, -1);
            if (minInterval < 200) minInterval = 200;
        }
        duk_pop(ctx);

        duk_get_prop_string(ctx, 1, "minDistanceMeters");
        if (duk_is_number(ctx, -1)) {
            minDistance = duk_get_number(ctx, -1);
            if (minDistance < 0) minDistance = 0;
        }
        duk_pop(ctx);
    }

    int watchId = s_nextWatchId++;

    // Save callback in global stash: "gps_cb_<id>"
    duk_push_global_stash(ctx);
    duk_push_sprintf(ctx, "gps_cb_%d", watchId);
    duk_dup(ctx, 0); // copy callback function
    duk_put_prop(ctx, -3);
    duk_pop(ctx); // pop global stash

    GPSWatchSubscription sub;
    sub.id = watchId;
    sub.minIntervalMs = minInterval;
    sub.minDistanceMeters = minDistance;
    sub.lastTriggerMs = 0;
    sub.lastLat = 0.0;
    sub.lastLon = 0.0;
    sub.hasPreviousFix = false;

    s_gpsWatchers.push_back(sub);

    duk_push_int(ctx, watchId);
    return 1;
}

static duk_ret_t js_gps_clearWatch(duk_context *ctx) {
    if (!duk_is_number(ctx, 0)) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    int watchId = duk_get_int(ctx, 0);

    bool found = false;
    for (auto it = s_gpsWatchers.begin(); it != s_gpsWatchers.end(); ) {
        if (it->id == watchId) {
            it = s_gpsWatchers.erase(it);
            found = true;
        } else {
            ++it;
        }
    }

    if (found) {
        // Remove callback from global stash
        duk_push_global_stash(ctx);
        duk_push_sprintf(ctx, "gps_cb_%d", watchId);
        duk_del_prop(ctx, -2);
        duk_pop(ctx);
    }

    duk_push_boolean(ctx, found);
    return 1;
}

// ----------------------------------------------------
// 6. Native C++ Fast Geodesy
// ----------------------------------------------------

static duk_ret_t js_gps_distanceBetween(duk_context *ctx) {
    double lat1 = duk_require_number(ctx, 0);
    double lon1 = duk_require_number(ctx, 1);
    double lat2 = duk_require_number(ctx, 2);
    double lon2 = duk_require_number(ctx, 3);

    double d = GPSEngine::distanceBetween(lat1, lon1, lat2, lon2);
    duk_push_number(ctx, d);
    return 1;
}

static duk_ret_t js_gps_courseTo(duk_context *ctx) {
    double lat1 = duk_require_number(ctx, 0);
    double lon1 = duk_require_number(ctx, 1);
    double lat2 = duk_require_number(ctx, 2);
    double lon2 = duk_require_number(ctx, 3);

    double c = GPSEngine::courseTo(lat1, lon1, lat2, lon2);
    duk_push_number(ctx, c);
    return 1;
}

static duk_ret_t js_gps_isInsideCircle(duk_context *ctx) {
    double lat = duk_require_number(ctx, 0);
    double lon = duk_require_number(ctx, 1);
    double cLat = duk_require_number(ctx, 2);
    double cLon = duk_require_number(ctx, 3);
    double radius = duk_require_number(ctx, 4);

    bool inside = GPSEngine::isInsideCircle(lat, lon, cLat, cLon, radius);
    duk_push_boolean(ctx, inside);
    return 1;
}

// ----------------------------------------------------
// 7. Update Frequency & Telemetry
// ----------------------------------------------------

static duk_ret_t js_gps_setRate(duk_context *ctx) {
    int rate = duk_require_int(ctx, 0);
    if (rate <= 0 || rate > 10) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    bool ok = GPSEngine::setRate((uint8_t)rate);
    duk_push_boolean(ctx, ok);
    return 1;
}

static duk_ret_t js_gps_getBaud(duk_context *ctx) {
    duk_push_uint(ctx, GPSEngine::getBaud());
    return 1;
}

// ----------------------------------------------------
// Poller & Lifecycle
// ----------------------------------------------------

void js_poll_gps(duk_context *ctx) {
    if (s_gpsWatchers.empty() || !ctx) return;

    GPSLocationData loc;
    if (!GPSEngine::getLocation(loc) || !loc.isValid) return;

    uint32_t now = millis();

    for (auto& w : s_gpsWatchers) {
        if (now - w.lastTriggerMs < w.minIntervalMs) continue;

        if (w.minDistanceMeters > 0.0 && w.hasPreviousFix) {
            double moved = GPSEngine::distanceBetween(w.lastLat, w.lastLon, loc.latitude, loc.longitude);
            if (moved < w.minDistanceMeters) continue;
        }

        w.lastTriggerMs = now;
        w.lastLat = loc.latitude;
        w.lastLon = loc.longitude;
        w.hasPreviousFix = true;

        // Retrieve callback from stash
        duk_push_global_stash(ctx);
        duk_push_sprintf(ctx, "gps_cb_%d", w.id);
        if (duk_get_prop(ctx, -2) && duk_is_function(ctx, -1)) {
            pushLocationObject(ctx, loc);
            if (duk_pcall(ctx, 1) != 0) {
                // Ignore JS error in watcher callback or print warning
                Serial.printf("[GPS] Watcher %d error: %s\n", w.id, duk_safe_to_string(ctx, -1));
            }
            duk_pop(ctx); // pop pcall result
        } else {
            duk_pop(ctx); // pop invalid property
        }
        duk_pop(ctx); // pop global stash
    }
}

void js_cleanup_gps(duk_context *ctx) {
    if (ctx) {
        duk_push_global_stash(ctx);
        for (const auto& w : s_gpsWatchers) {
            duk_push_sprintf(ctx, "gps_cb_%d", w.id);
            duk_del_prop(ctx, -2);
        }
        duk_pop(ctx);
    }
    s_gpsWatchers.clear();

    String pkg = JSBindings::getAppPackageName();
    if (pkg.length() > 0) {
        GPSEngine::releaseAllHolds(pkg);
    }
}

void js_init_gps(duk_context *ctx) {
    duk_push_object(ctx); // Global GPS object

    // Hardware & Power
    duk_push_c_function(ctx, js_gps_isAvailable, 0); duk_put_prop_string(ctx, -2, "isAvailable");
    duk_push_c_function(ctx, js_gps_isEnabled, 0);   duk_put_prop_string(ctx, -2, "isEnabled");
    duk_push_c_function(ctx, js_gps_enable, 0);      duk_put_prop_string(ctx, -2, "enable");
    duk_push_c_function(ctx, js_gps_enable, 0);      duk_put_prop_string(ctx, -2, "start");
    duk_push_c_function(ctx, js_gps_disable, 0);     duk_put_prop_string(ctx, -2, "disable");
    duk_push_c_function(ctx, js_gps_disable, 0);     duk_put_prop_string(ctx, -2, "stop");
    duk_push_c_function(ctx, js_gps_requestPermission, 0); duk_put_prop_string(ctx, -2, "requestPermission");

    // Status & Coordinates
    duk_push_c_function(ctx, js_gps_hasFix, 0);      duk_put_prop_string(ctx, -2, "hasFix");
    duk_push_c_function(ctx, js_gps_getStatus, 0);   duk_put_prop_string(ctx, -2, "getStatus");
    duk_push_c_function(ctx, js_gps_getLocation, 0); duk_put_prop_string(ctx, -2, "getLocation");
    duk_push_c_function(ctx, js_gps_getLastKnownLocation, 0); duk_put_prop_string(ctx, -2, "getLastKnownLocation");

    // Offline Time Sync
    duk_push_c_function(ctx, js_gps_getTime, 0);     duk_put_prop_string(ctx, -2, "getTime");
    duk_push_c_function(ctx, js_gps_syncSystemTime, 0); duk_put_prop_string(ctx, -2, "syncSystemTime");

    // Event Streaming
    duk_push_c_function(ctx, js_gps_watchPosition, 2); duk_put_prop_string(ctx, -2, "watchPosition");
    duk_push_c_function(ctx, js_gps_clearWatch, 1);    duk_put_prop_string(ctx, -2, "clearWatch");

    // Native Fast Geodesy
    duk_push_c_function(ctx, js_gps_distanceBetween, 4); duk_put_prop_string(ctx, -2, "distanceBetween");
    duk_push_c_function(ctx, js_gps_courseTo, 4);        duk_put_prop_string(ctx, -2, "courseTo");
    duk_push_c_function(ctx, js_gps_isInsideCircle, 5);  duk_put_prop_string(ctx, -2, "isInsideCircle");

    // Configuration & Telemetry
    duk_push_c_function(ctx, js_gps_setRate, 1);     duk_put_prop_string(ctx, -2, "setRate");
    duk_push_c_function(ctx, js_gps_getBaud, 0);     duk_put_prop_string(ctx, -2, "getBaud");

    duk_put_global_string(ctx, "GPS");
}

