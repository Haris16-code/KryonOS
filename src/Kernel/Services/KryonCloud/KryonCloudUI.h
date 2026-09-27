#ifndef KRYON_CLOUD_UI_H
#define KRYON_CLOUD_UI_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "KryonCloudManager.h"
#include "KryonCloudAI.h"

enum CloudUISubState {
    CLOUD_STATE_PAIRING = 0,
    CLOUD_STATE_OVERVIEW,
    CLOUD_STATE_KRYON_AI,
    CLOUD_STATE_KRYON_BEAM,
    CLOUD_STATE_STORAGE,
    CLOUD_STATE_LIMITS,
    CLOUD_STATE_BAN_STATUS
};

class KryonCloudUI {
public:
    static void init(TFT_eSPI *tft);
    static void draw();
    static void handleTouch(uint16_t x, uint16_t y);
    static void loop();
    static void resetToHome();

private:
    static TFT_eSPI *tftInstance;
    static CloudUISubState currentSubState;
    static int selectedIndex;
    static int scrollOffset;
    static unsigned long lastWaitPollTime;

    // Pairing Wizard Variables
    static String activePairingCode;
    static String activePairingId;
    static String activeChallenge;
    static int pairingExpiresIn;
    static bool isWaitingForClaim;
    static unsigned long pairingStartTime;

    // KryonBeam Variables
    static std::vector<BeamMessage> cachedBeamMessages;
    static std::vector<BeamMessage> cachedPublicMessages;
    static bool beamInboxLoaded;
    static bool beamPublicLoaded;
    static bool beamPublicScope;       // false: Direct Mailbox, true: #public Stream
    static int beamViewingIndex;      // -1: inbox list, >= 0: detail modal
    static bool beamComposing;         // true: compose modal open
    static String beamTargetHandle;    // e.g. "@handle" or "#public"
    static int beamMsgTypeIndex;       // 0: TEXT, 1: ALERT, 2: TELEMETRY, 3: COMMAND
    static String beamContent;         // message body
    static int beamPublicPage;         // Current page for #public stream (3 msgs / page)
    static String beamStatusToast;
    static unsigned long beamStatusToastTime;

    // Storage Explorer Variables
    static std::vector<CloudFileItem> cachedSharedFiles;
    static std::vector<CloudFileItem> cachedDeviceFiles;
    static bool manifestLoaded;
    static bool storageScopeDevice;
    static String storageStatusToast;
    static unsigned long storageStatusToastTime;

    // KryonAI Variables
    static String aiConsolePrompt;
    static String aiConsoleResponse;
    static bool aiStreamingActive;
    static int aiRemainingTokens;
    static int aiScrollOffset;

    // Ban Status Variables
    static CloudBanStatus cachedBanStatus;

    // Draw Subscreens
    static void drawTopNav();
    static void drawPairingScreen();
    static void drawOverviewScreen();
    static void drawKryonAIScreen();
    static void drawBeamScreen();
    static void drawStorageScreen();
    static void drawLimitsScreen();
    static void drawBanStatusScreen();

    // Modals
    static void drawBeamDetailModal();
    static void drawBeamComposeModal();

    // Touch Subscreens
    static void handleTopNavTouch(uint16_t x, uint16_t y);
    static void handlePairingTouch(uint16_t x, uint16_t y);
    static void handleOverviewTouch(uint16_t x, uint16_t y);
    static void handleKryonAITouch(uint16_t x, uint16_t y);
    static void handleBeamTouch(uint16_t x, uint16_t y);
    static void handleStorageTouch(uint16_t x, uint16_t y);
    static void handleLimitsTouch(uint16_t x, uint16_t y);
    static void handleBanStatusTouch(uint16_t x, uint16_t y);

    // Helpers
    static void startPairingInit();
    static void showLoadingScreen(const String& status, int progressPct);
    static void drawWrappedText(const String& text, int x, int y, int maxW, int maxH, uint16_t color, uint8_t font = 2, int scrollLine = 0);
    static int getTextLineCount(const String& text, int maxW, uint8_t font = 2);
};

#endif // KRYON_CLOUD_UI_H
