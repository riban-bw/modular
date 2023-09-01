/* riban modular
  
  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Provides the core firmware for hardware panels implemented on STM32F107C microcontrollers.
  Panels are generic using the same firmware and are setup at compile time using preprocessor directive PANEL_TYPE
*/

#include <Arduino.h>
#include <Wire.h> // I2C inteface
#include "panel.h"
#include "ws2812.h"

#define LEARN_I2C_ADDR 0x77 // I2C address to use when reset

enum {
  RUN_MODE_INIT,
  RUN_MODE_SETUP,
  RUN_MODE_RUN
};

uint8_t run_mode = RUN_MODE_INIT; // True when initialing
uint32_t i2cValue = 0; // Received I2C value to read / write
uint8_t i2cCommand = 0; // Received I2C command
uint8_t avCount = 0; // Index of ADC averaging filter
uint8_t muxAddr = 0; // Multiplexer address [0..7]
uint16_t switchValues[16]; // 32-bit flags representing value of banks of 32 GPI / switches
uint32_t scheduledFlash = 0; // Time that next LED flash change will occur
uint32_t scheduledFastFlash = 0; // Time that next LED flash change will occur
uint32_t scheduledPulse = 0; // Time that next LED pulse change will occur
uint32_t scheduledFastPulse = 0; // Time that next LED pulse change will occur
bool ledMute = false; // True to turn off all LEDs but retain config
bool flashOn = false; // True when flash is on
bool fastFlashOn = false; // True when fast flash is on
bool pulseDir = true; // True when pulse is increasing
uint8_t pulsePhase = 0; // Step through pulse fade
bool fastPulseDir = true; // True when fast pulse is increasing
uint8_t fastPulsePhase = 0; // Step through fast pulse fade
uint64_t changedFlags = 0; // Bitwise flags indicating a sensor value has changed
uint8_t i2cAddr = LEARN_I2C_ADDR; // Module's I2C address

struct SWITCH {
  uint8_t gpi; // GPI pin
  bool value = 0; // Current value
  uint32_t lastChange = 0; // Time of last value change
};

