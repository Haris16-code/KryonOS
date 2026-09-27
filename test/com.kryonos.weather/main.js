// KryonOS Weather App powered by Open-Meteo API
// http://open-meteo.com/en/docs

var SW = System.screenWidth();
var SH = System.screenHeight();

// 16-bit RGB565 Palette
var C_BLACK     = 0x0000;
var C_WHITE     = 0xFFFF;
var C_GRAY      = 0x7BEF;
var C_DARKGRAY  = 0x2124;
var C_LIGHTBLUE = 0x5D3F;
var C_BLUE      = 0x037D;
var C_NAVY      = 0x0A0F;
var C_CYAN      = 0x07FF;
var C_YELLOW    = 0xFEE0;
var C_GOLD      = 0xFEA0;
var C_ORANGE    = 0xFD20;
var C_RED       = 0xF800;
var C_GREEN     = 0x2E8B;
var C_PURPLE    = 0x799F;
var C_CARD_BG   = 0x11E8;
var C_CARD_BDR  = 0x2A6D;

// Pre-configured Global Cities (Lat, Lon, Name)
var CITIES = [
    { name: "New York",  lat: 40.7128,  lon: -74.0060,  tz: "America/New_York" },
    { name: "London",    lat: 51.5074,  lon: -0.1278,   tz: "Europe/London" },
    { name: "Tokyo",     lat: 35.6762,  lon: 139.6503,  tz: "Asia/Tokyo" },
    { name: "Paris",     lat: 48.8566,  lon: 2.3522,    tz: "Europe/Paris" },
    { name: "Dubai",     lat: 25.2048,  lon: 55.2708,   tz: "Asia/Dubai" },
    { name: "Singapore", lat: 1.3521,   lon: 103.8198,  tz: "Asia/Singapore" },
    { name: "Sydney",    lat: -33.8688, lon: 151.2093,  tz: "Australia/Sydney" },
    { name: "Berlin",    lat: 52.5200,  lon: 13.4050,   tz: "Europe/Berlin" },
    { name: "Toronto",   lat: 43.6532,  lon: -79.3832,  tz: "America/Toronto" },
    { name: "Islamabad", lat: 33.6844,  lon: 73.0479,   tz: "Asia/Karachi" }
];

var cityIndex = 0;
var useFahrenheit = false;
var weatherData = null;
var isLoading = false;
var errorMessage = "";
var showCityModal = false;
var modalScroll = 0;
var lastUpdated = "Never";

// WMO Weather Interpretation Codes
function getWeatherInfo(code, isDay) {
    if (code === undefined || code === null) code = 0;
    if (isDay === undefined) isDay = 1;

    switch (code) {
        case 0:  return { label: "Clear Sky",          icon: isDay ? "sun" : "moon",      theme: isDay ? 0x235F : 0x0847 };
        case 1:  return { label: "Mainly Clear",       icon: isDay ? "sun_cloud" : "moon_cloud", theme: isDay ? 0x2B9F : 0x08C8 };
        case 2:  return { label: "Partly Cloudy",     icon: "sun_cloud",  theme: 0x33DF };
        case 3:  return { label: "Overcast",          icon: "cloud",      theme: 0x2965 };
        case 45:
        case 48: return { label: "Foggy",             icon: "fog",        theme: 0x39E7 };
        case 51:
        case 53:
        case 55: return { label: "Drizzle",           icon: "drizzle",    theme: 0x2295 };
        case 56:
        case 57: return { label: "Freezing Drizzle",  icon: "snow",       theme: 0x3319 };
        case 61:
        case 63:
        case 65: return { label: "Rainy",             icon: "rain",       theme: 0x1A0F };
        case 66:
        case 67: return { label: "Freezing Rain",     icon: "snow_rain",  theme: 0x21EB };
        case 71:
        case 73:
        case 75:
        case 77: return { label: "Snow",              icon: "snow",       theme: 0x4B3D };
        case 80:
        case 81:
        case 82: return { label: "Rain Showers",      icon: "rain",       theme: 0x19ED };
        case 85:
        case 86: return { label: "Snow Showers",      icon: "snow",       theme: 0x3A5B };
        case 95:
        case 96:
        case 99: return { label: "Thunderstorm",      icon: "storm",      theme: 0x10E4 };
        default: return { label: "Clear",             icon: "sun",        theme: 0x235F };
    }
}

// Convert Celsius to current unit
function formatTemp(celsius) {
    if (celsius === undefined || celsius === null) return "--";
    if (useFahrenheit) {
        var f = (celsius * 9.0 / 5.0) + 32.0;
        return Math.round(f) + "°F";
    }
    return Math.round(celsius) + "°C";
}

