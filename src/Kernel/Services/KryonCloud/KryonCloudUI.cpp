#include "KryonCloudUI.h"
#include "../../../FileSystem/FileSystem.h"
#include "../../Core/HarixKernel.h"
#include "../../../Keyboard/MyKeyboard.h"

extern int currentState;

TFT_eSPI *KryonCloudUI::tftInstance = nullptr;
CloudUISubState KryonCloudUI::currentSubState = CLOUD_STATE_OVERVIEW;
int KryonCloudUI::selectedIndex = 0;
int KryonCloudUI::scrollOffset = 0;
unsigned long KryonCloudUI::lastWaitPollTime = 0;

String KryonCloudUI::activePairingCode = "";
String KryonCloudUI::activePairingId = "";
String KryonCloudUI::activeChallenge = "";
int KryonCloudUI::pairingExpiresIn = 1200;
bool KryonCloudUI::isWaitingForClaim = false;
unsigned long KryonCloudUI::pairingStartTime = 0;

std::vector<CloudFileItem> KryonCloudUI::cachedSharedFiles;
std::vector<CloudFileItem> KryonCloudUI::cachedDeviceFiles;
bool KryonCloudUI::manifestLoaded = false;
bool KryonCloudUI::storageScopeDevice = false;
String KryonCloudUI::storageStatusToast = "";
unsigned long KryonCloudUI::storageStatusToastTime = 0;

std::vector<BeamMessage> KryonCloudUI::cachedBeamMessages;
std::vector<BeamMessage> KryonCloudUI::cachedPublicMessages;
bool KryonCloudUI::beamInboxLoaded = false;
bool KryonCloudUI::beamPublicLoaded = false;
bool KryonCloudUI::beamPublicScope = false;
int KryonCloudUI::beamViewingIndex = -1;
bool KryonCloudUI::beamComposing = false;
String KryonCloudUI::beamTargetHandle = "";
int KryonCloudUI::beamMsgTypeIndex = 0;
String KryonCloudUI::beamContent = "Hello from KryonOS Node!";
int KryonCloudUI::beamPublicPage = 1;
String KryonCloudUI::beamStatusToast = "";
unsigned long KryonCloudUI::beamStatusToastTime = 0;

String KryonCloudUI::aiConsolePrompt = "Explain ESP32 dual-core FreeRTOS";
String KryonCloudUI::aiConsoleResponse = "Ready. Tap 'Ask Stream' or tap the prompt to edit.";
bool KryonCloudUI::aiStreamingActive = false;
int KryonCloudUI::aiRemainingTokens = 100;
int KryonCloudUI::aiScrollOffset = 0;
CloudBanStatus KryonCloudUI::cachedBanStatus;

// ============================================================================
// HIGH-CONTRAST VIBRANT PALETTE (ZERO DULL / MUDDY COLORS, ZERO CYAN)
// ============================================================================
#define CLOUD_BG          0x0000        // Pure deep obsidian black
#define CLOUD_NAV_BG      0x10A2        // Sleek elevated dark navy bar
#define CLOUD_CARD_BG     0x18E4        // Sleek modern elevated card background
#define CLOUD_CARD_BORDER 0x39E8        // Crisp visible border
#define CLOUD_ACCENT_BLUE 0x229F        // Vibrant Electric Royal Blue
#define CLOUD_PILL_INACT  0x2125        // Clean dark pill
#define CLOUD_TRACK_BG    0x2104        // Clean dark progress track background
#define CLOUD_BAR_AI      0xFDA0        // Vivid Amber / Gold for AI tokens
#define CLOUD_BAR_STR     0x07E0        // Vivid Neon Green for Storage (TFT_GREEN)
#define CLOUD_TEXT_MUTED  0xC618        // Clean readable silver

// ============================================================================
// LOADING SCREEN WITH ANIMATED PROGRESS BAR
// ============================================================================
void KryonCloudUI::showLoadingScreen(const String& status, int progressPct) {
    if (!tftInstance) return;
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    bool isLandscape = (screenH < 260);

    if (progressPct <= 20) {
        tftInstance->fillScreen(CLOUD_BG);

        // Title
        tftInstance->setTextColor(TFT_WHITE, CLOUD_BG);
        tftInstance->setTextDatum(MC_DATUM);
        int titleY = isLandscape ? 48 : 85;
        tftInstance->drawString("KryonCloud", screenW / 2, titleY, 4);

        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
        int subY = isLandscape ? 76 : 115;
        tftInstance->drawString("Connecting to Platform...", screenW / 2, subY, 2);
    }

    // Status Message Area
    int statusY = isLandscape ? 116 : 150;
    tftInstance->fillRect(10, statusY - 8, screenW - 20, 24, CLOUD_BG);
    tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(status.c_str(), screenW / 2, statusY + 4, 2);

    // Progress Bar Track
    int trackW = min(260, screenW - 40);
    int trackX = (screenW - trackW) / 2;
    int trackY = isLandscape ? 146 : 184;
    tftInstance->drawRoundRect(trackX, trackY, trackW, 16, 4, CLOUD_CARD_BORDER);
    tftInstance->fillRect(trackX + 2, trackY + 2, trackW - 4, 12, CLOUD_TRACK_BG);

    int maxFill = trackW - 4;
    int barW = (maxFill * progressPct) / 100;
    if (barW < 0) barW = 0;
    if (barW > maxFill) barW = maxFill;

    if (barW > 0) {
        tftInstance->fillRoundRect(trackX + 2, trackY + 2, barW, 12, 3, CLOUD_ACCENT_BLUE);
    }

    // Percentage
    int pctY = isLandscape ? 176 : 218;
    tftInstance->fillRect(screenW / 2 - 35, pctY - 10, 70, 20, CLOUD_BG);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_BG);
    tftInstance->drawString((String(progressPct) + "%").c_str(), screenW / 2, pctY, 2);
}

void KryonCloudUI::init(TFT_eSPI *tft) {
    tftInstance = tft;
}

void KryonCloudUI::resetToHome() {
    selectedIndex = 0;
    scrollOffset = 0;
    manifestLoaded = false;
    beamInboxLoaded = false;
    beamViewingIndex = -1;
    beamComposing = false;
    aiStreamingActive = false;

    // Show immediate loading animation before blocking network sync
    showLoadingScreen("Connecting to KryonCloud...", 15);

    if (KryonCloudManager::isPaired()) {
        currentSubState = CLOUD_STATE_OVERVIEW;
        showLoadingScreen("Checking Account Status...", 25);
        KryonCloudManager::checkBanStatus(cachedBanStatus);
        if (cachedBanStatus.isBanned) {
            currentSubState = CLOUD_STATE_BAN_STATUS;
            showLoadingScreen("Device Suspended", 100);
            delay(200);
            draw();
            return;
        }

        showLoadingScreen("Syncing Profile...", 50);
        KryonCloudManager::syncHeartbeat();
        if (cachedBanStatus.isBanned) {
            currentSubState = CLOUD_STATE_BAN_STATUS;
            showLoadingScreen("Device Suspended", 100);
            delay(200);
            draw();
            return;
        }

        showLoadingScreen("Fetching Limits...", 80);
        KryonCloudManager::fetchAccountLimits();
        if (cachedBanStatus.isBanned) {
            currentSubState = CLOUD_STATE_BAN_STATUS;
            showLoadingScreen("Device Suspended", 100);
            delay(200);
            draw();
            return;
        }

        showLoadingScreen("Ready!", 100);
    } else {
        currentSubState = CLOUD_STATE_PAIRING;
        activePairingCode = "";
        isWaitingForClaim = false;
        showLoadingScreen("Opening Pairing Wizard...", 85);
        showLoadingScreen("Ready!", 100);
    }
}

void KryonCloudUI::draw() {
    if (!tftInstance) return;

    if (cachedBanStatus.isBanned) {
        currentSubState = CLOUD_STATE_BAN_STATUS;
        drawBanStatusScreen();
        return;
    }

    if (!KryonCloudManager::isPaired() && currentSubState != CLOUD_STATE_PAIRING) {
        currentSubState = CLOUD_STATE_PAIRING;
    }

    if (currentSubState == CLOUD_STATE_PAIRING) {
        drawPairingScreen();
        return;
    }

    // Top Navigation & Header for Paired Views
    drawTopNav();

    switch (currentSubState) {
        case CLOUD_STATE_OVERVIEW:
            drawOverviewScreen();
            break;
        case CLOUD_STATE_KRYON_AI:
            drawKryonAIScreen();
            break;
        case CLOUD_STATE_KRYON_BEAM:
            drawBeamScreen();
            break;
        case CLOUD_STATE_STORAGE:
            drawStorageScreen();
            break;
        case CLOUD_STATE_LIMITS:
            drawLimitsScreen();
            break;
        case CLOUD_STATE_BAN_STATUS:
            drawBanStatusScreen();
            break;
        default:
            break;
    }
}