struct ADC {
  uint8_t gpi; // GPI pin
  uint32_t sum = 0; // Sum of last 16 samples
  uint16_t value = 0; // Current filtered / averaged value
  uint16_t lastValue = 0; // Last retrieved filtered / averaged value
  uint16_t avValues[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // Last 16 samples
};

struct LED {
  uint8_t gpi; // GPI pin
};

struct WS_LED {
    uint8_t led; // Index of LED in WS2812 chain
    uint8_t mode = 0; // WS2812_MODE
    uint8_t r1 = 100; // Primary red value
    uint8_t g1 = 100; // Primary green value
    uint8_t b1 = 0; // Primary blue value
    uint8_t r2 = 10; // Secondary red value
    uint8_t g2 = 10; // Secondary green value
    uint8_t b2 = 0; // Secondary blue value
    uint32_t value = 0; // Current value 0x00rrggbb used by fade
    bool dir = true; // Fade direction for pulse or state for flash
};

struct SWITCH switches[SWITCHES];
struct ADC adcs[ADCS];
struct LED leds[LEDS];
struct WS_LED wsleds[WSLEDS];

// Process switches
void processSwitches(uint32_t now) {
  //!@todo Add stretching, long press, etc.
  for (uint16_t i = 0; i < SWITCHES; ++i) {
    if (now - switches[i].lastChange < 20)
      continue; //debounce
    bool state = !digitalRead(switches[i].gpi);
    if (state != switches[i].value) {
      switches[i].value = state;
      switches[i].lastChange = now;
      switchValues[muxAddr] ^= (-state ^ switchValues[muxAddr]) & (1UL << i);
      changedFlags |= 1ULL << muxAddr;
    }
  }
}

// Process ADCs
void processAdcs(uint32_t now) {
  for (uint16_t i = 0; i < ADCS; ++i) {
    uint16_t value = analogRead(adcs[i].gpi);
    adcs[i].sum -= adcs[i].avValues[avCount];
    adcs[i].avValues[avCount] = value;
    adcs[i].sum += value;
    value = adcs[i].sum / 16;
    if (value != adcs[i].value) {
      changedFlags |= (1ULL << (0x10 + i));
      adcs[i].value = value;
    }
  }
  if (++avCount > 15)
    avCount = 0;
}

// Process WS2812 animation
void processWs2812(uint32_t now) {
  if (ledMute)
    return;
  bool doFlash = scheduledFlash < now;
  bool doFastFlash = scheduledFastFlash < now;
  bool doPulse = scheduledPulse < now;
  bool doFastPulse = scheduledFastPulse < now;
  if (doFlash) {
    flashOn = !flashOn;
    scheduledFlash += 500;
  }
  if (doFastFlash) {
    fastFlashOn = !fastFlashOn;
    scheduledFastFlash += 100;
  }
  if (doPulse) {
    if (pulseDir) {
      if(++pulsePhase == 255)
        pulseDir = false;
    } else {
      if(--pulsePhase == 0)
        pulseDir = true;
    }
    scheduledPulse += 10;
  }
  if (doFastPulse) {
     if(++fastPulsePhase == 255)
        fastPulseDir = false;
  } else {
    if(--pulsePhase == 0)
      fastPulseDir = true;
    scheduledFastPulse += 5;
  }
  for (uint16_t i = 0; i < WSLEDS; ++i) {
      if(wsleds[i].mode == WS2812_MODE_OFF) {
          ws2812_set(wsleds[i].led, 0, 0, 0);
          wsleds[i].mode = WS2812_MODE_IDLE;
          wsleds[i].dir = false;
      } else if (wsleds[i].mode == WS2812_MODE_ON) {
          ws2812_set(wsleds[i].led, wsleds[i].r1, wsleds[i].g1, wsleds[i].b1);
          wsleds[i].mode = WS2812_MODE_IDLE;
          wsleds[i].dir = true;
      } else if (wsleds[i].mode == WS2812_MODE_ON2) {
          ws2812_set(wsleds[i].led, wsleds[i].r2, wsleds[i].g2, wsleds[i].b2);
          wsleds[i].mode = WS2812_MODE_IDLE;
          wsleds[i].dir = true;
      } else if (wsleds[i].mode == WS2812_MODE_SLOW_FLASH && doFlash) {
          if (flashOn) {
            ws2812_set(wsleds[i].led, wsleds[i].r2, wsleds[i].g2, wsleds[i].b2);
          } else {
            ws2812_set(wsleds[i].led, wsleds[i].r1, wsleds[i].g1, wsleds[i].b1);
          }
      } else if (wsleds[i].mode == WS2812_MODE_FAST_FLASH && doFastFlash) {
          if (fastFlashOn) {
            ws2812_set(wsleds[i].led, wsleds[i].r2, wsleds[i].g2, wsleds[i].b2);
          } else {
            ws2812_set(wsleds[i].led, wsleds[i].r1, wsleds[i].g1, wsleds[i].b1);
          }
      } else if (wsleds[i].mode == WS2812_MODE_SLOW_PULSE && doPulse) {
          uint8_t r = wsleds[i].value >> 16;
          uint8_t g = wsleds[i].value >> 8;
          uint8_t b = wsleds[i].value;
          r = (wsleds[i].r1 - wsleds[i].r2) * pulsePhase / 256 + wsleds[i].r2;
          g = (wsleds[i].g1 - wsleds[i].g2) * pulsePhase / 256 + wsleds[i].g2;
          b = (wsleds[i].b1 - wsleds[i].b2) * pulsePhase / 256 + wsleds[i].b2;
        wsleds[i].value = r << 16 | g << 8 | b;
        ws2812_set(wsleds[i].led, r, g, b);
      } else if (wsleds[i].mode == WS2812_MODE_FAST_PULSE && doFastPulse) {
          uint8_t r = wsleds[i].value >> 16;
          uint8_t g = wsleds[i].value >> 8;
          uint8_t b = wsleds[i].value;
          r = (wsleds[i].r1 - wsleds[i].r2) * fastPulsePhase / 256 + wsleds[i].r2;
          g = (wsleds[i].g1 - wsleds[i].g2) * fastPulsePhase / 256 + wsleds[i].g2;
          b = (wsleds[i].b1 - wsleds[i].b2) * fastPulsePhase / 256 + wsleds[i].b2;
        wsleds[i].value = r << 16 | g << 8 | b;
        ws2812_set(wsleds[i].led, r, g, b);
      }
  }
  ws2812_refresh();
}

// Set module I2C address
void setAddr(uint8_t addr) {
  Wire.setSCL(SCL_PIN);
  Wire.setSDA(SDA_PIN);
  Wire.begin(addr, true);
  i2cAddr = addr;
  Wire.onRequest(onI2Crequest);
  Wire.onReceive(onI2Creceive);
}

// Reset and listen for new I2C address
void reset() {
  digitalWrite(NEXT_MODULE_PIN, LOW);
  ws2812_set_all(44, 66, 100);
  for (uint16_t i = 0; i < WSLEDS; ++i)
    wsleds[i].mode = WS2812_MODE_ON;
  setAddr(LEARN_I2C_ADDR);
  for (uint16_t i = 0; i < WSLEDS; ++i)
    wsleds[i].mode = WS2812_MODE_ON;
  ws2812_refresh();
  run_mode = RUN_MODE_SETUP;
}

// Initialise module
void setup(){
  //Disable next module
  pinMode(NEXT_MODULE_PIN, OUTPUT);
  digitalWrite(NEXT_MODULE_PIN, LOW);
  pinMode(RUN_PIN, INPUT_PULLDOWN);

  ws2812_init(WSLEDS); // Initalise WS2812 (via SPI) before GPI to allow resuse of MOSI & SCLK pins
  for (uint16_t i = 0; i < WSLEDS; ++ i) {
    wsleds[i].led = i;
  }

  uint8_t switch_gpis[] = GPI_PINS;
  for (uint16_t i = 0; i < SWITCHES; ++i) {
    switches[i].gpi = switch_gpis[i];
    pinMode(switch_gpis[i], INPUT_PULLUP);
  }
  uint8_t adc_gpis[] = ADC_PINS;
  for (uint16_t i = 0; i < ADCS; ++i) {
    adcs[i].gpi = adc_gpis[i];
  }
  pinMode(PC13, OUTPUT); // activity LED
  digitalWrite(PC13, HIGH);
}

// Main process loop
void loop() {
  static bool next_reset = false;
  static uint32_t next_second = 0;

  if (!digitalRead(RUN_PIN)) {
    run_mode = RUN_MODE_INIT;
    Wire.end();
  } else {
    if (run_mode == RUN_MODE_INIT)
      reset();
  }
  uint32_t now = millis();
  if (now > next_second) {
    digitalToggle(PC13);
    switch(run_mode) {
      case RUN_MODE_INIT:
        next_second = now + 100;
        break;
      case RUN_MODE_SETUP:
        next_second = now + 400;
        break;
      default:
        next_second = now + 1000;
        break;
    }
  }

  if (run_mode == RUN_MODE_RUN) {
    processSwitches(now);
    processAdcs(now);
    processWs2812(now);
  }
}

// Handle received I2C data
void onI2Creceive(int count) {
  if (count) {
    i2cCommand = Wire.read();
    --count;
  }
  switch(i2cCommand) {
    case 0xfe: // Set I2C address
      if (run_mode == RUN_MODE_SETUP && count) {
        uint8_t addr = Wire.read();
        --count;
        if (addr >= 10 && addr < 111) {
          setAddr(addr);
          run_mode = RUN_MODE_RUN;
          for (uint16_t i = 0; i < WSLEDS; ++i)
            wsleds[i].mode = WS2812_MODE_OFF;
          digitalWrite(NEXT_MODULE_PIN, HIGH);
          ws2812_refresh();
        }
      }
      break;
    case 0xff: // Reset
      reset();
      break;
    case 0xF1: // Set WS2812 mode and primary colour
    case 0xF2: // Set WS2812 mode and secondary colour
      if (count) {
        uint8_t led = Wire.read();
        --count;
        if (led < WSLEDS) {
          if (count == 1 || count == 4) {
            wsleds[led].mode = Wire.read();
            --count;
          }
          if (count == 3) {
            if (i2cCommand == 0xF1) {
              wsleds[led].r1 = Wire.read();
              wsleds[led].g1 = Wire.read();
              wsleds[led].b1 = Wire.read();
            } else {
              wsleds[led].r2 = Wire.read();
              wsleds[led].g2 = Wire.read();
              wsleds[led].b2 = Wire.read();
            }
            count -= 3;
          }
        }
      }
      break;
    case 0xF3: // Turn off all LEDs temporarily
      if (count) {
        ledMute = Wire.read();
        --count;
        if(ledMute)
          ws2812_refresh(ledMute);
      }
      break;
  }
  while (count) {
    Wire.read();
    --count;
  }
}

// Handle I2C request for data - assume command was set before request
void onI2Crequest() {
  static uint32_t response; // Response is always 3 bytes: 23:16=addr, 15:0=value
  static uint8_t readSessionPos = 0; // Index of session last read value
  static uint64_t readMask;
  uint8_t useResponse = true;
  response = i2cCommand << 16;
  
  if (i2cCommand == 0) {
    // Get next changed value
    if (changedFlags) {
      for (; readSessionPos < 64; ++readSessionPos) {
        readMask = (1ULL << readSessionPos);
        if (readMask & changedFlags) {
          if (readSessionPos < 0x10)
            response = ((0x10 + readSessionPos) << 16) | switchValues[readSessionPos];
          else if (readSessionPos < 0x10 + ADCS) {
            if (adcs[readSessionPos - 0x10].value != adcs[readSessionPos - 0x10].lastValue) {
              response = ((0x10 + readSessionPos) << 16) | adcs[readSessionPos - 0x10].value;
              adcs[readSessionPos - 0x10].lastValue = adcs[readSessionPos - 0x10].value;
            }
          }
          changedFlags &= ~readMask; // clear flag
          ++readSessionPos;
          break;
        }
      }
      if (readSessionPos > 63)
        readSessionPos = 0;
    } else {
      readSessionPos = 0;
    }
  } else if (i2cCommand < 0x10) {
    // General commands
  } else if (i2cCommand < 0x20) {
    // Get first bank of 32 switches
    response |= switchValues[i2cCommand - 0x10];
  } else if (i2cCommand - 0x20 < ADCS) {
    response |= adcs[i2cCommand - 0x20].value;
    //!@todo Handle multiplexed ADC
  } else if (i2cCommand == 0xF0) {
    response |= PANEL_TYPE;
  } else if (i2cCommand == 0xF1) {
    response = ((sizeof(BRAND) - 1) << 16) | ((sizeof(PLUGIN) - 1) << 8 | sizeof(MODEL) - 1);
  } else if (i2cCommand == 0xF2) {
    Wire.write(BRAND);
    useResponse = false;
  } else if (i2cCommand == 0xF3) {
    Wire.write(PLUGIN);
    useResponse = false;
  } else if (i2cCommand == 0xF4) {
    Wire.write(MODEL);
    useResponse = false;
  }
  if (useResponse)
    Wire.write((uint8_t*)&response, 3);
  delayMicroseconds(10); // Need delay to avoid bad read but too long will corrupt WS2812 bus
}
