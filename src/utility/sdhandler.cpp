#include "sdhandler.hpp"
#include <string.h>
#include <M5Unified.h>

#define SD_SPI_SCK_PIN 14
#define SD_SPI_MISO_PIN 13
#define SD_SPI_MOSI_PIN 12
#define SD_SPI_CS_PIN 4

void SDHandler::initSDCard()
{

    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN); // init SPI with custom pins before SD

    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000))
    {
        Serial.println("SD CARD NOT DETECTED"); // fixed: was printing DETECTED on failure
        // M5.Display.print("SD CARD NOT DETECTED");
        _status = SDSTATUS::UNDETECTED;
        while (1)
            ;
    }
    else
    {
        Serial.println("SD CARD DETECTED");
        // M5.Display.print("SD CARD DETECTED");
        _status = SDSTATUS::IDLE;
    }

    Serial.println("Write Test");
    auto writeFile = SD.open("/WriteTest.txt", FILE_WRITE, true);
    if (writeFile)
    {
        writeFile.print("Hello... SD CARD WRITE SUCCESS!\n");
        writeFile.close();
        Serial.println("Write Success");
        // technically true _status = SDSTATUS::IDLE;
        _status = SDSTATUS::WRITING;
    }
    else
    {
        Serial.println("write Fail");
        _status = SDSTATUS::ERROR;
    }
    delay(100);

    Serial.println("Read Test");
    auto readFile = SD.open("/WriteTest.txt", FILE_READ, false);
    if (readFile)
    {
        Serial.println("READ file Detected");
        readFile.close();
        // _status = SDSTATUS::IDLE;
        _status = SDSTATUS::READING;
    }
    else
    {
        Serial.println("READ file NOT Detected");
        _status = SDSTATUS::ERROR;
    }

    _status = SDSTATUS::IDLE; // init complete, return to idle regardless of test results
}

