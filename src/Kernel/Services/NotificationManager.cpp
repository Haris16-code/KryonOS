#include "NotificationManager.h"
#include "../../Hal/PWM/PWMEngine.h"

// ── Static member definitions ────────────────────────────────────────────────
NotificationItem NotificationManager::s_queue[NotificationManager::MAX_NOTIFICATIONS];
size_t NotificationManager::s_count = 0;
uint32_t NotificationManager::s_nextId = 1;
TFT_eSPI* NotificationManager::s_lastTft = nullptr;
TFT_eSprite* NotificationManager::s_cardSprite = nullptr;
TFT_eSprite* NotificationManager::s_shadowSprite = nullptr;
uint16_t* NotificationManager::s_savedBg = nullptr;
bool NotificationManager::s_bgCaptured = false;

// ── Init ─────────────────────────────────────────────────────────────────────
void NotificationManager::init() {
    clearAll();
}

TFT_eSprite* NotificationManager::getShadowSprite(TFT_eSPI* tft) {
    if (tft) s_lastTft = tft;
    if (!s_shadowSprite && s_lastTft) {
        ensureSprites(s_lastTft);
    }
    return s_shadowSprite;
}

// ── Sprite allocation ────────────────────────────────────────────────────────
void NotificationManager::ensureSprites(TFT_eSPI* tft) {
    if (!tft) return;
    s_lastTft = tft;

    if (!s_cardSprite) {
        s_cardSprite = new TFT_eSprite(tft);
        if (s_cardSprite) {
            void* buf = s_cardSprite->createSprite(CARD_W, CARD_H);
            if (!buf) {
                delete s_cardSprite;
                s_cardSprite = nullptr;
            }
        }
    }

    if (!s_shadowSprite) {
        s_shadowSprite = new TFT_eSprite(tft);
        if (s_shadowSprite) {
            void* buf = s_shadowSprite->createSprite(SHADOW_W, SHADOW_H);
            if (buf) {
                s_shadowSprite->fillScreen(0x0821);
            } else {
                delete s_shadowSprite;
                s_shadowSprite = nullptr;
            }
        }
    }

    if (!s_savedBg) {
        s_savedBg = (uint16_t*)heap_caps_malloc(SHADOW_W * SHADOW_H * sizeof(uint16_t), MALLOC_CAP_DEFAULT);
        if (s_savedBg) {
            for (int i = 0; i < SHADOW_W * SHADOW_H; i++) s_savedBg[i] = 0x0821;
        }
    }
}

void NotificationManager::captureBackground() {
    if (s_shadowSprite && s_savedBg) {
        uint16_t* src = (uint16_t*)s_shadowSprite->getPointer();
        if (src) {
            memcpy(s_savedBg, src, SHADOW_W * SHADOW_H * sizeof(uint16_t));
            s_bgCaptured = true;
        }
    }
}

void NotificationManager::restoreBgRegion(TFT_eSPI* tft, int16_t y, int16_t h) {
    if (!tft || h <= 0 || y >= SHADOW_H) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > SHADOW_H) h = SHADOW_H - y;
    if (h <= 0) return;

    if (s_savedBg && s_bgCaptured) {
        tft->pushImage(0, y, SHADOW_W, h, s_savedBg + (y * SHADOW_W));
    } else {
        tft->fillRect(0, y, SHADOW_W, h, 0x0821);
    }
}

void NotificationManager::restoreBgFull(TFT_eSPI* tft) {
    if (!tft) return;
    if (s_savedBg && s_bgCaptured) {
        tft->pushImage(0, 0, SHADOW_W, SHADOW_H, s_savedBg);
    } else {
        tft->fillRect(0, 0, SHADOW_W, SHADOW_H, 0x0821);
    }
    s_bgCaptured = false;
}

// ── Render card content into the sprite (only on new notification) ──────────
void NotificationManager::renderSpriteContent(const NotificationItem& item) {
    if (!s_cardSprite) return;

    uint16_t bgColor = 0x10A2;
    uint16_t borderColor = 0x4208;
    uint16_t badgeColor = 0x07FF;
    const char* badgeText = "i";

    switch (item.iconType) {
        case NOTIF_ICON_SUCCESS:
            borderColor = 0x07E0; badgeColor = 0x07E0; badgeText = "V"; break;
        case NOTIF_ICON_WARNING:
            borderColor = 0xFFE0; badgeColor = 0xFFE0; badgeText = "!"; break;
        case NOTIF_ICON_ERROR:
            borderColor = 0xF800; badgeColor = 0xF800; badgeText = "X"; break;
        default:
            borderColor = 0x07FF; badgeColor = 0x07FF; badgeText = "i"; break;
    }

    s_cardSprite->fillRoundRect(0, 0, CARD_W, CARD_H, CARD_R, bgColor);
    s_cardSprite->drawRoundRect(0, 0, CARD_W, CARD_H, CARD_R, borderColor);

    // Accent badge circle
    s_cardSprite->fillCircle(16, 21, 10, badgeColor);
    s_cardSprite->setTextColor(TFT_BLACK, badgeColor);
    s_cardSprite->setTextDatum(MC_DATUM);
    s_cardSprite->drawString(badgeText, 16, 21, 2);

    // Title
    s_cardSprite->setTextDatum(TL_DATUM);
    s_cardSprite->setTextColor(TFT_WHITE, bgColor);
    s_cardSprite->drawString(item.title, 34, 6, 2);

    // Message
    s_cardSprite->setTextColor(0xBDD7, bgColor);
    s_cardSprite->drawString(item.message, 34, 24, 1);
}

