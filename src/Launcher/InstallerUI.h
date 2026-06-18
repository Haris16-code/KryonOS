#ifndef INSTALLER_UI_H
#define INSTALLER_UI_H

#include <TFT_eSPI.h>
#include <Arduino.h>
#include "../File System/FileSystem.h"

struct AppMetadata {
    String name;
    String packageName;
    String version;
    int api;
    String author;
    String type;
    String category;
    String description;
    String changelog;
    String folderPath; // Full path to the app folder (e.g. /sd/Downloads/Calculator/)
    bool valid;
};

class InstallerUI {
private:
    static TFT_eSPI *tftInstance;
    static FileEntry files[200];
    static int fileCount;
    static String currentPath;
    
    // Scrolling states
    static int scrollOffset;
    static int selectedIndex;
    
    static String selectedFile;
    static bool showActionDialog;
    
    // App metadata cache for display names on root items
    static String displayNames[200];
    static bool isAppPackage[200];

    static void scanSD();
    static void drawFileList();
    static void drawActionDialog();
    static void drawHelp();
    static void drawInstallProgress(int current, int total);
    static AppMetadata parseAppJson(const String& folderPath);
    static void performInstall(const String& srcFolder, const String& appName, bool overwrite);

public:
    static String autoInstallPath;
    static void init(TFT_eSPI *tft);
    static void draw();
    static void handleTouch(uint16_t x, uint16_t y);
};

#endif // INSTALLER_UI_H
