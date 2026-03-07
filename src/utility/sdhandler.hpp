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

    SDSTATUS getSDStatus() { 
        return _status;
     } //getter returning current status
    char* getSDStatusStr();

private:
   SDSTATUS _status = SDSTATUS::UNDETECTED; //undetected by defualt

};