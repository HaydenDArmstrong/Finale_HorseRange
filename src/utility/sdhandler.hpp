#pragma once
#include <SD.h>
#include <SPI.h>

// ============================================================
// ENUMERATIONS
// ============================================================

enum class SDSTATUS {
    UNDETECTED,
    IDLE,
    READING,
    WRITING,
    ERROR
};

enum class DartType {
    DART_1CC = 1,        // 1.0cc dart (practice/real variants)
    DART_1_5CC = 2,      // 1.5cc dart (estimate to 2.0cc)
    DART_2CC = 3         // 2.0cc dart (full power)
};

enum class CSVReadStatus {
    SUCCESS = 0,
    FILE_NOT_FOUND = 1,
    PARSE_ERROR = 2,
    BUFFER_OVERFLOW = 3,
    NO_DATA = 4,
    UNKNOWN_ERROR = 5
};

// ============================================================
// SD HANDLER CLASS
// ============================================================

class SDHandler {
public:
    // Initialize SD card and run diagnostics
    void initSDCard();

    // Update SD status (placeholder for potential async operations)
    void update() { /* Reserved for future use */ }

    // Read CSV ballistics table for given air density and dart type
    // Returns true on success, false on failure
    bool csvRead(float rho, DartType dartType);

    // Alternative signature for backward compatibility (float dartType)
    bool csvRead(float rho, float dartTypeFloat) {
        return csvRead(rho, static_cast<DartType>(static_cast<int>(dartTypeFloat)));
    }

    // Get current SD card status
    SDSTATUS getSDStatus() const { return _status; }

    // Get human-readable status string
    const char* getSDStatusStr() const;

    // Look up all valid gauge/distance pairs for a given angle
    // Fills outGauges[] and outDistances[] arrays
    // Returns the number of valid pairs found (max: maxResults)
    int gaugesPossible(float angle, float* outGauges, float* outDistances, int maxResults);

    // Get debug information about parsed CSV table
    void printTableStats() const;

private:
    // ========== Configuration ==========
    SDSTATUS _status = SDSTATUS::UNDETECTED;

    // ========== Table Storage (16×16 fixed size) ==========
    static constexpr int MAX_ROWS = 16;
    static constexpr int MAX_COLS = 16;

    float _colHeaders[MAX_COLS] = {0};  // Gauge pressures (MPa)
    float _rowHeaders[MAX_ROWS] = {0};  // Angles (degrees)
    float _distTable[MAX_ROWS][MAX_COLS] = {0};  // Distances (Ft)

    int _numRows = 0;
    int _numCols = 0;
    int _maxCols = 0;

    // ========== Helper Methods ==========

    // Build CSV filename based on air density and dart type
    void buildCSVFilename(float rho, DartType dartType, char* bufOutput, size_t bufSize) const;

    // Parse float from buffer, handling text suffixes like "30ft"
    bool parseFloatFromBuffer(const char* buffer, float& outValue) const;

    // Reset internal table arrays to zero
    void resetTableData();

    // Check if table has valid data
    bool hasValidData() const;
};