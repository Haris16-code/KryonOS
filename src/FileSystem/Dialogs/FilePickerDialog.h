#ifndef FILE_PICKER_DIALOG_H
#define FILE_PICKER_DIALOG_H

#include <Arduino.h>
#include <vector>
#include <TFT_eSPI.h>

struct FilePickerOptions {
    String title;
    String startPath;
    std::vector<String> extensions;
    bool allowDrives;
    bool showHidden;
    bool isDirectoryPicker;
    bool showCreate;
    String sandboxRoot;

    FilePickerOptions()
        : title("Select File"),
          startPath("/local"),
          allowDrives(true),
          showHidden(false),
          isDirectoryPicker(false),
          showCreate(false),
          sandboxRoot("") {}
};

class FilePickerDialog {
public:
    static String show(TFT_eSPI* tft, const FilePickerOptions& options);

private:
    struct DialogItem {
        String name;
        String fullPath;
        bool isDir;
        size_t sizeBytes;
    };

    static void scanDirectory(const String& path, const FilePickerOptions& options, std::vector<DialogItem>& items);
    static void render(TFT_eSPI* tft, const String& currentPath, const std::vector<DialogItem>& items, int page, int selectedIdx, const FilePickerOptions& options);
    static bool matchesExtension(const String& filename, const std::vector<String>& extensions);
};

#endif // FILE_PICKER_DIALOG_H
