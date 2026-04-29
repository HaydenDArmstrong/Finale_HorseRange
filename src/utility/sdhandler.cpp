#include "sdhandler.hpp"
#include <string.h>
#include <M5Unified.h>


// SPI PIN DEFINITIONS


#define SD_SPI_SCK_PIN 14
#define SD_SPI_MISO_PIN 13
#define SD_SPI_MOSI_PIN 12
#define SD_SPI_CS_PIN 4


// INITIALIZATION


void SDHandler::initSDCard() {
    // Initialize SPI with custom pins before SD
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);

    // Attempt to mount SD card
    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        Serial.println("[SD] ERROR: SD card not detected!");
        _status = SDSTATUS::UNDETECTED;
        return;
    }

    Serial.println("[SD] SD card detected");
    _status = SDSTATUS::IDLE;

    // === DIAGNOSTIC WRITE TEST ===
    Serial.println("[SD] Running write test...");
    File writeFile = SD.open("/WriteTest.txt", FILE_WRITE, true);
    if (writeFile) {
        writeFile.print("SD card write test successful.\n");
        writeFile.close();
        Serial.println("[SD] Write test passed");
    } else {
        Serial.println("[SD] Write test failed");
        _status = SDSTATUS::ERROR;
        return;
    }

    // === DIAGNOSTIC READ TEST ===
    Serial.println("[SD] Running read test...");
    File readFile = SD.open("/WriteTest.txt", FILE_READ);
    if (readFile) {
        Serial.println("[SD] Read test passed");
        readFile.close();
    } else {
        Serial.println("[SD] Read test failed");
        _status = SDSTATUS::ERROR;
        return;
    }

    _status = SDSTATUS::IDLE;
    Serial.println("[SD] Initialization complete");
}


// STATUS METHODS


const char* SDHandler::getSDStatusStr() const {
    switch (_status) {
        case SDSTATUS::UNDETECTED:  return "UNDETECTED";
        case SDSTATUS::IDLE:        return "IDLE";
        case SDSTATUS::READING:     return "READING";
        case SDSTATUS::WRITING:     return "WRITING";
        case SDSTATUS::ERROR:       return "ERROR";
        default:                    return "UNKNOWN";
    }
}


// CSV FILENAME GENERATION


void SDHandler::buildCSVFilename(float rho, DartType dartType, char* bufOutput, size_t bufSize) const {
    // Round air density to nearest 0.05 kg/m³
    float roundedRho = round(rho / 0.05f) * 0.05f;

    int intPart = (int)roundedRho;
    int decPart = round((roundedRho - intPart) * 100);

    // Select filename based on dart type
    // Note: dart types 1.5 and 2.0 currently map to same file (both are 2cc estimates)
    const char* filenamePattern = nullptr;

    switch (dartType) {
        case DartType::DART_1CC:
            // 1cc darts use "real2cc" table
            filenamePattern = "/Target Book G2 real2cc - rho-%d.%02d.csv";
            break;

        case DartType::DART_1_5CC:
            // 1.5cc darts estimated as 2.0cc
            filenamePattern = "/Target Book G2 practice1cc - rho-%d.%02d.csv";
            break;

        case DartType::DART_2CC:
            // 2cc darts use real tables
            filenamePattern = "/Target Book G2 real2cc - rho-%d.%02d.csv";
            break;

        default:
            Serial.printf("[SD] ERROR: Unknown dart type %d\n", static_cast<int>(dartType));
            snprintf(bufOutput, bufSize, "/UNKNOWN_rho-%d.%02d.csv", intPart, decPart);
            return;
    }

    snprintf(bufOutput, bufSize, filenamePattern, intPart, decPart);
    Serial.printf("[SD] Generated filename: %s\n", bufOutput);
}


// CSV READING & PARSING


