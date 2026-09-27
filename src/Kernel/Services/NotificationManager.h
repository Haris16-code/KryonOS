#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>

enum NotificationIconType {
    NOTIF_ICON_INFO = 0,
    NOTIF_ICON_WARNING = 1,
    NOTIF_ICON_ERROR = 2,
    NOTIF_ICON_SUCCESS = 3
};

enum NotificationState {
    NOTIF_STATE_IDLE = 0,
    NOTIF_STATE_SLIDE_IN = 1,
    NOTIF_STATE_DISPLAYING = 2,
    NOTIF_STATE_SLIDE_OUT = 3
};

struct NotificationItem {
    uint32_t id;
    char title[28];
    char message[52];
    uint8_t iconType;
    uint32_t durationMs;
    uint32_t displayStartTime;
    uint32_t animStartTime;
    int16_t currentY;
    uint8_t state;
    bool playSound;
};

class NotificationManager {
public:
    static const size_t MAX_NOTIFICATIONS = 4;
    static const int16_t CARD_X = 8;
    static const int16_t CARD_W = 224;
    static const int16_t CARD_H = 42;
    static const int16_t CARD_R = 6;
    static const int16_t RESTING_Y = 8;
    static const int16_t HIDDEN_Y = -44;
    static const int16_t SHADOW_W = 240;
    static const int16_t SHADOW_H = 64;

    static void init();
    static int post(const String& title, const String& message, const String& iconStr = "info", uint32_t durationMs = 3000, bool playSound = false);
    static bool dismiss(uint32_t id);
    static void clearAll();
    static void updateAndRender(TFT_eSPI* tft);
    static bool hasActiveNotification();
    static TFT_eSprite* getShadowSprite(TFT_eSPI* tft = nullptr);

private:
    static NotificationItem s_queue[MAX_NOTIFICATIONS];
    static size_t s_count;
    static uint32_t s_nextId;
    static TFT_eSPI* s_lastTft;
    static TFT_eSprite* s_cardSprite;
    static TFT_eSprite* s_shadowSprite;
    static uint16_t* s_savedBg;
    static bool s_bgCaptured;

    static void ensureSprites(TFT_eSPI* tft);
    static void captureBackground();
    static void restoreBgRegion(TFT_eSPI* tft, int16_t y, int16_t h);
    static void restoreBgFull(TFT_eSPI* tft);
    static void renderSpriteContent(const NotificationItem& item);
    static void pushCardToScreen(TFT_eSPI* tft, int16_t y);
    static void advanceQueue(TFT_eSPI* tft);
};

#endif // NOTIFICATION_MANAGER_H

