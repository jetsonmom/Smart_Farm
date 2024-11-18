const int relay_led = 27;    // LED 릴레이
const int relay_pump = 14;   // 펌프 릴레이
const int relay_fan = 25;    // 팬 릴레이

// 모든 릴레이에 대해 HIGH를 ON으로 통일
const int RELAY_ON = 0;    // HIGH = 켜짐
const int RELAY_OFF = 1;   // LOW = 꺼짐

// 시간 저장 변수
int currentHour = 0;
int currentMinute = 0;
int currentSecond = 0;
int lastActionMinute = -1;  // 마지막 동작 시간 추적

// 타이머 변수
unsigned long cycleStartTime = 0;
bool isCycleActive = false;

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
}

void loop() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        
        if (command.startsWith("TIME:")) {
            String timeStr = command.substring(5);
            
            // 시:분:초 파싱
            currentHour = timeStr.substring(0, 2).toInt();
            currentMinute = timeStr.substring(3, 5).toInt();
            currentSecond = timeStr.substring(6, 8).toInt();
            
            // LED 제어를 가장 먼저 처리
            if (currentHour >= 5 && currentHour <= 20) {
                digitalWrite(relay_led, RELAY_ON);
                Serial.println("Day mode - LED ON");
            } else {
                digitalWrite(relay_led, RELAY_OFF);
                Serial.println("Night mode - LED OFF");
            }
            
            // 나머지 장치 제어
            controlDevices();
            sendStatus();
            
            // 디버그 출력
            Serial.print("Time: ");
            Serial.print(currentHour);
            Serial.print(":");
            Serial.print(currentMinute);
            Serial.print(":");
            Serial.println(currentSecond);
        }
    }
}

void controlDevices() {
    // 주간 모드 (5시-20시)
    if (currentHour >= 5 && currentHour <= 20) {
        // 매 시간 정각(00분)에 동작 시작
        if (currentMinute == 0 && currentSecond == 0) {
            if (!isCycleActive) {
                // 새로운 사이클 시작
                isCycleActive = true;
                cycleStartTime = millis();
                lastActionMinute = currentMinute;
                Serial.println("New cycle started");
            }
        }
        
        if (isCycleActive) {
            unsigned long elapsedTime = (millis() - cycleStartTime) / 1000;  // 경과 시간(초)
            
            if (elapsedTime <= 140) {
                // 처음 140초: 펌프와 팬 ON
                digitalWrite(relay_pump, RELAY_ON);
                digitalWrite(relay_fan, RELAY_ON);
                Serial.println("Pump and Fan ON");
            } 
            else if (elapsedTime <= 360) {
                // 140-360초: 팬만 ON
                digitalWrite(relay_pump, RELAY_OFF);
                digitalWrite(relay_fan, RELAY_ON);
                Serial.println("Fan only ON");
            }
            else {
                // 360초 이후: 모두 OFF
                digitalWrite(relay_pump, RELAY_OFF);
                digitalWrite(relay_fan, RELAY_OFF);
                isCycleActive = false;
                Serial.println("Cycle completed - Pump and Fan OFF");
            }
        }
        
        // 다음 사이클을 위해 마지막 동작 시간 업데이트
        if (currentMinute != 0) {
            lastActionMinute = -1;  // 다음 사이클을 위해 리셋
        }
    }
    else {
        // 야간 모드에서는 펌프와 팬만 OFF
        digitalWrite(relay_pump, RELAY_OFF);
        digitalWrite(relay_fan, RELAY_OFF);
        isCycleActive = false;
    }
}

void sendStatus() {
    Serial.print("STATUS:");
    Serial.print(digitalRead(relay_led) == RELAY_ON ? "ON" : "OFF");
    Serial.print(",");
    Serial.print(digitalRead(relay_pump) == RELAY_ON ? "ON" : "OFF");
    Serial.print(",");
    Serial.println(digitalRead(relay_fan) == RELAY_ON ? "ON" : "OFF");
}
