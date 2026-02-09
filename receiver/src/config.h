#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID "CESI_Recherche"
#define WIFI_PASS "#F2uV4R3*"

// MQTT Configuration
#define MQTT_SERVER "10.191.64.102"
#define MQTT_PORT 1883
#define MQTT_TOPIC_CAFET "cesi/cafet"
#define MQTT_TOPIC_FABLAB "cesi/fablab"

// LoRa Configuration (Pins pour Arduino R4 WiFi)
#define LORA_SS_PIN 10
#define LORA_RST_PIN 9
// Note: User uses D2 for buttons. Moving DIO0 definition to avoid conflict, 
// though check if physical wiring requires this pin.
#define LORA_DIO0_PIN 5 

// UI Configuration (Grove Buttons)
#define BUTTON_LEFT_PIN 2
#define BUTTON_RIGHT_PIN 3
#define MENU_TIMEOUT_MS 10000

// Blockchain/Security Configuration
#define LORA_SHARED_SECRET "IoT_Secure_P@ssw0rd_2026"
#define GENESIS_HASH "0000000000000000000000000000000000000000000000000000000000000000"

// Sensor Configuration
#define SENSOR_DHT_PIN 4
#define SENSOR_DHT_TYPE DHT22 // Change to DHT11 if needed

// Sensor Network IDs
#define FABLAB_ID 1    // Sender (Remote)
#define CAFETERIA_ID 2 // Receiver (Local)

#endif // CONFIG_H