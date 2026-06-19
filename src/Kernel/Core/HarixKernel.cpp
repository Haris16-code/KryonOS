#include "HarixKernel.h"
#include "../../Runtime/JSBindings.h"
#include "../../File System/FileSystem.h"

duk_context *HarixKernel::ctx = nullptr;
TFT_eSPI *HarixKernel::tftInstance = nullptr;

static void *my_alloc(void *udata, duk_size_t size) {
    if (size == 0) return nullptr;
    
    void *p = malloc(size);
    if (!p) {
        Serial.println("out of memory");
    }
    return p;
}

static void *my_realloc(void *udata, void *ptr, duk_size_t size) {
    if (size == 0) {
        free(ptr);
        return nullptr;
    }
    
    void *p = realloc(ptr, size);
    if (!p) {
        Serial.println("out of memory");
    }
    return p;
}

static void my_free(void *udata, void *ptr) {
    free(ptr);
}

// Dummy fatal error handler if duktape aborts
static void my_fatal(void *udata, const char *msg) {
    Serial.print("Duktape fatal error: ");
    Serial.println(msg ? msg : "no message");
    
    if (HarixKernel::tftInstance) {
        HarixKernel::tftInstance->fillScreen(TFT_RED);
        HarixKernel::tftInstance->setTextColor(TFT_WHITE, TFT_RED);
        HarixKernel::tftInstance->drawString("Out Of Ram Error", 10, 20, 4);
        HarixKernel::tftInstance->drawString("Please turn off WiFi in", 10, 60, 2);
        HarixKernel::tftInstance->drawString("setting to free the ram", 10, 80, 2);
        HarixKernel::tftInstance->drawString("and make this app running", 10, 100, 2);
        
        // Draw an 'X' to close/reboot
        HarixKernel::tftInstance->fillRoundRect(200, 0, 40, 30, 5, TFT_WHITE);
        HarixKernel::tftInstance->setTextColor(TFT_RED, TFT_WHITE);
        HarixKernel::tftInstance->drawString("X", 215, 8, 2);
        
        // Wait for user to touch the X before rebooting!
        uint16_t tx, ty;
        while(true) {
            if (HarixKernel::tftInstance->getTouch(&tx, &ty)) {
                if (tx >= 200 && ty <= 40) break;
            }
            delay(50);
        }
    }
    
    if (msg && strstr(msg, "alloc")) {
        Serial.println("out of memory");
    }
    ESP.restart(); // Reboot when they close it
}

void HarixKernel::init(TFT_eSPI *tft) {
    tftInstance = tft;
    // Duktape heap is no longer initialized here to save 60-80KB of RAM for the WebServer/WiFi.
    // It will be allocated on-demand in runFile() and checkSyntax().
    Serial.println("HarixKernel initialized successfully.");
}

void HarixKernel::checkJSError(duk_context *ctx, duk_int_t result) {
    if (result != 0) {
        String errorMsg = "";
        if (duk_is_error(ctx, -1)) {
            duk_get_prop_string(ctx, -1, "stack");
            errorMsg = duk_safe_to_string(ctx, -1);
            duk_pop(ctx);
        } else {
            errorMsg = duk_safe_to_string(ctx, -1);
        }
        
        // Intercept hidden OS Exit signal
        if (errorMsg.indexOf("OS_EXIT") != -1) {
            duk_pop(ctx); // pop the error
            return; // Cleanly exit execution without printing red screen
        }
        
        // Intercept OOM signals
        if (errorMsg.indexOf("alloc") != -1 || errorMsg.indexOf("out of memory") != -1) {
            if (tftInstance) {
                tftInstance->fillScreen(TFT_RED);
                tftInstance->setTextColor(TFT_WHITE, TFT_RED);
                tftInstance->drawString("Out Of Ram Error", 10, 20, 4);
                tftInstance->drawString("Please turn off WiFi in", 10, 60, 2);
                tftInstance->drawString("setting to free the ram", 10, 80, 2);
                tftInstance->drawString("and make this app running", 10, 100, 2);
                
                // Draw an 'X' to close
                tftInstance->fillRoundRect(200, 0, 40, 30, 5, TFT_WHITE);
                tftInstance->setTextColor(TFT_RED, TFT_WHITE);
                tftInstance->drawString("X", 215, 8, 2);
                
                uint16_t tx, ty;
                while(true) {
                    if (tftInstance->getTouch(&tx, &ty)) {
                        if (tx >= 200 && ty <= 40) break;
                    }
                    delay(50);
                }
            }
            duk_pop(ctx);
            // This is a soft-error (not Duktape fatal), so we can just return safely to Launcher
            return;
        }
        
        Serial.print("JS Execution Error: ");
        Serial.println(errorMsg);
        
        if (tftInstance) {
            tftInstance->fillScreen(TFT_RED);
            tftInstance->setTextColor(TFT_WHITE, TFT_RED);
            tftInstance->setTextDatum(TL_DATUM);
            tftInstance->drawString("JS EXCEPTION!", 10, 10, 4);
            
            // Draw up to 10 lines of the error message
            int yPos = 50;
            int startIdx = 0;
            while (startIdx < errorMsg.length() && yPos < 300) {
                int nextNewline = errorMsg.indexOf('\n', startIdx);
                if (nextNewline == -1) nextNewline = errorMsg.length();
                String line = errorMsg.substring(startIdx, nextNewline);
                tftInstance->drawString(line, 10, yPos, 2);
                yPos += 20;
                startIdx = nextNewline + 1;
            }
            
            // Draw an 'X' to close
            tftInstance->fillRoundRect(200, 0, 40, 30, 5, TFT_WHITE);
            tftInstance->setTextColor(TFT_RED, TFT_WHITE);
            tftInstance->drawString("X", 215, 8, 2);
            
            uint16_t tx, ty;
            while(true) {
                if (tftInstance->getTouch(&tx, &ty)) {
                    if (tx >= 200 && ty <= 40) break;
                }
                delay(50);
            }
        }
    }
    duk_pop(ctx); // pop result or error
}

