#pragma once
#include "sensors/imu_sensor.hpp"


// ENVIRONMENTAL SNAPSHOT


struct EnvironmentalSnapshot {
    // Sensor readings
    float temperature;         // °C
    float pressure;            // Pa
    float altitude;            // m
    float airDensity;          // kg/m³
    
    // Quality metrics
    uint8_t sensorHealth;      // 0-100%
    float tempStability;       // variance of last N readings
    float pressureStability;   // variance of last N readings
    uint32_t captureTime;      // milliseconds since boot
};


// ENVIRONMENTAL MONITOR


class EnvironmentalMonitor {
private:
    static constexpr int HISTORY_SIZE = 10;
    static constexpr int HEALTH_THRESHOLD_TEMP = 2.0f;     // °C
    static constexpr int HEALTH_THRESHOLD_PRESSURE = 100;  // Pa
    
    // Rolling buffers for stability calculation
    float tempHistory[HISTORY_SIZE] = {0};
    float pressureHistory[HISTORY_SIZE] = {0};
    int historyIndex = 0;
    
    // Statistics
    float lastTemp = 0.0f;
    float lastPressure = 0.0f;
    uint32_t updateCount = 0;
    
public:
    
    // Capture current environmental state
    EnvironmentalSnapshot captureSnapshot(IMUSensor& imu) {
        EnvironmentalSnapshot snap = {0};
        
        Baro baro = imu.getBaro();
        snap.temperature = baro.cTemp;
        snap.pressure = baro.pressure;
        snap.altitude = baro.altitude;
        snap.airDensity = imu.airDensityCalc();
        snap.captureTime = millis();
        
        // Update history buffers
        tempHistory[historyIndex] = snap.temperature;
        pressureHistory[historyIndex] = snap.pressure;
        historyIndex = (historyIndex + 1) % HISTORY_SIZE;
        updateCount++;
        
        // Calculate stability metrics
        snap.tempStability = calculateStdDev(tempHistory, HISTORY_SIZE);
        snap.pressureStability = calculateStdDev(pressureHistory, HISTORY_SIZE);
        
        // Calculate sensor health score
        snap.sensorHealth = calculateHealthScore(snap);
        
        return snap;
    }
    
    // Calculate standard deviation of array
    static float calculateStdDev(const float* data, int size) {
        if (size <= 1) return 0.0f;
        
        // Calculate mean
        float sum = 0.0f;
        for (int i = 0; i < size; i++) {
            sum += data[i];
        }
        float mean = sum / size;
        
        // Calculate variance
        float variance = 0.0f;
        for (int i = 0; i < size; i++) {
            float diff = data[i] - mean;
            variance += diff * diff;
        }
        variance /= size;
        
        return sqrtf(variance);
    }
    
    // Calculate overall sensor health (0-100%)
    uint8_t calculateHealthScore(const EnvironmentalSnapshot& snap) {
        uint8_t score = 100;
        
        // Penalize for temperature variance
        if (snap.tempStability > HEALTH_THRESHOLD_TEMP) {
            score -= (snap.tempStability / HEALTH_THRESHOLD_TEMP) * 20;
        }
        
        // Penalize for pressure variance
        if (snap.pressureStability > HEALTH_THRESHOLD_PRESSURE) {
            score -= (snap.pressureStability / HEALTH_THRESHOLD_PRESSURE) * 20;
        }
        
        // Penalize if readings are unrealistic
        if (snap.airDensity < 0.8f || snap.airDensity > 1.5f) {
            score -= 15;  // Out of normal range
        }
        
        // Ensure score stays 0-100
        if (score < 0) score = 0;
        if (score > 100) score = 100;
        
        return score;
    }
    
    // Get temperature trend (rising/falling/stable)
    const char* getTempTrend(const EnvironmentalSnapshot& prev, 
                             const EnvironmentalSnapshot& curr) {
        float diff = curr.temperature - prev.temperature;
        
        if (fabsf(diff) < 0.3f) return "STABLE";
        return (diff > 0) ? "RISING" : "FALLING";
    }
    
    // Estimate environmental impact on ballistics (as percentage)
    float estimateEnvironmentalImpact(float seaLevelDensity = 1.225f) {
        if (seaLevelDensity <= 0) return 0.0f;
        
        float lastSnap = pressureHistory[(historyIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE];
        return ((1.225f - lastSnap / 101325.0f) / 1.225f) * 100.0f;
    }
    
    uint32_t getUpdateCount() const { return updateCount; }
};


// SIGNAL QUALITY MONITOR (BLE)


struct SignalQuality {
    int8_t rssi;               // dBm (-30 to -100)
    uint8_t qualityPercent;    // 0-100%
    uint32_t lastUpdateMs;     // last successful read
    bool isStale;              // older than threshold
};

class SignalQualityMonitor {
private:
    static constexpr int RSSI_THRESHOLD_MS = 5000;  // stale after 5s
    static constexpr int8_t RSSI_EXCELLENT = -50;   // dBm
    static constexpr int8_t RSSI_POOR = -100;       // dBm
    
    int8_t lastRSSI = -100;
    uint32_t lastUpdateMs = 0;
    
public:
    
    // Update RSSI reading (call this from BLE code)
    void updateRSSI(int8_t rssiValue) {
        lastRSSI = rssiValue;
        lastUpdateMs = millis();
       // Serial.printf("[SIGNAL] RSSI: %d dBm\n", rssiValue);
    }
    
    // Get current signal quality
    SignalQuality getQuality() {
        SignalQuality sq = {0};
        sq.rssi = lastRSSI;
        sq.lastUpdateMs = lastUpdateMs;
        sq.isStale = (millis() - lastUpdateMs) > RSSI_THRESHOLD_MS;
        
        // Convert RSSI to percentage (0-100%)
        // -50 dBm = 100%, -100 dBm = 0%
        if (lastRSSI >= RSSI_EXCELLENT) {
            sq.qualityPercent = 100;
        } else if (lastRSSI <= RSSI_POOR) {
            sq.qualityPercent = 0;
        } else {
            sq.qualityPercent = (lastRSSI - RSSI_POOR) * 100 / (RSSI_EXCELLENT - RSSI_POOR);
        }
        
        return sq;
    }
    
    // Get signal quality as text
    const char* getQualityString() {
        SignalQuality sq = getQuality();
        
        if (sq.isStale) return "NO SIGNAL";
        if (sq.qualityPercent >= 80) return "EXCELLENT";
        if (sq.qualityPercent >= 60) return "GOOD";
        if (sq.qualityPercent >= 40) return "FAIR";
        if (sq.qualityPercent >= 20) return "WEAK";
        return "POOR";
    }
};