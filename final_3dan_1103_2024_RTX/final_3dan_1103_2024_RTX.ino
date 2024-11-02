#include <stdio.h>
#include <DS1302.h>
#include <WiFi.h>
#include <time.h>

// Pin definitions for ESP32
const int kCePin   = 27;    // Chip Enable
const int kIoPin   = 33;    // Input/Output
const int kSclkPin = 32;    // Serial Clock

// 릴레이 핀 설정
const int relay_led = 26;    // LED 릴레이
const int relay_pump = 25;   // 펌프 릴레이
const int relay_fan = 14;    // 팬 릴레이

// 일반 릴레이 상태 설정 (LOW가 켜짐)
const int RELAY_ON = 1;    
const int RELAY_OFF = 0;   

// LED 릴레이 상태 설정 (HIGH가 켜짐)
const int LED_ON = 1;    
const int LED_OFF = 0;   

// WiFi credentials
const char* ssid = "TP-Link_0744";
const char* password = "75317768";

// NTP 서버 설정
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 32400;  // UTC+9 (한국 시간)
const int daylightOffset_sec = 0;

// 운영 시간 설정
const int START_HOUR = 5;   // 오전 5시 시작
const int END_HOUR = 20;    // 오후 8시 종료

// DS1302 객체 생성
DS1302 rtc(kCePin, kIoPin, kSclkPin);

// 작동 상태 변수
unsigned long lastOperationTime = 0;
bool operationStarted = false;
int lastPrintedSecond = -1;

String dayAsString(const Time::Day day) {
    switch (day) {
        case Time::kSunday: return "Sunday";
        case Time::kMonday: return "Monday";
        case Time::kTuesday: return "Tuesday";
        case Time::kWednesday: return "Wednesday";
        case Time::kThursday: return "Thursday";
        case Time::kFriday: return "Friday";
        case Time::kSaturday: return "Saturday";
    }
    return "(unknown day)";
}

Time::Day getDayOfWeek(int wday) {
    switch (wday) {
        case 0: return Time::kSunday;
        case 1: return Time::kMonday;
        case 2: return Time::kTuesday;
        case 3: return Time::kWednesday;
        case 4: return Time::kThursday;
        case 5: return Time::kFriday;
        case 6: return Time::kSaturday;
        default: return Time::kSunday;
    }
}

void syncTimeFromNTP() {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("NTP 시간 가져오기 실패");
        return;
    }

    Time t(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec,
        getDayOfWeek(timeinfo.tm_wday)
    );

    rtc.writeProtect(false);
    rtc.halt(false);
    rtc.time(t);

    Serial.println("RTC 시간이 NTP와 동기화되었습니다.");
}

void turnOffAllDevices() {
    digitalWrite(relay_led, 0);     // LED는 별도의 OFF 값 사용
    digitalWrite(relay_pump, RELAY_OFF);
    digitalWrite(relay_fan, RELAY_OFF);
    Serial.println("모든 장치 OFF");
}

void printSystemStatus(Time t) {
    Serial.println("\n=== 시스템 상태 ===");
    Serial.printf("현재 시간: %s %04d-%02d-%02d %02d:%02d:%02d\n",
            dayAsString(t.day).c_str(),
            t.yr, t.mon, t.date,
            t.hr, t.min, t.sec);
    Serial.printf("운영 시간: %02d:00 ~ %02d:00\n", START_HOUR, END_HOUR);
    Serial.printf("운영 상태: %s\n", 
        (t.hr >= START_HOUR && t.hr < END_HOUR) ? "운영 중" : "운영 종료");
    Serial.printf("LED 상태: %s\n", digitalRead(relay_led) == LED_ON ? "ON" : "OFF");
    Serial.printf("펌프 상태: %s\n", digitalRead(relay_pump) == RELAY_ON ? "ON" : "OFF");
    Serial.printf("팬 상태: %s\n", digitalRead(relay_fan) == RELAY_ON ? "ON" : "OFF");
    Serial.println("==================");
}

void setup() {
    Serial.begin(115200);
    Serial.println("시스템 시작");

    // 릴레이 핀 초기화
    pinMode(relay_led, OUTPUT);
    pinMode(relay_pump, OUTPUT);
    pinMode(relay_fan, OUTPUT);
    
    turnOffAllDevices();

    // WiFi 연결
    Serial.printf("WiFi 연결 중: %s\n", ssid);
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi 연결됨");
        
        // NTP 시간 설정
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        
        // RTC 시간 동기화
        syncTimeFromNTP();
    } else {
        Serial.println("\nWiFi 연결 실패");
    }
}

void loop() {
    unsigned long currentMillis = millis();
    Time t = rtc.time();
    
    // 1초마다 상태 출력
    if (t.sec != lastPrintedSecond) {
        printSystemStatus(t);
        lastPrintedSecond = t.sec;
    }

    // 운영 시간 체크 및 장치 제어
    if (t.hr >= START_HOUR && t.hr < END_HOUR) {
        digitalWrite(relay_led, LED_ON);    // LED는 별도의 ON 값 사용
        
        // 5분마다 작동
        if (t.min % 5 == 0) {
            if (!operationStarted) {
                operationStarted = true;
                lastOperationTime = currentMillis;
                
                digitalWrite(relay_pump, RELAY_ON);
                digitalWrite(relay_fan, RELAY_ON);
                Serial.println("펌프 & 팬 작동 시작");
            }
            
            // 작동 시간 체크
            unsigned long operationDuration = currentMillis - lastOperationTime;
            
            // 펌프는 80초 작동
            if (operationDuration >= 80000 && digitalRead(relay_pump) == RELAY_ON) {
                digitalWrite(relay_pump, RELAY_OFF);
                Serial.println("펌프 작동 종료");
            }
            
            // 팬은 200초 작동
            if (operationDuration >= 200000 && digitalRead(relay_fan) == RELAY_ON) {
                digitalWrite(relay_fan, RELAY_OFF);
                Serial.println("팬 작동 종료");
            }
        } else {
            operationStarted = false;
        }
    } else {
        turnOffAllDevices();
        operationStarted = false;
    }
    
    delay(100);
}