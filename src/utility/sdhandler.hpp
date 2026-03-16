#pragma once
#include <SD.h>
#include <SPI.h>

enum class SDSTATUS {
    UNDETECTED,
    IDLE,
    READING,
    WRITING,
    ERROR
};

class SDHandler {
public:
    void initSDCard();
    void update();
    void csvRead(float rho);

    SDSTATUS getSDStatus() { 
        return _status;
     } //getter returning current status
    char* getSDStatusStr();

private:
    void buildCSVFilename(float rho, char* bufOutput, size_t bufSize);
   SDSTATUS _status = SDSTATUS::UNDETECTED; //undetected by defualt

};