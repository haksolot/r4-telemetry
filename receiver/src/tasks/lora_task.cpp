#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <RTC.h> // Required for Time Sync
#include "../config.h"
#include "../utils/data_manager.h"

// --- CONFIGURATION ---
const int LED_PIN = 13;
const int LORA_BAUD_RATE = 9600;

// --- SHA256 / HMAC IMPLEMENTATION (Minimal) ---
struct SHA256_CTX {
    uint8_t data[64];
    uint32_t datalen;
    unsigned long long bitlen;
    uint32_t state[8];
};

#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))

static const uint32_t k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static void sha256_transform(SHA256_CTX *ctx, const uint8_t data[]) {
    uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];
    for (i = 0, j = 0; i < 16; ++i, j += 4) 
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    for (; i < 64; ++i)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + k[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d; d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(SHA256_CTX *ctx) {
    ctx->datalen = 0; ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85; ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c; ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
}

static void sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len) {
    for (size_t i = 0; i < len; ++i) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(SHA256_CTX *ctx, uint8_t hash[]) {
    uint32_t i = ctx->datalen;
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) ctx->data[i++] = 0x00;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += ctx->datalen * 8;
    for(int k=0; k<8; k++) ctx->data[63-k] = (uint8_t)(ctx->bitlen >> (k*8)); // Big Endian
    sha256_transform(ctx, ctx->data);
    for (i = 0; i < 4; ++i) {
        hash[i] = (ctx->state[0] >> (24 - i * 8)) & 0xff;
        hash[i+4] = (ctx->state[1] >> (24 - i * 8)) & 0xff;
        hash[i+8] = (ctx->state[2] >> (24 - i * 8)) & 0xff;
        hash[i+12] = (ctx->state[3] >> (24 - i * 8)) & 0xff;
        hash[i+16] = (ctx->state[4] >> (24 - i * 8)) & 0xff;
        hash[i+20] = (ctx->state[5] >> (24 - i * 8)) & 0xff;
        hash[i+24] = (ctx->state[6] >> (24 - i * 8)) & 0xff;
        hash[i+28] = (ctx->state[7] >> (24 - i * 8)) & 0xff;
    }
}

static void hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len, uint8_t *output) {
    uint8_t k_ipad[64], k_opad[64], tk[32];
    if (key_len > 64) {
        SHA256_CTX ctx; sha256_init(&ctx); sha256_update(&ctx, key, key_len); sha256_final(&ctx, tk);
        key = tk; key_len = 32;
    }
    memset(k_ipad, 0x36, 64); memset(k_opad, 0x5c, 64);
    for (size_t i = 0; i < key_len; i++) { k_ipad[i] ^= key[i]; k_opad[i] ^= key[i]; }
    
    SHA256_CTX ctx;
    uint8_t innerHash[32];
    sha256_init(&ctx); sha256_update(&ctx, k_ipad, 64); sha256_update(&ctx, data, data_len); sha256_final(&ctx, innerHash);
    sha256_init(&ctx); sha256_update(&ctx, k_opad, 64); sha256_update(&ctx, innerHash, 32); sha256_final(&ctx, output);
}

// --- UTILS ---

static String byteToHexString(uint8_t b) {
    String s = String(b, HEX);
    if (s.length() < 2) s = "0" + s;
    return s;
}

static String hashToString(const uint8_t *hash) {
    String s = "";
    for(int i = 0; i < 32; i++) s += byteToHexString(hash[i]);
    return s;
}

static void hexStringToBytes(const String &hexString, uint8_t *bytes, size_t len) {
    for (size_t i = 0; i < len; i++) {
        String byteStr = hexString.substring(i*2, i*2+2);
        char byteChar[3] = {byteStr[0], byteStr[1], '\0'};
        bytes[i] = (uint8_t)strtol(byteChar, NULL, 16);
    }
}

static void sendAT(String cmd) {
    Serial1.print(cmd + "\r\n");
}

static String readResponse(uint16_t timeout = 500) {
    String s = "";
    uint32_t t0 = millis();
    while (millis() - t0 < timeout) {
        while (Serial1.available()) {
            s += (char)Serial1.read();
        }
        delay(1); 
    }
    return s;
}

