//2024 10월 23일 테스트  시작전 ntpclient 라이브러리랑 DOIT ESP32 DEVKIT V1확인할 것, 포트 UPLOAD SPEED등을 체크
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// WiFi 설정
const char* ssid = "TP-Link_0744_EXT";
const char* password = "75317768";

// 릴레이 핀 설정
const int relay_led =27;    // LED 릴레이
const int relay_pump = 14;   // 펌프 릴레이
const int relay_fan = 25;    // 팬 릴레이

// 모든 릴레이에 대해 HIGH를 ON으로 통일
const int RELAY_ON = 0;    // HIGH = 켜짐
const int RELAY_OFF = 1;   // LOW = 꺼짐

// NTP 클라이언트 설정
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

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
    
    WiFi_Setup();
          
    }


void WiFi_Setup() {
    Serial.print("WiFi 연결 중: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        digitalWrite(relay_pump, RELAY_OFF);
        digitalWrite(relay_led, RELAY_OFF);
        digitalWrite(relay_fan, RELAY_OFF);
    }

    Serial.println("\nWiFi 연결됨");
    Serial.print("IP 주소: ");
    Serial.println(WiFi.localIP());

    timeClient.begin();
    timeClient.setTimeOffset(32400);
}

void loop() {
    // WiFi 연결 체크
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi 연결 끊김 - 재연결 시도");
        digitalWrite(relay_pump, RELAY_OFF);
        digitalWrite(relay_led, RELAY_OFF);
        digitalWrite(relay_fan, RELAY_OFF);
        WiFi_Setup();
        return;
    }

    // 시간 업데이트
    while(!timeClient.update()) {
        timeClient.forceUpdate();
        break;
    }

    int Hour = timeClient.getHours();
    int Min = timeClient.getMinutes();
    int Sec = timeClient.getSeconds();

    // 시간 출력
    Serial.printf("\n현재 시간: %02d:%02d:%02d\n", Hour, Min, Sec);

    // 주간 모드 (5시-20시)
    if (Hour >= 5 && Hour <= 20) {
        // LED ON
        digitalWrite(relay_led, RELAY_ON);
        Serial.println("LED ON");

        // 5분 간격 동작
        if (Min % 60 == 0) {
            if (Sec <= 140) {
                // 펌프와 팬 동시 동작
                digitalWrite(relay_pump, RELAY_ON);
                digitalWrite(relay_fan, RELAY_ON);
                Serial.println("상태: 펌프 ON, 팬 ON (140초 구간)");
                Serial.printf("펌프 릴레이 상태: %d\n", digitalRead(relay_pump));
            }
            else if (Sec <= 360) {
                // 팬만 동작
                digitalWrite(relay_pump, RELAY_OFF);
                digitalWrite(relay_fan, RELAY_ON);
                Serial.println("상태: 펌프 OFF, 팬 ON (140-300초 구간)");
            }
            else {
                // 대기 상태
                digitalWrite(relay_pump, RELAY_OFF);
                digitalWrite(relay_fan, RELAY_OFF);
                Serial.println("상태: 대기 중 (300초 이후)");
            }
        }
        else {
            digitalWrite(relay_pump, RELAY_OFF);
            digitalWrite(relay_fan, RELAY_OFF);
        }
    }
    else {
        // 야간 모드
        Serial.println("야간 모드 - 모든 장치 OFF");
        digitalWrite(relay_led, RELAY_OFF);
        digitalWrite(relay_pump, RELAY_OFF);
        digitalWrite(relay_fan, RELAY_OFF);
    }

    // 장치 상태 출력
    Serial.println("\n=== 장치 상태 ===");
    Serial.printf("LED   상태: %s\n", digitalRead(relay_led) == RELAY_ON ? "ON" : "OFF");
    Serial.printf("펌프 상태: %s\n", digitalRead(relay_pump) == RELAY_ON ? "ON" : "OFF");
    Serial.printf("팬    상태: %s\n", digitalRead(relay_fan) == RELAY_ON ? "ON" : "OFF");
    
}