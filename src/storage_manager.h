#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD.h>

// Storage priority: SD (if available) -> SPIFFS -> Hardcoded defaults
class StorageManager {
private:
    bool sdAvailable;
    bool spiffsAvailable;

    // Internal helpers
    bool fileExists(fs::FS &fs, const char* path);
    String readFile(fs::FS &fs, const char* path);
    bool writeFile(fs::FS &fs, const char* path, const String& content);
    bool deleteFile(fs::FS &fs, const char* path);

public:
    StorageManager();

    // Initialize storage systems
    bool begin();

    // High-level file operations (auto-selects storage)
    String loadFile(const char* path);
    bool saveFile(const char* path, const String& content);
    bool exists(const char* path);
    bool remove(const char* path);

    // Storage status
    bool isSDAvailable() { return sdAvailable; }
    bool isSPIFFSAvailable() { return spiffsAvailable; }

    // Get storage type for a path
    String getStorageType(const char* path);

    // Copy file from SD to SPIFFS (for web editor buffer)
    bool copyToSPIFFS(const char* path);

    // Copy file from SPIFFS to SD (persist edits)
    bool copyToSD(const char* path);
};

#endif // STORAGE_MANAGER_H
