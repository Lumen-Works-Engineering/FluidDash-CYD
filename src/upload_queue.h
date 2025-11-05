#ifndef UPLOAD_QUEUE_H
#define UPLOAD_QUEUE_H

#include <Arduino.h>
#include <queue>

// Define max upload size
#define MAX_UPLOAD_SIZE 8192  // 8KB chunks

struct UploadCommand {
    String filename;
    String data;
    bool isQueued;
};

class SDUploadQueue {
private:
    std::queue<UploadCommand> cmdQueue;
    static const int MAX_QUEUE_SIZE = 10;

public:
    SDUploadQueue() {}

    // Add command to queue
    bool enqueue(const String& filename, const String& data) {
        if (cmdQueue.size() >= MAX_QUEUE_SIZE) {
            Serial.println("[Queue] ERROR: Upload queue full!");
            return false;
        }

        UploadCommand cmd;
        cmd.filename = filename;
        cmd.data = data;
        cmd.isQueued = true;

        cmdQueue.push(cmd);
        Serial.printf("[Queue] Upload queued: %s (%d bytes)\n",
                      filename.c_str(), data.length());
        return true;
    }

    // Check if commands pending
    bool hasPending() {
        return !cmdQueue.empty();
    }

    // Get next command (does NOT remove from queue)
    UploadCommand peek() {
        if (cmdQueue.empty()) {
            return {"", "", false};
        }
        return cmdQueue.front();
    }

    // Remove command after successful processing
    void dequeue() {
        if (!cmdQueue.empty()) {
            cmdQueue.pop();
        }
    }

    // Get queue size
    int size() {
        return cmdQueue.size();
    }

    // Clear queue
    void clear() {
        while (!cmdQueue.empty()) {
            cmdQueue.pop();
        }
    }
};

#endif // UPLOAD_QUEUE_H
