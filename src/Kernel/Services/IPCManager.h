#ifndef IPC_MANAGER_H
#define IPC_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <map>
#include "../../Runtime/duktape.h"

struct AppRegistration {
    String appId;
    String appPath;
    String name;
    String category;
    std::vector<String> fileAssociations;
    bool allowCompanionLaunch;
};

struct IPCMessage {
    String senderAppId;
    String targetAppId;
    String action;
    String payload;
    uint32_t timestamp;
};

class IPCManager {
public:
    static void init();

    // App Registry & File Associations
    static void registerApp(const String& appId, const String& appPath, const String& name, 
                            const String& category, const std::vector<String>& fileAssociations, 
                            bool allowCompanionLaunch = true);
    static void clearRegistry();
    static String findAppForFile(const String& filePath);
    static bool canLaunchCompanion(const String& targetAppId);
    static String getAppPath(const String& targetAppId);
    static const std::vector<AppRegistration>& getRegisteredApps();

    // Launch Parameter Store & Context
    static void setCurrentAppId(const String& appId);
    static String getCurrentAppId();
    static void setLaunchArgs(const String& targetAppId, const String& jsonArgs);
    static String getLaunchArgs();

    // Intent Switch Dispatcher
    static bool requestLaunch(const String& targetAppId, const String& jsonArgs);
    static bool openFile(const String& filePath);
    static bool hasPendingLaunch();
    static String getPendingAppPath();
    static void clearPendingLaunch();

    // Runtime Message Queue (Mailbox)
    static bool sendMessage(const String& targetAppId, const String& action, const String& payload);
    static void setOnMessageCallback(duk_context* ctx, duk_idx_t funcIdx);
    static void clearMessageCallback();
    static void dispatchPendingMessages(duk_context* ctx);

private:
    static std::vector<AppRegistration> s_apps;
    static String s_currentAppId;
    static String s_pendingAppPath;
    static String s_currentLaunchArgs;
    static bool s_hasPendingLaunch;

    static std::vector<IPCMessage> s_messageQueue;
    static bool s_hasCallback;
};

#endif // IPC_MANAGER_H
