#include <WiFi.h>

// WiFi 설정
const char* ssid = "TP-Link_0744";
const char* password = "75317768";

// 릴레이 핀 설정
const int relay_led = 26;    // LED 릴레이
const int relay_pump = 25;   // 펌프 릴레이
const int relay_fan = 14;    // 팬 릴레이

// 릴레이 상태 설정 (LOW가 켜짐)
const int RELAY_ON = 0;    
const int RELAY_OFF = 1;   

// 타이머 변수
unsigned long previousMillis = 0;
const long interval = 300000;    // 5분 = 300,000 밀리초
int cycleState = 0;             // 0: 대기, 1: 펌프+팬 ON, 2: 팬만 ON

void setup() {
    Serial.begin(115200);
    
    // 릴레이 핀 초기화
    pinMode(relay_led, OUTPUT);
    pinMode(relay_pump, OUTPUT);
    pinMode(relay_fan, OUTPUT);
    
    // 초기 상태는 모두 OFF
    digitalWrite(relay_led, RELAY_OFF);
    digitalWrite(relay_pump, RELAY_OFF);
    digitalWrite(relay_fan, RELAY_OFF);
    
    // WiFi 연결
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected");
}

void loop() {
    unsigned long currentMillis = millis();
    
    // 5분마다 새로운 주기 시작
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        cycleState = 1;  // 새로운 주기 시작
        Serial.println("새로운 주기 시작!");
    }
    
    // 주기 상태에 따른 동작
    switch (cycleState) {
        case 1:  // 펌프와 팬 모두 ON (처음 100초)
            digitalWrite(relay_led, RELAY_ON);
            digitalWrite(relay_pump, RELAY_ON);
            digitalWrite(relay_fan, RELAY_ON);
            Serial.println("펌프 ON, 팬 ON");
            
            if (currentMillis - previousMillis >= 80000) {  // 100초 후
                cycleState = 2;
                Serial.println("80초 경과 - 펌프 종료");
            }
            break;
            
        case 2:  // 팬만 ON (100초 ~ 200초)
            digitalWrite(relay_pump, RELAY_OFF);
            digitalWrite(relay_fan, RELAY_ON);
            Serial.println("팬만 ON");
            
            if (currentMillis - previousMillis >= 200000) {  // 200초 후
                cycleState = 0;
                digitalWrite(relay_fan, RELAY_OFF);
                Serial.println("200초 경과 - 팬 종료");
            }
            break;
            
        default:  // 대기 상태
            digitalWrite(relay_pump, RELAY_OFF);
            digitalWrite(relay_fan, RELAY_OFF);
            break;
    }
    
    // 상태 출력 (1초마다)
    static unsigned long lastPrintMillis = 0;
    if (currentMillis - lastPrintMillis >= 1000) {
        lastPrintMillis = currentMillis;
        
        Serial.printf("경과 시간: %d초, 상태: %d\n", 
            (currentMillis - previousMillis) / 1000, 
            cycleState);
    }
    
    delay(100);  // 약간의 딜레이
}