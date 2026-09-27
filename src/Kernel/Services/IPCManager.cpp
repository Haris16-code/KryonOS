#include "IPCManager.h"
#include "NotificationManager.h"

std::vector<AppRegistration> IPCManager::s_apps;
String IPCManager::s_currentAppId = "";
String IPCManager::s_pendingAppPath = "";
String IPCManager::s_currentLaunchArgs = "";
bool IPCManager::s_hasPendingLaunch = false;
std::vector<IPCMessage> IPCManager::s_messageQueue;
bool IPCManager::s_hasCallback = false;

static const char* IPC_CALLBACK_STASH_KEY = "\xff\xff_kryon_ipc_callback";

void IPCManager::init() {
    s_hasPendingLaunch = false;
    s_pendingAppPath = "";
    s_currentLaunchArgs = "";
    s_messageQueue.clear();
    s_hasCallback = false;
}

void IPCManager::registerApp(const String& appId, const String& appPath, const String& name, 
                            const String& category, const std::vector<String>& fileAssociations, 
                            bool allowCompanionLaunch) {
    for (size_t i = 0; i < s_apps.size(); i++) {
        if (s_apps[i].appId.equalsIgnoreCase(appId)) {
            // Update existing entry
            s_apps[i].appPath = appPath;
            s_apps[i].name = name;
            s_apps[i].category = category;
            s_apps[i].fileAssociations = fileAssociations;
            s_apps[i].allowCompanionLaunch = allowCompanionLaunch;
            return;
        }
    }

    AppRegistration reg;
    reg.appId = appId;
    reg.appPath = appPath;
    reg.name = name;
    reg.category = category;
    reg.fileAssociations = fileAssociations;
    reg.allowCompanionLaunch = allowCompanionLaunch;
    s_apps.push_back(reg);

    Serial.printf("[IPCManager] Registered App: '%s' (%s) [Associations: %u, Companion: %s]\n",
                  appId.c_str(), name.c_str(), (unsigned int)fileAssociations.size(), 
                  allowCompanionLaunch ? "true" : "false");
}

void IPCManager::clearRegistry() {
    s_apps.clear();
}

const std::vector<AppRegistration>& IPCManager::getRegisteredApps() {
    return s_apps;
}

String IPCManager::findAppForFile(const String& filePath) {
    int dotIdx = filePath.lastIndexOf('.');
    if (dotIdx == -1) return "";

    String ext = filePath.substring(dotIdx);
    ext.toLowerCase();

    for (size_t i = 0; i < s_apps.size(); i++) {
        for (size_t j = 0; j < s_apps[i].fileAssociations.size(); j++) {
            String assoc = s_apps[i].fileAssociations[j];
            assoc.toLowerCase();
            if (assoc == ext || assoc == ext.substring(1)) {
                return s_apps[i].appId;
            }
        }
    }
    return "";
}

bool IPCManager::canLaunchCompanion(const String& targetAppId) {
    for (size_t i = 0; i < s_apps.size(); i++) {
        if (s_apps[i].appId.equalsIgnoreCase(targetAppId)) {
            return s_apps[i].allowCompanionLaunch;
        }
    }
    return true; // Default to true if not found
}

String IPCManager::getAppPath(const String& targetAppId) {
    for (size_t i = 0; i < s_apps.size(); i++) {
        if (s_apps[i].appId.equalsIgnoreCase(targetAppId) || s_apps[i].name.equalsIgnoreCase(targetAppId)) {
            return s_apps[i].appPath;
        }
    }
    return "";
}

void IPCManager::setCurrentAppId(const String& appId) {
    s_currentAppId = appId;
}

String IPCManager::getCurrentAppId() {
    return s_currentAppId;
}

void IPCManager::setLaunchArgs(const String& targetAppId, const String& jsonArgs) {
    s_currentLaunchArgs = jsonArgs;
}

String IPCManager::getLaunchArgs() {
    return s_currentLaunchArgs;
}

