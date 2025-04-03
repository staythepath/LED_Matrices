#include "LogManager.h"

// LogManager implementation
LogManager::LogManager() {
    // Create mutex for thread safety
    logMutex = xSemaphoreCreateMutex();
    
    // Attempt to load logs from file
    loadLogsFromFile();
    
    // Initial log entry
    info("LogManager initialized");
}

void LogManager::log(LogLevel level, const String& message) {
    // Take mutex
    if (logMutex != NULL && xSemaphoreTake(logMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        // Trim logs if exceeding maximum
        if (logs.size() >= MAX_LOG_ENTRIES) {
            logs.erase(logs.begin());
        }
        
        // Create log entry
        LogEntry entry;
        entry.timestamp = millis();
        entry.level = level;
        entry.message = message;
        
        // Add to logs
        logs.push_back(entry);
        
        // Always print to Serial for debugging
        Serial.print(millis());
        Serial.print(" [");
        Serial.print(levelToString(level));
        Serial.print("] ");
        Serial.println(message);
        
        // Release mutex
        xSemaphoreGive(logMutex);
        
        // Save to file at INFO level and above
        if (level >= INFO && level % 10 == 0) { // Save every 10th INFO or above message to reduce writes
            saveLogsToFile();
        }
    } else {
        // Mutex timeout - just print to Serial
        Serial.print(millis());
        Serial.print(" [");
        Serial.print(levelToString(level));
        Serial.print("-MUTEX_TIMEOUT] ");
        Serial.println(message);
    }
}

void LogManager::debug(const String& message) {
    log(DEBUG, message);
}

void LogManager::info(const String& message) {
    log(INFO, message);
}

void LogManager::warning(const String& message) {
    log(WARNING, message);
}

void LogManager::error(const String& message) {
    log(ERROR, message);
}

void LogManager::critical(const String& message) {
    log(CRITICAL, message);
}

String LogManager::getLogs() {
    return getLogs(DEBUG);
}

String LogManager::getLogs(LogLevel minLevel) {
    String result = "";
    
    // Take mutex with shorter timeout
    if (logMutex != NULL && xSemaphoreTake(logMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        try {
            // Reserve space for the result to avoid frequent reallocations
            size_t estimatedSize = logs.size() * 50; // Rough estimate of average log line length
            result.reserve(estimatedSize > 10000 ? 10000 : estimatedSize); // Cap at 10KB
            
            for (const auto& entry : logs) {
                if (entry.level >= minLevel) {
                    // Format: [timestamp] [LEVEL] message
                    result += "[";
                    result += String(entry.timestamp);
                    result += "] [";
                    result += levelToString(entry.level);
                    result += "] ";
                    result += entry.message;
                    result += "\n";
                }
            }
        } catch (...) {
            result = "Error formatting logs";
            Serial.println("Exception in getLogs()");
        }
        
        // Release mutex
        xSemaphoreGive(logMutex);
    } else {
        result = "[ERROR] Failed to acquire log mutex - system might be busy";
        Serial.println("Failed to acquire log mutex in getLogs()");
    }
    
    return result;
}

bool LogManager::saveLogsToFile() {
    bool success = false;
    
    // Only proceed if SPIFFS is available
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS for log saving");
        return false;
    }
    
    // Take mutex
    if (logMutex != NULL && xSemaphoreTake(logMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Open file for writing
        File file = SPIFFS.open("/logs.txt", "w");
        if (file) {
            // Write all logs
            for (const auto& entry : logs) {
                // Format: [timestamp] [LEVEL] message
                file.print("[");
                file.print(entry.timestamp);
                file.print("] [");
                file.print(levelToString(entry.level));
                file.print("] ");
                file.println(entry.message);
            }
            
            file.close();
            success = true;
        } else {
            Serial.println("Failed to open log file for writing");
        }
        
        // Release mutex
        xSemaphoreGive(logMutex);
    } else {
        Serial.println("Failed to acquire log mutex for saving");
    }
    
    SPIFFS.end();
    return success;
}

bool LogManager::loadLogsFromFile() {
    bool success = false;
    
    // Only proceed if SPIFFS is available
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS for log loading");
        return false;
    }
    
    // Take mutex
    if (logMutex != NULL && xSemaphoreTake(logMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Check if file exists
        if (SPIFFS.exists("/logs.txt")) {
            // Open file for reading
            File file = SPIFFS.open("/logs.txt", "r");
            if (file) {
                // Clear current logs
                logs.clear();
                
                // Read all lines
                while (file.available()) {
                    String line = file.readStringUntil('\n');
                    
                    // Parse line
                    if (line.length() > 10) { // Basic validation
                        // Try to parse [timestamp] [LEVEL] message format
                        int firstBracket = line.indexOf(']');
                        if (firstBracket > 0) {
                            int secondBracket = line.indexOf(']', firstBracket + 1);
                            if (secondBracket > 0) {
                                // Extract parts
                                String timestampStr = line.substring(1, firstBracket);
                                String levelStr = line.substring(firstBracket + 3, secondBracket);
                                String msg = line.substring(secondBracket + 2);
                                
                                // Create log entry
                                LogEntry entry;
                                entry.timestamp = timestampStr.toInt();
                                entry.message = msg;
                                
                                // Parse level
                                if (levelStr == "DEBUG") entry.level = DEBUG;
                                else if (levelStr == "INFO") entry.level = INFO;
                                else if (levelStr == "WARNING") entry.level = WARNING;
                                else if (levelStr == "ERROR") entry.level = ERROR;
                                else if (levelStr == "CRITICAL") entry.level = CRITICAL;
                                else entry.level = INFO; // Default
                                
                                // Add to logs
                                logs.push_back(entry);
                            }
                        }
                    }
                }
                
                file.close();
                success = true;
                
                // Log successful load
                LogEntry entry;
                entry.timestamp = millis();
                entry.level = INFO;
                entry.message = "Loaded " + String(logs.size()) + " log entries from file";
                logs.push_back(entry);
            } else {
                Serial.println("Failed to open log file for reading");
            }
        } else {
            Serial.println("Log file does not exist, starting fresh");
            success = true; // Not an error, just no previous logs
        }
        
        // Release mutex
        xSemaphoreGive(logMutex);
    } else {
        Serial.println("Failed to acquire log mutex for loading");
    }
    
    SPIFFS.end();
    return success;
}

void LogManager::clearLogs() {
    // Take mutex
    if (logMutex != NULL && xSemaphoreTake(logMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Clear logs
        logs.clear();
        
        // Log clear action
        LogEntry entry;
        entry.timestamp = millis();
        entry.level = INFO;
        entry.message = "Logs cleared";
        logs.push_back(entry);
        
        // Save to file
        saveLogsToFile();
        
        // Release mutex
        xSemaphoreGive(logMutex);
    } else {
        Serial.println("Failed to acquire log mutex for clearing");
    }
}

String LogManager::levelToString(LogLevel level) {
    switch (level) {
        case DEBUG: return "DEBUG";
        case INFO: return "INFO";
        case WARNING: return "WARNING";
        case ERROR: return "ERROR";
        case CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// Global log functions
void systemLog(LogManager::LogLevel level, const String& message) {
    LogManager::getInstance().log(level, message);
}

void systemDebug(const String& message) {
    LogManager::getInstance().debug(message);
}

void systemInfo(const String& message) {
    LogManager::getInstance().info(message);
}

void systemWarning(const String& message) {
    LogManager::getInstance().warning(message);
}

void systemError(const String& message) {
    LogManager::getInstance().error(message);
}

void systemCritical(const String& message) {
    LogManager::getInstance().critical(message);
}