char *SDHandler::getSDStatusStr()
{                    // const: string literals are read-only
    switch (_status) // depending on current enum
    {
    case SDSTATUS::UNDETECTED:
        return "UNDETECTED";
    case SDSTATUS::IDLE:
        return "IDLE";
    case SDSTATUS::READING:
        return "READING";
    case SDSTATUS::WRITING:
        return "WRITING";
    case SDSTATUS::ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

void SDHandler::buildCSVFilename(float rho, float dartType, char *bufOutput, size_t bufSize)
{
    Serial.printf("Building FILENAME\n");
    float roundedVal = round(rho / 0.05f) * 0.05f; // rounds to 0.05

    int intPart = (int)roundedVal;
    int decPart = round((roundedVal - intPart) * 100);

    //Based off of google sheet given filenames

    if (dartType == 0.5)
    { //IF 0.5CC dart is used. For now we just go from 1.0 estimate
        snprintf(bufOutput, bufSize, "/Target Book G2 practice 1cc - rho-%d.%02d.csv", intPart, decPart);
    }
    if (dartType == 1.5)
    { //IF 0.5CC dart is used. For now we just go for 2.0 estimate
        snprintf(bufOutput, bufSize, "/Target Book G2 practice 1cc - rho-%d.%02d.csv", intPart, decPart);  
    }

    if (dartType == 1.0)
    { //TODO: Repleace with real instead of practice when propre sheet is developed.
    snprintf(bufOutput, bufSize, "/Target Book G2 real2cc - rho-%d.%02d.csv", intPart, decPart);
    }
    if (dartType == 2.0)
    {
      snprintf(bufOutput, bufSize, "/Target Book G2 real2cc - rho-%d.%02d.csv", intPart, decPart);  
    }
}

void SDHandler::csvRead(float rho, float dartType)
{
    char airFileName[64] = {0};
    buildCSVFilename(rho, dartType, airFileName, sizeof(airFileName));
    Serial.println(airFileName);

    // reset state so re-calling with new rho starts clean
    _numRows = 0;
    _numCols = 0;
    _maxCols = 0;
    memset(_colHeaders, 0, sizeof(_colHeaders));
    memset(_rowHeaders, 0, sizeof(_rowHeaders));
    memset(_distTable, 0, sizeof(_distTable));

    auto readFile = SD.open(airFileName, FILE_READ, false);

    if (!readFile)
    {
        Serial.println("READ file NOT Detected");
        M5.Display.printf("FILE NOT FOUND");
        _status = SDSTATUS::ERROR;
        return;
    }

    int bufIdx = 0;
    char buff[16] = {0};
    char c;
    float val;

    while (readFile.available())
    {
        c = readFile.read();

        // skip non-ASCII encoding garbage (Â°, Ï etc.)
        // these are bytes > 127, plain text is always <= 127
        if ((unsigned char)c > 127)
            continue;

        // skip carriage return (windows line endings produce \r\n)
        if (c == '\r')
            continue;

        if (c == ',' || c == '\n')
        {
            // only store if buffer has something — empty cell means skip
            if (bufIdx > 0)
            {
                buff[bufIdx] = '\0';      // NEED Null terminate here
                sscanf(buff, "%f", &val); // sscanf stops at letters, so "30ft" → 30.0

                if (_numRows == 0)
                {
                    _colHeaders[_numCols++] = val;
                }
                else if (_numCols == 0)
                {
                    _rowHeaders[_numRows] = val;
                    _numCols++;
                }
                else
                {
                    _distTable[_numRows][_numCols++] = val;
                }

                bufIdx = 0;
                memset(buff, 0, sizeof(buff));
            }

            if (c == '\n')
            {
                if (_numCols > _maxCols)
                    _maxCols = _numCols;

                Serial.printf("  EOL: _numCols=%d _maxCols=%d\n", _numCols, _maxCols);

                _numRows++;
                _numCols = 0;
            }
        }
        else
        {
            if (bufIdx < 15)
                buff[bufIdx++] = c;
        }
    }

    // flush last cell — file may not end with \n
    if (bufIdx > 0)
    {
        sscanf(buff, "%f", &val);
        if (_numRows == 0)
        {
            _colHeaders[_numCols] = val;
        }
        else if (_numCols == 0)
        {
            _rowHeaders[_numRows] = val;
        }
        else
        {
            _distTable[_numRows][_numCols] = val;
        }
    }
    float currentVal;
    int j = 0;
    // debug: confirm parse results
    Serial.printf("Parsed %d rows, %d cols\n", _numRows, _maxCols);
    for (int i = 0; i < _maxCols; i++)
        Serial.printf("  colHeader[%d] = %.2f\n", i, _colHeaders[i]);
    for (int i = 0; i < _numRows; i++)
        Serial.printf("  rowHeader[%d] = %.2f\n", i, _rowHeaders[i]);
    for (int i = 0; i < _numRows; i++)
    {
        for (j = 0; j < _maxCols; j++)
        {
            currentVal = _distTable[i][j];
            Serial.printf("[%d,%d]: %.2f", i, j, currentVal);
        }
        Serial.println();
    }

    readFile.close();
    _status = SDSTATUS::READING;
}

// float SDHandler::lookupGauge(float angle, float inputDistance)
// {
//     // 1. Find best row matching angle
//     int bestRow = 0;
//     float bestDifference = fabs(_rowHeaders[0] - angle);
//     for (int i = 1; i < _numRows; i++)
//     {
//         float difference = fabs(_rowHeaders[i] - angle);
//         if (difference < bestDifference)
//         {
//             bestDifference = difference;
//             bestRow = i;
//         }
//     }

//     // 2. Search table values in that row for closest distance
//     int bestCol = 1;
//     bestDifference = fabs(_distTable[bestRow][1] - inputDistance);
//     for (int j = 2; j < _maxCols; j++)
//     {
//         float diff = fabs(_distTable[bestRow][j] - inputDistance);
//         if (diff < bestDifference)
//         {
//             bestDifference = diff;
//             bestCol = j;
//         }
//     }

//     Serial.printf("Bestcol: %d, column header: %d, bestRow %d", bestCol, _colHeaders[bestCol-1], bestRow);

//     // 3. Return the column header (gauge) for that column
//     return;// -1 because colHeaders has no corner offset
// }
// once we store as a 2d array, we can know perform lookup
int SDHandler::gaugesPossible(float angle, float* outGauges, float* outDistances, int maxResults)
{
    Serial.printf("LOOKUp\n");
    int bestRow = 0;
    float bestDifference = fabs(_rowHeaders[0] - angle);

    for (int i = 1; i < _numRows; i++)
    {
        float difference = fabs(_rowHeaders[i] - angle);
        if (difference < bestDifference)
        {
            bestDifference = difference;
            bestRow = i;
        }
    }

    int count = 0;
    for (int j = 0; j < _maxCols && count < maxResults; j++)
    {
        int physicalCol = j + 1;
        float dist = _distTable[bestRow][physicalCol];

        if (dist <= 0.0f) continue; // skip empty cells

        outGauges[count]    = _colHeaders[j];
        outDistances[count] = dist;

        Serial.printf("  Gauge: %.2f -> Distance: %.2f\n", outGauges[count], outDistances[count]);
        count++;
    }

    return count;
}