bool SDHandler::csvRead(float rho, DartType dartType) {
    // Reset internal state for fresh parse
    resetTableData();

    // Build and attempt to open CSV file
    char csvFilename[96] = {0};
    buildCSVFilename(rho, dartType, csvFilename, sizeof(csvFilename));

    Serial.printf("[SD] Attempting to open: %s\n", csvFilename);
    File readFile = SD.open(csvFilename, FILE_READ);

    if (!readFile) {
        Serial.printf("[SD ERROR] File not found: %s\n", csvFilename);
        _status = SDSTATUS::ERROR;
        return false;
    }

    Serial.println("[SD] File opened, starting parse...");
    _status = SDSTATUS::READING;

    // ========== CSV PARSING LOGIC ==========
    // Format: comma-separated values, newline-separated rows
    // Row 0: column headers (gauge pressures)
    // Col 0: row headers (angles)
    // Data: distance values

    int bufIdx = 0;
    char buffer[16] = {0};
    char c;
    float parsedValue;
    bool parseError = false;

    while (readFile.available() && !parseError) {
        c = readFile.read();

        // Skip non-ASCII encoding (bytes > 127)
        if ((unsigned char)c > 127) {
            continue;
        }

        // Skip carriage returns (Windows line endings)
        if (c == '\r') {
            continue;
        }

        // Cell delimiter (comma) or row delimiter (newline)
        if (c == ',' || c == '\n') {
            if (bufIdx > 0) {
                buffer[bufIdx] = '\0';

                // Parse the cell value
                if (!parseFloatFromBuffer(buffer, parsedValue)) {
                    Serial.printf("[SD] Parse error at row %d, col %d: '%s'\n", 
                                  _numRows, _numCols, buffer);
                    parseError = true;
                    break;
                }

                // Store value in appropriate location
                if (_numRows == 0) {
                    // Header row - store column gauge values
                    if (_numCols >= MAX_COLS) {
                        Serial.printf("[SD] ERROR: Too many columns (%d > %d)\n", _numCols, MAX_COLS);
                        parseError = true;
                        break;
                    }
                    _colHeaders[_numCols++] = parsedValue;
                } else if (_numCols == 0) {
                    // First column - store row angle value
                    if (_numRows >= MAX_ROWS) {
                        Serial.printf("[SD] ERROR: Too many rows (%d > %d)\n", _numRows, MAX_ROWS);
                        parseError = true;
                        break;
                    }
                    _rowHeaders[_numRows] = parsedValue;
                    _numCols++;
                } else {
                    // Data cell
                    _distTable[_numRows][_numCols++] = parsedValue;
                }

                bufIdx = 0;
                memset(buffer, 0, sizeof(buffer));
            }

            // End of row
            if (c == '\n') {
                if (_numCols > _maxCols) {
                    _maxCols = _numCols;
                }
                _numRows++;
                _numCols = 0;
            }
        } else {
            // Regular character - add to buffer
            if (bufIdx < (int)sizeof(buffer) - 1) {
                buffer[bufIdx++] = c;
            }
        }
    }

    // Flush last cell if file doesn't end with newline
    if (bufIdx > 0 && !parseError) {
        buffer[bufIdx] = '\0';
        if (parseFloatFromBuffer(buffer, parsedValue)) {
            if (_numRows == 0) {
                _colHeaders[_numCols] = parsedValue;
            } else if (_numCols == 0) {
                _rowHeaders[_numRows] = parsedValue;
            } else {
                _distTable[_numRows][_numCols] = parsedValue;
            }
        }
    }

    readFile.close();

    if (parseError) {
        Serial.println("[SD] Parse error encountered!");
        _status = SDSTATUS::ERROR;
        return false;
    }

    if (!hasValidData()) {
        Serial.println("[SD] ERROR: No valid data parsed from CSV");
        _status = SDSTATUS::ERROR;
        return false;
    }

    _status = SDSTATUS::IDLE;
    printTableStats();
    return true;
}


// BALLISTICS LOOKUP


int SDHandler::gaugesPossible(float angle, float* outGauges, float* outDistances, int maxResults) {
    // Validate inputs
    if (!outGauges || !outDistances || maxResults < 1) {
        Serial.println("[SD] ERROR: Invalid parameters to gaugesPossible()");
        return 0;
    }

    if (!hasValidData()) {
        Serial.println("[SD] ERROR: No CSV data loaded!");
        return 0;
    }

    // Find the row with the closest matching angle
    int bestRow = 1;  // Start at row 1 (skip header row 0)
    float bestDifference = fabsf(_rowHeaders[1] - angle);

    for (int i = 2; i < _numRows; i++) {
        float difference = fabsf(_rowHeaders[i] - angle);
        if (difference < bestDifference) {
            bestDifference = difference;
            bestRow = i;
        }
    }

    Serial.printf("[SD] Found best row %d for angle %.2f° (delta=%.2f°)\n",
                  bestRow, _rowHeaders[bestRow], bestDifference);

    // Extract all valid gauge/distance pairs from this row
    int count = 0;
    for (int j = 0; j < _maxCols && count < maxResults; j++) {
        float dist = _distTable[bestRow][j];
        float gauge = _colHeaders[j];

        // Skip invalid/empty cells (distance <= 0)
        if (dist <= 0.0f) {
            continue;
        }

        outGauges[count] = gauge;
        outDistances[count] = dist;
        count++;

        Serial.printf("[SD]   [%d] Gauge=%.2f MPa, Distance=%.2f Ft\n", 
                      j, gauge, dist);
    }

    Serial.printf("[SD] Found %d valid gauge/distance pairs\n", count);
    return count;
}


// HELPER METHODS


bool SDHandler::parseFloatFromBuffer(const char* buffer, float& outValue) const {
    // Use sscanf to parse float - stops at non-numeric characters
    // E.g., "30ft" -> 30.0, "25.5" -> 25.5, "invalid" -> 0.0
    int result = sscanf(buffer, "%f", &outValue);
    return result == 1;
}

void SDHandler::resetTableData() {
    _numRows = 0;
    _numCols = 0;
    _maxCols = 0;
    memset(_colHeaders, 0, sizeof(_colHeaders));
    memset(_rowHeaders, 0, sizeof(_rowHeaders));
    memset(_distTable, 0, sizeof(_distTable));
}

bool SDHandler::hasValidData() const {
    return (_numRows > 1) && (_maxCols > 1);
}

void SDHandler::printTableStats() const {
    if (!hasValidData()) {
        Serial.println("[SD] No valid table data to display");
        return;
    }

    Serial.printf("[SD] Parsed table: %d rows × %d cols\n", _numRows, _maxCols);

    Serial.print("[SD] Gauges (MPa): ");
    for (int i = 0; i < _maxCols && i < 10; i++) {
        Serial.printf("%.1f ", _colHeaders[i]);
    }
    if (_maxCols > 10) Serial.print("...");
    Serial.println();

    Serial.print("[SD] Angles (deg): ");
    for (int i = 1; i < _numRows && i < 10; i++) {
        Serial.printf("%.1f ", _rowHeaders[i]);
    }
    if (_numRows > 10) Serial.print("...");
    Serial.println();
}