// Vector Icon Renderers
function drawWeatherIcon(type, cx, cy, size) {
    var r = size || 14;
    
    if (type === "sun") {
        System.fillCircle(cx, cy, r, C_YELLOW);
        System.drawCircle(cx, cy, r, C_ORANGE);
        // Sun rays
        for (var a = 0; a < 8; a++) {
            var rad = a * (Math.PI / 4.0);
            var x1 = Math.round(cx + Math.cos(rad) * (r + 3));
            var y1 = Math.round(cy + Math.sin(rad) * (r + 3));
            var x2 = Math.round(cx + Math.cos(rad) * (r + 8));
            var y2 = Math.round(cy + Math.sin(rad) * (r + 8));
            System.drawLine(x1, y1, x2, y2, C_ORANGE);
        }
    } else if (type === "moon") {
        System.fillCircle(cx, cy, r, C_YELLOW);
        System.fillCircle(cx + 6, cy - 4, r - 2, 0x0847); // Cutout to make crescent
    } else if (type === "sun_cloud" || type === "moon_cloud") {
        // Sun behind
        System.fillCircle(cx + 8, cy - 6, r - 4, C_YELLOW);
        // Cloud in front
        System.fillCircle(cx - 6, cy + 2, r - 5, C_WHITE);
        System.fillCircle(cx + 4, cy - 1, r - 3, C_WHITE);
        System.fillCircle(cx + 12, cy + 4, r - 6, C_WHITE);
        System.fillRoundRect(cx - 10, cy + 3, 24, 8, 3, C_WHITE);
    } else if (type === "cloud" || type === "fog") {
        System.fillCircle(cx - 8, cy, r - 4, C_WHITE);
        System.fillCircle(cx + 2, cy - 4, r - 2, C_WHITE);
        System.fillCircle(cx + 10, cy + 2, r - 5, C_WHITE);
        System.fillRoundRect(cx - 12, cy + 1, 26, 9, 4, C_WHITE);
        if (type === "fog") {
            System.drawFastHLine(cx - 14, cy + 13, 28, C_CYAN);
            System.drawFastHLine(cx - 10, cy + 16, 20, C_CYAN);
        }
    } else if (type === "rain" || type === "drizzle") {
        // Cloud
        System.fillCircle(cx - 6, cy - 4, r - 5, C_GRAY);
        System.fillCircle(cx + 3, cy - 8, r - 3, C_WHITE);
        System.fillCircle(cx + 11, cy - 3, r - 6, C_GRAY);
        System.fillRoundRect(cx - 10, cy - 3, 23, 8, 3, C_WHITE);
        // Raindrops
        System.drawLine(cx - 6, cy + 8, cx - 8, cy + 15, C_CYAN);
        System.drawLine(cx + 1, cy + 8, cx - 1, cy + 15, C_CYAN);
        System.drawLine(cx + 8, cy + 8, cx + 6, cy + 15, C_CYAN);
    } else if (type === "snow" || type === "snow_rain") {
        // Cloud
        System.fillCircle(cx - 6, cy - 4, r - 5, C_GRAY);
        System.fillCircle(cx + 3, cy - 8, r - 3, C_WHITE);
        System.fillRoundRect(cx - 10, cy - 3, 23, 8, 3, C_WHITE);
        // Snowflakes
        System.fillCircle(cx - 6, cy + 11, 2, C_WHITE);
        System.fillCircle(cx + 2, cy + 13, 2, C_WHITE);
        System.fillCircle(cx + 8, cy + 10, 2, C_WHITE);
    } else if (type === "storm") {
        // Dark cloud
        System.fillCircle(cx - 6, cy - 4, r - 5, C_DARKGRAY);
        System.fillCircle(cx + 3, cy - 8, r - 3, C_GRAY);
        System.fillRoundRect(cx - 10, cy - 3, 23, 8, 3, C_DARKGRAY);
        // Lightning bolt
        System.drawLine(cx, cy + 5, cx - 4, cy + 12, C_YELLOW);
        System.drawLine(cx - 4, cy + 12, cx + 2, cy + 12, C_YELLOW);
        System.drawLine(cx + 2, cy + 12, cx - 2, cy + 18, C_YELLOW);
    }
}