void HarixKernel::executeJS(const char* jsCode) {
    if (!ctx) return;
    
    duk_int_t rc = duk_peval_string(ctx, jsCode);
    checkJSError(ctx, rc);
}

// Struct to pass data to the syntax check task
struct SyntaxCheckParams {
    const char* jsCode;
    String result;
    bool done;
};

static void syntaxCheckTask(void* param) {
    SyntaxCheckParams* p = (SyntaxCheckParams*)param;
    
    duk_context *tempCtx = duk_create_heap(my_alloc, my_realloc, my_free, nullptr, nullptr);
    if (!tempCtx) {
        p->result = "Out of Memory allocating JS heap";
        p->done = true;
        vTaskDelete(NULL);
        return;
    }
    
    duk_int_t rc = duk_pcompile_string(tempCtx, 0, p->jsCode);
    if (rc != 0) {
        p->result = duk_safe_to_string(tempCtx, -1);
        Serial.printf("Syntax Error: %s\n", p->result.c_str());
    } else {
        p->result = "";
    }
    duk_pop(tempCtx);
    duk_destroy_heap(tempCtx);
    
    p->done = true;
    vTaskDelete(NULL);
}

String HarixKernel::checkSyntax(const char* jsCode) {
    SyntaxCheckParams params;
    params.jsCode = jsCode;
    params.result = "";
    params.done = false;
    
    // Run in a dedicated task with 16KB stack to avoid overflowing loopTask
    BaseType_t created = xTaskCreatePinnedToCore(
        syntaxCheckTask,
        "syntaxChk",
        16384,        // 16KB stack just for this task
        &params,
        1,            // Low priority
        NULL,
        1             // Run on Core 1
    );
    
    if (created != pdPASS) {
        return "Failed to create syntax check task";
    }
    
    // Block until the task finishes
    while (!params.done) {
        delay(10);
    }
    
    return params.result;
}

void HarixKernel::runFile(const char* filePath) {
    if (ctx) {
        duk_destroy_heap(ctx);
        ctx = nullptr;
    }

    ctx = duk_create_heap(my_alloc, my_realloc, my_free, nullptr, my_fatal);
    if (!ctx) {
        Serial.println("Failed to create Duktape heap for app.");
        if (tftInstance) {
            tftInstance->fillScreen(TFT_RED);
            tftInstance->setTextColor(TFT_WHITE, TFT_RED);
            tftInstance->drawString("Out Of Ram Error", 10, 20, 4);
            tftInstance->drawString("Please turn off WiFi in", 10, 60, 2);
            tftInstance->drawString("setting to free the ram", 10, 80, 2);
            tftInstance->drawString("and make this app running", 10, 100, 2);
            
            // Draw an 'X' to close
            tftInstance->fillRoundRect(200, 0, 40, 30, 5, TFT_WHITE);
            tftInstance->setTextColor(TFT_RED, TFT_WHITE);
            tftInstance->drawString("X", 215, 8, 2);
            
            uint16_t tx, ty;
            while(true) {
                if (tftInstance->getTouch(&tx, &ty)) {
                    if (tx >= 200 && ty <= 40) break;
                }
                delay(50);
            }
        }
        return; // Soft exit back to OS
    }

    JSBindings::init(ctx, tftInstance);
    
    {
        String content = FileSystem::readTextFile(filePath);
        if (content.length() == 0) {
            Serial.print("Failed to read JS file: ");
            Serial.println(filePath);
            duk_destroy_heap(ctx);
            ctx = nullptr;
            return;
        }

        duk_push_string(ctx, filePath);
        duk_int_t rc = duk_pcompile_string_filename(ctx, 0, content.c_str());
        if (rc != 0) {
            checkJSError(ctx, rc);
            duk_destroy_heap(ctx);
            ctx = nullptr;
            return;
        }
    } // `content` String is destroyed here, freeing 50KB+ of RAM before the app runs

    duk_int_t rc = duk_pcall(ctx, 0);
    checkJSError(ctx, rc);
    
    // Destroy heap after app exits to free RAM
    duk_destroy_heap(ctx);
    ctx = nullptr;
}

void HarixKernel::loop() {
    
}
