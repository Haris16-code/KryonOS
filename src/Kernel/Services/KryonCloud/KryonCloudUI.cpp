#include "KryonCloudUI.h"
#include "../../../File System/FileSystem.h"
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

    if (progressPct <= 20) {
        tftInstance->fillScreen(CLOUD_BG);

        // Title
        tftInstance->setTextColor(TFT_WHITE, CLOUD_BG);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("KryonCloud", 120, 85, 4);

        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
        tftInstance->drawString("Connecting to Platform...", 120, 115, 2);
    }

    // Status Message Area
    tftInstance->fillRect(10, 150, 220, 24, CLOUD_BG);
    tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(status.c_str(), 120, 162, 2);

    // Progress Bar Track
    tftInstance->drawRoundRect(20, 184, 200, 16, 4, CLOUD_CARD_BORDER);
    tftInstance->fillRect(22, 186, 196, 12, CLOUD_TRACK_BG);

    int barW = (196 * progressPct) / 100;
    if (barW < 0) barW = 0;
    if (barW > 196) barW = 196;

    if (barW > 0) {
        tftInstance->fillRoundRect(22, 186, barW, 12, 3, CLOUD_ACCENT_BLUE);
    }

    // Percentage
    tftInstance->fillRect(90, 208, 60, 20, CLOUD_BG);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_BG);
    tftInstance->drawString((String(progressPct) + "%").c_str(), 120, 218, 2);
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
    // Header Bar
    tftInstance->fillRect(0, 0, 240, 56, CLOUD_NAV_BG);
    tftInstance->drawFastHLine(0, 56, 240, CLOUD_CARD_BORDER);

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
    tftInstance->fillRoundRect(208, 4, 26, 20, 4, 0x8800);
    tftInstance->setTextColor(TFT_WHITE, 0x8800);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("X", 221, 14, 2);

    // 5 Tab Buttons (Width = 46px each, gap = 2px)
    const char* tabNames[] = { "Home", "AI", "Beam", "Drive", "Limits" };
    CloudUISubState tabStates[] = {
        CLOUD_STATE_OVERVIEW,
        CLOUD_STATE_KRYON_AI,
        CLOUD_STATE_KRYON_BEAM,
        CLOUD_STATE_STORAGE,
        CLOUD_STATE_LIMITS
    };

    for (int i = 0; i < 5; i++) {
        int tx = 3 + i * 47;
        bool active = (currentSubState == tabStates[i]);

        if (active) {
            tftInstance->fillRoundRect(tx, 30, 45, 22, 4, CLOUD_ACCENT_BLUE);
            tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
        } else {
            tftInstance->fillRoundRect(tx, 30, 45, 22, 4, CLOUD_PILL_INACT);
            tftInstance->drawRoundRect(tx, 30, 45, 22, 4, 0x31A7);
            tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_PILL_INACT);
        }
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(tabNames[i], tx + 22, 41, 2);
    }
}

void KryonCloudUI::handleTopNavTouch(uint16_t x, uint16_t y) {
    // Close button
    if (x >= 200 && y <= 26) {
        currentState = 0; // STATE_LAUNCHER
        return;
    }

    // Tab switching
    if (y >= 28 && y <= 54) {
        int tabIdx = (x - 3) / 47;
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

    tftInstance->fillRect(10, 80, 220, 160, CLOUD_BG);
    tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Contacting Cloud Hub...", 120, 150, 2);

    bool ok = KryonCloudManager::initPairingSession(activePairingCode, activePairingId, 
                                                    activeChallenge, pairingExpiresIn);
    if (ok) {
        isWaitingForClaim = true;
        pairingStartTime = millis();
        drawPairingScreen();
    } else {
        tftInstance->fillRect(10, 80, 220, 160, CLOUD_BG);
        tftInstance->setTextColor(TFT_RED, CLOUD_BG);
        tftInstance->drawString("Pairing Init Failed", 120, 140, 2);
        tftInstance->drawString("Check WiFi Connection", 120, 170, 2);
    }
}

void KryonCloudUI::drawPairingScreen() {
    tftInstance->fillScreen(CLOUD_BG);

    // Header Frame
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, CLOUD_CARD_BORDER);
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, CLOUD_NAV_BG);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_NAV_BG);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("KryonCloud Link", 120, 21, 2);

    if (activePairingCode.length() == 0) {
        startPairingInit();
        return;
    }

    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
    tftInstance->setTextDatum(TC_DATUM);
    tftInstance->drawString("Open your browser & visit:", 120, 48, 2);
    
    tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
    tftInstance->drawString("kryonos.harislab.tech", 120, 68, 2);

    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
    tftInstance->drawString("Enter this Pairing Code:", 120, 95, 2);

    // Large Code Box
    tftInstance->fillRoundRect(15, 120, 210, 55, 8, CLOUD_CARD_BG);
    tftInstance->drawRoundRect(15, 120, 210, 55, 8, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(activePairingCode.c_str(), 120, 148, 4);

    tftInstance->setTextColor(TFT_GREEN, CLOUD_BG);
    tftInstance->drawString("Waiting for user claim...", 120, 195, 2);

    // Action Buttons
    tftInstance->fillRoundRect(15, 240, 100, 35, 5, CLOUD_PILL_INACT);
    tftInstance->drawRoundRect(15, 240, 100, 35, 5, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_PILL_INACT);
    tftInstance->drawString("NEW CODE", 65, 257, 2);

    tftInstance->fillRoundRect(125, 240, 100, 35, 5, 0x9000);
    tftInstance->setTextColor(TFT_WHITE, 0x9000);
    tftInstance->drawString("CANCEL", 175, 257, 2);
}

void KryonCloudUI::handlePairingTouch(uint16_t x, uint16_t y) {
    if (y >= 235 && y <= 280) {
        if (x >= 15 && x <= 115) {
            activePairingCode = "";
            startPairingInit();
        } else if (x >= 125 && x <= 225) {
            currentState = 0; // STATE_LAUNCHER
        }
    }
}

