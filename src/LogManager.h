#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <Arduino.h>
#include <vector>
#include <String.h>
#include <FS.h>
#include <SPIFFS.h>

// Maximum log entries to keep in memory (to avoid running out of memory)
#define MAX_LOG_ENTRIES 1000

// Maximum log file size for persistent storage
#define MAX_LOG_FILE_SIZE 100000 // 100KB

class LogManager {
public:
    // Singleton pattern
    static LogManager& getInstance() {
        static LogManager instance;
        return instance;
    }

    // Log levels
    enum LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };

    // Add log entry
    void log(LogLevel level, const String& message);
    void debug(const String& message);
    void info(const String& message);
    void warning(const String& message);
    void error(const String& message);
    void critical(const String& message);

    // Get all logs as a string
    String getLogs();
    
    // Get logs for a specific level or above
    String getLogs(LogLevel minLevel);
    
    // Write logs to SPIFFS
    bool saveLogsToFile();
    
    // Load logs from SPIFFS
    bool loadLogsFromFile();
    
    // Clear logs
    void clearLogs();

private:
    // Private constructor (singleton)
    LogManager();
    
    // Log entry structure
    struct LogEntry {
        unsigned long timestamp;
        LogLevel level;
        String message;
    };

    // Private methods
    String levelToString(LogLevel level);
    
    // Log storage
    std::vector<LogEntry> logs;
    
    // Mutex for thread safety
    SemaphoreHandle_t logMutex;
};

// Global log function for easy access
void systemLog(LogManager::LogLevel level, const String& message);
void systemDebug(const String& message);
void systemInfo(const String& message);
void systemWarning(const String& message);
void systemError(const String& message);
void systemCritical(const String& message);

#endif // LOGMANAGER_H