// Fetch weather from Open-Meteo REST API
function fetchWeather() {
    if (!Network.isConnected()) {
        errorMessage = "WiFi Disconnected!";
        drawUI();
        return;
    }

    isLoading = true;
    errorMessage = "";
    drawUI();

    var city = CITIES[cityIndex];
    var url = "https://api.open-meteo.com/v1/forecast?latitude=" + city.lat +
              "&longitude=" + city.lon +
              "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,precipitation,weather_code,wind_speed_10m" +
              "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max" +
              "&timezone=auto";

    console.log("[Weather] Fetching Open-Meteo URL:", url);

    var res = Network.get(url, {}, 8000, false);
    isLoading = false;

    if (res.status === 200) {
        try {
            weatherData = JSON.parse(res.body);
            lastUpdated = System.getTime();
            console.log("[Weather] Successfully updated weather for", city.name);
        } catch (e) {
            errorMessage = "JSON Parse Error";
            console.error("[Weather] JSON Parse Exception:", e);
        }
    } else {
        errorMessage = "HTTP " + res.status + ": " + (res.error ? res.error : "Failed");
        console.error("[Weather] HTTP Request Error:", errorMessage);
    }

    drawUI();
}

// Day of week calculator from date string (YYYY-MM-DD)
function getDayName(dateStr, offset) {
    if (offset === 0) return "Today";
    var days = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
    if (dateStr && dateStr.length >= 10) {
        var parts = dateStr.split("-");
        var y = parseInt(parts[0], 10);
        var m = parseInt(parts[1], 10);
        var d = parseInt(parts[2], 10);
        // Zeller's congruence for day of week
        if (m < 3) { m += 12; y -= 1; }
        var k = y % 100;
        var j = Math.floor(y / 100);
        var f = d + Math.floor((13 * (m + 1)) / 5) + k + Math.floor(k / 4) + Math.floor(j / 4) + (5 * j);
        var dayIdx = ((f + 5) % 7); // 0=Sun, 1=Mon, ..., 6=Sat
        return days[dayIdx];
    }
    return "+" + offset + "d";
}