// ============================================================================
// 2. OVERVIEW SCREEN (HOME)
// ============================================================================
void KryonCloudUI::drawOverviewScreen() {
    tftInstance->fillRect(0, 57, 240, 263, CLOUD_BG);

    // Account Profile Card
    tftInstance->fillRoundRect(8, 62, 224, 72, 6, CLOUD_CARD_BG);
    tftInstance->drawRoundRect(8, 62, 224, 72, 6, CLOUD_CARD_BORDER);
    
    tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString(KryonCloudManager::getUserName().c_str(), 16, 68, 2);
    
    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
    tftInstance->drawString(KryonCloudManager::getAccountEmail().c_str(), 16, 88, 2);

    tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
    tftInstance->drawString(("Handle: " + KryonCloudManager::getBeamHandle()).c_str(), 16, 108, 2);

    // Live Quotas Quick Summary Card
    const CloudLimits& lim = KryonCloudManager::getLimits();
    tftInstance->fillRoundRect(8, 140, 224, 96, 6, CLOUD_CARD_BG);
    tftInstance->drawRoundRect(8, 140, 224, 96, 6, CLOUD_CARD_BORDER);

    tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString("Daily AI Quota:", 16, 146, 2);
    tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
    tftInstance->drawString((String(lim.dailyAiRemaining) + " left").c_str(), 155, 146, 2);

    // AI Progress Bar (Green when plenty available, Orange/Red only when depleted)
    tftInstance->drawRoundRect(16, 166, 208, 8, 3, CLOUD_CARD_BORDER);
    tftInstance->fillRect(17, 167, 206, 6, CLOUD_TRACK_BG);
    int aiW = (lim.dailyAiLimit > 0) ? (lim.dailyAiUsed * 206) / lim.dailyAiLimit : 0;
    if (aiW > 206) aiW = 206;
    if (aiW > 0) {
        uint16_t aiBarColor = (aiW > 185) ? TFT_RED : ((aiW > 154) ? TFT_ORANGE : 0x07E0);
        tftInstance->fillRoundRect(17, 167, aiW, 6, 2, aiBarColor);
    }

    // Cloud Storage Summary
    tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
    tftInstance->drawString("Cloud Storage:", 16, 182, 2);
    tftInstance->setTextColor(TFT_GREEN, CLOUD_CARD_BG);
    tftInstance->drawString((String(lim.storageRemainingBytes / 1024) + " KB free").c_str(), 135, 182, 2);

    // Storage Progress Bar
    tftInstance->drawRoundRect(16, 202, 208, 8, 3, CLOUD_CARD_BORDER);
    tftInstance->fillRect(17, 203, 206, 6, CLOUD_TRACK_BG);
    int strW = (lim.storageQuotaBytes > 0) ? (lim.storageUsedBytes * 206) / lim.storageQuotaBytes : 0;
    if (strW > 206) strW = 206;
    if (strW > 0) {
        uint16_t strBarColor = (strW > 185) ? TFT_RED : ((strW > 154) ? TFT_ORANGE : 0x07E0);
        tftInstance->fillRoundRect(17, 203, strW, 6, 2, strBarColor);
    }

    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
    tftInstance->drawString("Node Status: Online & Verified", 16, 218, 1);

    // Action Buttons
    tftInstance->fillRoundRect(8, 244, 108, 34, 5, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Sync Fresh", 62, 261, 2);

    tftInstance->fillRoundRect(124, 244, 108, 34, 5, 0x9000);
    tftInstance->setTextColor(TFT_WHITE, 0x9000);
    tftInstance->drawString("Unpair Board", 178, 261, 2);
}

void KryonCloudUI::handleOverviewTouch(uint16_t x, uint16_t y) {
    if (y < 56) {
        handleTopNavTouch(x, y);
        return;
    }

    if (y >= 244 && y <= 278) {
        if (x >= 8 && x <= 116) {
            // Sync Fresh Data
            tftInstance->fillRect(8, 244, 108, 34, 0x0215);
            tftInstance->setTextColor(TFT_YELLOW, 0x0215);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Syncing...", 62, 261, 2);
            KryonCloudManager::syncAllFreshData();
            drawOverviewScreen();
        } else if (x >= 124 && x <= 232) {
            // Unpair Device
            KryonCloudManager::unpair();
            resetToHome();
            draw();
        }
    }
}

// ============================================================================
// ============================================================================
// 3. KRYONAI SCREEN (Interactive Studio with Keyboard & Smooth Scrolling)
// ============================================================================
void KryonCloudUI::drawKryonAIScreen() {
    tftInstance->fillRect(0, 57, 240, 263, CLOUD_BG);

    // Prompt Card (Tap to edit)
    tftInstance->fillRoundRect(8, 62, 224, 40, 5, CLOUD_CARD_BG);
    tftInstance->drawRoundRect(8, 62, 224, 40, 5, CLOUD_ACCENT_BLUE);
    
    tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString("Prompt (Tap to type):", 14, 66, 1);
    
    tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
    String dispPrompt = aiConsolePrompt;
    if (dispPrompt.length() > 28) dispPrompt = dispPrompt.substring(0, 25) + "...";
    tftInstance->drawString(dispPrompt.c_str(), 14, 80, 2);

    // Action Buttons: Single "Ask AI" + "Clear" + "UP" + "DN"
    tftInstance->fillRoundRect(8, 106, 84, 26, 4, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Ask AI", 50, 119, 2);

    tftInstance->fillRoundRect(96, 106, 48, 26, 4, 0x6000);
    tftInstance->setTextColor(TFT_WHITE, 0x6000);
    tftInstance->drawString("Clear", 120, 119, 2);

    tftInstance->fillRoundRect(148, 106, 40, 26, 4, CLOUD_PILL_INACT);
    tftInstance->drawRoundRect(148, 106, 40, 26, 4, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_PILL_INACT);
    tftInstance->drawString("UP", 168, 119, 2);

    tftInstance->fillRoundRect(192, 106, 40, 26, 4, CLOUD_PILL_INACT);
    tftInstance->drawRoundRect(192, 106, 40, 26, 4, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_PILL_INACT);
    tftInstance->drawString("DN", 212, 119, 2);

    // Response Window (Strictly bounded)
    tftInstance->fillRoundRect(8, 136, 224, 146, 5, CLOUD_CARD_BG);
    tftInstance->drawRoundRect(8, 136, 224, 146, 5, CLOUD_CARD_BORDER);

    tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString("AI Response:", 14, 140, 1);

    // Render bounded text with scrolling
    drawWrappedText(aiConsoleResponse, 14, 154, 212, 120, TFT_WHITE, 2, aiScrollOffset);

    // Footer info
    const CloudLimits& lim = KryonCloudManager::getLimits();
    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(("Daily AI Remaining: " + String(lim.dailyAiRemaining) + " / " + String(lim.dailyAiLimit)).c_str(), 120, 298, 1);
}

void KryonCloudUI::handleKryonAITouch(uint16_t x, uint16_t y) {
    if (y < 56) {
        handleTopNavTouch(x, y);
        return;
    }

    // Tap prompt box -> Open Keyboard
    if (y >= 62 && y <= 102) {
        String input = MyKeyboard::getString(aiConsolePrompt, "KryonAI Prompt:", 160);
        if (input.length() > 0) {
            aiConsolePrompt = input;
        }
        draw();
        return;
    }

    // Action Buttons (y: 106 - 132)
    if (y >= 106 && y <= 132) {
        if (x >= 8 && x <= 92) {
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
            tftInstance->fillRect(10, 152, 220, 126, CLOUD_CARD_BG);
            tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
            tftInstance->setTextDatum(TL_DATUM);
            tftInstance->drawString("Connecting & Streaming...", 14, 154, 2);

            aiStreamingActive = true;
            KryonCloudAI::stream(
                aiConsolePrompt,
                [](const String& token) {
                    if (tftInstance) {
                        aiConsoleResponse += token;
                        int totalL = getTextLineCount(aiConsoleResponse, 212, 2);
                        aiScrollOffset = (totalL > 7) ? (totalL - 7) : 0;
                        tftInstance->fillRect(10, 152, 220, 126, CLOUD_CARD_BG);
                        drawWrappedText(aiConsoleResponse, 14, 154, 212, 120, TFT_WHITE, 2, aiScrollOffset);
                    }
                },
                [](const String& fullText, const AiUsageStats& usage) {
                    aiConsoleResponse = fullText;
                    aiStreamingActive = false;
                    int totalL = getTextLineCount(aiConsoleResponse, 212, 2);
                    aiScrollOffset = (totalL > 7) ? (totalL - 7) : 0;
                    if (tftInstance) {
                        tftInstance->fillRect(10, 152, 220, 126, CLOUD_CARD_BG);
                        drawWrappedText(aiConsoleResponse, 14, 154, 212, 120, TFT_WHITE, 2, aiScrollOffset);
                    }
                },
                [](const String& err) {
                    aiConsoleResponse = "Error: " + err;
                    aiStreamingActive = false;
                    if (tftInstance) {
                        tftInstance->fillRect(10, 152, 220, 126, CLOUD_CARD_BG);
                        drawWrappedText(aiConsoleResponse, 14, 154, 212, 120, TFT_WHITE, 2, 0);
                    }
                }
            );
            drawKryonAIScreen();
            return;
        } else if (x >= 96 && x <= 144) {
            // Clear
            aiConsoleResponse = "Ready. Tap 'Ask AI' or tap the prompt to edit.";
            aiScrollOffset = 0;
            drawKryonAIScreen();
            return;
        } else if (x >= 148 && x <= 188) {
            // UP button
            if (aiScrollOffset > 0) {
                aiScrollOffset = max(0, aiScrollOffset - 2);
                tftInstance->fillRect(10, 152, 220, 126, CLOUD_CARD_BG);
                drawWrappedText(aiConsoleResponse, 14, 154, 212, 120, TFT_WHITE, 2, aiScrollOffset);
            }
            return;
        } else if (x >= 192 && x <= 236) {
            // DN button
            int totalL = getTextLineCount(aiConsoleResponse, 212, 2);
            if (aiScrollOffset < totalL - 7) {
                aiScrollOffset = min(totalL - 7, aiScrollOffset + 2);
                tftInstance->fillRect(10, 152, 220, 126, CLOUD_CARD_BG);
                drawWrappedText(aiConsoleResponse, 14, 154, 212, 120, TFT_WHITE, 2, aiScrollOffset);
            }
            return;
        }
    }

    // Tap in Response Window area to scroll smoothly
    if (y >= 136 && y <= 282) {
        int totalL = getTextLineCount(aiConsoleResponse, 212, 2);
        if (y < 210) {
            // Tap top half -> Scroll UP
            if (aiScrollOffset > 0) {
                aiScrollOffset = max(0, aiScrollOffset - 2);
                tftInstance->fillRect(10, 152, 220, 126, CLOUD_CARD_BG);
                drawWrappedText(aiConsoleResponse, 14, 154, 212, 120, TFT_WHITE, 2, aiScrollOffset);
            }
        } else {
            // Tap bottom half -> Scroll DOWN
            if (aiScrollOffset < totalL - 7) {
                aiScrollOffset = min(totalL - 7, aiScrollOffset + 2);
                tftInstance->fillRect(10, 152, 220, 126, CLOUD_CARD_BG);
                drawWrappedText(aiConsoleResponse, 14, 154, 212, 120, TFT_WHITE, 2, aiScrollOffset);
            }
        }
    }
}

// ============================================================================
// 4. KRYONBEAM MESSENGER SCREEN (Direct Mailbox + Paginated #public Stream)
// ============================================================================
void KryonCloudUI::drawBeamScreen() {
    tftInstance->fillRect(0, 57, 240, 263, CLOUD_BG);

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
    tftInstance->fillRoundRect(8, 60, 110, 24, 4, (!beamPublicScope) ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
    if (beamPublicScope) tftInstance->drawRoundRect(8, 60, 110, 24, 4, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, (!beamPublicScope) ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Direct Mailbox", 63, 72, 2);

    tftInstance->fillRoundRect(122, 60, 110, 24, 4, beamPublicScope ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
    if (!beamPublicScope) tftInstance->drawRoundRect(122, 60, 110, 24, 4, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, beamPublicScope ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
    tftInstance->drawString("#public Stream", 177, 72, 2);

    // Row 2: Action Buttons
    tftInstance->fillRoundRect(8, 88, 72, 22, 4, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
    tftInstance->drawString("Compose", 44, 99, 2);

    tftInstance->fillRoundRect(84, 88, 72, 22, 4, CLOUD_PILL_INACT);
    tftInstance->drawRoundRect(84, 88, 72, 22, 4, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_PILL_INACT);
    tftInstance->drawString("Refresh", 120, 99, 2);

    if (!beamPublicScope) {
        tftInstance->fillRoundRect(160, 88, 72, 22, 4, 0x6000);
        tftInstance->setTextColor(TFT_WHITE, 0x6000);
        tftInstance->drawString("Clear All", 196, 99, 2);
    } else {
        tftInstance->fillRoundRect(160, 88, 72, 22, 4, 0x03E0);
        tftInstance->setTextColor(TFT_WHITE, 0x03E0);
        tftInstance->drawString("Broadcast", 196, 99, 2);
    }

    // Status Toast Banner (e.g. "Message Sent Successfully!")
    int listY = 114;
    if (beamStatusToast.length() > 0) {
        bool isErr = beamStatusToast.indexOf("Failed") >= 0;
        uint16_t toastBg = isErr ? TFT_RED : 0x03E0;
        tftInstance->fillRoundRect(8, 112, 224, 20, 4, toastBg);
        tftInstance->setTextColor(TFT_WHITE, toastBg);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString(beamStatusToast.c_str(), 120, 122, 2);
        listY = 136;
    }

    // Messages Area
    if (!beamPublicScope) {
        // Direct Mailbox
        if (!beamInboxLoaded) {
            tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Checking Mailbox...", 120, 180, 2);
            beamInboxLoaded = KryonCloudManager::pollBeamInbox(cachedBeamMessages, 2);
            tftInstance->fillRect(0, listY, 240, 260 - listY, CLOUD_BG);
        }

        if (cachedBeamMessages.empty()) {
            tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("No Direct Messages", 120, 175, 2);
            tftInstance->drawString("Tap 'Compose' to send @handle", 120, 198, 2);
        } else {
            int y = listY;
            for (size_t i = 0; i < 3 && i < cachedBeamMessages.size(); i++) {
                tftInstance->fillRoundRect(8, y, 224, 40, 4, CLOUD_CARD_BG);
                tftInstance->drawRoundRect(8, y, 224, 40, 4, CLOUD_CARD_BORDER);

                tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
                tftInstance->setTextDatum(TL_DATUM);
                tftInstance->drawString(cachedBeamMessages[i].senderHandle.c_str(), 14, y + 4, 2);

                uint16_t badgeColor = TFT_WHITE;
                if (cachedBeamMessages[i].msgType == "ALERT") badgeColor = TFT_RED;
                else if (cachedBeamMessages[i].msgType == "TELEMETRY") badgeColor = TFT_GREEN;
                else if (cachedBeamMessages[i].msgType == "COMMAND") badgeColor = TFT_ORANGE;

                tftInstance->setTextColor(badgeColor, CLOUD_CARD_BG);
                tftInstance->drawString(("[" + cachedBeamMessages[i].msgType + "]").c_str(), 150, y + 4, 2);

                tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
                String snippet = cachedBeamMessages[i].content;
                if (snippet.length() > 25) snippet = snippet.substring(0, 22) + "...";
                tftInstance->drawString(snippet.c_str(), 14, y + 21, 2);

                y += 44;
            }
        }
    } else {
        // #public Stream (Paginated in safe 3-message chunks)
        if (!beamPublicLoaded) {
            tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Loading #public channel...", 120, 180, 2);
            beamPublicLoaded = KryonCloudManager::pollPublicBeamMessages(cachedPublicMessages, "public", 3, beamPublicPage);
            tftInstance->fillRect(0, listY, 240, 260 - listY, CLOUD_BG);
        }

        if (cachedPublicMessages.empty()) {
            tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("No Messages on this page", 120, 175, 2);
            tftInstance->drawString("Tap 'Broadcast' to post", 120, 198, 2);
        } else {
            int y = listY;
            for (size_t i = 0; i < 3 && i < cachedPublicMessages.size(); i++) {
                tftInstance->fillRoundRect(8, y, 224, 40, 4, CLOUD_CARD_BG);
                tftInstance->drawRoundRect(8, y, 224, 40, 4, CLOUD_CARD_BORDER);

                tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
                tftInstance->setTextDatum(TL_DATUM);
                tftInstance->drawString(cachedPublicMessages[i].senderHandle.c_str(), 14, y + 4, 2);

                tftInstance->setTextColor(CLOUD_BAR_STR, CLOUD_CARD_BG);
                tftInstance->drawString("#public", 160, y + 4, 2);

                tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
                String snippet = cachedPublicMessages[i].content;
                if (snippet.length() > 25) snippet = snippet.substring(0, 22) + "...";
                tftInstance->drawString(snippet.c_str(), 14, y + 21, 2);

                y += 44;
            }
        }

        // Pagination Bar (y: 252 - 274)
        bool hasPrev = (beamPublicPage > 1);
        bool hasNext = (cachedPublicMessages.size() == 3);

        tftInstance->fillRoundRect(8, 252, 64, 22, 3, hasPrev ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
        if (!hasPrev) tftInstance->drawRoundRect(8, 252, 64, 22, 3, CLOUD_CARD_BORDER);
        tftInstance->setTextColor(hasPrev ? TFT_WHITE : CLOUD_TEXT_MUTED, hasPrev ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("< Prev", 40, 263, 2);

        tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
        tftInstance->drawString(("Page " + String(beamPublicPage)).c_str(), 120, 263, 2);

        tftInstance->fillRoundRect(168, 252, 64, 22, 3, hasNext ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
        if (!hasNext) tftInstance->drawRoundRect(168, 252, 64, 22, 3, CLOUD_CARD_BORDER);
        tftInstance->setTextColor(hasNext ? TFT_WHITE : CLOUD_TEXT_MUTED, hasNext ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
        tftInstance->drawString("Next >", 200, 263, 2);
    }

    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(("Node: " + KryonCloudManager::getBeamHandle() + (beamPublicScope ? " | #public" : " | Mailbox")).c_str(), 120, 304, 1);
}

void KryonCloudUI::drawBeamDetailModal() {
    const auto& msgList = beamPublicScope ? cachedPublicMessages : cachedBeamMessages;
    if (beamViewingIndex < 0 || (size_t)beamViewingIndex >= msgList.size()) return;
    const auto& msg = msgList[beamViewingIndex];

    tftInstance->fillRoundRect(10, 60, 220, 235, 6, CLOUD_CARD_BG);
    tftInstance->drawRoundRect(10, 60, 220, 235, 6, CLOUD_ACCENT_BLUE);

    tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString(("From: " + msg.senderHandle).c_str(), 18, 68, 2);

    tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
    tftInstance->drawString(beamPublicScope ? "Channel: #public" : ("Type: [" + msg.msgType + "]").c_str(), 18, 88, 2);

    // Message Body (Safely bounded)
    tftInstance->fillRoundRect(16, 110, 208, 130, 4, CLOUD_BG);
    tftInstance->drawRoundRect(16, 110, 208, 130, 4, CLOUD_CARD_BORDER);
    drawWrappedText(msg.content, 22, 116, 196, 118, TFT_WHITE, 2);

    // Close Button
    tftInstance->fillRoundRect(16, 250, 208, 34, 4, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("CLOSE MESSAGE", 120, 267, 2);
}

void KryonCloudUI::drawBeamComposeModal() {
    tftInstance->fillRoundRect(10, 60, 220, 240, 6, CLOUD_CARD_BG);
    tftInstance->drawRoundRect(10, 60, 220, 240, 6, CLOUD_ACCENT_BLUE);

    tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
    tftInstance->setTextDatum(TC_DATUM);
    tftInstance->drawString(beamPublicScope ? "Public Broadcast" : "Direct Message", 120, 68, 2);

    // Recipient Box
    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString(beamPublicScope ? "Channel:" : "Recipient (@handle):", 16, 90, 1);
    
    tftInstance->fillRoundRect(16, 104, 208, 26, 3, CLOUD_BG);
    tftInstance->drawRoundRect(16, 104, 208, 26, 3, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
    if (beamPublicScope) {
        tftInstance->drawString("#public (Global Channel)", 22, 110, 2);
    } else {
        String dispTarget = (beamTargetHandle.length() > 0) ? beamTargetHandle : "Tap to enter @handle";
        tftInstance->drawString(dispTarget.c_str(), 22, 110, 2);
    }

    // Content Box (Taller and cleaner without type selector)
    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
    tftInstance->drawString("Message Body (Tap to type):", 16, 138, 1);

    tftInstance->fillRoundRect(16, 152, 208, 66, 3, CLOUD_BG);
    tftInstance->drawRoundRect(16, 152, 208, 66, 3, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_BG);
    String dispBody = (beamContent.length() > 0) ? beamContent : "Tap to type message...";
    drawWrappedText(dispBody, 22, 158, 196, 54, TFT_WHITE, 2);

    // Send & Cancel
    tftInstance->fillRoundRect(16, 230, 98, 32, 4, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("SEND", 65, 246, 2);

    tftInstance->fillRoundRect(126, 230, 98, 32, 4, 0x6000);
    tftInstance->setTextColor(TFT_WHITE, 0x6000);
    tftInstance->drawString("CANCEL", 175, 246, 2);
}

void KryonCloudUI::handleBeamTouch(uint16_t x, uint16_t y) {
    if (y < 56) {
        handleTopNavTouch(x, y);
        return;
    }

    if (beamViewingIndex >= 0) {
        if (y >= 246 && y <= 286) {
            beamViewingIndex = -1;
            drawBeamScreen();
        }
        return;
    }

    if (beamComposing) {
        // Target handle tap (Direct messaging only)
        if (y >= 100 && y <= 134 && !beamPublicScope) {
            String res = MyKeyboard::getString(beamTargetHandle, "Recipient (@handle):", 32);
            drawTopNav();
            tftInstance->fillRect(0, 57, 240, 263, CLOUD_BG);
            if (res.length() > 0) beamTargetHandle = res;
            drawBeamComposeModal();
            return;
        }

        // Body tap
        if (y >= 150 && y <= 222) {
            String res = MyKeyboard::getString(beamContent, "Message Body:", 120);
            drawTopNav();
            tftInstance->fillRect(0, 57, 240, 263, CLOUD_BG);
            if (res.length() > 0) beamContent = res;
            drawBeamComposeModal();
            return;
        }

        // Action Buttons (Send / Cancel)
        if (y >= 226 && y <= 268) {
            if (x >= 16 && x <= 114) {
                // Send Message - always TEXT
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
            } else if (x >= 126 && x <= 224) {
                beamComposing = false;
                drawBeamScreen();
            }
        }
        return;
    }

    // Row 1: Scope Switcher touches (y: 58 - 84)
    if (y >= 58 && y <= 84) {
        if (x >= 8 && x <= 118) {
            if (beamPublicScope) {
                beamPublicScope = false;
                beamInboxLoaded = false;
                drawBeamScreen();
            }
        } else if (x >= 122 && x <= 232) {
            if (!beamPublicScope) {
                beamPublicScope = true;
                beamPublicLoaded = false;
                beamPublicPage = 1;
                drawBeamScreen();
            }
        }
        return;
    }

    // Row 2: Action Bar Buttons (y: 86 - 114)
    if (y >= 86 && y <= 114) {
        if (x >= 8 && x <= 80) {
            // Compose
            beamComposing = true;
            if (beamPublicScope) {
                beamTargetHandle = "#public";
            } else if (beamTargetHandle == "#public" || beamTargetHandle == "*") {
                beamTargetHandle = "";
            }
            drawBeamComposeModal();
        } else if (x >= 84 && x <= 156) {
            // Refresh
            if (beamPublicScope) beamPublicLoaded = false;
            else beamInboxLoaded = false;
            drawBeamScreen();
        } else if (x >= 160 && x <= 232) {
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

    // Pagination Row touches for #public Stream (y: 248 - 278)
    if (beamPublicScope && y >= 248 && y <= 278) {
        if (x >= 8 && x <= 74) {
            if (beamPublicPage > 1) {
                beamPublicPage--;
                beamPublicLoaded = false;
                drawBeamScreen();
            }
            return;
        } else if (x >= 166 && x <= 232) {
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
    if (y >= startY && y <= startY + 130) {
        int idx = (y - startY) / 44;
        const auto& list = beamPublicScope ? cachedPublicMessages : cachedBeamMessages;
        if (idx >= 0 && (size_t)idx < list.size() && idx < 3) {
            beamViewingIndex = idx;
            drawBeamDetailModal();
        }
    }
}

// ============================================================================
// 5. CLOUD STORAGE EXPLORER SCREEN (With Device Config Backup & Restore)
// ============================================================================
void KryonCloudUI::drawStorageScreen() {
    tftInstance->fillRect(0, 57, 240, 263, CLOUD_BG);

    // Check toast expiration
    if (storageStatusToast.length() > 0 && millis() - storageStatusToastTime > 3500) {
        storageStatusToast = "";
    }

    // Row 1: Scope Switcher (Shared vs Device)
    tftInstance->fillRoundRect(8, 60, 110, 22, 4, storageScopeDevice ? CLOUD_PILL_INACT : CLOUD_ACCENT_BLUE);
    if (storageScopeDevice) tftInstance->drawRoundRect(8, 60, 110, 22, 4, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, storageScopeDevice ? CLOUD_PILL_INACT : CLOUD_ACCENT_BLUE);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("Shared (/cloud)", 63, 71, 2);

    tftInstance->fillRoundRect(122, 60, 110, 22, 4, storageScopeDevice ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
    if (!storageScopeDevice) tftInstance->drawRoundRect(122, 60, 110, 22, 4, CLOUD_CARD_BORDER);
    tftInstance->setTextColor(TFT_WHITE, storageScopeDevice ? CLOUD_ACCENT_BLUE : CLOUD_PILL_INACT);
    tftInstance->drawString("Device (/device)", 177, 71, 2);

    // Row 2: Backup & Restore Action Buttons
    tftInstance->fillRoundRect(8, 86, 110, 22, 4, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
    tftInstance->drawString("Create Backup", 63, 97, 2);

    tftInstance->fillRoundRect(122, 86, 110, 22, 4, 0x03E0);
    tftInstance->setTextColor(TFT_WHITE, 0x03E0);
    tftInstance->drawString("Restore Backup", 177, 97, 2);

    // Status Toast Banner
    int listY = 112;
    if (storageStatusToast.length() > 0) {
        bool isErr = (storageStatusToast.indexOf("Failed") >= 0 || storageStatusToast.indexOf("No ") >= 0);
        uint16_t toastBg = isErr ? TFT_RED : 0x03E0;
        tftInstance->fillRoundRect(8, 110, 224, 20, 4, toastBg);
        tftInstance->setTextColor(TFT_WHITE, toastBg);
        tftInstance->drawString(storageStatusToast.c_str(), 120, 120, 2);
        listY = 134;
    }

    if (!manifestLoaded) {
        tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("Fetching Manifest...", 120, 180, 2);
        manifestLoaded = KryonCloudManager::fetchStorageManifest(cachedSharedFiles, cachedDeviceFiles);
        tftInstance->fillRect(0, listY, 240, 260 - listY, CLOUD_BG);
    }

    const auto& list = storageScopeDevice ? cachedDeviceFiles : cachedSharedFiles;

    if (list.empty()) {
        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("No Files in this scope", 120, 170, 2);
        tftInstance->drawString(storageScopeDevice ? "Tap 'Create Backup' to sync" : "Upload via Web Server", 120, 195, 2);
    } else {
        int y = listY;
        for (size_t i = 0; i < 4 && i < list.size(); i++) {
            tftInstance->fillRoundRect(8, y, 224, 34, 4, CLOUD_CARD_BG);
            tftInstance->drawRoundRect(8, y, 224, 34, 4, CLOUD_CARD_BORDER);

            tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
            tftInstance->setTextDatum(TL_DATUM);
            String fn = list[i].filename;
            if (fn.length() > 20) fn = fn.substring(0, 17) + "...";
            tftInstance->drawString(fn.c_str(), 14, y + 8, 2);

            tftInstance->setTextColor(TFT_GREEN, CLOUD_CARD_BG);
            tftInstance->drawString((String(list[i].fileSize / 1024) + " KB").c_str(), 170, y + 8, 2);

            y += 38;
        }
    }

    // Storage status footer
    const CloudLimits& lim = KryonCloudManager::getLimits();
    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(("Storage: " + String(lim.storageUsedBytes / 1024) + " KB / " + String(lim.storageQuotaMb) + " MB").c_str(), 120, 302, 1);
}

void KryonCloudUI::handleStorageTouch(uint16_t x, uint16_t y) {
    if (y < 56) {
        handleTopNavTouch(x, y);
        return;
    }

    // Scope Switcher (y: 58 - 82)
    if (y >= 58 && y <= 82) {
        if (x <= 118 && storageScopeDevice) {
            storageScopeDevice = false;
            drawStorageScreen();
        } else if (x > 118 && !storageScopeDevice) {
            storageScopeDevice = true;
            drawStorageScreen();
        }
        return;
    }

    // Action Buttons (Create Backup & Restore Backup, y: 84 - 108)
    if (y >= 84 && y <= 108) {
        if (x >= 8 && x <= 118) {
            // Create Backup
            tftInstance->fillRoundRect(16, 130, 208, 65, 5, CLOUD_CARD_BG);
            tftInstance->drawRoundRect(16, 130, 208, 65, 5, CLOUD_ACCENT_BLUE);
            tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Backing Up Configs...", 120, 162, 2);

            bool ok = KryonCloudManager::createDeviceBackup();
            storageStatusToast = ok ? "Backup Saved to Cloud!" : "Backup Failed / No Files";
            storageStatusToastTime = millis();
            manifestLoaded = false;
            drawStorageScreen();
            return;
        } else if (x >= 122 && x <= 232) {
            // Restore Backup
            tftInstance->fillRoundRect(16, 130, 208, 65, 5, CLOUD_CARD_BG);
            tftInstance->drawRoundRect(16, 130, 208, 65, 5, CLOUD_ACCENT_BLUE);
            tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Restoring Backup...", 120, 162, 2);

            bool ok = KryonCloudManager::restoreDeviceBackup();
            storageStatusToast = ok ? "Backup Restored to Disk!" : "No Backup Found / Error";
            storageStatusToastTime = millis();
            drawStorageScreen();
            return;
        }
    }

    // File item click -> Download
    int startY = (storageStatusToast.length() > 0) ? 134 : 112;
    if (y >= startY && y <= 285) {
        int idx = (y - startY) / 38;
        const auto& list = storageScopeDevice ? cachedDeviceFiles : cachedSharedFiles;
        if (idx >= 0 && idx < (int)list.size()) {
            tftInstance->fillRect(8, startY + idx * 38, 224, 34, CLOUD_ACCENT_BLUE);
            tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
            tftInstance->setTextDatum(MC_DATUM);
            tftInstance->drawString("Downloading...", 120, startY + idx * 38 + 17, 2);

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
    tftInstance->fillRect(0, 57, 240, 263, CLOUD_BG);

    const CloudLimits& lim = KryonCloudManager::getLimits();

    // AI Quota Card
    tftInstance->fillRoundRect(8, 62, 224, 68, 5, CLOUD_CARD_BG);
    tftInstance->drawRoundRect(8, 62, 224, 68, 5, CLOUD_CARD_BORDER);

    tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
    tftInstance->setTextDatum(TL_DATUM);
    tftInstance->drawString("Daily AI Limit", 16, 68, 2);

    tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
    tftInstance->drawString((String(lim.dailyAiUsed) + " / " + String(lim.dailyAiLimit) + " used").c_str(), 120, 68, 2);

    int aiPct = (lim.dailyAiLimit > 0) ? (lim.dailyAiUsed * 206) / lim.dailyAiLimit : 0;
    if (aiPct > 206) aiPct = 206;
    uint16_t aiBarColor = (aiPct > 185) ? TFT_RED : ((aiPct > 154) ? TFT_ORANGE : 0x07E0);

    tftInstance->drawRoundRect(16, 92, 208, 10, 3, CLOUD_CARD_BORDER);
    tftInstance->fillRect(17, 93, 206, 8, CLOUD_TRACK_BG);
    if (aiPct > 0) {
        tftInstance->fillRoundRect(17, 93, aiPct, 8, 2, aiBarColor);
    }

    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
    tftInstance->drawString(("Remaining Today: " + String(lim.dailyAiRemaining) + " requests").c_str(), 16, 110, 1);

    // Storage Quota Card
    tftInstance->fillRoundRect(8, 136, 224, 68, 5, CLOUD_CARD_BG);
    tftInstance->drawRoundRect(8, 136, 224, 68, 5, CLOUD_CARD_BORDER);

    tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
    tftInstance->drawString("Cloud Storage", 16, 142, 2);

    tftInstance->setTextColor(TFT_GREEN, CLOUD_CARD_BG);
    tftInstance->drawString((String(lim.storageUsedBytes / 1024) + " KB / " + String(lim.storageQuotaMb) + " MB").c_str(), 110, 142, 2);

    int strPct = (lim.storageQuotaBytes > 0) ? (lim.storageUsedBytes * 206) / lim.storageQuotaBytes : 0;
    if (strPct > 206) strPct = 206;
    uint16_t strBarColor = (strPct > 185) ? TFT_RED : ((strPct > 154) ? TFT_ORANGE : 0x07E0);

    tftInstance->drawRoundRect(16, 166, 208, 10, 3, CLOUD_CARD_BORDER);
    tftInstance->fillRect(17, 167, 206, 8, CLOUD_TRACK_BG);
    if (strPct > 0) {
        tftInstance->fillRoundRect(17, 167, strPct, 8, 2, strBarColor);
    }

    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_CARD_BG);
    tftInstance->drawString(("Free Space: " + String(lim.storageRemainingBytes / 1024) + " KB remaining").c_str(), 16, 184, 1);

    // Reset Schedule & Health Card
    tftInstance->fillRoundRect(8, 210, 224, 44, 5, CLOUD_CARD_BG);
    tftInstance->drawRoundRect(8, 210, 224, 44, 5, CLOUD_CARD_BORDER);

    tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
    tftInstance->drawString("Quota Reset:", 16, 216, 1);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_CARD_BG);
    tftInstance->drawString("Daily at 00:00 UTC", 90, 216, 1);

    tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
    tftInstance->drawString("Account Tier:", 16, 234, 1);
    tftInstance->setTextColor(TFT_GREEN, CLOUD_CARD_BG);
    tftInstance->drawString("Verified Hardware Node", 90, 234, 1);

    // Refresh Quota Button
    tftInstance->fillRoundRect(8, 260, 224, 32, 5, CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString("REFRESH QUOTAS", 120, 276, 2);

    tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
    tftInstance->drawString("KryonCloud Account Gateway", 120, 304, 1);
}

void KryonCloudUI::handleLimitsTouch(uint16_t x, uint16_t y) {
    if (y < 56) {
        handleTopNavTouch(x, y);
        return;
    }

    if (y >= 260 && y <= 294) {
        KryonCloudManager::fetchAccountLimits();
        drawLimitsScreen();
    }
}

// ============================================================================
// 7. BAN / APPEAL STATUS SCREEN
// ============================================================================
void KryonCloudUI::drawBanStatusScreen() {
    tftInstance->fillScreen(CLOUD_BG);

    // Frame & Header
    tftInstance->drawRoundRect(3, 3, 234, 314, 5, CLOUD_CARD_BORDER);
    tftInstance->fillRoundRect(6, 6, 228, 30, 5, CLOUD_NAV_BG);
    tftInstance->drawRoundRect(6, 6, 228, 30, 5, cachedBanStatus.isBanned ? TFT_RED : CLOUD_ACCENT_BLUE);
    tftInstance->setTextColor(cachedBanStatus.isBanned ? TFT_RED : TFT_WHITE, CLOUD_NAV_BG);
    tftInstance->setTextDatum(MC_DATUM);
    tftInstance->drawString(cachedBanStatus.isBanned ? "Account / Device Ban Notice" : "Security & Ban Status", 120, 21, 2);

    if (cachedBanStatus.isBanned) {
        bool isDeviceBan = cachedBanStatus.deviceBanned && !cachedBanStatus.accountBanned;

        // Card container
        tftInstance->fillRoundRect(10, 42, 220, 226, 6, CLOUD_CARD_BG);
        tftInstance->drawRoundRect(10, 42, 220, 226, 6, TFT_RED);

        tftInstance->setTextColor(TFT_RED, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TC_DATUM);
        tftInstance->drawString(isDeviceBan ? "DEVICE SUSPENDED" : "ACCOUNT SUSPENDED", 120, 48, 2);

        // Ban Reason
        tftInstance->setTextColor(TFT_YELLOW, CLOUD_CARD_BG);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString("Reason:", 16, 72, 1);
        String reasonStr = (cachedBanStatus.banReason.length() > 0) ? cachedBanStatus.banReason : "Terms of Service Violation";
        drawWrappedText(reasonStr, 16, 86, 208, 32, TFT_WHITE, 1);

        // Instruction Box
        tftInstance->fillRoundRect(14, 122, 212, 140, 4, CLOUD_BG);
        tftInstance->drawRoundRect(14, 122, 212, 140, 4, CLOUD_CARD_BORDER);

        tftInstance->setTextColor(TFT_YELLOW, CLOUD_BG);
        tftInstance->setTextDatum(TL_DATUM);
        tftInstance->drawString("To submit an appeal:", 18, 128, 1);

        tftInstance->setTextColor(TFT_WHITE, CLOUD_BG);
        tftInstance->drawString("Please Login to your Kryon Account here:", 18, 144, 1);

        tftInstance->setTextColor(CLOUD_BAR_STR, CLOUD_BG);
        tftInstance->drawString("https://kryonos.harislab.tech/login", 18, 160, 1);

        tftInstance->setTextColor(TFT_WHITE, CLOUD_BG);
        if (isDeviceBan) {
            tftInstance->drawString("go to connected device,", 18, 180, 1);
            tftInstance->drawString("select this device and", 18, 194, 1);
            tftInstance->drawString("submit appeal.", 18, 208, 1);
        } else {
            tftInstance->drawString("go to your account and", 18, 180, 1);
            tftInstance->drawString("then submit appeal.", 18, 194, 1);
        }

        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
        tftInstance->drawString(("Appeal Status: " + (cachedBanStatus.appealStatus.length() > 0 ? cachedBanStatus.appealStatus : "NONE")).c_str(), 18, 240, 1);

        // Exit button
        tftInstance->fillRoundRect(10, 274, 220, 36, 5, CLOUD_ACCENT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("EXIT TO LAUNCHER", 120, 292, 2);
    } else {
        tftInstance->setTextColor(TFT_GREEN, CLOUD_BG);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("STATUS: ACTIVE / GOOD", 120, 100, 2);
        tftInstance->setTextColor(CLOUD_TEXT_MUTED, CLOUD_BG);
        tftInstance->drawString("No suspensions or bans.", 120, 130, 1);

        tftInstance->fillRoundRect(20, 240, 200, 38, 5, CLOUD_ACCENT_BLUE);
        tftInstance->setTextColor(TFT_WHITE, CLOUD_ACCENT_BLUE);
        tftInstance->setTextDatum(MC_DATUM);
        tftInstance->drawString("< BACK TO OVERVIEW", 120, 259, 2);
    }
}

void KryonCloudUI::handleBanStatusTouch(uint16_t x, uint16_t y) {
    if (cachedBanStatus.isBanned) {
        if (y >= 270 && y <= 314) {
            currentState = 0; // STATE_LAUNCHER
        }
    } else {
        if (y >= 235 && y <= 285) {
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
