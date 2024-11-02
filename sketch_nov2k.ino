#include <stdio.h>

const int relay_led = 26;  // LED 릴레이 핀

void setup() {
    Serial.begin(115200);
    pinMode(relay_led, OUTPUT);
    
    Serial.println("LED 릴레이 테스트 시작");
}

void loop() {
    Serial.println("LED OFF 시도 (0)");
    digitalWrite(relay_led, 0);
    delay(1000);
    
    Serial.println("LED OFF 시도 (1)");
    digitalWrite(relay_led, 1);
    delay(3000);
}