// Draw Main Dashboard UI
function drawUI() {
    var city = CITIES[cityIndex];
    var curr = weatherData && weatherData.current ? weatherData.current : null;
    var info = curr ? getWeatherInfo(curr.weather_code, curr.is_day) : { label: "Connecting...", icon: "sun", theme: C_BLUE };

    // Background Sky Gradient / Theme
    System.fillScreen(C_BLACK);
    System.fillRect(0, 0, SW, 125, info.theme);

    // Header City Selector Bar
    System.fillRect(0, 0, SW, 36, 0x0000);
    System.drawFastHLine(0, 36, SW, 0x2124);

    // Left Arrow
    System.fillRoundRect(5, 5, 28, 26, 4, C_DARKGRAY);
    System.setTextColor(C_CYAN, C_DARKGRAY);
    System.drawString("<", 14, 10, 2);

    // City Name (Tap to open modal)
    System.setTextColor(C_WHITE, 0x0000);
    System.drawString(city.name, 45, 10, 2);

    // Right Arrow
    System.fillRoundRect(135, 5, 28, 26, 4, C_DARKGRAY);
    System.setTextColor(C_CYAN, C_DARKGRAY);
    System.drawString(">", 144, 10, 2);

    // Unit Switcher (°C / °F)
    System.fillRoundRect(168, 5, 32, 26, 4, useFahrenheit ? C_ORANGE : C_BLUE);
    System.setTextColor(C_WHITE, useFahrenheit ? C_ORANGE : C_BLUE);
    System.drawString(useFahrenheit ? "°F" : "°C", 174, 10, 2);

    // Refresh Button
    System.fillRoundRect(205, 5, 30, 26, 4, C_GREEN);
    System.setTextColor(C_WHITE, C_GREEN);
    System.drawString("R", 215, 10, 2);

    if (isLoading) {
        System.setTextColor(C_YELLOW, info.theme);
        System.drawString("Fetching Open-Meteo...", 40, 65, 2);
        System.fillCircle(120, 95, 8, C_YELLOW);
        return;
    }

    if (errorMessage.length > 0) {
        System.setTextColor(C_RED, C_BLACK);
        System.drawString("Weather Error:", 10, 50, 2);
        System.setTextColor(C_YELLOW, C_BLACK);
        System.drawString(errorMessage, 10, 75, 2);

        System.fillRoundRect(20, 110, 200, 35, 6, C_BLUE);
        System.setTextColor(C_WHITE, C_BLUE);
        System.drawString("Tap to Reconnect WiFi", 30, 120, 2);
        return;
    }

    if (!curr) {
        System.setTextColor(C_WHITE, info.theme);
        System.drawString("Tap 'R' to Load Data", 50, 75, 2);
        return;
    }

    // Main Current Weather Hero
    drawWeatherIcon(info.icon, 45, 75, 22);

    // Large Temperature
    System.setTextColor(C_WHITE, info.theme);
    var tempStr = formatTemp(curr.temperature_2m);
    System.drawString(tempStr, 95, 55, 4);

    // Condition & Feels Like
    System.setTextColor(C_YELLOW, info.theme);
    System.drawString(info.label, 98, 90, 2);

    // 2x2 Metric Cards Grid (Y: 130 to 205)
    var cardW = 108;
    var cardH = 34;

    // Card 1: Humidity
    System.fillRoundRect(8, 130, cardW, cardH, 5, C_CARD_BG);
    System.drawRoundRect(8, 130, cardW, cardH, 5, C_CARD_BDR);
    System.setTextColor(C_CYAN, C_CARD_BG);
    System.drawString("Humidity", 14, 134, 1);
    System.setTextColor(C_WHITE, C_CARD_BG);
    System.drawString(curr.relative_humidity_2m + "%", 14, 146, 2);

    // Card 2: Wind Speed
    System.fillRoundRect(124, 130, cardW, cardH, 5, C_CARD_BG);
    System.drawRoundRect(124, 130, cardW, cardH, 5, C_CARD_BDR);
    System.setTextColor(C_CYAN, C_CARD_BG);
    System.drawString("Wind Speed", 130, 134, 1);
    System.setTextColor(C_WHITE, C_CARD_BG);
    System.drawString(curr.wind_speed_10m + " km/h", 130, 146, 2);

    // Card 3: Feels Like
    System.fillRoundRect(8, 168, cardW, cardH, 5, C_CARD_BG);
    System.drawRoundRect(8, 168, cardW, cardH, 5, C_CARD_BDR);
    System.setTextColor(C_ORANGE, C_CARD_BG);
    System.drawString("Feels Like", 14, 172, 1);
    System.setTextColor(C_WHITE, C_CARD_BG);
    System.drawString(formatTemp(curr.apparent_temperature), 14, 184, 2);

    // Card 4: Precipitation
    System.fillRoundRect(124, 168, cardW, cardH, 5, C_CARD_BG);
    System.drawRoundRect(124, 168, cardW, cardH, 5, C_CARD_BDR);
    System.setTextColor(C_ORANGE, C_CARD_BG);
    System.drawString("Precipitation", 130, 172, 1);
    System.setTextColor(C_WHITE, C_CARD_BG);
    System.drawString(curr.precipitation + " mm", 130, 184, 2);

    // 4-Day Outlook Forecast Strip (Y: 208 to 280)
    System.fillRoundRect(8, 208, 224, 76, 6, 0x0926);
    System.drawRoundRect(8, 208, 224, 76, 6, C_CARD_BDR);

    var daily = weatherData.daily;
    if (daily && daily.temperature_2m_max) {
        var colW = 56;
        for (var d = 0; d < 4; d++) {
            var colX = 8 + (d * colW);
            var dateStr = (daily.time && daily.time[d]) ? daily.time[d] : "";
            var dayLabel = (d === 0) ? "Today" : getDayName(dateStr, d);
            var dayCode = daily.weather_code ? daily.weather_code[d] : 0;
            var dayInfo = getWeatherInfo(dayCode, 1);
            var maxT = Math.round(daily.temperature_2m_max[d]);
            var minT = Math.round(daily.temperature_2m_min[d]);

            System.setTextColor(d === 0 ? C_CYAN : C_GRAY, 0x0926);
            System.drawString(dayLabel, colX + 10, 212, 1);

            drawWeatherIcon(dayInfo.icon, colX + 26, 238, 9);

            System.setTextColor(C_WHITE, 0x0926);
            System.drawString(maxT + "°", colX + 12, 253, 1);
            System.setTextColor(C_GRAY, 0x0926);
            System.drawString(minT + "°", colX + 32, 253, 1);

            if (d < 3) {
                System.drawFastVLine(colX + colW, 214, 64, 0x1A6B);
            }
        }
    }

    // Status Footer (Y: 290 to 318)
    System.setTextColor(C_GRAY, C_BLACK);
    System.drawString("Open-Meteo • Updated " + lastUpdated, 10, 292, 1);

    // Exit Button in Footer
    System.drawRoundRect(175, 290, 58, 24, 4, C_DARKGRAY);
    System.setTextColor(C_WHITE, C_BLACK);
    System.drawString("EXIT", 188, 294, 1);

    // City Selector Modal
    if (showCityModal) {
        drawCityModal();
    }
}

