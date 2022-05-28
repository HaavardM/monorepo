/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "Particle.h"
#line 1 "/home/haavard/Dev/mqtt_test/src/mqtt_test.ino"
// This #include statement was automatically added by the Particle IDE.
#include <MQTT-TLS.h>
#include "jwt.h"

void setup_mqtt();
void pirTriggerInterrupt();
void setup();
void loop();
#line 5 "/home/haavard/Dev/mqtt_test/src/mqtt_test.ino"
void callback(char* topic, byte* payload, unsigned int length);

// use GlobalSign Root CA - R2
#define GOOGLE_IOT_CORE_CA_PEM                                          \
"-----BEGIN CERTIFICATE-----\r\n" \
"MIIBxTCCAWugAwIBAgINAfD3nVndblD3QnNxUDAKBggqhkjOPQQDAjBEMQswCQYD\r\n" \
"VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzERMA8G\r\n" \
"A1UEAxMIR1RTIExUU1IwHhcNMTgxMTAxMDAwMDQyWhcNNDIxMTAxMDAwMDQyWjBE\r\n" \
"MQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExM\r\n" \
"QzERMA8GA1UEAxMIR1RTIExUU1IwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAATN\r\n" \
"8YyO2u+yCQoZdwAkUNv5c3dokfULfrA6QJgFV2XMuENtQZIG5HUOS6jFn8f0ySlV\r\n" \
"eORCxqFyjDJyRn86d+Iko0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUw\r\n" \
"AwEB/zAdBgNVHQ4EFgQUPv7/zFLrvzQ+PfNA0OQlsV+4u1IwCgYIKoZIzj0EAwID\r\n" \
"SAAwRQIhAPKuf/VtBHqGw3TUwUIq7TfaExp3bH7bjCBmVXJupT9FAiBr0SmCtsuk\r\n" \
"miGgpajjf/gFigGM34F9021bCWs1MbL0SA==\r\n" \
"-----END CERTIFICATE-----"

const char googleIotCoreCaPem[] = GOOGLE_IOT_CORE_CA_PEM;
MQTT client("mqtt.2030.ltsapis.goog", 8883, callback, 768);  // set max packet size to 768 for JWT password.

// recieve message
void callback(char* topic, byte* payload, unsigned int length) {
    /*char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;
    String message(p);
    
    int number = (int)strtol(message, NULL, 16); 
    int b = number % 0x100;
    number /= 0x100;
    int g = number % 0x100;
    number /=0x100;
    int r = number;
    RGB.color(r, g, b);
    delay(1000);
    */
}

#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)
unsigned long lastSync = millis();




jwt_ctx_t jwt_ctx; 

void setup_mqtt() {
    client.disconnect();
    if (millis() - lastSync > ONE_DAY_MILLIS) {
        Particle.syncTime();
        lastSync = millis();
    }
    char jwt[256];
    int ret = 0;
    size_t jwt_size;
    if ((ret = jwt_gen(&jwt_ctx, jwt, sizeof(jwt), &jwt_size)) != 0) {
        Serial.printf("jwt error %d\n", ret);
    }
    jwt[jwt_size] = 0;


    // connect to Google IoT Core
    client.enableTls(googleIotCoreCaPem, sizeof(googleIotCoreCaPem));
    if(client.connect("projects/wearebrews/locations/europe-west1/registries/brews-iot/devices/test", "unused", jwt)) {
        Serial.println("connection successfull");
    } else {
        Serial.println("connection error");
    }
                   
    // publish/subscribe to Google IoT Core
    if (client.isConnected()) {
        Serial.printf("client connected at %s\n", Time.timeStr(Time.now()).c_str());
        client.subscribe("/devices/test/config");
    }
}

bool pirTriggered = false;
void pirTriggerInterrupt() {
    pirTriggered = true;
}

void setup() {
    pinMode(D7, OUTPUT);
    pinMode(D5, INPUT);
    attachInterrupt(D5, pirTriggerInterrupt, RISING);
    RGB.control(true);
    jwt_init(&jwt_ctx);
    setup_mqtt();
}

const unsigned char PRESENT[] = "PRESENT";
const unsigned char NOT_PRESENT[] = "NOT_PRESENT";

void loop() {
    static bool pirState = false;
    if(client.isConnected()) {
        digitalWrite(D7, HIGH);
    } else {
        digitalWrite(D7, LOW);
        Serial.printf("client disconnected at %s\n", Time.timeStr(Time.now()).c_str());
        setup_mqtt();
    }

    if (pirTriggered) {
        pirState = true;
        pirTriggered = false;
        RGB.color(0xFF0000);
        client.publish("/devices/test/events/pir", PRESENT, sizeof(PRESENT), MQTT::QOS1);
    }

    if (pirState && !digitalRead(D5)) {
        pirState = false;
        RGB.color(0x0000FF);
        client.publish("/devices/test/events/pir", NOT_PRESENT, sizeof(NOT_PRESENT), MQTT::QOS1);
    }

    client.loop();
    delay(500);
}