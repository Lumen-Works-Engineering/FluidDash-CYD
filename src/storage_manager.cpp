#include "storage_manager.h"

StorageManager::StorageManager() : sdAvailable(false), spiffsAvailable(false) {}

bool StorageManager::begin() {
    Serial.println("[StorageMgr] Initializing storage systems...");

    // Try to initialize SD card
    sdAvailable = SD.begin();
    if (sdAvailable) {
        Serial.println("[StorageMgr] SD card initialized");
    } else {
        Serial.println("[StorageMgr] SD card not available");
    }

    // Initialize SPIFFS (LittleFS)
    spiffsAvailable = LittleFS.begin(true); // format on fail
    if (spiffsAvailable) {
        Serial.println("[StorageMgr] SPIFFS initialized");
    } else {
        Serial.println("[StorageMgr] SPIFFS initialization failed");
    }

    return spiffsAvailable; // Require at least SPIFFS
}

bool StorageManager::fileExists(fs::FS &fs, const char* path) {
    File file = fs.open(path, "r");
    if (!file) {
        return false;
    }
    file.close();
    return true;
}

String StorageManager::readFile(fs::FS &fs, const char* path) {
    File file = fs.open(path, "r");
    if (!file) {
        return "";
    }

    String content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();

    return content;
}

bool StorageManager::writeFile(fs::FS &fs, const char* path, const String& content) {
    yield();
    delay(5);

    File file = fs.open(path, "w");
    if (!file) {
        Serial.printf("[StorageMgr] Failed to open %s for writing\n", path);
        return false;
    }

    // Write in chunks with yields
    const size_t CHUNK_SIZE = 512;
    const char* data = content.c_str();
    size_t dataLen = content.length();

    for (size_t i = 0; i < dataLen; i += CHUNK_SIZE) {
        size_t chunkLen = min(CHUNK_SIZE, dataLen - i);
        size_t written = file.write((uint8_t*)(data + i), chunkLen);

        if (written != chunkLen) {
            Serial.printf("[StorageMgr] Write error at offset %d\n", i);
            file.close();
            return false;
        }
        yield();
    }

    file.close();
    yield();
    delay(10);

    Serial.printf("[StorageMgr] Wrote %d bytes to %s\n", dataLen, path);
    return true;
}

bool StorageManager::deleteFile(fs::FS &fs, const char* path) {
    if (!fileExists(fs, path)) {
        return false;
    }
    return fs.remove(path);
}

String StorageManager::loadFile(const char* path) {
    // Priority: SD -> SPIFFS -> Empty
    if (sdAvailable && fileExists(SD, path)) {
        Serial.printf("[StorageMgr] Loading %s from SD\n", path);
        return readFile(SD, path);
    }

    if (spiffsAvailable && fileExists(LittleFS, path)) {
        Serial.printf("[StorageMgr] Loading %s from SPIFFS\n", path);
        return readFile(LittleFS, path);
    }

    Serial.printf("[StorageMgr] File not found: %s\n", path);
    return "";
}

bool StorageManager::saveFile(const char* path, const String& content) {
    // Always save to SPIFFS (reliable)
    // Later can be synced to SD if available
    if (!spiffsAvailable) {
        Serial.println("[StorageMgr] SPIFFS not available for save");
        return false;
    }

    // Ensure parent directory exists
    if (!LittleFS.exists("/screens")) {
        Serial.println("[StorageMgr] Creating /screens directory in SPIFFS...");
        if (!LittleFS.mkdir("/screens")) {
            Serial.println("[StorageMgr] ERROR: Failed to create /screens directory");
            return false;
        }
        Serial.println("[StorageMgr] /screens directory created successfully");
    }

    Serial.printf("[StorageMgr] Saving %s to SPIFFS\n", path);
    return writeFile(LittleFS, path, content);
}

bool StorageManager::exists(const char* path) {
    if (sdAvailable && fileExists(SD, path)) {
        return true;
    }
    if (spiffsAvailable && fileExists(LittleFS, path)) {
        return true;
    }
    return false;
}

bool StorageManager::remove(const char* path) {
    bool removed = false;

    if (sdAvailable && fileExists(SD, path)) {
        removed = deleteFile(SD, path);
        Serial.printf("[StorageMgr] Removed %s from SD: %s\n", path, removed ? "OK" : "FAIL");
    }

    if (spiffsAvailable && fileExists(LittleFS, path)) {
        removed = deleteFile(LittleFS, path) || removed;
        Serial.printf("[StorageMgr] Removed %s from SPIFFS: %s\n", path, removed ? "OK" : "FAIL");
    }

    return removed;
}

String StorageManager::getStorageType(const char* path) {
    if (sdAvailable && fileExists(SD, path)) {
        return "SD";
    }
    if (spiffsAvailable && fileExists(LittleFS, path)) {
        return "SPIFFS";
    }
    return "NONE";
}

bool StorageManager::copyToSPIFFS(const char* path) {
    if (!sdAvailable || !spiffsAvailable) {
        return false;
    }

    if (!fileExists(SD, path)) {
        Serial.printf("[StorageMgr] Source file not found on SD: %s\n", path);
        return false;
    }

    String content = readFile(SD, path);
    if (content.length() == 0) {
        Serial.printf("[StorageMgr] Empty or failed to read from SD: %s\n", path);
        return false;
    }

    bool success = writeFile(LittleFS, path, content);
    Serial.printf("[StorageMgr] Copy %s SD->SPIFFS: %s\n", path, success ? "OK" : "FAIL");
    return success;
}

bool StorageManager::copyToSD(const char* path) {
    if (!sdAvailable || !spiffsAvailable) {
        return false;
    }

    if (!fileExists(LittleFS, path)) {
        Serial.printf("[StorageMgr] Source file not found on SPIFFS: %s\n", path);
        return false;
    }

    String content = readFile(LittleFS, path);
    if (content.length() == 0) {
        Serial.printf("[StorageMgr] Empty or failed to read from SPIFFS: %s\n", path);
        return false;
    }

    bool success = writeFile(SD, path, content);
    Serial.printf("[StorageMgr] Copy %s SPIFFS->SD: %s\n", path, success ? "OK" : "FAIL");
    return success;
}
