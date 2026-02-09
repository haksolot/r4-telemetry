#include "wifi_task.h"
#include <WiFiS3.h>
#include <Arduino_FreeRTOS.h>
#include <PubSubClient.h>
#include "../config.h"
#include "../utils/data_manager.h"
#include "../utils/time_manager.h"

// ---------------------- MQTT Setup ----------------------
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ---------------------- Task ----------------------
void wifiTask(void *pvParameters) {
    // Configuration du serveur MQTT
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setBufferSize(512); // Augmentation du buffer pour supporter les JSON longs

    for (;;) {
        // --- 1. GESTION WIFI ---
        if (WiFi.status() != WL_CONNECTED) {
            setWifiStatus(false);
            setMqttStatus(false); // Si pas de WiFi, MQTT est forcément off
            
            Serial.println("[WiFi] Connexion perdue ou non établie. Tentative...");
            WiFi.begin(WIFI_SSID, WIFI_PASS);

            int tryCount = 0;
            // On attend max 10 secondes (20 * 500ms)
            while (WiFi.status() != WL_CONNECTED && tryCount < 20) {
                vTaskDelay(500 / portTICK_PERIOD_MS);
                Serial.print(".");
                tryCount++;
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("\n[WiFi] Connecté ! IP: " + WiFi.localIP().toString());
                setWifiStatus(true);
                
                // Synchro NTP au réveil du WiFi (Première tentative)
                if (syncTimeWithNTP()) {
                    setTimeSyncStatus(true);
                }
            } else {
                 Serial.println("\n[WiFi] Échec. Prochaine tentative dans 5s...");
                 vTaskDelay(5000 / portTICK_PERIOD_MS);
                 continue; // On recommence la boucle pour re-tester le WiFi
            }
        } else {
            setWifiStatus(true); // WiFi est OK
            
            // Si l'heure n'est pas encore synchronisée, on réessaie périodiquement
            SystemData data = getSystemData();
            if (!data.timeSynced) {
                static unsigned long lastNtpTry = 0;
                if (millis() - lastNtpTry > 60000) { // Retry toutes les 60s
                    lastNtpTry = millis();
                    Serial.println("[WiFi] Retry NTP Sync...");
                    if (syncTimeWithNTP()) {
                        setTimeSyncStatus(true);
                    }
                }
            }
        }

        // --- 2. GESTION MQTT ---
        if (WiFi.status() == WL_CONNECTED) {
            if (!mqttClient.connected()) {
                setMqttStatus(false);
                Serial.println("[MQTT] Connexion au broker...");
                
                if (mqttClient.connect("ArduinoPasserelle")) {
                    Serial.println("[MQTT] Connecté !");
                    setMqttStatus(true);
                } else {
                    Serial.print("[MQTT] Échec, rc=");
                    Serial.println(mqttClient.state());
                    // Délai avant retry MQTT
                    vTaskDelay(5000 / portTICK_PERIOD_MS);
                }
            }
            
            if (mqttClient.connected()) {
                setMqttStatus(true);
                mqttClient.loop();

                // --- 3. PUBLICATION ---
                SystemData data = getSystemData();

                // Publication Locale (Cafeteria)
                if (!isnan(data.localTemperature)) {
                    String payload = "{";
                    payload += "\"source\":\"cafeteria\",";
                    payload += "\"temperature\":" + String(data.localTemperature) + ",";
                    payload += "\"humidity\":" + String(data.localHumidity) + ",";
                    payload += "\"loraStatus\":" + String(data.loraModuleConnected ? "true" : "false") + ",";
                    payload += "\"wifiStatus\":true,";
                    payload += "\"mqttStatus\":true,";
                    payload += "\"loraRssi\":" + String(data.lastRssi);
                    payload += "}";
                    
                    if (mqttClient.publish(MQTT_TOPIC_CAFET, payload.c_str())) {
                         // Publication réussie
                    } else {
                         Serial.println("[MQTT] Erreur lors de la publication Cafet");
                    }
                }

                // Publication Distante (Fablab)
                if (!isnan(data.fablab.temperature)) {
                    String payload = "{";
                    payload += "\"source\":\"fablab\",";
                    payload += "\"temperature\":" + String(data.fablab.temperature) + ",";
                    payload += "\"humidity\":" + String(data.fablab.humidity) + ",";
                    payload += "\"lastUpdate\":" + String(millis() - data.fablab.lastUpdate) + ",";
                    payload += "\"loraRssi\":" + String(data.lastRssi);
                    payload += "}";
                    
                    mqttClient.publish(MQTT_TOPIC_FABLAB, payload.c_str());
                }
            }
        }

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
