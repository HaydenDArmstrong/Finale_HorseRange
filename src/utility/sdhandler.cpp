#include "sdhandler.hpp"

#define SD_SPI_SCK_PIN  14
#define SD_SPI_MISO_PIN 13
#define SD_SPI_MOSI_PIN 12
#define SD_SPI_CS_PIN   4


void SDHandler::initSDCard(){

    if(!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        Serial.println("SD CARD DETECTED");
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

}

char* SDHandler::getSDStatusStr() {
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