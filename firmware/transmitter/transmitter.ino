/**
 * @file transmitter.ino
 * @brief RF 433 MHz Light Sensing & Digital Telemetry Transmitter
 * @author K Azaan Ahamed (23BEC0362), S S Sreenithie (23BEC0423)
 *
 * Pin Connections:
 *   - LDR Module (Analog In)  : A0
 *   - RF 433 MHz TX Data      : Digital Pin 12
 *   - Pushbutton (Calibration): Digital Pin 2 (Internal Pullup)
 *   - Status LED              : Digital Pin 13
 */

#include <Arduino.h>

const int PIN_LDR_ANALOG = A0;
const int PIN_RF_TX      = 12;
const int PIN_BTN_CALIB  = 2;
const int PIN_LED_TX     = 13;

// Packet Protocol Definition
// [0xAA (Preamble)] [0x55 (Sync)] [LDR_HIGH] [LDR_LOW] [DIGIT_A] [DIGIT_B] [DIFF_C] [CHECKSUM]
struct TelemetryPacket {
    uint8_t preamble; // 0xAA
    uint8_t sync;     // 0x55
    uint16_t ldrRaw;  // 0 - 1023
    uint8_t digitA;   // Tens digit of normalized light
    uint8_t digitB;   // Units digit of normalized light
    uint8_t diffC;    // Absolute difference |A - B| constrained 0-7
    uint8_t checksum;
};

// Software Manchester/OOK Bit-Banging Transmitter for 433 MHz Link
const unsigned int BIT_PERIOD_US = 500; // 2 kbps bit rate

void sendBit(bool bit) {
    if (bit) {
        // High bit: 500us HIGH, 500us LOW (Manchester encoding)
        digitalWrite(PIN_RF_TX, HIGH);
        delayMicroseconds(BIT_PERIOD_US);
        digitalWrite(PIN_RF_TX, LOW);
        delayMicroseconds(BIT_PERIOD_US);
    } else {
        // Low bit: 500us LOW, 500us HIGH
        digitalWrite(PIN_RF_TX, LOW);
        delayMicroseconds(BIT_PERIOD_US);
        digitalWrite(PIN_RF_TX, HIGH);
        delayMicroseconds(BIT_PERIOD_US);
    }
}

void sendByte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        sendBit((byte >> i) & 0x01);
    }
}

void transmitPacket(const TelemetryPacket& pkt) {
    digitalWrite(PIN_LED_TX, HIGH);

    // Send training preamble pulses for AGC lock on receiver
    for (int i = 0; i < 8; i++) {
        digitalWrite(PIN_RF_TX, HIGH);
        delayMicroseconds(BIT_PERIOD_US);
        digitalWrite(PIN_RF_TX, LOW);
        delayMicroseconds(BIT_PERIOD_US);
    }

    sendByte(pkt.preamble);
    sendByte(pkt.sync);
    sendByte((uint8_t)(pkt.ldrRaw >> 8));
    sendByte((uint8_t)(pkt.ldrRaw & 0xFF));
    sendByte(pkt.digitA);
    sendByte(pkt.digitB);
    sendByte(pkt.diffC);
    sendByte(pkt.checksum);

    // Flush to low idle
    digitalWrite(PIN_RF_TX, LOW);
    digitalWrite(PIN_LED_TX, LOW);
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_LDR_ANALOG, INPUT);
    pinMode(PIN_RF_TX, OUTPUT);
    digitalWrite(PIN_RF_TX, LOW);
    pinMode(PIN_BTN_CALIB, INPUT_PULLUP);
    pinMode(PIN_LED_TX, OUTPUT);

    Serial.println(F("=== RF 433 MHz LDR Light Transmitter Node Online ==="));
}

void loop() {
    // 1. Oversample LDR reading for noise rejection
    long sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogRead(PIN_LDR_ANALOG);
        delay(2);
    }
    uint16_t rawLdr = sum / 16;

    // 2. Normalize and compute digits (00 to 99 range)
    int norm = map(rawLdr, 0, 1023, 0, 99);
    norm = constrain(norm, 0, 99);

    uint8_t digitA = norm / 10;
    uint8_t digitB = norm % 10;
    uint8_t diffC  = abs((int)digitA - (int)digitB);
    if (diffC > 7) diffC = 7; // Clamp to 3-bit max

    // 3. Assemble Packet with Checksum
    TelemetryPacket pkt;
    pkt.preamble = 0xAA;
    pkt.sync     = 0x55;
    pkt.ldrRaw   = rawLdr;
    pkt.digitA   = digitA;
    pkt.digitB   = digitB;
    pkt.diffC    = diffC;
    pkt.checksum = (uint8_t)(pkt.preamble ^ pkt.sync ^ (pkt.ldrRaw >> 8) ^
                             (pkt.ldrRaw & 0xFF) ^ pkt.digitA ^ pkt.digitB ^ pkt.diffC);

    // 4. Broadcast wirelessly over RF
    transmitPacket(pkt);

    Serial.print(F("[TX] LDR Raw=")); Serial.print(rawLdr);
    Serial.print(F(" | Norm=")); Serial.print(norm);
    Serial.print(F(" | A=")); Serial.print(digitA);
    Serial.print(F(" | B=")); Serial.print(digitB);
    Serial.print(F(" | Diff C=")); Serial.println(diffC);

    delay(100); // 10 Hz telemetry rate
}