// ── Push the card sprite to the screen at position Y ────────────────────────
void NotificationManager::pushCardToScreen(TFT_eSPI* tft, int16_t y) {
    if (!tft || !s_cardSprite) return;

    uint16_t* sprBuf = (uint16_t*)s_cardSprite->getPointer();
    if (!sprBuf) return;

    if (y >= 0) {
        // Fully on screen
        tft->pushImage(CARD_X, y, CARD_W, CARD_H, sprBuf);
    } else {
        // Partially off-screen at top edge (y < 0)
        int16_t skipRows = -y;
        int16_t visH = CARD_H - skipRows;
        if (visH > 0 && visH <= CARD_H) {
            tft->pushImage(CARD_X, 0, CARD_W, visH, sprBuf + (skipRows * CARD_W));
        }
    }
}

// ── Post a notification ─────────────────────────────────────────────────────
int NotificationManager::post(const String& title, const String& message, const String& iconStr, uint32_t durationMs, bool playSound) {
    if (s_count >= MAX_NOTIFICATIONS) return -1;

    size_t idx = s_count;
    s_queue[idx].id = s_nextId++;
    if (s_nextId == 0) s_nextId = 1;

    strncpy(s_queue[idx].title, title.c_str(), sizeof(s_queue[idx].title) - 1);
    s_queue[idx].title[sizeof(s_queue[idx].title) - 1] = '\0';

    strncpy(s_queue[idx].message, message.c_str(), sizeof(s_queue[idx].message) - 1);
    s_queue[idx].message[sizeof(s_queue[idx].message) - 1] = '\0';

    if (iconStr.equalsIgnoreCase("warning") || iconStr.equalsIgnoreCase("warn")) {
        s_queue[idx].iconType = NOTIF_ICON_WARNING;
    } else if (iconStr.equalsIgnoreCase("error") || iconStr.equalsIgnoreCase("danger")) {
        s_queue[idx].iconType = NOTIF_ICON_ERROR;
    } else if (iconStr.equalsIgnoreCase("success") || iconStr.equalsIgnoreCase("ok")) {
        s_queue[idx].iconType = NOTIF_ICON_SUCCESS;
    } else {
        s_queue[idx].iconType = NOTIF_ICON_INFO;
    }

    s_queue[idx].durationMs = constrain(durationMs, 500, 15000);
    s_queue[idx].state = NOTIF_STATE_SLIDE_IN;
    s_queue[idx].currentY = HIDDEN_Y;
    s_queue[idx].animStartTime = millis();
    s_queue[idx].displayStartTime = 0;
    s_queue[idx].playSound = playSound;

    // If this is the FIRST notification, ensure sprites, capture background & render card
    if (idx == 0 && s_lastTft) {
        ensureSprites(s_lastTft);
        captureBackground();
        renderSpriteContent(s_queue[0]);
    }

    s_count++;

    if (playSound) {
#ifdef BUZZER_PIN
        if (!PWMEngine::isReservedPin(BUZZER_PIN)) {
            PWMEngine::setTone(BUZZER_PIN, 2200, 80);
        }
#else
        if (!PWMEngine::isReservedPin(19)) {
            PWMEngine::setTone(19, 2200, 80);
        }
#endif
    }

    Serial.printf("[Notification] Enqueued #%u: '%s' - '%s' (%ums)\n",
                  s_queue[idx].id, s_queue[idx].title, s_queue[idx].message, s_queue[idx].durationMs);
    return s_queue[idx].id;
}

// ── Dismiss a specific notification ─────────────────────────────────────────
bool NotificationManager::dismiss(uint32_t id) {
    if (s_count == 0) return false;

    for (size_t i = 0; i < s_count; i++) {
        if (s_queue[i].id == id) {
            if (i == 0) {
                // Active notification: trigger slide-out
                if (s_queue[0].state != NOTIF_STATE_SLIDE_OUT && s_queue[0].state != NOTIF_STATE_IDLE) {
                    s_queue[0].state = NOTIF_STATE_SLIDE_OUT;
                    s_queue[0].animStartTime = millis();
                }
            } else {
                // Queued notification: remove from queue
                for (size_t j = i; j < s_count - 1; j++) {
                    s_queue[j] = s_queue[j + 1];
                }
                s_count--;
            }
            return true;
        }
    }
    return false;
}

