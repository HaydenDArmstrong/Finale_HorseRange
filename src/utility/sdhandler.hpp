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
    void csvRead(float rho, float dartType);

    SDSTATUS getSDStatus()
    {
        return _status;
    } // getter returning current status
    char *getSDStatusStr();

    float lookupDistance(float angle, float mass);

    float lookupGauge(float angle, float inputDistance);

private:
    void buildCSVFilename(float rho, float dartType,  char *bufOutput, size_t bufSize);
    SDSTATUS _status = SDSTATUS::UNDETECTED; // undetected by defualt
    //increased to 16 for extra space
    float _colHeaders[16] = {0};
    float _rowHeaders[16] = {0};
    float _distTable[16][16] = {0};
    int _numCols = 0;
    int _numRows = 0;
    int _maxCols = 0;
};