static bool initLoRaModule() {
    Serial.println("[LoRa] Initialisation du module...");
    Serial1.begin(LORA_BAUD_RATE);
    
    // 1. Test de présence (AT)
    // On essaie plusieurs fois
    for (int i = 0; i < 3; i++) {
        // Vider le buffer
        while(Serial1.available()) Serial1.read();
        
        sendAT("AT");
        String resp = readResponse(1000);
        
        if (resp.indexOf("OK") != -1) {
            Serial.println("[LoRa] Module détecté !");
            
            // 2. Configuration
            sendAT("AT+MODE=TEST"); delay(100); readResponse();
            sendAT("AT+TEST=RFCFG,868,SF7,125,12,15,14"); delay(100); readResponse();
            
            // 3. Passage en mode réception
            sendAT("AT+TEST=RXLRPKT");
            delay(200);
            readResponse(); // Flush confirm

            return true;
        }
        Serial.println("[LoRa] Pas de réponse... tentative " + String(i+1));
        delay(500);
    }
    
    Serial.println("[LoRa] ERREUR CRITIQUE: Module LoRa non détecté.");
    return false;
}

static void sendAck(const uint8_t *hashToSend) {
// ... (reste des fonctions inchangées)
    String hashStr = hashToString(hashToSend);
    Serial.println("[LoRa] Sending ACK: " + hashStr);
    
    // Clear buffer
    while(Serial1.available()) Serial1.read();

    // Send ACK
    sendAT("AT+TEST=TXLRPKT,\"" + hashStr + "\"");
    
    // Wait for TX DONE
    uint32_t tStart = millis();
    while(millis() - tStart < 2000) {
        String line = Serial1.readStringUntil('\n');
        if (line.indexOf("TX DONE") != -1) break;
    }

    // Re-arm RX
    sendAT("AT+TEST=RXLRPKT");
}

