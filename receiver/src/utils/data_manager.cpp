#include "data_manager.h"
#include <Arduino_FreeRTOS.h>
#include "../config.h"

static SystemData currentState;
static SemaphoreHandle_t dataMutex;

void initDataManager() {
    dataMutex = xSemaphoreCreateBinary();
    if (dataMutex != NULL) {
        xSemaphoreGive(dataMutex);
    }
    memset(&currentState, 0, sizeof(SystemData));
    currentState.loraModuleConnected = false;
    currentState.dhtModuleConnected = false;
    currentState.lastRssi = -999;
    currentState.wifiConnected = false;
    currentState.mqttConnected = false;
    currentState.timeSynced = false;
}

void updateLocalData(float temp, float hum) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        currentState.localTemperature = temp;
        currentState.localHumidity = hum;
        xSemaphoreGive(dataMutex);
    }
}

void updateRemoteData(uint8_t sensorId, float temp, float hum) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        if (sensorId == CAFETERIA_ID) {
            Serial.println("[DataManager] Mise a jour CAFET -> Memoire Partagee");
            currentState.cafeteria.temperature = temp;
            currentState.cafeteria.humidity = hum;
            currentState.cafeteria.lastUpdate = millis();
        } else if (sensorId == FABLAB_ID) {
            Serial.println("[DataManager] Mise a jour FABLAB -> Memoire Partagee");
            currentState.fablab.temperature = temp;
            currentState.fablab.humidity = hum;
            currentState.fablab.lastUpdate = millis();
        } else {
            Serial.print("[DataManager] ID Inconnu ignore: ");
            Serial.println(sensorId);
        }
        xSemaphoreGive(dataMutex);
    }
}
void setLastRssi(int rssi) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        currentState.lastRssi = rssi;
        xSemaphoreGive(dataMutex);
    }
}

void setLoraStatus(bool isConnected) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        currentState.loraModuleConnected = isConnected;
        xSemaphoreGive(dataMutex);
    }
}

void setDhtStatus(bool isConnected) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        currentState.dhtModuleConnected = isConnected;
        xSemaphoreGive(dataMutex);
    }
}

void setWifiStatus(bool isConnected) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        currentState.wifiConnected = isConnected;
        xSemaphoreGive(dataMutex);
    }
}

void setMqttStatus(bool isConnected) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        currentState.mqttConnected = isConnected;
        xSemaphoreGive(dataMutex);
    }
}

void setTimeSyncStatus(bool isSynced) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        currentState.timeSynced = isSynced;
        xSemaphoreGive(dataMutex);
    }
}

SystemData getSystemData() {
    SystemData data;
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        data = currentState;
        xSemaphoreGive(dataMutex);
    }
    return data;
}