// ── Clear all notifications ─────────────────────────────────────────────────
void NotificationManager::clearAll() {
    if (s_count > 0 && s_lastTft) {
        restoreBgFull(s_lastTft);
    }
    for (size_t i = 0; i < MAX_NOTIFICATIONS; i++) {
        s_queue[i].id = 0;
        s_queue[i].state = NOTIF_STATE_IDLE;
        s_queue[i].currentY = HIDDEN_Y;
    }
    s_count = 0;
}

bool NotificationManager::hasActiveNotification() {
    return (s_count > 0 && s_queue[0].state != NOTIF_STATE_IDLE);
}

// ── Advance queue to next notification ──────────────────────────────────────
void NotificationManager::advanceQueue(TFT_eSPI* tft) {
    if (s_count == 0) return;

    for (size_t i = 0; i < s_count - 1; i++) {
        s_queue[i] = s_queue[i + 1];
    }
    s_count--;

    if (s_count > 0) {
        // Start next notification
        s_queue[0].state = NOTIF_STATE_SLIDE_IN;
        s_queue[0].currentY = HIDDEN_Y;
        s_queue[0].animStartTime = millis();
        s_queue[0].displayStartTime = 0;
        if (tft) {
            ensureSprites(tft);
            captureBackground();
            renderSpriteContent(s_queue[0]);
        }
    }
}

// ── Main update & render loop (called from System.delay) ────────────────────
void NotificationManager::updateAndRender(TFT_eSPI* tft) {
    if (!tft) return;
    s_lastTft = tft;

    if (s_count == 0) return;

    ensureSprites(tft);
    if (!s_cardSprite) return;

    // If background wasn't captured yet, capture it now
    if (!s_bgCaptured) {
        captureBackground();
    }

    NotificationItem& item = s_queue[0];
    uint32_t now = millis();
    const uint32_t ANIM_MS = 180;

    int16_t prevY = item.currentY;
    int16_t newY = prevY;

    switch (item.state) {
        case NOTIF_STATE_SLIDE_IN: {
            uint32_t elapsed = now - item.animStartTime;
            if (elapsed >= ANIM_MS) {
                newY = RESTING_Y;
                item.state = NOTIF_STATE_DISPLAYING;
                item.displayStartTime = now;
            } else {
                float t = (float)elapsed / (float)ANIM_MS;
                float ease = 1.0f - powf(1.0f - t, 3.0f); // easeOutCubic
                newY = HIDDEN_Y + (int16_t)((float)(RESTING_Y - HIDDEN_Y) * ease);
            }
            break;
        }

        case NOTIF_STATE_DISPLAYING: {
            newY = RESTING_Y;
            if (now - item.displayStartTime >= item.durationMs) {
                item.state = NOTIF_STATE_SLIDE_OUT;
                item.animStartTime = now;
            }
            break;
        }

        case NOTIF_STATE_SLIDE_OUT: {
            uint32_t elapsed = now - item.animStartTime;
            if (elapsed >= ANIM_MS) {
                // Animation complete — fully off-screen
                item.currentY = HIDDEN_Y;
                item.state = NOTIF_STATE_IDLE;
                restoreBgFull(tft);
                advanceQueue(tft);
                return;
            } else {
                float t = (float)elapsed / (float)ANIM_MS;
                float ease = t * t * t; // easeInCubic
                newY = RESTING_Y + (int16_t)((float)(HIDDEN_Y - RESTING_Y) * ease);
            }
            break;
        }

        default:
            return;
    }

    // Only draw if position actually changed or at resting position initially
    if (newY == prevY && item.state == NOTIF_STATE_DISPLAYING && prevY == RESTING_Y) return;

    // ── Differential background restore ─────────────────────────────────
    if (newY > prevY) {
        // SLIDING DOWN: restore top strip above card
        if (newY > 0) {
            restoreBgRegion(tft, 0, newY);
        }
    } else if (newY < prevY) {
        // SLIDING UP: restore vacated bottom strip below card
        int16_t prevBot = prevY + CARD_H;
        int16_t newBot = newY + CARD_H;
        if (prevBot > 0) {
            int16_t clearTop = max((int16_t)0, newBot);
            int16_t clearH = prevBot - clearTop;
            if (clearH > 0) {
                restoreBgRegion(tft, clearTop, clearH);
            }
        }
    }

    // Push the card sprite at the new Y position
    pushCardToScreen(tft, newY);
    item.currentY = newY;
}


