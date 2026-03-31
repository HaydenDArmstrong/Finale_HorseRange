#pragma once
#include <SD.h>
#include <SPI.h>

enum class SDSTATUS
{
    UNDETECTED,
    IDLE,
    READING,
    WRITING,
    ERROR
};

class SDHandler
{
public:
    void initSDCard();
    void update();
    void csvRead(float rho);

    SDSTATUS getSDStatus()
    {
        return _status;
    } // getter returning current status
    char *getSDStatusStr();

    float lookupDistance(float angle, float mass);

private:
    void buildCSVFilename(float rho, char *bufOutput, size_t bufSize);
    SDSTATUS _status = SDSTATUS::UNDETECTED; // undetected by defualt
    float _colHeaders[10] = {0};
    float _rowHeaders[10] = {0};
    float _distTable[10][10] = {0};
    int _numCols = 0;
    int _numRows = 0;
    int _maxCols = 0;
};