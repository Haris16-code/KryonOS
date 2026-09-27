#include "HarixKernel.h"
#include "../../Runtime/JSBindings.h"
#include "../../File System/FileSystem.h"
#include "../../Settings/TouchDriver.h"
#include "../Services/NotificationManager.h"
#include "../Services/IPCManager.h"

duk_context *HarixKernel::ctx = nullptr;
TFT_eSPI *HarixKernel::tftInstance = nullptr;

#include <esp_heap_caps.h>

static void *my_alloc(void *udata, duk_size_t size) {
    if (size == 0) return nullptr;
    
    void *p = nullptr;
#if defined(BOARD_HAS_PSRAM)
    if (psramFound()) {
        // Allocate directly from the 8MB Octal PSRAM pool
        p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
#endif
    if (!p) {
        // Fallback to internal SRAM
        p = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (!p) {
        p = malloc(size);
    }
    if (!p) {
        Serial.printf("[HarixKernel] Out of memory allocating %u bytes! (Free PSRAM: %u, Free Heap: %u)\n", 
                      (unsigned int)size, ESP.getFreePsram(), ESP.getFreeHeap());
    }
    return p;
}

static void *my_realloc(void *udata, void *ptr, duk_size_t size) {
    if (size == 0) {
        if (ptr) free(ptr);
        return nullptr;
    }
    if (!ptr) {
        return my_alloc(udata, size);
    }
    
    void *p = nullptr;
#if defined(BOARD_HAS_PSRAM)
    if (psramFound()) {
        p = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
#endif
    if (!p) {
        p = heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (!p) {
        p = realloc(ptr, size);
    }
    if (!p) {
        Serial.printf("[HarixKernel] Out of memory reallocating %u bytes! (Free PSRAM: %u, Free Heap: %u)\n", 
                      (unsigned int)size, ESP.getFreePsram(), ESP.getFreeHeap());
    }
    return p;
}

static void my_free(void *udata, void *ptr) {
    if (ptr) {
        free(ptr);
    }
}

static void printToAllSerials(const String& str) {
    Serial.print(str);
    Serial.flush();
#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
    Serial0.print(str);
    Serial0.flush();
#endif
}

// Dummy fatal error handler if duktape aborts
static void my_fatal(void *udata, const char *msg) {
    String errStr = "\n================================================================================\n";
    errStr += "[KryonOS Duktape Fatal Error] ";
    errStr += (msg ? msg : "no message");
    errStr += "\n================================================================================\n";
    printToAllSerials(errStr);
    
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
            if (TouchDriver::getTouch(&tx, &ty)) {
                if (tx >= 200 && ty <= 40) break;
            }
            delay(50);
        }
    }
    
    if (msg && strstr(msg, "alloc")) {
        printToAllSerials("out of memory\n");
    }
    ESP.restart(); // Reboot when they close it
}

void HarixKernel::init(TFT_eSPI *tft) {
    tftInstance = tft;
    NotificationManager::init();
    IPCManager::init();
    // Duktape heap is no longer initialized here to save 60-80KB of RAM for the WebServer/WiFi.
    // It will be allocated on-demand in runFile() and checkSyntax().
    printToAllSerials("HarixKernel initialized successfully.\n");
}

void HarixKernel::checkJSError(duk_context *ctx, duk_int_t result) {
    if (result != 0) {
        String errName = "Error";
        String errMsg = "";
        String fileName = "";
        int lineNumber = -1;
        String stackTrace = "";
        String fullError = "";

        if (duk_is_error(ctx, -1)) {
            if (duk_get_prop_string(ctx, -1, "name")) {
                errName = duk_safe_to_string(ctx, -1);
            }
            duk_pop(ctx);

            if (duk_get_prop_string(ctx, -1, "message")) {
                errMsg = duk_safe_to_string(ctx, -1);
            }
            duk_pop(ctx);

            if (duk_get_prop_string(ctx, -1, "fileName")) {
                fileName = duk_safe_to_string(ctx, -1);
            }
            duk_pop(ctx);

            if (duk_get_prop_string(ctx, -1, "lineNumber")) {
                lineNumber = duk_get_int(ctx, -1);
            }
            duk_pop(ctx);

            if (duk_get_prop_string(ctx, -1, "stack")) {
                stackTrace = duk_safe_to_string(ctx, -1);
            }
            duk_pop(ctx);

            fullError = duk_safe_to_string(ctx, -1);
        } else {
            fullError = duk_safe_to_string(ctx, -1);
            errMsg = fullError;
        }

        // Intercept hidden OS Exit signal
        if (fullError.indexOf("OS_EXIT") != -1 || errMsg.indexOf("OS_EXIT") != -1) {
            duk_pop(ctx);
            return;
        }

        // Intercept OOM signals
        if (fullError.indexOf("alloc") != -1 || fullError.indexOf("out of memory") != -1 || errMsg.indexOf("out of memory") != -1) {
            String oomReport = "\n================================================================================\n";
            oomReport += "[KryonOS JS Error] OUT OF MEMORY (Heap Exhausted)\n";
            oomReport += "================================================================================\n\n";
            printToAllSerials(oomReport);

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
                    if (TouchDriver::getTouch(&tx, &ty)) {
                        if (tx >= 200 && ty <= 40) break;
                    }
                    delay(50);
                }
            }
            duk_pop(ctx);
            return;
        }

        // Output rich formatted error to all Serial Monitors (USB CDC and Hardware UART)
        String report = "\n================================================================================\n";
        report += "[KryonOS JS Exception] " + errName + "\n";
        report += "--------------------------------------------------------------------------------\n";
        if (fileName.length() > 0) {
            report += "File       : " + fileName + "\n";
        }
        if (lineNumber > 0) {
            report += "Line       : " + String(lineNumber) + "\n";
        }
        if (errMsg.length() > 0) {
            report += "Message    : " + errMsg + "\n";
        }
        if (stackTrace.length() > 0) {
            report += "Stack Trace:\n" + stackTrace + "\n";
        } else if (fullError.length() > 0) {
            report += "Details    :\n" + fullError + "\n";
        }
        report += "================================================================================\n\n";

        printToAllSerials(report);

        if (tftInstance) {
            tftInstance->fillScreen(TFT_RED);
            tftInstance->setTextColor(TFT_WHITE, TFT_RED);
            tftInstance->setTextDatum(TL_DATUM);
            tftInstance->drawString("JS EXCEPTION!", 10, 10, 4);
            
            int yPos = 40;
            if (errName.length() > 0) {
                tftInstance->drawString(errName + ":", 10, yPos, 2);
                yPos += 18;
            }
            if (fileName.length() > 0 || lineNumber > 0) {
                String loc = (fileName.length() > 0 ? fileName : "app.js") + ":" + String(lineNumber);
                tftInstance->drawString(loc, 10, yPos, 2);
                yPos += 18;
            }
            
            // Draw lines of message / stack
            String displayStr = (errMsg.length() > 0) ? errMsg : fullError;
            int startIdx = 0;
            while (startIdx < displayStr.length() && yPos < 290) {
                int nextNewline = displayStr.indexOf('\n', startIdx);
                if (nextNewline == -1) nextNewline = displayStr.length();
                String line = displayStr.substring(startIdx, nextNewline);
                tftInstance->drawString(line, 10, yPos, 2);
                yPos += 18;
                startIdx = nextNewline + 1;
            }

            // Draw an 'X' to close
            tftInstance->fillRoundRect(200, 0, 40, 30, 5, TFT_WHITE);
            tftInstance->setTextColor(TFT_RED, TFT_WHITE);
            tftInstance->drawString("X", 215, 8, 2);
            
            uint16_t tx, ty;
            while(true) {
                if (TouchDriver::getTouch(&tx, &ty)) {
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
        String errName = "SyntaxError";
        String errMsg = "";
        int lineNumber = -1;
        String stack = "";

        if (duk_is_error(tempCtx, -1)) {
            if (duk_get_prop_string(tempCtx, -1, "name")) errName = duk_safe_to_string(tempCtx, -1);
            duk_pop(tempCtx);
            if (duk_get_prop_string(tempCtx, -1, "message")) errMsg = duk_safe_to_string(tempCtx, -1);
            duk_pop(tempCtx);
            if (duk_get_prop_string(tempCtx, -1, "lineNumber")) lineNumber = duk_get_int(tempCtx, -1);
            duk_pop(tempCtx);
            if (duk_get_prop_string(tempCtx, -1, "stack")) stack = duk_safe_to_string(tempCtx, -1);
            duk_pop(tempCtx);
            p->result = duk_safe_to_string(tempCtx, -1);
        } else {
            p->result = duk_safe_to_string(tempCtx, -1);
            errMsg = p->result;
        }

        String synReport = "\n================================================================================\n";
        synReport += "[KryonOS JS Syntax Error] " + errName + "\n";
        synReport += "--------------------------------------------------------------------------------\n";
        if (lineNumber > 0) {
            synReport += "Line       : " + String(lineNumber) + "\n";
        }
        if (errMsg.length() > 0) {
            synReport += "Message    : " + errMsg + "\n";
        }
        if (stack.length() > 0) {
            synReport += "Stack Trace:\n" + stack + "\n";
        }
        synReport += "================================================================================\n\n";
        printToAllSerials(synReport);
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
        printToAllSerials("Failed to create Duktape heap for app.\n");
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
                if (TouchDriver::getTouch(&tx, &ty)) {
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
            String err = "Failed to read JS file: " + String(filePath) + "\n";
            printToAllSerials(err);
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
    
    // Cleanup active HTTP servers, sockets and JS callbacks
    JSBindings::cleanup(ctx);

    // Destroy heap after app exits to free RAM
    duk_destroy_heap(ctx);
    ctx = nullptr;
}

void HarixKernel::loop() {
    
}
