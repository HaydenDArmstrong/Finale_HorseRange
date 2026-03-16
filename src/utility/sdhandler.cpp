#include "sdhandler.hpp"

#define SD_SPI_SCK_PIN  14
#define SD_SPI_MISO_PIN 13
#define SD_SPI_MOSI_PIN 12
#define SD_SPI_CS_PIN   4


void SDHandler::initSDCard(){

    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN); //init SPI with custom pins before SD

    if(!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        Serial.println("SD CARD NOT DETECTED"); //fixed: was printing DETECTED on failure
        //M5.Display.print("SD CARD NOT DETECTED");
        _status = SDSTATUS::UNDETECTED;
        while (1);
    } else {
        Serial.println("SD CARD DETECTED");
       // M5.Display.print("SD CARD DETECTED");
       _status = SDSTATUS::IDLE;
    }

    Serial.println("Write Test");
    auto writeFile = SD.open("/WriteTest.txt", FILE_WRITE, true);
    if (writeFile) {
        writeFile.print("Hello... SD CARD WRITE SUCCESS!\n");
        writeFile.close();
        Serial.println("Write Success");
       //technically true _status = SDSTATUS::IDLE;
       _status = SDSTATUS::WRITING;
    } else {
        Serial.println("write Fail");
        _status = SDSTATUS::ERROR;
    }
    delay(100);

    Serial.println("Read Test");
    auto readFile = SD.open("/WriteTest.txt", FILE_READ, false);
    if (readFile) {
        Serial.println("READ file Detected");
        readFile.close();
       // _status = SDSTATUS::IDLE;
       _status = SDSTATUS::READING;
    } else {
        Serial.println("READ file NOT Detected");
        _status = SDSTATUS::ERROR;
    }

    _status = SDSTATUS::IDLE; //init complete, return to idle regardless of test results

}

char* SDHandler::getSDStatusStr() { //const: string literals are read-only
    switch (_status) //depending on current enum
    {
    case SDSTATUS::UNDETECTED: return "UNDETECTED";
    case SDSTATUS::IDLE:       return "IDLE";
    case SDSTATUS::READING:    return "READING";
    case SDSTATUS::WRITING:    return "WRITING";
    case SDSTATUS::ERROR:      return "ERROR";
    default:                   return "UNKNOWN";
    }
}

void SDHandler::buildCSVFilename(float rho, char* bufOutput, size_t bufSize) {
    float roundedVal = round(rho / 0.05f) * 0.05f; //rounds to 0.05

    int intPart = (int)roundedVal;
    int decPart = round((roundedVal - intPart) * 100);

    snprintf(bufOutput, bufSize, "/%d-%02d.csv", intPart, decPart);
} 
void SDHandler::csvRead(float rho) {
    char airFileName[16] = {0}; //filename of air density
    buildCSVFilename(rho, airFileName, sizeof(airFileName));

    Serial.println(airFileName); //debug

    Serial.println("Read");
    auto readFile = SD.open(airFileName, FILE_READ, false);
    //TODO: Current CSV file determined by the rho value (rho-rhodecimal.txt)
    //open current file
    int rowT = 0; //row tracker
    int colT = 0; //column tracker
    int bufIdx = 0; //buffer tracker

    char c;
    float val;

    float colHeaders[10];
    float rowHeaders[10];
    float distTable[10][10];

    if (readFile) {
        char buff[16] = {0}; //wont have a string bigger than 16 char
        while (readFile.available()) {
            c = readFile.read();
            if (c == ',' || c == '\n') { //at every comma (cell end) or newline (row end)
                sscanf(buff, "%f", &val); // parse the data into float

                if (rowT == 0) { //first column
                    colHeaders[colT] = val;
                } else if (colT == 0) { //first row
                    rowHeaders[rowT] = val;
                } else {
                    distTable[rowT][colT] = val; //if not in first colum first row, normal data
                }

                bufIdx = 0;
                buff[0] = '\0';

                if (c == ',') { //specific to cells
                    colT++; //increase column
                    if (colT >= 10) { colT = 9; } //bounds: clamp so we dont overflow distTable
                } else {  // '\n' new row
                    rowT++; //increase row count
                    colT = 0; //reset column count
                    if (rowT >= 10) { rowT = 9; } //bounds: clamp so we dont overflow distTable
                }
            } else {
                if (bufIdx < 15) buff[bufIdx++] = c; //bounds: guard against cell longer than 15 chars
            }
        }
        readFile.close();

        //flush: last cell wont have a trailing delimiter so process whatever is left in buff
        if (bufIdx > 0) {
            sscanf(buff, "%f", &val);
            if (rowT == 0) {
                colHeaders[colT] = val;
            } else if (colT == 0) {
                rowHeaders[rowT] = val;
            } else {
                distTable[rowT][colT] = val;
            }
        }

       // _status = SDSTATUS::IDLE;
       _status = SDSTATUS::READING;
    } else {
        Serial.println("READ file NOT Detected");
        _status = SDSTATUS::ERROR;
    }

}