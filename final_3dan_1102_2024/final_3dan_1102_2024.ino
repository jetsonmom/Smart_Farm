#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>

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

// NTP 클라이언트 설정
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// 운영 시간 설정
const int START_HOUR = 5;   // 오전 5시 시작
const int END_HOUR = 20;    // 오후 9시 종료

// WiFi 체크 간격 설정
unsigned long lastWiFiCheckTime = 0;
const unsigned long WIFI_CHECK_INTERVAL = 10000; // 10초마다 체크

// 마지막 동작 시간 기록용 변수
unsigned long lastOperationTime = 0;
bool operationStarted = false;

void WiFi_Setup() {
    Serial.print("WiFi 연결 중: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {  // 최대 10초 대기
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi 연결됨");
        Serial.print("IP 주소: ");
        Serial.println(WiFi.localIP());
        timeClient.begin();
        timeClient.setTimeOffset(32400);  // UTC+9
    } else {
        Serial.println("\nWiFi 연결 실패");
    }
}

void turnOffAllDevices() {
    digitalWrite(relay_led, RELAY_OFF);
    digitalWrite(relay_pump, RELAY_OFF);
    digitalWrite(relay_fan, RELAY_OFF);
}

void setup() {
    Serial.begin(115200);
    
    // 릴레이 핀 초기화
    pinMode(relay_led, OUTPUT);
    pinMode(relay_pump, OUTPUT);
    pinMode(relay_fan, OUTPUT);
    
    // 초기 상태는 모두 OFF
    turnOffAllDevices();
    
    // WiFi 초기 연결
    WiFi_Setup();
}

void loop() {
    unsigned long currentMillis = millis();

    // WiFi 연결 상태 주기적 체크
    if (currentMillis - lastWiFiCheckTime >= WIFI_CHECK_INTERVAL) {
        lastWiFiCheckTime = currentMillis;
        
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi 연결 끊김 - 재연결 시도");
            turnOffAllDevices();
            WiFi_Setup();
            return;
        }
    }

    // NTP 시간 업데이트
    if (!timeClient.update()) {
        timeClient.forceUpdate();
    }

    int currentHour = timeClient.getHours();
    int currentMinute = timeClient.getMinutes();
    int currentSecond = timeClient.getSeconds();
    
    // 현재 시간 출력 (1초마다)
    static int lastPrintedSecond = -1;
    if (currentSecond != lastPrintedSecond) {
        Serial.printf("현재 시간: %02d:%02d:%02d\n", currentHour, currentMinute, currentSecond);
        lastPrintedSecond = currentSecond;
    }
    
    // 운영 시간 체크 (05:00 ~ 21:00)
    if (currentHour >= START_HOUR && currentHour < END_HOUR) {
        digitalWrite(relay_led, RELAY_ON);  // LED는 운영 시간 동안 항상 ON
        
        // 60분마다 작동
        if (currentMinute  == 0) {
            if (!operationStarted) {
                operationStarted = true;
                lastOperationTime = currentMillis;
                
                // 펌프와 팬 켜기
                digitalWrite(relay_pump, RELAY_ON);
                digitalWrite(relay_fan, RELAY_ON);
                Serial.println("펌프 & 팬 ON");
            }
            
            // 동작 시간 체크
            unsigned long operationDuration = currentMillis - lastOperationTime;
            
            // 펌프는 60초 동작
            if (operationDuration >= 60000 && digitalRead(relay_pump) == RELAY_ON) {
                digitalWrite(relay_pump, RELAY_OFF);
                Serial.println("펌프 OFF");
            }
            
            // 팬은 200초 동작
            if (operationDuration >= 200000 && digitalRead(relay_fan) == RELAY_ON) {
                digitalWrite(relay_fan, RELAY_OFF);
                Serial.println("팬 OFF");
            }
        } else {
            operationStarted = false;  // 다음 5분 간격을 위해 초기화
        }
    } else {
        // 운영 시간 외에는 모든 장치 OFF
        turnOffAllDevices();
        operationStarted = false;
        Serial.println("운영 시간 외 - 모든 장치 OFF");
    }
    
    delay(100);  // 100ms 딜레이로 CPU 부하 감소
}