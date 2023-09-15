#include <Arduino.h>
#include "detect.h"
#include "panel_types.h"

uint8_t detectBit;
uint32_t bootTimer;
uint8_t bootMode;
uint8_t uid[13];
uint8_t i2cAddress;

uint8_t getI2cAddress() {
  return i2cAddress;
}

void init_detect() {
  pinMode(DETECT_PIN, INPUT);
  bootMode = 0;
  detectBit = 0;
  bootTimer = 0;
  i2cAddress = 0;

  uint32_t x = HAL_GetUIDw0();
  for (uint8_t i = 0; i < 4; ++i)
    uid[i] = x >> (i * 8);
  x = HAL_GetUIDw1();
  for (uint8_t i = 0; i < 4; ++i)
    uid[i + 4] = x >> (i * 8);
  x = HAL_GetUIDw2();
  for (uint8_t i = 0; i < 4; ++i)
    uid[i + 8] = x >> (i * 8);

  for (int i = 0; i < 12; ++i)
    uid[12] += uid[i];
  uid[12] = (~uid[12]) + 1;
}

bool detect() {
  if (bootMode > 13)
    return false;
  uint32_t now = micros();

  // Reset detection on timeout
  if (bootMode < 13 && bootMode > 1 && (bootTimer + 500 < now))
    bootMode = 0;

  bool detectState = digitalRead(DETECT_PIN);
  if (bootMode == 0) {
    // Waiting for start bit
    if (!detectState)
      return true;
    detectBit = 0;
    bootMode = 1;
    bootTimer = now;
  } else if (bootMode == 1) {
    // Check for start bit length
    if (detectState)
      return true;
    if (bootTimer + 200 > now) {
      // Too short for start bit
      // Serial.printf("Start bit too short %u - %u = %u\n", now, bootTimer, now - bootTimer);
      bootMode = 0;
      return true;
    }
    bootMode = 2;
    bootTimer = now;
  } else if (bootMode == 2) {
    // Assert presence bit
    pinMode(DETECT_PIN, OUTPUT);
    bootMode = 3;
    bootTimer = now;
  } else if (bootMode == 3) {
    if (bootTimer + 140 > now)
      return true;
    pinMode(DETECT_PIN, INPUT);
    bootMode = 4;
    bootTimer = now;
  } else if (bootMode == 4) {
    // Waiting for previous bit to clear on all devices
    if (!detectState)
      return true;
    bootMode = 5;
  } else if (bootMode == 5) {
    // Waiting for clock
    if (detectState)
      return true;
    bootMode = 6;
    bootTimer = now;
  } else if (bootMode == 6) {
    // Process bit
    if ((uid[detectBit / 8] >> (detectBit % 8)) & 1) {
      pinMode(DETECT_PIN, OUTPUT);
      bootMode = 8;
    } else {
      bootMode = 7;
    }
    bootTimer = now;
  } else if (bootMode == 7) {
    // Listening for bit to be asserted by other modules
    if (bootTimer + 130 > now)
      return true;
    if (!detectState) {
      // Another device has higher UID so drop out of detection
      Serial.printf("Other device detected\n");
      bootMode = 0;
    } else {
      // Prepare to process next bit
      bootMode = 9;
    }
  } else if (bootMode == 8) {
    // Assert bit for approx. 100us
    if (bootTimer + 140 > now)
      return true;
    pinMode(DETECT_PIN, INPUT);
    bootMode = 9;
  } else if (bootMode == 9) {
    // End of bit processing
    if (++detectBit >= 104) {
      // End of uid processing - we win!!!
      bootMode = 10;
      detectBit = 0;
    } else {
      // Process next bit
      bootMode = 4;
    }
  } else if (bootMode == 10) {
    // We have the highest UID so receive I2C
    // Wait for end of last bit
    if (!detectState)
      return true;
    bootMode = 11;
  } else if (bootMode == 11) {
    // Wait for start of clock
    if (detectState)
      return true;
    bootMode = 12;
    bootTimer = now;
  } else if (bootMode == 12) {
    // Check for long clock == 1
    if (bootTimer + 90 > now)
      return true;
    if (!detectState)
      i2cAddress |= 1 << detectBit;
    if (++detectBit < 8)
      bootMode = 10;
    else
      bootMode = 13;
  } else if (bootMode == 13) {
    // I2C address set (0 if invalid)
    Serial.printf("UID: 0x");
    for (uint8_t i = 0; i < 13; ++i)
      Serial.printf("%02x", uid[i]);
    Serial.printf(" set I2C address to 0x%02x\n", i2cAddress);
    if (i2cAddress)
      bootMode = 14;
    else
      bootMode = 0;
  }
  return true;
}