// ============================================================================
// TOP NAVIGATION BAR (Dynamic title + 5 Subscreen Tabs)
// 1. Home (KryonCloud) 2. AI (KryonAI) 3. Beam (KryonBeam) 4. Drive (KryonDrive) 5. Limits (Usage Limits)
// ============================================================================
void KryonCloudUI::drawTopNav() {
    int screenW = tftInstance->width();

    // Header Bar
    tftInstance->fillRect(0, 0, screenW, 56, CLOUD_NAV_BG);
    tftInstance->drawFastHLine(0, 56, screenW, CLOUD_CARD_BORDER);

    // Title & Status Dot
    bool online = KryonCloudManager::isConnected();
    tftInstance->fillCircle(12, 14, 4, online ? TFT_GREEN : TFT_RED);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_NAV_BG);
    tftInstance->setTextDatum(ML_DATUM);

    // Dynamic Title per Subscreen
    const char* subTitles[] = {
        "KryonCloud",     // CLOUD_STATE_PAIRING
        "KryonCloud",     // CLOUD_STATE_OVERVIEW
        "KryonAI",        // CLOUD_STATE_KRYON_AI
        "KryonBeam",      // CLOUD_STATE_KRYON_BEAM
        "KryonDrive",     // CLOUD_STATE_STORAGE
        "Usage Limits",   // CLOUD_STATE_LIMITS
        "Account Status"  // CLOUD_STATE_BAN_STATUS
    };
    const char* activeTitle = subTitles[currentSubState];
    tftInstance->drawString(activeTitle, 22, 14, 2);

    // Top-Right Exit Button [X]
    tftInstance->fillRoundRect(screenW - 32, 4, 26, 20, 4, 0x8800);
    tftInstance->setTextColor(TFT_WHITE, 0x8800);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("X", screenW - 19, 14, 2);

    // 5 Tab Buttons
    const char* tabNames[] = { "Home", "AI", "Beam", "Drive", "Limits" };
    CloudUISubState tabStates[] = {
        CLOUD_STATE_OVERVIEW,
        CLOUD_STATE_KRYON_AI,
        CLOUD_STATE_KRYON_BEAM,
        CLOUD_STATE_STORAGE,
        CLOUD_STATE_LIMITS
    };

    int tabW = (screenW - 6) / 5;
    for (int i = 0; i < 5; i++) {
        int tx = 3 + i * tabW;
        bool active = (currentSubState == tabStates[i]);

        if (active) {
            tftInstance->fillRoundRect(tx, 30, tabW - 2, 22, 4, CLOUD_ACCENT_BLUE);
            tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
        } else {
            tftInstance->fillRoundRect(tx, 30, tabW - 2, 22, 4, CLOUD_PILL_INACT);
            tftInstance->drawRoundRect(tx, 30, tabW - 2, 22, 4, 0x31A7);
            tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_PILL_INACT);
        }
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(tabNames[i], tx + (tabW - 2) / 2, 41, 2);
    }
}

void KryonCloudUI::handleTopNavTouch(uint16_t x, uint16_t y) {
    if (!tftInstance) return;
    int screenW = tftInstance->width();

    // Close button
    if (x >= screenW - 40 && y <= 26) {
        currentState = 0; // STATE_LAUNCHER
        return;
    }

    // Tab switching
    if (y >= 28 && y <= 54) {
        int tabW = (screenW - 6) / 5;
        int tabIdx = (x - 3) / tabW;
        if (tabIdx >= 0 && tabIdx < 5) {
            CloudUISubState tabStates[] = {
                CLOUD_STATE_OVERVIEW,
                CLOUD_STATE_KRYON_AI,
                CLOUD_STATE_KRYON_BEAM,
                CLOUD_STATE_STORAGE,
                CLOUD_STATE_LIMITS
            };
            if (currentSubState != tabStates[tabIdx]) {
                currentSubState = tabStates[tabIdx];
                beamViewingIndex = -1;
                beamComposing = false;
                draw();
            }
        }
    }
}

// ============================================================================
// 1. PAIRING WIZARD SCREEN
// ============================================================================
void KryonCloudUI::startPairingInit() {
    if (activePairingCode.length() > 0) return;
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();

    tftInstance->fillRect(10, 80, screenW - 20, screenH - 90, CLOUD_BG);
    tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Contacting Cloud Hub...", screenW / 2, screenH / 2, 2);

    bool ok = KryonCloudManager::initPairingSession(activePairingCode, activePairingId, 
                                                    activeChallenge, pairingExpiresIn);
    if (ok) {
        isWaitingForClaim = true;
        pairingStartTime = millis();
        drawPairingScreen();
    } else {
        tftInstance->fillRect(10, 80, screenW - 20, screenH - 90, CLOUD_BG);
        tftInstance->setTextColor(TFT_RED, CLOUD_BG);
        tftInstance->drawString("Pairing Init Failed", screenW / 2, screenH / 2 - 15, 2);
        tftInstance->drawString("Check WiFi Connection", screenW / 2, screenH / 2 + 15, 2);
    }
}