// Draw City Selection Modal
function drawCityModal() {
    System.fillRect(15, 40, 210, 240, 0x10A2);
    System.drawRoundRect(15, 40, 210, 240, 6, C_CYAN);

    System.fillRect(16, 41, 208, 26, C_BLUE);
    System.setTextColor(C_WHITE, C_BLUE);
    System.drawString("Select City", 75, 47, 2);

    // Close X
    System.setTextColor(C_RED, C_BLUE);
    System.drawString("X", 205, 47, 2);

    for (var i = 0; i < 5; i++) {
        var idx = i + modalScroll;
        if (idx >= CITIES.length) break;

        var rowY = 70 + (i * 32);
        var isSelected = (idx === cityIndex);
        var bg = isSelected ? 0x235F : 0x18C3;

        System.fillRoundRect(22, rowY, 196, 28, 4, bg);
        if (isSelected) System.drawRoundRect(22, rowY, 196, 28, 4, C_YELLOW);

        System.setTextColor(isSelected ? C_YELLOW : C_WHITE, bg);
        System.drawString(CITIES[idx].name, 32, rowY + 6, 2);
    }

    // Custom City button
    System.fillRoundRect(22, 235, 196, 32, 4, 0x0410);
    System.drawRoundRect(22, 235, 196, 32, 4, C_GREEN);
    System.setTextColor(C_GREEN, 0x0410);
    System.drawString("+ Custom Coordinates", 40, 243, 2);
}

// Initial Fetch & Draw
fetchWeather();

// Touch & Event Loop
var lastTouchTime = 0;

while (true) {
    var t = System.getTouch();

    if (t.touched && (System.millis() - lastTouchTime > 250)) {
        lastTouchTime = System.millis();

        if (showCityModal) {
            // Close button (X)
            if (t.x >= 190 && t.y >= 40 && t.y <= 70) {
                showCityModal = false;
                drawUI();
            }
            // City Rows (5 rows)
            for (var r = 0; r < 5; r++) {
                var rowY = 70 + (r * 32);
                if (t.x >= 22 && t.x <= 218 && t.y >= rowY && t.y <= rowY + 28) {
                    var picked = r + modalScroll;
                    if (picked < CITIES.length) {
                        cityIndex = picked;
                        showCityModal = false;
                        fetchWeather();
                    }
                }
            }
            // Custom Coordinates Button
            if (t.x >= 22 && t.x <= 218 && t.y >= 235 && t.y <= 270) {
                var cName = System.prompt("Enter City Name:", "My City");
                if (cName && cName.length > 0) {
                    var cLat = parseFloat(System.prompt("Enter Latitude (e.g. 37.77):", "37.77"));
                    var cLon = parseFloat(System.prompt("Enter Longitude (e.g. -122.41):", "-122.41"));
                    if (!isNaN(cLat) && !isNaN(cLon)) {
                        CITIES.push({ name: cName, lat: cLat, lon: cLon, tz: "auto" });
                        cityIndex = CITIES.length - 1;
                    }
                }
                showCityModal = false;
                fetchWeather();
            }
        } else {
            // Header: Left City Arrow
            if (t.x >= 5 && t.x <= 35 && t.y >= 5 && t.y <= 35) {
                cityIndex = (cityIndex - 1 + CITIES.length) % CITIES.length;
                fetchWeather();
            }
            // Header: City Name Title (Opens Modal)
            else if (t.x >= 40 && t.x <= 130 && t.y >= 5 && t.y <= 35) {
                showCityModal = true;
                drawUI();
            }
            // Header: Right City Arrow
            else if (t.x >= 135 && t.x <= 165 && t.y >= 5 && t.y <= 35) {
                cityIndex = (cityIndex + 1) % CITIES.length;
                fetchWeather();
            }
            // Header: Unit Toggle (°C / °F)
            else if (t.x >= 168 && t.x <= 200 && t.y >= 5 && t.y <= 35) {
                useFahrenheit = !useFahrenheit;
                drawUI();
            }
            // Header: Refresh Button
            else if (t.x >= 205 && t.x <= 238 && t.y >= 5 && t.y <= 35) {
                fetchWeather();
            }
            // Error Reconnect Prompt
            else if (errorMessage.length > 0 && t.x >= 20 && t.x <= 220 && t.y >= 110 && t.y <= 150) {
                Network.showWiFiPrompt();
                fetchWeather();
            }
            // Exit Button
            else if (t.x >= 170 && t.y >= 285) {
                break; // Exit app back to KryonOS Launcher
            }
        }
    }

    System.delay(50);
}
