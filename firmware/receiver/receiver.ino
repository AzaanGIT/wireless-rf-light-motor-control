/**
 * @file receiver.ino
 * @brief RF 433 MHz Telemetry Receiver, Closed-Loop PWM Motor Controller & I2C LCD UI
 * @author K Azaan Ahamed (23BEC0362), S S Sreenithie (23BEC0423)
 *
 * Pin Connections:
 *   - RF 433 MHz RX Data      : Digital Pin 11
 *   - L298N Motor Driver ENA  : Digital Pin 9 (Timer 1 PWM)
 *   - L298N Motor Driver IN1  : Digital Pin 8
 *   - L298N Motor Driver IN2  : Digital Pin 10
 *   - Binary LEDs:
 *       * LED0 (c0 - LSB)     : Digital Pin 2
 *       * LED1 (c1)           : Digital Pin 3
 *       * LED2 (c2 - MSB)     : Digital Pin 4
 *   - I2C LCD Display (16x2)  : SDA -> A4, SCL -> A5
 *   - Status / RF Sync LED    : Digital Pin 13
 */

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Pin assignments
const int PIN_RF_RX    = 11;
const int PIN_MOTOR_ENA = 9;  // PWM speed pin
const int PIN_MOTOR_IN1 = 8;
const int PIN_MOTOR_IN2 = 10;

const int PIN_LED_C0   = 2;
const int PIN_LED_C1   = 3;
const int PIN_LED_C2   = 4;
const int PIN_LED_SYNC = 13;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Timing and Safety parameters
const int LIGHT_THRESHOLD = 50;       // Below this threshold motor remains stationary
const unsigned long RF_FAILSAFE_TIMEOUT_MS = 2000; // Watchdog failsafe cutoff

unsigned long lastPacketTimestamp = 0;
int currentPwm = 0;

void setMotorSpeed(int pwm) {
    pwm = constrain(pwm, 0, 255);
    currentPwm = pwm;
    if (pwm == 0) {
        digitalWrite(PIN_MOTOR_IN1, LOW);
        digitalWrite(PIN_MOTOR_IN2, LOW);
        analogWrite(PIN_MOTOR_ENA, 0);
    } else {
        // Forward direction
        digitalWrite(PIN_MOTOR_IN1, HIGH);
        digitalWrite(PIN_MOTOR_IN2, LOW);
        analogWrite(PIN_MOTOR_ENA, pwm);
    }
}

void setBinaryLeds(uint8_t diffC) {
    digitalWrite(PIN_LED_C0, (diffC & 0x01) ? HIGH : LOW);
    digitalWrite(PIN_LED_C1, (diffC & 0x02) ? HIGH : LOW);
    digitalWrite(PIN_LED_C2, (diffC & 0x04) ? HIGH : LOW);
}

void updateLcd(uint16_t ldrRaw, uint8_t a, uint8_t b, uint8_t c, int pwm) {
    lcd.setCursor(0, 0);
    lcd.print("LDR:");
    lcd.print(ldrRaw);
    lcd.print(" A:");
    lcd.print(a);
    lcd.print(" B:");
    lcd.print(b);
    lcd.print("  ");

    lcd.setCursor(0, 1);
    lcd.print("C:");
    lcd.print(c);
    lcd.print(" PWM:");
    lcd.print(pwm);
    lcd.print("    ");
}

void setup() {
    Serial.begin(115200);

    pinMode(PIN_RF_RX, INPUT);
    pinMode(PIN_MOTOR_ENA, OUTPUT);
    pinMode(PIN_MOTOR_IN1, OUTPUT);
    pinMode(PIN_MOTOR_IN2, OUTPUT);

    pinMode(PIN_LED_C0, OUTPUT);
    pinMode(PIN_LED_C1, OUTPUT);
    pinMode(PIN_LED_C2, OUTPUT);
    pinMode(PIN_LED_SYNC, OUTPUT);

    setMotorSpeed(0);
    setBinaryLeds(0);

    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("RF Motor Control");
    lcd.setCursor(0, 1);
    lcd.print("Waiting for TX..");

    Serial.println(F("=== RF 433 MHz Receiver & Motor Actuator Node Online ==="));
}

void loop() {
    // In actual hardware, demodulate Manchester/ASK packet.
    // Here we also accept serial simulated telemetry for testing:
    // Syntax: "RX <ldrRaw> <digitA> <digitB> <diffC>"
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.startsWith("RX")) {
            int raw = 0, a = 0, b = 0, c = 0;
            if (sscanf(line.c_str(), "RX %d %d %d %d", &raw, &a, &b, &c) >= 1) {
                lastPacketTimestamp = millis();
                digitalWrite(PIN_LED_SYNC, HIGH);

                // Closed-loop speed mapping:
                // Motor speed is proportional to light intensity above threshold
                int pwm = 0;
                if (raw > LIGHT_THRESHOLD) {
                    pwm = map(raw, LIGHT_THRESHOLD, 1023, 60, 255); // 60 is min start torque
                }
                setMotorSpeed(pwm);
                setBinaryLeds((uint8_t)c);
                updateLcd(raw, a, b, c, pwm);

                Serial.print(F("[RX ACK] LDR=")); Serial.print(raw);
                Serial.print(F(" -> Motor PWM=")); Serial.print(pwm);
                Serial.print(F(" | Binary C=")); Serial.println(c);
                digitalWrite(PIN_LED_SYNC, LOW);
            }
        }
    }

    // Safety watchdog: shut down motor if RF packet link drops
    if (millis() - lastPacketTimestamp > RF_FAILSAFE_TIMEOUT_MS && currentPwm > 0) {
        setMotorSpeed(0);
        setBinaryLeds(0);
        lcd.setCursor(0, 1);
        lcd.print("RF LINK LOST!   ");
        Serial.println(F("[SAFETY] Watchdog fired: RF telemetry signal timed out. Motor stopped."));
    }

    delay(10);
}