void KryonCloudUI::drawPairingScreen() {
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();

    tftInstance->fillScreen(CLOUD_BG);

    // Header Frame
    tftInstance->drawRoundRect(3, 3, screenW - 6, screenH - 6, 5, CLOUD_CARD_BORDER);
    tftInstance->fillRoundRect(6, 6, screenW - 12, 30, 5, CLOUD_NAV_BG);
    tftInstance->drawRoundRect(6, 6, screenW - 12, 30, 5, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_NAV_BG);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("KryonCloud Link", screenW / 2, 21, 2);

    if (activePairingCode.length() == 0) {
        startPairingInit();
        return;
    }

    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
    tftInstance->setTextDatum(TC_DATUM);
    tftInstance->drawString("Open your browser & visit:", screenW / 2, 44, 2);
    
    tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
    tftInstance->drawString("kryonos.harislab.tech", screenW / 2, 62, 2);

    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
    int labelY = (screenH < 260) ? 82 : 95;
    tftInstance->drawString("Enter this Pairing Code:", screenW / 2, labelY, 2);

    // Large Code Box
    int boxY = (screenH < 260) ? 102 : 120;
    int boxH = (screenH < 260) ? 44 : 55;
    int boxW = min(260, screenW - 30);
    int boxX = (screenW - boxW) / 2;
    tftInstance->fillRoundRect(boxX, boxY, boxW, boxH, 8, CLOUD_CARD_BG);
    tftInstance->drawRoundRect(boxX, boxY, boxW, boxH, 8, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(activePairingCode.c_str(), screenW / 2, boxY + boxH / 2, 4);

    tftInstance->setTextColor(TFT_GREEN, CLOUD_BG);
    int waitY = (screenH < 260) ? 158 : 195;
    tftInstance->drawString("Waiting for user claim...", screenW / 2, waitY, 2);

    // Action Buttons
    int btnY = (screenH < 260) ? (screenH - 46) : 240;
    int btnW = (screenW - 40) / 2;
    tftInstance->fillRoundRect(15, btnY, btnW, 35, 5, CLOUD_PILL_INACT);
    tftInstance->drawRoundRect(15, btnY, btnW, 35, 5, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_PILL_INACT);
    tftInstance->drawString("NEW CODE", 15 + btnW / 2, btnY + 17, 2);

    tftInstance->fillRoundRect(25 + btnW, btnY, btnW, 35, 5, 0x9000);
    tftInstance->setTextColor(TFT_WHITE, 0x9000);
    tftInstance->drawString("CANCEL", 25 + btnW + btnW / 2, btnY + 17, 2);
}

void KryonCloudUI::handlePairingTouch(uint16_t x, uint16_t y) {
    if (!tftInstance) return;
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    int btnY = (screenH < 260) ? (screenH - 46) : 240;
    int btnW = (screenW - 40) / 2;

    if (y >= btnY - 5 && y <= btnY + 40) {
        if (x >= 15 && x <= (15 + btnW)) {
            activePairingCode = "";
            startPairingInit();
        } else if (x >= (25 + btnW) && x <= (25 + 2 * btnW)) {
            currentState = 0; // STATE_LAUNCHER
        }
    }
}

// ============================================================================
// 2. OVERVIEW SCREEN (HOME)
// ============================================================================
void KryonCloudUI::drawOverviewScreen() {
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    bool isLandscape = (screenH < 260);

    tftInstance->fillRect(0, 57, screenW, screenH - 57, CLOUD_BG);

    const CloudLimits& lim = KryonCloudManager::getLimits();

    if (isLandscape) {
        // Landscape Mode (e.g. T-Deck 320x240): High-density dual-column layout
        int colW = (screenW - 24) / 2;
        int c1X = 8;
        int c2X = c1X + colW + 8;

        // --- Column 1: Account Profile Card ---
        tftInstance->fillRoundRect(c1X, 62, colW, 110, 6, CLOUD_CARD_BG);
        tftInstance->drawRoundRect(c1X, 62, colW, 110, 6, CLOUD_CARD_BORDER);

        tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TL_DATUM);
        String uName = KryonCloudManager::getUserName();
        if (uName.length() > 16) uName = uName.substring(0, 14) + "..";
        tftInstance->drawString(uName.c_str(), c1X + 8, 68, 2);

        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
        String email = KryonCloudManager::getAccountEmail();
        if (email.length() > 18) email = email.substring(0, 16) + "..";
        tftInstance->drawString(email.c_str(), c1X + 8, 88, 1);

        tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
        tftInstance->drawString(("@" + KryonCloudManager::getBeamHandle()).c_str(), c1X + 8, 106, 2);

        tftInstance->setTextColor(TFT_GREEN, CLOUD_CARD_BG);
        tftInstance->drawString("Status: Verified Node", c1X + 8, 132, 1);

        // --- Column 1: Action Buttons below Profile Card ---
        int btnY = 180;
        int btnH = 34;
        int b1W = (colW - 6) / 2;

        tftInstance->fillRoundRect(c1X, btnY, b1W, btnH, 5, CLOUD_ACCENT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Sync", c1X + b1W / 2, btnY + btnH / 2, 2);

        tftInstance->fillRoundRect(c1X + b1W + 6, btnY, b1W, btnH, 5, 0x9000);
        tftInstance->setTextColor(TFT_WHITE, 0x9000);
        tftInstance->drawString("Unpair", c1X + b1W + 6 + b1W / 2, btnY + btnH / 2, 2);

        // --- Column 2: Live Quotas Summary Card ---
        tftInstance->fillRoundRect(c2X, 62, colW, 152, 6, CLOUD_CARD_BG);
        tftInstance->drawRoundRect(c2X, 62, colW, 152, 6, CLOUD_CARD_BORDER);

        tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString("Daily AI Quota:", c2X + 8, 68, 2);

        tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TR_DATUM);
        tftInstance->drawString((String(lim.dailyAiRemaining) + " left").c_str(), c2X + colW - 8, 68, 2);

        int barTrackW = colW - 16;
        tftInstance->drawRoundRect(c2X + 8, 88, barTrackW, 8, 3, CLOUD_CARD_BORDER);
        tftInstance->fillRect(c2X + 9, 89, barTrackW - 2, 6, CLOUD_TRACK_BG);
        int aiW = (lim.dailyAiLimit > 0) ? (lim.dailyAiUsed * (barTrackW - 2)) / lim.dailyAiLimit : 0;
        if (aiW > barTrackW - 2) aiW = barTrackW - 2;
        if (aiW > 0) {
            uint16_t aiBarColor = (aiW > (barTrackW * 85) / 100) ? TFT_RED : ((aiW > (barTrackW * 70) / 100) ? TFT_ORANGE : 0x07E0);
            tftInstance->fillRoundRect(c2X + 9, 89, aiW, 6, 2, aiBarColor);
        }

        tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString("Cloud Storage:", c2X + 8, 106, 2);

        tftInstance->setTextColor(TFT_GREEN, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TR_DATUM);
        tftInstance->drawString((String(lim.storageRemainingBytes / 1024) + "K free").c_str(), c2X + colW - 8, 106, 2);

        tftInstance->drawRoundRect(c2X + 8, 126, barTrackW, 8, 3, CLOUD_CARD_BORDER);
        tftInstance->fillRect(c2X + 9, 127, barTrackW - 2, 6, CLOUD_TRACK_BG);
        int strW = (lim.storageQuotaBytes > 0) ? (lim.storageUsedBytes * (barTrackW - 2)) / lim.storageQuotaBytes : 0;
        if (strW > barTrackW - 2) strW = barTrackW - 2;
        if (strW > 0) {
            uint16_t strBarColor = (strW > (barTrackW * 85) / 100) ? TFT_RED : ((strW > (barTrackW * 70) / 100) ? TFT_ORANGE : 0x07E0);
            tftInstance->fillRoundRect(c2X + 9, 127, strW, 6, 2, strBarColor);
        }

        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString(("Total: " + String(lim.storageQuotaMb) + " MB Cloud Disk").c_str(), c2X + 8, 142, 1);

    } else {
        // Portrait Mode (e.g. 240x320)
        int cardW = screenW - 16;
        tftInstance->fillRoundRect(8, 62, cardW, 72, 6, CLOUD_CARD_BG);
        tftInstance->drawRoundRect(8, 62, cardW, 72, 6, CLOUD_CARD_BORDER);
        
        tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString(KryonCloudManager::getUserName().c_str(), 16, 68, 2);
        
        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
        tftInstance->drawString(KryonCloudManager::getAccountEmail().c_str(), 16, 88, 2);

        tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
        tftInstance->drawString(("Handle: " + KryonCloudManager::getBeamHandle()).c_str(), 16, 108, 2);

        tftInstance->fillRoundRect(8, 140, cardW, 96, 6, CLOUD_CARD_BG);
        tftInstance->drawRoundRect(8, 140, cardW, 96, 6, CLOUD_CARD_BORDER);

        tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString("Daily AI Quota:", 16, 146, 2);
        tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
        tftInstance->drawString((String(lim.dailyAiRemaining) + " left").c_str(), cardW - 70, 146, 2);

        int barTrackW = cardW - 16;
        tftInstance->drawRoundRect(16, 166, barTrackW, 8, 3, CLOUD_CARD_BORDER);
        tftInstance->fillRect(17, 167, barTrackW - 2, 6, CLOUD_TRACK_BG);
        int aiW = (lim.dailyAiLimit > 0) ? (lim.dailyAiUsed * (barTrackW - 2)) / lim.dailyAiLimit : 0;
        if (aiW > barTrackW - 2) aiW = barTrackW - 2;
        if (aiW > 0) {
            uint16_t aiBarColor = (aiW > (barTrackW * 85) / 100) ? TFT_RED : ((aiW > (barTrackW * 70) / 100) ? TFT_ORANGE : 0x07E0);
            tftInstance->fillRoundRect(17, 167, aiW, 6, 2, aiBarColor);
        }

        tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
        tftInstance->drawString("Cloud Storage:", 16, 182, 2);
        tftInstance->setTextColor(TFT_GREEN, CLOUD_CARD_BG);
        tftInstance->drawString((String(lim.storageRemainingBytes / 1024) + " KB free").c_str(), cardW - 90, 182, 2);

        tftInstance->drawRoundRect(16, 202, barTrackW, 8, 3, CLOUD_CARD_BORDER);
        tftInstance->fillRect(17, 203, barTrackW - 2, 6, CLOUD_TRACK_BG);
        int strW = (lim.storageQuotaBytes > 0) ? (lim.storageUsedBytes * (barTrackW - 2)) / lim.storageQuotaBytes : 0;
        if (strW > barTrackW - 2) strW = barTrackW - 2;
        if (strW > 0) {
            uint16_t strBarColor = (strW > (barTrackW * 85) / 100) ? TFT_RED : ((strW > (barTrackW * 70) / 100) ? TFT_ORANGE : 0x07E0);
            tftInstance->fillRoundRect(17, 203, strW, 6, 2, strBarColor);
        }

        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
        tftInstance->drawString("Node Status: Online & Verified", 16, 218, 1);

        int btnW = (cardW - 8) / 2;
        tftInstance->fillRoundRect(8, 244, btnW, 34, 5, CLOUD_ACCENT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Sync Fresh", 8 + btnW / 2, 261, 2);

        tftInstance->fillRoundRect(8 + btnW + 8, 244, btnW, 34, 5, 0x9000);
        tftInstance->setTextColor(TFT_WHITE, 0x9000);
        tftInstance->drawString("Unpair Board", 8 + btnW + 8 + btnW / 2, 261, 2);
    }
}

void KryonCloudUI::handleOverviewTouch(uint16_t x, uint16_t y) {
    if (y < 56) {
        handleTopNavTouch(x, y);
        return;
    }

    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    bool isLandscape = (screenH < 260);

    if (isLandscape) {
        int colW = (screenW - 24) / 2;
        int c1X = 8;
        int btnY = 180;
        int btnH = 34;
        int b1W = (colW - 6) / 2;

        if (y >= btnY && y <= btnY + btnH) {
            if (x >= c1X && x <= c1X + b1W) {
                // Sync
                tftInstance->fillRect(c1X, btnY, b1W, btnH, 0x0215);
                tftInstance->setTextColor(TFT_YELLOW, 0x0215);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("Syncing...", c1X + b1W / 2, btnY + btnH / 2, 2);
                KryonCloudManager::syncAllFreshData();
                drawOverviewScreen();
            } else if (x >= c1X + b1W + 6 && x <= c1X + 2 * b1W + 6) {
                // Unpair
                KryonCloudManager::unpair();
                resetToHome();
                draw();
            }
        }
    } else {
        int cardW = screenW - 16;
        int btnW = (cardW - 8) / 2;
        if (y >= 244 && y <= 278) {
            if (x >= 8 && x <= 8 + btnW) {
                // Sync Fresh Data
                tftInstance->fillRect(8, 244, btnW, 34, 0x0215);
                tftInstance->setTextColor(TFT_YELLOW, 0x0215);
                tftInstance->setTextDatum(MC_DATUM);
                tftInstance->drawString("Syncing...", 8 + btnW / 2, 261, 2);
                KryonCloudManager::syncAllFreshData();
                drawOverviewScreen();
            } else if (x >= 8 + btnW + 8 && x <= 8 + 2 * btnW + 8) {
                // Unpair Device
                KryonCloudManager::unpair();
                resetToHome();
                draw();
            }
        }
    }
}

// ============================================================================
// ============================================================================
// 3. KRYONAI SCREEN (Interactive Studio with Keyboard & Smooth Scrolling)
// ============================================================================
void KryonCloudUI::drawKryonAIScreen() {
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    bool isLandscape = (screenH < 260);

    tftInstance->fillRect(0, 57, screenW, screenH - 57, CLOUD_BG);

    int cardW = screenW - 16;
    int promptH = isLandscape ? 32 : 40;

    // Prompt Card (Tap to edit)
    tftInstance->fillRoundRect(8, 60, cardW, promptH, 5, CLOUD_CARD_BG);
    tftInstance->drawRoundRect(8, 60, cardW, promptH, 5, CLOUD_ACCENT_BLUE);
    
    tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString("Prompt (Tap to type):", 14, 62, 1);
    
    tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
    String dispPrompt = aiConsolePrompt;
    int maxPromptLen = isLandscape ? 38 : 28;
    if ((int)dispPrompt.length() > maxPromptLen) dispPrompt = dispPrompt.substring(0, maxPromptLen - 3) + "...";
    tftInstance->drawString(dispPrompt.c_str(), 14, isLandscape ? 74 : 78, 2);

    // Action Buttons: "Ask AI" + "Clear" + "UP" + "DN"
    int btnY = isLandscape ? 96 : 106;
    int btnH = isLandscape ? 24 : 26;

    tftInstance->fillRoundRect(8, btnY, 76, btnH, 4, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Ask AI", 46, btnY + btnH / 2, 2);

    tftInstance->fillRoundRect(88, btnY, 46, btnH, 4, 0x6000);
    tftInstance->setTextColor(TFT_WHITE, 0x6000);
    tftInstance->drawString("Clear", 111, btnY + btnH / 2, 2);

    tftInstance->fillRoundRect(138, btnY, 36, btnH, 4, CLOUD_PILL_INACT);
    tftInstance->drawRoundRect(138, btnY, 36, btnH, 4, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_PILL_INACT);
    tftInstance->drawString("UP", 156, btnY + btnH / 2, 2);

    tftInstance->fillRoundRect(178, btnY, 36, btnH, 4, CLOUD_PILL_INACT);
    tftInstance->drawRoundRect(178, btnY, 36, btnH, 4, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_PILL_INACT);
    tftInstance->drawString("DN", 196, btnY + btnH / 2, 2);

    const CloudLimits& lim = KryonCloudManager::getLimits();
    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
    tftInstance->setTextDatum(TR_DATUM);
    tftInstance->drawString(("Daily AI: " + String(lim.dailyAiRemaining)).c_str(), screenW - 8, btnY + 4, 2);

    // Response Window (Strictly bounded)
    int respY = isLandscape ? 124 : 136;
    int respH = isLandscape ? (screenH - respY - 6) : 146;

    tftInstance->fillRoundRect(8, respY, cardW, respH, 5, CLOUD_CARD_BG);
    tftInstance->drawRoundRect(8, respY, cardW, respH, 5, CLOUD_CARD_BORDER);

    tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString("AI Response:", 14, respY + 2, 1);

    // Render bounded text with scrolling
    int wrapW = cardW - 14;
    int wrapH = respH - 16;
    drawWrappedText(aiConsoleResponse, 14, respY + 14, wrapW, wrapH, TFT_WHITE, 2, aiScrollOffset);

    // Footer info for portrait mode
    if (!isLandscape) {
        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(("Daily AI Remaining: " + String(lim.dailyAiRemaining) + " / " + String(lim.dailyAiLimit)).c_str(), screenW / 2, screenH - 22, 1);
    }
}

void KryonCloudUI::handleKryonAITouch(uint16_t x, uint16_t y) {
    if (y < 56) {
        handleTopNavTouch(x, y);
        return;
    }

    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    bool isLandscape = (screenH < 260);

    int cardW = screenW - 16;
    int promptH = isLandscape ? 32 : 40;
    int btnY = isLandscape ? 96 : 106;
    int btnH = isLandscape ? 24 : 26;
    int respY = isLandscape ? 124 : 136;
    int respH = isLandscape ? (screenH - respY - 6) : 146;
    int wrapW = cardW - 14;
    int wrapH = respH - 16;
    int maxLines = isLandscape ? 5 : 7;

    // Tap prompt box -> Open Keyboard
    if (y >= 60 && y <= 60 + promptH) {
        String input = MyKeyboard::getString(aiConsolePrompt, "KryonAI Prompt:", 160);
        if (input.length() > 0) {
            aiConsolePrompt = input;
        }
        draw();
        return;
    }

    // Action Buttons
    if (y >= btnY && y <= btnY + btnH) {
        if (x >= 8 && x <= 84) {
            // Ask AI Button
            if (!KryonCloudManager::isConnected()) {
                aiConsoleResponse = "WiFi not connected! Connect in Settings.";
                drawKryonAIScreen();
                return;
            }
            if (!KryonCloudManager::isPaired()) {
                aiConsoleResponse = "Device not paired! Pair in Overview tab first.";
                drawKryonAIScreen();
                return;
            }

            aiConsoleResponse = "";
            aiScrollOffset = 0;
            tftInstance->fillRect(10, respY + 14, wrapW, wrapH, CLOUD_CARD_BG);
            tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
            tftInstance->setTextDatum(TL_DATUM);
            tftInstance->drawString("Connecting & Streaming...", 14, respY + 14, 2);

            aiStreamingActive = true;
            KryonCloudAI::stream(
                aiConsolePrompt,
                [wrapW, wrapH, respY, maxLines](const String& token) {
                    if (tftInstance) {
                        aiConsoleResponse += token;
                        int totalL = getTextLineCount(aiConsoleResponse, wrapW, 2);
                        aiScrollOffset = (totalL > maxLines) ? (totalL - maxLines) : 0;
                        tftInstance->fillRect(10, respY + 14, wrapW, wrapH, CLOUD_CARD_BG);
                        drawWrappedText(aiConsoleResponse, 14, respY + 14, wrapW, wrapH, TFT_WHITE, 2, aiScrollOffset);
                    }
                },
                [wrapW, wrapH, respY, maxLines](const String& fullText, const AiUsageStats& usage) {
                    aiConsoleResponse = fullText;
                    aiStreamingActive = false;
                    int totalL = getTextLineCount(aiConsoleResponse, wrapW, 2);
                    aiScrollOffset = (totalL > maxLines) ? (totalL - maxLines) : 0;
                    if (tftInstance) {
                        tftInstance->fillRect(10, respY + 14, wrapW, wrapH, CLOUD_CARD_BG);
                        drawWrappedText(aiConsoleResponse, 14, respY + 14, wrapW, wrapH, TFT_WHITE, 2, aiScrollOffset);
                    }
                },
                [wrapW, wrapH, respY](const String& err) {
                    aiConsoleResponse = "Error: " + err;
                    aiStreamingActive = false;
                    if (tftInstance) {
                        tftInstance->fillRect(10, respY + 14, wrapW, wrapH, CLOUD_CARD_BG);
                        drawWrappedText(aiConsoleResponse, 14, respY + 14, wrapW, wrapH, TFT_WHITE, 2, 0);
                    }
                }
            );
            drawKryonAIScreen();
            return;
        } else if (x >= 88 && x <= 134) {
            // Clear
            aiConsoleResponse = "Ready. Tap 'Ask AI' or tap the prompt to edit.";
            aiScrollOffset = 0;
            drawKryonAIScreen();
            return;
        } else if (x >= 138 && x <= 174) {
            // UP button
            if (aiScrollOffset > 0) {
                aiScrollOffset = max(0, aiScrollOffset - 2);
                tftInstance->fillRect(10, respY + 14, wrapW, wrapH, CLOUD_CARD_BG);
                drawWrappedText(aiConsoleResponse, 14, respY + 14, wrapW, wrapH, TFT_WHITE, 2, aiScrollOffset);
            }
            return;
        } else if (x >= 178 && x <= 214) {
            // DN button
            int totalL = getTextLineCount(aiConsoleResponse, wrapW, 2);
            if (aiScrollOffset < totalL - maxLines) {
                aiScrollOffset = min(totalL - maxLines, aiScrollOffset + 2);
                tftInstance->fillRect(10, respY + 14, wrapW, wrapH, CLOUD_CARD_BG);
                drawWrappedText(aiConsoleResponse, 14, respY + 14, wrapW, wrapH, TFT_WHITE, 2, aiScrollOffset);
            }
            return;
        }
    }

    // Tap in Response Window area to scroll smoothly
    if (y >= respY && y <= respY + respH) {
        int totalL = getTextLineCount(aiConsoleResponse, wrapW, 2);
        if (y < respY + respH / 2) {
            // Tap top half -> Scroll UP
            if (aiScrollOffset > 0) {
                aiScrollOffset = max(0, aiScrollOffset - 2);
                tftInstance->fillRect(10, respY + 14, wrapW, wrapH, CLOUD_CARD_BG);
                drawWrappedText(aiConsoleResponse, 14, respY + 14, wrapW, wrapH, TFT_WHITE, 2, aiScrollOffset);
            }
        } else {
            // Tap bottom half -> Scroll DOWN
            if (aiScrollOffset < totalL - maxLines) {
                aiScrollOffset = min(totalL - maxLines, aiScrollOffset + 2);
                tftInstance->fillRect(10, respY + 14, wrapW, wrapH, CLOUD_CARD_BG);
                drawWrappedText(aiConsoleResponse, 14, respY + 14, wrapW, wrapH, TFT_WHITE, 2, aiScrollOffset);
            }
        }
    }
}

// ============================================================================
// 4. KRYONBEAM MESSENGER SCREEN (Direct Mailbox + Paginated #public Stream)
// ============================================================================
void KryonCloudUI::drawBeamScreen() {
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    bool isLandscape = (screenH < 260);

    tftInstance->fillRect(0, 57, screenW, screenH - 57, CLOUD_BG);

    if (beamViewingIndex >= 0) {
        drawBeamDetailModal();
        return;
    }

    if (beamComposing) {
        drawBeamComposeModal();
        return;
    }

    // Check toast expiration
    if (beamStatusToast.length() > 0 && millis() - beamStatusToastTime > 3500) {
        beamStatusToast = "";
    }

    // Row 1: Scope Switcher (Mailbox vs #public Stream)
    int pillW = (screenW - 24) / 2;
    tftInstance->fillRoundRect(8, 60, pillW, 24, 4, (!beamPublicScope) ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
    if (beamPublicScope) tftInstance->drawRoundRect(8, 60, pillW, 24, 4, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, (!beamPublicScope) ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Direct Mailbox", 8 + pillW / 2, 72, 2);

    tftInstance->fillRoundRect(8 + pillW + 8, 60, pillW, 24, 4, beamPublicScope ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
    if (!beamPublicScope) tftInstance->drawRoundRect(8 + pillW + 8, 60, pillW, 24, 4, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, beamPublicScope ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
    tftInstance->drawString("#public Stream", 8 + pillW + 8 + pillW / 2, 72, 2);

    // Row 2: Action Buttons
    int bW = (screenW - 32) / 3;
    tftInstance->fillRoundRect(8, 88, bW, 22, 4, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
    tftInstance->drawString("Compose", 8 + bW / 2, 99, 2);

    tftInstance->fillRoundRect(8 + bW + 8, 88, bW, 22, 4, CLOUD_PILL_INACT);
    tftInstance->drawRoundRect(8 + bW + 8, 88, bW, 22, 4, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_PILL_INACT);
    tftInstance->drawString("Refresh", 8 + bW + 8 + bW / 2, 99, 2);

    int b3X = 8 + 2 * (bW + 8);
    if (!beamPublicScope) {
        tftInstance->fillRoundRect(b3X, 88, bW, 22, 4, 0x6000);
        tftInstance->setTextColor(TFT_WHITE, 0x6000);
        tftInstance->drawString("Clear All", b3X + bW / 2, 99, 2);
    } else {
        tftInstance->fillRoundRect(b3X, 88, bW, 22, 4, 0x03E0);
        tftInstance->setTextColor(TFT_WHITE, 0x03E0);
        tftInstance->drawString("Broadcast", b3X + bW / 2, 99, 2);
    }

    // Status Toast Banner (e.g. "Message Sent Successfully!")
    int listY = 114;
    if (beamStatusToast.length() > 0) {
        bool isErr = beamStatusToast.indexOf("Failed") >= 0;
        uint16_t toastBg = isErr ? TFT_RED : 0x03E0;
        tftInstance->fillRoundRect(8, 112, screenW - 16, 20, 4, toastBg);
        tftInstance->setTextColor(TFT_WHITE, toastBg);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(beamStatusToast.c_str(), screenW / 2, 122, 2);
        listY = 136;
    }

    int cardW = screenW - 16;
    int cardH = isLandscape ? 34 : 40;
    int cardGap = isLandscape ? 38 : 44;
    int maxMsgs = isLandscape ? 2 : 3;

    // Messages Area
    if (!beamPublicScope) {
        // Direct Mailbox
        if (!beamInboxLoaded) {
            tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Checking Mailbox...", screenW / 2, 160, 2);
            beamInboxLoaded = KryonCloudManager::pollBeamInbox(cachedBeamMessages, 2);
            tftInstance->fillRect(0, listY, screenW, screenH - listY, CLOUD_BG);
        }

        if (cachedBeamMessages.empty()) {
            tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("No Direct Messages", screenW / 2, 165, 2);
            tftInstance->drawString("Tap 'Compose' to send @handle", screenW / 2, 188, 2);
        } else {
            int y = listY;
            for (size_t i = 0; i < (size_t)maxMsgs && i < cachedBeamMessages.size(); i++) {
                tftInstance->fillRoundRect(8, y, cardW, cardH, 4, CLOUD_CARD_BG);
                tftInstance->drawRoundRect(8, y, cardW, cardH, 4, CLOUD_CARD_BORDER);

                tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
                tftInstance->setTextDatum(TL_DATUM);
                tftInstance->drawString(cachedBeamMessages[i].senderHandle.c_str(), 14, y + 4, 2);

                uint16_t badgeColor = TFT_WHITE;
                if (cachedBeamMessages[i].msgType == "ALERT") badgeColor = TFT_RED;
                else if (cachedBeamMessages[i].msgType == "TELEMETRY") badgeColor = TFT_GREEN;
                else if (cachedBeamMessages[i].msgType == "COMMAND") badgeColor = TFT_ORANGE;

                tftInstance->setTextColor(badgeColor, CLOUD_CARD_BG);
                tftInstance->setTextDatum(TR_DATUM);
                tftInstance->drawString(("[" + cachedBeamMessages[i].msgType + "]").c_str(), cardW, y + 4, 2);

                tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
                tftInstance->setTextDatum(TL_DATUM);
                String snippet = cachedBeamMessages[i].content;
                int maxSnip = isLandscape ? 36 : 25;
                if ((int)snippet.length() > maxSnip) snippet = snippet.substring(0, maxSnip - 3) + "...";
                tftInstance->drawString(snippet.c_str(), 14, y + (isLandscape ? 17 : 21), 2);

                y += cardGap;
            }
        }
    } else {
        // #public Stream (Paginated in safe 3-message chunks)
        if (!beamPublicLoaded) {
            tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Loading #public channel...", screenW / 2, 160, 2);
            beamPublicLoaded = KryonCloudManager::pollPublicBeamMessages(cachedPublicMessages, "public", 3, beamPublicPage);
            tftInstance->fillRect(0, listY, screenW, screenH - listY, CLOUD_BG);
        }

        if (cachedPublicMessages.empty()) {
            tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("No Messages on this page", screenW / 2, 165, 2);
            tftInstance->drawString("Tap 'Broadcast' to post", screenW / 2, 188, 2);
        } else {
            int y = listY;
            for (size_t i = 0; i < (size_t)maxMsgs && i < cachedPublicMessages.size(); i++) {
                tftInstance->fillRoundRect(8, y, cardW, cardH, 4, CLOUD_CARD_BG);
                tftInstance->drawRoundRect(8, y, cardW, cardH, 4, CLOUD_CARD_BORDER);

                tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
                tftInstance->setTextDatum(TL_DATUM);
                tftInstance->drawString(cachedPublicMessages[i].senderHandle.c_str(), 14, y + 4, 2);

                tftInstance->setTextColor(CLOUD_BAR_STR, CLOUD_CARD_BG);
                tftInstance->setTextDatum(TR_DATUM);
                tftInstance->drawString("#public", cardW, y + 4, 2);

                tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
                tftInstance->setTextDatum(TL_DATUM);
                String snippet = cachedPublicMessages[i].content;
                int maxSnip = isLandscape ? 36 : 25;
                if ((int)snippet.length() > maxSnip) snippet = snippet.substring(0, maxSnip - 3) + "...";
                tftInstance->drawString(snippet.c_str(), 14, y + (isLandscape ? 17 : 21), 2);

                y += cardGap;
            }
        }

        // Pagination Bar
        bool hasPrev = (beamPublicPage > 1);
        bool hasNext = (cachedPublicMessages.size() == 3);
        int pagY = isLandscape ? 190 : 252;
        int pagH = 22;

        tftInstance->fillRoundRect(8, pagY, 64, pagH, 3, hasPrev ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
        if (!hasPrev) tftInstance->drawRoundRect(8, pagY, 64, pagH, 3, CLOUD_CARD_BORDER);
        tftInstance->setTextColor(hasPrev ? TFT_WHITE : CLOUD_TEXT_MUTED, hasPrev ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("< Prev", 40, pagY + pagH / 2, 2);

        tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
        tftInstance->drawString(("Page " + String(beamPublicPage)).c_str(), screenW / 2, pagY + pagH / 2, 2);

        tftInstance->fillRoundRect(screenW - 72, pagY, 64, pagH, 3, hasNext ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
        if (!hasNext) tftInstance->drawRoundRect(screenW - 72, pagY, 64, pagH, 3, CLOUD_CARD_BORDER);
        tftInstance->setTextColor(hasNext ? TFT_WHITE : CLOUD_TEXT_MUTED, hasNext ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
        tftInstance->drawString("Next >", screenW - 40, pagY + pagH / 2, 2);
    }

    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(("Node: " + KryonCloudManager::getBeamHandle() + (beamPublicScope ? " | #public" : " | Mailbox")).c_str(), screenW / 2, screenH - 12, 1);
}

void KryonCloudUI::drawBeamDetailModal() {
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    const auto& msgList = beamPublicScope ? cachedPublicMessages : cachedBeamMessages;
    if (beamViewingIndex < 0 || (size_t)beamViewingIndex >= msgList.size()) return;
    const auto& msg = msgList[beamViewingIndex];

    int modalW = screenW - 20;
    int modalH = screenH - 66;
    int modalX = 10;
    int modalY = 60;

    tftInstance->fillRoundRect(modalX, modalY, modalW, modalH, 6, CLOUD_CARD_BG);
    tftInstance->drawRoundRect(modalX, modalY, modalW, modalH, 6, CLOUD_ACCENT_BLUE);

    tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString(("From: " + msg.senderHandle).c_str(), modalX + 8, modalY + 6, 2);

    tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
    tftInstance->drawString(beamPublicScope ? "Channel: #public" : ("Type: [" + msg.msgType + "]").c_str(), modalX + 8, modalY + 24, 2);

    // Message Body (Safely bounded)
    int bodyY = modalY + 44;
    int bodyH = modalH - 78;
    tftInstance->fillRoundRect(modalX + 6, bodyY, modalW - 12, bodyH, 4, CLOUD_BG);
    tftInstance->drawRoundRect(modalX + 6, bodyY, modalW - 12, bodyH, 4, CLOUD_CARD_BORDER);
    drawWrappedText(msg.content, modalX + 10, bodyY + 4, modalW - 20, bodyH - 8, TFT_WHITE, 2);

    // Close Button
    int btnY = modalY + modalH - 30;
    tftInstance->fillRoundRect(modalX + 6, btnY, modalW - 12, 26, 4, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("CLOSE MESSAGE", screenW / 2, btnY + 13, 2);
}

void KryonCloudUI::drawBeamComposeModal() {
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    int modalW = screenW - 20;
    int modalH = screenH - 66;
    int modalX = 10;
    int modalY = 60;

    tftInstance->fillRoundRect(modalX, modalY, modalW, modalH, 6, CLOUD_CARD_BG);
    tftInstance->drawRoundRect(modalX, modalY, modalW, modalH, 6, CLOUD_ACCENT_BLUE);

    tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
    tftInstance->setTextDatum(TC_DATUM);
    tftInstance->drawString(beamPublicScope ? "Public Broadcast" : "Direct Message", screenW / 2, modalY + 6, 2);

    // Recipient Box
    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString(beamPublicScope ? "Channel:" : "Recipient (@handle):", modalX + 8, modalY + 24, 1);
    
    int targetBoxY = modalY + 38;
    tftInstance->fillRoundRect(modalX + 6, targetBoxY, modalW - 12, 24, 3, CLOUD_BG);
    tftInstance->drawRoundRect(modalX + 6, targetBoxY, modalW - 12, 24, 3, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
    if (beamPublicScope) {
        tftInstance->drawString("#public (Global Channel)", modalX + 12, targetBoxY + 4, 2);
    } else {
        String dispTarget = (beamTargetHandle.length() > 0) ? beamTargetHandle : "Tap to enter @handle";
        tftInstance->drawString(dispTarget.c_str(), modalX + 12, targetBoxY + 4, 2);
    }

    // Content Box
    int bodyLabelY = targetBoxY + 28;
    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
    tftInstance->drawString("Message Body (Tap to type):", modalX + 8, bodyLabelY, 1);

    int bodyY = bodyLabelY + 14;
    int bodyH = modalH - (bodyY - modalY) - 34;
    tftInstance->fillRoundRect(modalX + 6, bodyY, modalW - 12, bodyH, 3, CLOUD_BG);
    tftInstance->drawRoundRect(modalX + 6, bodyY, modalW - 12, bodyH, 3, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_BG);
    String dispBody = (beamContent.length() > 0) ? beamContent : "Tap to type message...";
    drawWrappedText(dispBody, modalX + 10, bodyY + 4, modalW - 20, bodyH - 8, TFT_WHITE, 2);

    // Send & Cancel
    int btnY = modalY + modalH - 28;
    int btnW = (modalW - 18) / 2;
    tftInstance->fillRoundRect(modalX + 6, btnY, btnW, 24, 4, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("SEND", modalX + 6 + btnW / 2, btnY + 12, 2);

    tftInstance->fillRoundRect(modalX + 6 + btnW + 6, btnY, btnW, 24, 4, 0x6000);
    tftInstance->setTextColor(TFT_WHITE, 0x6000);
    tftInstance->drawString("CANCEL", modalX + 6 + btnW + 6 + btnW / 2, btnY + 12, 2);
}

void KryonCloudUI::handleBeamTouch(uint16_t x, uint16_t y) {
    if (y < 56) {
        handleTopNavTouch(x, y);
        return;
    }

    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    bool isLandscape = (screenH < 260);

    int modalW = screenW - 20;
    int modalH = screenH - 66;
    int modalX = 10;
    int modalY = 60;

    if (beamViewingIndex >= 0) {
        int btnY = modalY + modalH - 32;
        if (y >= btnY - 4 && y <= btnY + 30) {
            beamViewingIndex = -1;
            drawBeamScreen();
        }
        return;
    }

    if (beamComposing) {
        int targetBoxY = modalY + 38;
        // Target handle tap (Direct messaging only)
        if (y >= targetBoxY && y <= targetBoxY + 26 && !beamPublicScope) {
            String res = MyKeyboard::getString(beamTargetHandle, "Recipient (@handle):", 32);
            drawTopNav();
            tftInstance->fillRect(0, 57, screenW, screenH - 57, CLOUD_BG);
            if (res.length() > 0) beamTargetHandle = res;
            drawBeamComposeModal();
            return;
        }

        // Body tap
        int bodyLabelY = targetBoxY + 28;
        int bodyY = bodyLabelY + 14;
        int bodyH = modalH - (bodyY - modalY) - 34;
        if (y >= bodyY && y <= bodyY + bodyH) {
            String res = MyKeyboard::getString(beamContent, "Message Body:", 120);
            drawTopNav();
            tftInstance->fillRect(0, 57, screenW, screenH - 57, CLOUD_BG);
            if (res.length() > 0) beamContent = res;
            drawBeamComposeModal();
            return;
        }

        // Action Buttons (Send / Cancel)
        int btnY = modalY + modalH - 30;
        int btnW = (modalW - 18) / 2;
        if (y >= btnY && y <= btnY + 28) {
            if (x >= modalX + 6 && x <= modalX + 6 + btnW) {
                // Send Message
                bool ok = false;
                if (beamPublicScope || beamTargetHandle == "#public" || beamTargetHandle.equalsIgnoreCase("public")) {
                    ok = KryonCloudManager::broadcastPublicBeam(beamContent, "public");
                    beamStatusToast = ok ? "Broadcast Posted to #public!" : "Broadcast Failed!";
                } else {
                    String target = (beamTargetHandle.length() > 0) ? beamTargetHandle : "@node";
                    ok = KryonCloudManager::sendBeamMessage(target, beamContent, "TEXT");
                    beamStatusToast = ok ? "Message Sent to " + target + "!" : "Message Send Failed!";
                }
                beamStatusToastTime = millis();
                beamComposing = false;
                beamInboxLoaded = false;
                beamPublicLoaded = false;
                drawBeamScreen();
            } else if (x >= modalX + 6 + btnW + 6 && x <= modalX + 6 + 2 * btnW + 6) {
                beamComposing = false;
                drawBeamScreen();
            }
        }
        return;
    }

    // Row 1: Scope Switcher touches (y: 58 - 84)
    int pillW = (screenW - 24) / 2;
    if (y >= 58 && y <= 84) {
        if (x >= 8 && x <= 8 + pillW) {
            if (beamPublicScope) {
                beamPublicScope = false;
                beamInboxLoaded = false;
                drawBeamScreen();
            }
        } else if (x >= 8 + pillW + 8 && x <= 8 + 2 * pillW + 8) {
            if (!beamPublicScope) {
                beamPublicScope = true;
                beamPublicLoaded = false;
                beamPublicPage = 1;
                drawBeamScreen();
            }
        }
        return;
    }

    // Row 2: Action Buttons (y: 86 - 110)
    int bW = (screenW - 32) / 3;
    if (y >= 86 && y <= 110) {
        if (x >= 8 && x <= 8 + bW) {
            // Compose
            beamComposing = true;
            if (beamPublicScope) {
                beamTargetHandle = "#public";
            } else if (beamTargetHandle == "#public" || beamTargetHandle == "*") {
                beamTargetHandle = "";
            }
            drawBeamComposeModal();
        } else if (x >= 8 + bW + 8 && x <= 8 + 2 * bW + 8) {
            // Refresh
            if (beamPublicScope) beamPublicLoaded = false;
            else beamInboxLoaded = false;
            drawBeamScreen();
        } else if (x >= 8 + 2 * (bW + 8) && x <= 8 + 3 * bW + 16) {
            if (!beamPublicScope) {
                // Clear All / Ack
                std::vector<String> ackIds;
                for (const auto& m : cachedBeamMessages) ackIds.push_back(m.id);
                if (!ackIds.empty()) {
                    KryonCloudManager::acknowledgeBeamMessages(ackIds);
                    cachedBeamMessages.clear();
                }
                drawBeamScreen();
            } else {
                // Broadcast directly to #public
                beamTargetHandle = "#public";
                beamComposing = true;
                drawBeamComposeModal();
            }
        }
        return;
    }

    // Pagination Row touches for #public Stream
    int pagY = isLandscape ? 190 : 252;
    if (beamPublicScope && y >= pagY - 4 && y <= pagY + 26) {
        if (x >= 8 && x <= 74) {
            if (beamPublicPage > 1) {
                beamPublicPage--;
                beamPublicLoaded = false;
                drawBeamScreen();
            }
            return;
        } else if (x >= screenW - 74 && x <= screenW - 8) {
            if (cachedPublicMessages.size() == 3) {
                beamPublicPage++;
                beamPublicLoaded = false;
                drawBeamScreen();
            }
            return;
        }
    }

    // Tap on a message card
    int startY = (beamStatusToast.length() > 0) ? 136 : 114;
    int cardGap = isLandscape ? 38 : 44;
    int maxMsgs = isLandscape ? 2 : 3;
    if (y >= startY && y <= startY + maxMsgs * cardGap) {
        int idx = (y - startY) / cardGap;
        const auto& list = beamPublicScope ? cachedPublicMessages : cachedBeamMessages;
        if (idx >= 0 && (size_t)idx < list.size() && idx < maxMsgs) {
            beamViewingIndex = idx;
            drawBeamDetailModal();
        }
    }
}

// ============================================================================
// 5. CLOUD STORAGE EXPLORER SCREEN (With Device Config Backup & Restore)
// ============================================================================
void KryonCloudUI::drawStorageScreen() {
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    bool isLandscape = (screenH < 260);

    tftInstance->fillRect(0, 57, screenW, screenH - 57, CLOUD_BG);

    // Check toast expiration
    if (storageStatusToast.length() > 0 && millis() - storageStatusToastTime > 3500) {
        storageStatusToast = "";
    }

    // Row 1: Scope Switcher (Shared vs Device)
    int pillW = (screenW - 24) / 2;
    tftInstance->fillRoundRect(8, 60, pillW, 22, 4, storageScopeDevice ? CLOUD_PILL_INACT : CLOUD_ACCENT_BLUE);
    if (storageScopeDevice) tftInstance->drawRoundRect(8, 60, pillW, 22, 4, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, storageScopeDevice ? CLOUD_PILL_INACT : CLOUD_ACCENT_BLUE);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Shared (/cloud)", 8 + pillW / 2, 71, 2);

    tftInstance->fillRoundRect(8 + pillW + 8, 60, pillW, 22, 4, storageScopeDevice ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
    if (!storageScopeDevice) tftInstance->drawRoundRect(8 + pillW + 8, 60, pillW, 22, 4, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, storageScopeDevice ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
    tftInstance->drawString("Device (/device)", 8 + pillW + 8 + pillW / 2, 71, 2);

    // Row 2: Backup & Restore Action Buttons
    tftInstance->fillRoundRect(8, 86, pillW, 22, 4, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
    tftInstance->drawString("Create Backup", 8 + pillW / 2, 97, 2);

    tftInstance->fillRoundRect(8 + pillW + 8, 86, pillW, 22, 4, 0x03E0);
    tftInstance->setTextColor(TFT_WHITE, 0x03E0);
    tftInstance->drawString("Restore Backup", 8 + pillW + 8 + pillW / 2, 97, 2);

    // Status Toast Banner
    int listY = 112;
    if (storageStatusToast.length() > 0) {
        bool isErr = (storageStatusToast.indexOf("Failed") >= 0 || storageStatusToast.indexOf("No ") >= 0);
        uint16_t toastBg = isErr ? TFT_RED : 0x03E0;
        tftInstance->fillRoundRect(8, 110, screenW - 16, 20, 4, toastBg);
        tftInstance->setTextColor(TFT_WHITE, toastBg);
        tftInstance->drawString(storageStatusToast.c_str(), screenW / 2, 120, 2);
        listY = 134;
    }

    if (!manifestLoaded) {
        tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Fetching Manifest...", screenW / 2, 160, 2);
        manifestLoaded = KryonCloudManager::fetchStorageManifest(cachedSharedFiles, cachedDeviceFiles);
        tftInstance->fillRect(0, listY, screenW, screenH - listY, CLOUD_BG);
    }

    const auto& list = storageScopeDevice ? cachedDeviceFiles : cachedSharedFiles;
    int cardW = screenW - 16;
    int cardH = isLandscape ? 26 : 34;
    int cardGap = isLandscape ? 30 : 38;
    int maxFiles = isLandscape ? 3 : 4;

    if (list.empty()) {
        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("No Files in this scope", screenW / 2, 165, 2);
        tftInstance->drawString(storageScopeDevice ? "Tap 'Create Backup' to sync" : "Upload via Web Server", screenW / 2, 188, 2);
    } else {
        int y = listY;
        for (size_t i = 0; i < (size_t)maxFiles && i < list.size(); i++) {
            tftInstance->fillRoundRect(8, y, cardW, cardH, 4, CLOUD_CARD_BG);
            tftInstance->drawRoundRect(8, y, cardW, cardH, 4, CLOUD_CARD_BORDER);

            tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
            tftInstance->setTextDatum(TL_DATUM);
            String fn = list[i].filename;
            int maxFn = isLandscape ? 30 : 20;
            if ((int)fn.length() > maxFn) fn = fn.substring(0, maxFn - 3) + "...";
            tftInstance->drawString(fn.c_str(), 14, y + (isLandscape ? 5 : 8), 2);

            tftInstance->setTextColor(TFT_GREEN, CLOUD_CARD_BG);
            tftInstance->setTextDatum(TR_DATUM);
            tftInstance->drawString((String(list[i].fileSize / 1024) + " KB").c_str(), cardW, y + (isLandscape ? 5 : 8), 2);

            y += cardGap;
        }
    }

    // Storage status footer
    const CloudLimits& lim = KryonCloudManager::getLimits();
    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(("Storage: " + String(lim.storageUsedBytes / 1024) + " KB / " + String(lim.storageQuotaMb) + " MB").c_str(), screenW / 2, screenH - 12, 1);
}

void KryonCloudUI::handleStorageTouch(uint16_t x, uint16_t y) {
    if (y < 56) {
        handleTopNavTouch(x, y);
        return;
    }

    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    bool isLandscape = (screenH < 260);
    int pillW = (screenW - 24) / 2;

    // Scope Switcher (y: 58 - 82)
    if (y >= 58 && y <= 82) {
        if (x <= 8 + pillW && storageScopeDevice) {
            storageScopeDevice = false;
            drawStorageScreen();
        } else if (x > 8 + pillW && !storageScopeDevice) {
            storageScopeDevice = true;
            drawStorageScreen();
        }
        return;
    }

    // Action Buttons (Create Backup & Restore Backup, y: 84 - 108)
    if (y >= 84 && y <= 108) {
        int modalW = min(260, screenW - 20);
        int modalX = (screenW - modalW) / 2;
        if (x >= 8 && x <= 8 + pillW) {
            // Create Backup
            tftInstance->fillRoundRect(modalX, 130, modalW, 65, 5, CLOUD_CARD_BG);
            tftInstance->drawRoundRect(modalX, 130, modalW, 65, 5, CLOUD_ACCENT_BLUE);
            tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Backing Up Configs...", screenW / 2, 162, 2);

            bool ok = KryonCloudManager::createDeviceBackup();
            storageStatusToast = ok ? "Backup Saved to Cloud!" : "Backup Failed / No Files";
            storageStatusToastTime = millis();
            manifestLoaded = false;
            drawStorageScreen();
            return;
        } else if (x >= 8 + pillW + 8 && x <= 8 + 2 * pillW + 8) {
            // Restore Backup
            tftInstance->fillRoundRect(modalX, 130, modalW, 65, 5, CLOUD_CARD_BG);
            tftInstance->drawRoundRect(modalX, 130, modalW, 65, 5, CLOUD_ACCENT_BLUE);
            tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Restoring Backup...", screenW / 2, 162, 2);

            bool ok = KryonCloudManager::restoreDeviceBackup();
            storageStatusToast = ok ? "Backup Restored to Disk!" : "No Backup Found / Error";
            storageStatusToastTime = millis();
            drawStorageScreen();
            return;
        }
    }

    // File item click -> Download
    int startY = (storageStatusToast.length() > 0) ? 134 : 112;
    int cardGap = isLandscape ? 30 : 38;
    int cardH = isLandscape ? 26 : 34;
    int maxFiles = isLandscape ? 3 : 4;
    if (y >= startY && y <= startY + maxFiles * cardGap) {
        int idx = (y - startY) / cardGap;
        const auto& list = storageScopeDevice ? cachedDeviceFiles : cachedSharedFiles;
        if (idx >= 0 && idx < (int)list.size() && idx < maxFiles) {
            tftInstance->fillRect(8, startY + idx * cardGap, screenW - 16, cardH, CLOUD_ACCENT_BLUE);
            tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Downloading...", screenW / 2, startY + idx * cardGap + cardH / 2, 2);

            String localDest = "/local/cloud_" + list[idx].filename;
            KryonCloudManager::downloadCloudFile(list[idx].path, localDest, list[idx].scope);
            drawStorageScreen();
        }
    }
}

// ============================================================================
// 6. USAGE & QUOTAS SCREEN
// ============================================================================
void KryonCloudUI::drawLimitsScreen() {
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    bool isLandscape = (screenH < 260);

    tftInstance->fillRect(0, 57, screenW, screenH - 57, CLOUD_BG);

    const CloudLimits& lim = KryonCloudManager::getLimits();

    if (isLandscape) {
        // High density dual-column layout for landscape
        int colW = (screenW - 24) / 2;
        int c1X = 8;
        int c2X = c1X + colW + 8;
        int cardH = 110;

        // Card 1: AI Quota
        tftInstance->fillRoundRect(c1X, 62, colW, cardH, 5, CLOUD_CARD_BG);
        tftInstance->drawRoundRect(c1X, 62, colW, cardH, 5, CLOUD_CARD_BORDER);

        tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString("Daily AI Limit", c1X + 8, 68, 2);

        tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TR_DATUM);
        tftInstance->drawString((String(lim.dailyAiUsed) + "/" + String(lim.dailyAiLimit)).c_str(), c1X + colW - 8, 68, 2);

        int barTrackW = colW - 16;
        int aiPct = (lim.dailyAiLimit > 0) ? (lim.dailyAiUsed * barTrackW) / lim.dailyAiLimit : 0;
        if (aiPct > barTrackW) aiPct = barTrackW;
        uint16_t aiBarColor = (aiPct > (barTrackW * 85) / 100) ? TFT_RED : ((aiPct > (barTrackW * 70) / 100) ? TFT_ORANGE : 0x07E0);

        tftInstance->drawRoundRect(c1X + 8, 92, barTrackW, 8, 3, CLOUD_CARD_BORDER);
        tftInstance->fillRect(c1X + 9, 93, barTrackW - 2, 6, CLOUD_TRACK_BG);
        if (aiPct > 0) {
            tftInstance->fillRoundRect(c1X + 9, 93, aiPct - 2, 6, 2, aiBarColor);
        }

        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString(("Left: " + String(lim.dailyAiRemaining)).c_str(), c1X + 8, 108, 1);

        // Card 2: Storage Quota
        tftInstance->fillRoundRect(c2X, 62, colW, cardH, 5, CLOUD_CARD_BG);
        tftInstance->drawRoundRect(c2X, 62, colW, cardH, 5, CLOUD_CARD_BORDER);

        tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
        tftInstance->drawString("Cloud Storage", c2X + 8, 68, 2);

        tftInstance->setTextColor(TFT_GREEN, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TR_DATUM);
        tftInstance->drawString((String(lim.storageQuotaMb) + " MB").c_str(), c2X + colW - 8, 68, 2);

        int strPct = (lim.storageQuotaBytes > 0) ? (lim.storageUsedBytes * barTrackW) / lim.storageQuotaBytes : 0;
        if (strPct > barTrackW) strPct = barTrackW;
        uint16_t strBarColor = (strPct > (barTrackW * 85) / 100) ? TFT_RED : ((strPct > (barTrackW * 70) / 100) ? TFT_ORANGE : 0x07E0);

        tftInstance->drawRoundRect(c2X + 8, 92, barTrackW, 8, 3, CLOUD_CARD_BORDER);
        tftInstance->fillRect(c2X + 9, 93, barTrackW - 2, 6, CLOUD_TRACK_BG);
        if (strPct > 0) {
            tftInstance->fillRoundRect(c2X + 9, 93, strPct - 2, 6, 2, strBarColor);
        }

        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString(("Free: " + String(lim.storageRemainingBytes / 1024) + " KB").c_str(), c2X + 8, 108, 1);

        // Refresh Button
        int btnY = 180;
        tftInstance->fillRoundRect(8, btnY, screenW - 16, 32, 5, CLOUD_ACCENT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("REFRESH QUOTAS", screenW / 2, btnY + 16, 2);

        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
        tftInstance->drawString("KryonCloud Account Gateway", screenW / 2, screenH - 12, 1);

    } else {
        int cardW = screenW - 16;
        tftInstance->fillRoundRect(8, 62, cardW, 68, 5, CLOUD_CARD_BG);
        tftInstance->drawRoundRect(8, 62, cardW, 68, 5, CLOUD_CARD_BORDER);

        tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString("Daily AI Limit", 16, 68, 2);

        tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
        tftInstance->drawString((String(lim.dailyAiUsed) + " / " + String(lim.dailyAiLimit) + " used").c_str(), cardW - 90, 68, 2);

        int barTrackW = cardW - 16;
        int aiPct = (lim.dailyAiLimit > 0) ? (lim.dailyAiUsed * barTrackW) / lim.dailyAiLimit : 0;
        if (aiPct > barTrackW) aiPct = barTrackW;
        uint16_t aiBarColor = (aiPct > (barTrackW * 85) / 100) ? TFT_RED : ((aiPct > (barTrackW * 70) / 100) ? TFT_ORANGE : 0x07E0);

        tftInstance->drawRoundRect(16, 92, barTrackW, 10, 3, CLOUD_CARD_BORDER);
        tftInstance->fillRect(17, 93, barTrackW - 2, 8, CLOUD_TRACK_BG);
        if (aiPct > 0) {
            tftInstance->fillRoundRect(17, 93, aiPct - 2, 8, 2, aiBarColor);
        }

        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
        tftInstance->drawString(("Remaining Today: " + String(lim.dailyAiRemaining) + " requests").c_str(), 16, 110, 1);

        // Storage Quota Card
        tftInstance->fillRoundRect(8, 136, cardW, 68, 5, CLOUD_CARD_BG);
        tftInstance->drawRoundRect(8, 136, cardW, 68, 5, CLOUD_CARD_BORDER);

        tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
        tftInstance->drawString("Cloud Storage", 16, 142, 2);

        tftInstance->setTextColor(TFT_GREEN, CLOUD_CARD_BG);
        tftInstance->drawString((String(lim.storageUsedBytes / 1024) + " KB / " + String(lim.storageQuotaMb) + " MB").c_str(), cardW - 110, 142, 2);

        int strPct = (lim.storageQuotaBytes > 0) ? (lim.storageUsedBytes * barTrackW) / lim.storageQuotaBytes : 0;
        if (strPct > barTrackW) strPct = barTrackW;
        uint16_t strBarColor = (strPct > (barTrackW * 85) / 100) ? TFT_RED : ((strPct > (barTrackW * 70) / 100) ? TFT_ORANGE : 0x07E0);

        tftInstance->drawRoundRect(16, 166, barTrackW, 10, 3, CLOUD_CARD_BORDER);
        tftInstance->fillRect(17, 167, barTrackW - 2, 8, CLOUD_TRACK_BG);
        if (strPct > 0) {
            tftInstance->fillRoundRect(17, 167, strPct - 2, 8, 2, strBarColor);
        }

        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
        tftInstance->drawString(("Free Space: " + String(lim.storageRemainingBytes / 1024) + " KB remaining").c_str(), 16, 184, 1);

        // Reset Schedule & Health Card
        tftInstance->fillRoundRect(8, 210, cardW, 44, 5, CLOUD_CARD_BG);
        tftInstance->drawRoundRect(8, 210, cardW, 44, 5, CLOUD_CARD_BORDER);

        tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
        tftInstance->drawString("Quota Reset:", 16, 216, 1);
        tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
        tftInstance->drawString("Daily at 00:00 UTC", 90, 216, 1);

        tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
        tftInstance->drawString("Account Tier:", 16, 234, 1);
        tftInstance->setTextColor(TFT_GREEN, CLOUD_CARD_BG);
        tftInstance->drawString("Verified Hardware Node", 90, 234, 1);

        // Refresh Quota Button
        tftInstance->fillRoundRect(8, 260, cardW, 32, 5, CLOUD_ACCENT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("REFRESH QUOTAS", screenW / 2, 276, 2);

        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
        tftInstance->drawString("KryonCloud Account Gateway", screenW / 2, 304, 1);
    }
}

void KryonCloudUI::handleLimitsTouch(uint16_t x, uint16_t y) {
    if (y < 56) {
        handleTopNavTouch(x, y);
        return;
    }

    int screenH = tftInstance->height();
    bool isLandscape = (screenH < 260);
    int btnY = isLandscape ? 180 : 260;

    if (y >= btnY && y <= btnY + 34) {
        KryonCloudManager::fetchAccountLimits();
        drawLimitsScreen();
    }
}

// ============================================================================
// 7. BAN / APPEAL STATUS SCREEN
// ============================================================================
void KryonCloudUI::drawBanStatusScreen() {
    int screenW = tftInstance->width();
    int screenH = tftInstance->height();
    bool isLandscape = (screenH < 260);

    tftInstance->fillScreen(CLOUD_BG);

    // Frame & Header
    tftInstance->drawRoundRect(3, 3, screenW - 6, screenH - 6, 5, CLOUD_CARD_BORDER);
    tftInstance->fillRoundRect(6, 6, screenW - 12, 30, 5, CLOUD_NAV_BG);
    tftInstance->drawRoundRect(6, 6, screenW - 12, 30, 5, cachedBanStatus.isBanned ? TFT_RED : CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(cachedBanStatus.isBanned ? TFT_RED : TFT_WHITE, CLOUD_NAV_BG);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(cachedBanStatus.isBanned ? "Account / Device Ban Notice" : "Security & Ban Status", screenW / 2, 21, 2);

    if (cachedBanStatus.isBanned) {
        bool isDeviceBan = cachedBanStatus.deviceBanned && !cachedBanStatus.accountBanned;
        int cardH = isLandscape ? (screenH - 85) : 226;

        // Card container
        tftInstance->fillRoundRect(10, 42, screenW - 20, cardH, 6, CLOUD_CARD_BG);
        tftInstance->drawRoundRect(10, 42, screenW - 20, cardH, 6, TFT_RED);

        tftInstance->setTextColor(TFT_RED, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TC_DATUM);
        tftInstance->drawString(isDeviceBan ? "DEVICE SUSPENDED" : "ACCOUNT SUSPENDED", screenW / 2, 48, 2);

        // Ban Reason
        tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString("Reason:", 16, 70, 1);
        String reasonStr = (cachedBanStatus.banReason.length() > 0) ? cachedBanStatus.banReason : "Terms of Service Violation";
        drawWrappedText(reasonStr, 16, 82, screenW - 32, 28, TFT_WHITE, 1);

        // Instruction Box
        int instBoxY = isLandscape ? 112 : 122;
        int instBoxH = isLandscape ? (cardH - (instBoxY - 42) - 8) : 140;
        tftInstance->fillRoundRect(14, instBoxY, screenW - 28, instBoxH, 4, CLOUD_BG);
        tftInstance->drawRoundRect(14, instBoxY, screenW - 28, instBoxH, 4, CLOUD_CARD_BORDER);

        tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString("To submit an appeal:", 18, instBoxY + 6, 1);

        tftInstance->setTextColor(TFT_WHITE, CLOUD_BG);
        tftInstance->drawString("Login to Kryon Account: kryonos.harislab.tech", 18, instBoxY + 20, 1);

        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
        tftInstance->drawString(("Appeal: " + (cachedBanStatus.appealStatus.length() > 0 ? cachedBanStatus.appealStatus : "NONE")).c_str(), 18, instBoxY + 36, 1);

        // Exit button
        int exitBtnY = screenH - 38;
        tftInstance->fillRoundRect(10, exitBtnY, screenW - 20, 32, 5, CLOUD_ACCENT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("EXIT TO LAUNCHER", screenW / 2, exitBtnY + 16, 2);
    } else {
        tftInstance->setTextColor(TFT_GREEN, CLOUD_BG);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("STATUS: ACTIVE / GOOD", screenW / 2, 90, 2);
        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
        tftInstance->drawString("No suspensions or bans.", screenW / 2, 115, 1);

        int backBtnW = min(220, screenW - 40);
        int backBtnX = (screenW - backBtnW) / 2;
        int backBtnY = screenH - 50;
        tftInstance->fillRoundRect(backBtnX, backBtnY, backBtnW, 36, 5, CLOUD_ACCENT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("< BACK TO OVERVIEW", screenW / 2, backBtnY + 18, 2);
    }
}

void KryonCloudUI::handleBanStatusTouch(uint16_t x, uint16_t y) {
    int screenH = tftInstance->height();
    if (cachedBanStatus.isBanned) {
        if (y >= screenH - 44 && y <= screenH - 4) {
            currentState = 0; // STATE_LAUNCHER
        }
    } else {
        if (y >= screenH - 56 && y <= screenH - 10) {
            currentSubState = CLOUD_STATE_OVERVIEW;
            draw();
        }
    }
}

// ============================================================================
// TEXT WRAPPING, SCROLLING & SCREEN BOUNDS PROTECTION HELPERS
// ============================================================================
int KryonCloudUI::getTextLineCount(const String& text, int maxW, uint8_t font) {
    if (text.length() == 0) return 0;
    int charWidth = (font == 1) ? 6 : 8;
    int maxCharsPerLine = maxW / charWidth;
    int count = 0;
    int currentLineLen = 0;
    String word = "";

    for (size_t i = 0; i <= text.length(); i++) {
        char c = (i < text.length()) ? text[i] : ' ';

        if (c == '\n' || c == ' ' || i == text.length()) {
            if (currentLineLen + word.length() > maxCharsPerLine) {
                count++;
                currentLineLen = 0;
            }
            if (currentLineLen > 0) currentLineLen++;
            currentLineLen += word.length();
            word = "";

            if (c == '\n') {
                count++;
                currentLineLen = 0;
            }
        } else {
            word += c;
        }
    }
    if (currentLineLen > 0) count++;
    return count;
}

void KryonCloudUI::drawWrappedText(const String& text, int x, int y, int maxW, int maxH, uint16_t color, uint8_t font, int scrollLine) {
    if (!tftInstance || text.length() == 0) return;

    tftInstance->setTextColor(color);
    tftInstance->setTextDatum(TL_DATUM);

    int lineHeight = (font == 1) ? 10 : 16;
    int charWidth = (font == 1) ? 6 : 8;
    int maxCharsPerLine = maxW / charWidth;
    int maxVisibleLines = maxH / lineHeight;

    std::vector<String> lines;
    String currentLine = "";
    String word = "";

    for (size_t i = 0; i <= text.length(); i++) {
        char c = (i < text.length()) ? text[i] : ' ';

        if (c == '\n' || c == ' ' || i == text.length()) {
            if (currentLine.length() + word.length() > maxCharsPerLine) {
                lines.push_back(currentLine);
                currentLine = "";
            }
            if (currentLine.length() > 0) currentLine += " ";
            currentLine += word;
            word = "";

            if (c == '\n') {
                lines.push_back(currentLine);
                currentLine = "";
            }
        } else {
            word += c;
        }
    }
    if (currentLine.length() > 0) {
        lines.push_back(currentLine);
    }

    if (lines.empty()) return;

    int totalLines = lines.size();
    if (scrollLine < 0) scrollLine = 0;
    if (scrollLine > totalLines - maxVisibleLines && totalLines > maxVisibleLines) {
        scrollLine = totalLines - maxVisibleLines;
    } else if (totalLines <= maxVisibleLines) {
        scrollLine = 0;
    }

    int curY = y;
    for (int i = scrollLine; i < totalLines && (i - scrollLine) < maxVisibleLines; i++) {
        tftInstance->drawString(lines[i], x, curY, font);
        curY += lineHeight;
    }
}

// ============================================================================
// MAIN LOOP & TOUCH DISPATCHER
// ============================================================================
void KryonCloudUI::loop() {
    if (currentSubState == CLOUD_STATE_PAIRING && isWaitingForClaim && activePairingCode.length() > 0) {
        if (millis() - lastWaitPollTime > 3000) {
            lastWaitPollTime = millis();
            bool claimSuccess = KryonCloudManager::pollPairingWait(activePairingCode, 5000);
            if (claimSuccess) {
                isWaitingForClaim = false;
                activePairingCode = "";
                currentSubState = CLOUD_STATE_OVERVIEW;
                draw();
            }
        }
    }
}

void KryonCloudUI::handleTouch(uint16_t x, uint16_t y) {
    if (cachedBanStatus.isBanned) {
        handleBanStatusTouch(x, y);
        return;
    }

    switch (currentSubState) {
        case CLOUD_STATE_PAIRING:
            handlePairingTouch(x, y);
            break;
        case CLOUD_STATE_OVERVIEW:
            handleOverviewTouch(x, y);
            break;
        case CLOUD_STATE_KRYON_AI:
            handleKryonAITouch(x, y);
            break;
        case CLOUD_STATE_KRYON_BEAM:
            handleBeamTouch(x, y);
            break;
        case CLOUD_STATE_STORAGE:
            handleStorageTouch(x, y);
            break;
        case CLOUD_STATE_LIMITS:
            handleLimitsTouch(x, y);
            break;
        case CLOUD_STATE_BAN_STATUS:
            handleBanStatusTouch(x, y);
            break;
    }
}