bool IPCManager::requestLaunch(const String& targetAppId, const String& jsonArgs) {
    String path = getAppPath(targetAppId);
    if (path.length() == 0) {
        Serial.printf("[IPCManager] Error: App '%s' not found.\n", targetAppId.c_str());
        NotificationManager::post("Launch Error", "App not found: " + targetAppId, "error", 3000, true);
        return false;
    }

    if (!canLaunchCompanion(targetAppId)) {
        Serial.printf("[IPCManager] Error: App '%s' does not allow companion launch.\n", targetAppId.c_str());
        NotificationManager::post("Permission Denied", "App restricted companion launch", "warning", 3000, true);
        return false;
    }

    s_pendingAppPath = path;
    s_currentLaunchArgs = jsonArgs;
    s_hasPendingLaunch = true;

    Serial.printf("[IPCManager] Launch requested for '%s' (%s)\n", targetAppId.c_str(), path.c_str());
    return true;
}

bool IPCManager::openFile(const String& filePath) {
    String handlerAppId = findAppForFile(filePath);
    if (handlerAppId.length() == 0) {
        Serial.printf("[IPCManager] Error: No handler app found for file: %s\n", filePath.c_str());
        NotificationManager::post("File Open Error", "No handler for " + filePath, "warning", 3000, true);
        return false;
    }

    String jsonArgs = "{\"filePath\":\"" + filePath + "\"}";
    return requestLaunch(handlerAppId, jsonArgs);
}

bool IPCManager::hasPendingLaunch() {
    return s_hasPendingLaunch;
}

String IPCManager::getPendingAppPath() {
    return s_pendingAppPath;
}

void IPCManager::clearPendingLaunch() {
    s_hasPendingLaunch = false;
    s_pendingAppPath = "";
}

bool IPCManager::sendMessage(const String& targetAppId, const String& action, const String& payload) {
    if (s_messageQueue.size() >= 16) {
        s_messageQueue.erase(s_messageQueue.begin()); // Drop oldest if mailbox full
    }

    IPCMessage msg;
    msg.senderAppId = s_currentAppId;
    msg.targetAppId = targetAppId;
    msg.action = action;
    msg.payload = payload;
    msg.timestamp = millis();
    s_messageQueue.push_back(msg);

    Serial.printf("[IPCManager] Message queued: From '%s' -> '%s' [Action: %s]\n",
                  msg.senderAppId.c_str(), msg.targetAppId.c_str(), msg.action.c_str());
    return true;
}

void IPCManager::setOnMessageCallback(duk_context* ctx, duk_idx_t funcIdx) {
    if (!ctx) return;
    duk_push_global_stash(ctx);
    duk_dup(ctx, funcIdx);
    duk_put_prop_string(ctx, -2, IPC_CALLBACK_STASH_KEY);
    duk_pop(ctx);
    s_hasCallback = true;
}

void IPCManager::clearMessageCallback() {
    s_hasCallback = false;
}

void IPCManager::dispatchPendingMessages(duk_context* ctx) {
    if (!ctx || !s_hasCallback || s_messageQueue.empty()) return;

    for (auto it = s_messageQueue.begin(); it != s_messageQueue.end(); ) {
        if (it->targetAppId.equalsIgnoreCase(s_currentAppId) || it->targetAppId == "*") {
            duk_push_global_stash(ctx);
            if (duk_get_prop_string(ctx, -1, IPC_CALLBACK_STASH_KEY)) {
                if (duk_is_function(ctx, -1)) {
                    duk_push_string(ctx, it->senderAppId.c_str());
                    duk_push_string(ctx, it->action.c_str());
                    duk_push_string(ctx, it->payload.c_str());

                    if (duk_pcall(ctx, 3) != 0) {
                        Serial.printf("[IPCManager] Callback error: %s\n", duk_safe_to_string(ctx, -1));
                    }
                    duk_pop(ctx); // pop result or error
                } else {
                    duk_pop(ctx); // pop non-function
                }
            } else {
                duk_pop(ctx); // pop undefined
            }
            duk_pop(ctx); // pop stash

            it = s_messageQueue.erase(it);
        } else {
            ++it;
        }
    }
}