static void processLoRaPacket(const String& dataLine, const String& rssiLine) {

    // 1. Parse RSSI from the second line

    if (rssiLine.length() > 0) {

        int firstComma = rssiLine.indexOf(',');

        String rssiStr;

        if (firstComma != -1) {

            rssiStr = rssiLine.substring(0, firstComma);

        } else {

            rssiStr = rssiLine;

        }

        rssiStr.trim();

        int rssi = rssiStr.toInt();



        if (rssi < 0) {

            Serial.print("[LoRa] RSSI = ");

            Serial.println(rssi);

            setLastRssi(rssi);

        } else {

             Serial.println("[LoRa] Warning: Parsed RSSI is not a negative value from line: " + rssiLine);

        }

    } else {

        Serial.println("[LoRa] Warning: Did not receive a second line for RSSI.");

    }



    // 2. Parse data from the first line

    int firstQuote = dataLine.indexOf('"');

    int lastQuote = dataLine.lastIndexOf('"');



    if (firstQuote != -1 && lastQuote > firstQuote) {

        String hexContent = dataLine.substring(firstQuote + 1, lastQuote);



        // Expect: 6 bytes Data + 32 bytes HMAC = 38 bytes => 76 hex chars

        if (hexContent.length() == 76) { 

            uint8_t payload[38];

            hexStringToBytes(hexContent, payload, 38);



            uint8_t dataPart[6];

            uint8_t receivedHash[32];

            

            memcpy(dataPart, payload, 6);

            memcpy(receivedHash, payload + 6, 32);



            // Verify HMAC

            uint8_t calculatedHash[32];

            hmac_sha256(

                (const uint8_t*)LORA_SHARED_SECRET, strlen(LORA_SHARED_SECRET),

                dataPart, 6, 

                calculatedHash

            );



            if (memcmp(receivedHash, calculatedHash, 32) == 0) {

                // Determine TYPE

                uint8_t msgType = dataPart[1];



                if (msgType == 2) { 

                    // --- TYPE 2: DATA REPORT ---

                    uint8_t sensorId = dataPart[0];

                    float tempVal = (int16_t)(dataPart[2] | (dataPart[3] << 8)) / 100.0;

                    float humVal = (int16_t)(dataPart[4] | (dataPart[5] << 8)) / 100.0;



                    Serial.println("\n========== [LoRa] PAQUET DATA RECU ==========");

                    Serial.print("  Source ID   : "); Serial.print(sensorId);

                    if (sensorId == CAFETERIA_ID)      Serial.println(" (CAFETERIA)");

                    else if (sensorId == FABLAB_ID)    Serial.println(" (FABLAB)");

                    else                               Serial.println(" (INCONNU)");

                    Serial.print("  Temperature : "); Serial.print(tempVal); Serial.println(" C");

                    Serial.print("  Humidite    : "); Serial.print(humVal); Serial.println(" %");

                    Serial.println("=============================================");



                    updateRemoteData(sensorId, tempVal, humVal);

                    setLoraStatus(true);

                    

                    digitalWrite(LED_PIN, HIGH);

                    delay(50);

                    digitalWrite(LED_PIN, LOW);



                    // Send Standard ACK (Hash)

                    sendAck(receivedHash);



                } else if (msgType == 3) {

                    // --- TYPE 3: TIME REQUEST ---

                    Serial.println("\n[LoRa] TIME SYNC REQUEST RECEIVED.");

                    

                    // Get Current Time

                    RTCTime current;

                    RTC.getTime(current);

                    uint32_t now = current.getUnixTime();

                    Serial.print("  Current Unix Time: "); Serial.println(now);



                    // Prepare Response Packet (Type 4)

                    uint8_t respPayload[6];

                    respPayload[0] = dataPart[0];

                    respPayload[1] = 4;

                    respPayload[2] = (uint8_t)(now & 0xFF);

                    respPayload[3] = (uint8_t)((now >> 8) & 0xFF);

                    respPayload[4] = (uint8_t)((now >> 16) & 0xFF);

                    respPayload[5] = (uint8_t)((now >> 24) & 0xFF);



                    // Sign

                    uint8_t respHmac[32];

                    hmac_sha256((const uint8_t*)LORA_SHARED_SECRET, strlen(LORA_SHARED_SECRET), respPayload, 6, respHmac);



                    // To Hex

                    String hexResp = "";

                    for(int i=0; i<6; i++) hexResp += byteToHexString(respPayload[i]);

                    for(int i=0; i<32; i++) hexResp += byteToHexString(respHmac[i]);



                    // Send

                    Serial.println("[LoRa] Sending Time Response...");

                    while(Serial1.available()) Serial1.read();

                    sendAT("AT+TEST=TXLRPKT,\"" + hexResp + "\"");

                    

                    uint32_t tStart = millis();

                    while(millis() - tStart < 2000) {

                        String line = Serial1.readStringUntil('\n');

                        if (line.indexOf("TX DONE") != -1) break;

                    }

                    

                    sendAT("AT+TEST=RXLRPKT");

                }



            } else {

                Serial.println("[LoRa] ERROR: Invalid HMAC signature.");

            }

        } else {

             Serial.println("[LoRa] Ignored: Invalid length (" + String(hexContent.length()) + ")");

        }

    } else {

        Serial.println("[LoRa] Parse error.");

    }

}

void loraTask(void *pvParameters) {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Boucle d'initialisation (Retry infini tant que pas connecté)
    while (true) {
        if (initLoRaModule()) {
            setLoraStatus(true);
            break; // Sortie de la boucle d'init pour aller vers la boucle RX
        } else {
            setLoraStatus(false);
            Serial.println("[LoRaTask] Nouvelle tentative dans 5s...");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
    }

    Serial.println("[LoRaTask] En attente de paquets...");

    for (;;) {
        if (Serial1.available()) {
            String line = Serial1.readStringUntil('\n');
            line.trim();

            if (line.startsWith("+TEST: RX \"")) {
                // Data packet line found. The next line should be the RSSI.
                String rssiLine = "";
                uint32_t startTime = millis();
                // Wait a short time (e.g., up to 100ms) for the next line to arrive.
                while (millis() - startTime < 100) {
                    if (Serial1.available()) {
                        rssiLine = Serial1.readStringUntil('\n');
                        rssiLine.trim();
                        break;
                    }
                    vTaskDelay(5 / portTICK_PERIOD_MS);
                }
                
                // Process the packet with both the data line and the (potentially empty) rssi line.
                processLoRaPacket(line, rssiLine);

            } else if (line.length() > 0) {
                // This is some other unsolicited line from the module (e.g., "+OK")
                // You can log it if needed for debugging.
                // Serial.print("[LoRa/Other] "); Serial.println(line);
            }
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS); 
    }
}

