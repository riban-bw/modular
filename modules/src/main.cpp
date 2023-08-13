/* riban modular
  
  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with Foobar. If not, see <https://www.gnu.org/licenses/>.

  Provides the core firmware for hardware modules implemented on STM32F107C microcontrollers.
  Modules are generic using the same firmware and are configured at compile time using preprocessor directive MODULE_TYPE
*/

#include <Arduino.h>
#include <Wire.h> // I2C inteface
#include "modular.h"
//#include "switch.h"
//#include "adc.h"
//#include "led.h"
#include "ws2812.h"

#define NEXT_MODULE_PIN PC14 // GPI connected to next module's reset pin
#define LEARN_I2C_ADDR 0x77 // I2C address to use when reset

const static uint32_t module_type = MODULE_TYPE;

bool configured = false; // True when initialised and I2C address set
uint16_t i2cOffset = 0; // I2C offset to read / write
uint32_t i2cValue = 0; // Received I2C value to read / write
uint8_t i2cCommand = 0; // Received I2C command
uint8_t avCount = 0; // Index of ADC averaging filter
uint32_t switchValues = 0; // 32-bit flags representing value of first 32 GPI / switches
uint32_t scheduledFlash = 0; // Time that next LED flash change will occur
uint32_t scheduledFastFlash = 0; // Time that next LED flash change will occur
uint32_t scheduledPulse = 0; // Time that next LED pulse change will occur
uint32_t scheduledFastPulse = 0; // Time that next LED pulse change will occur


struct SWITCH {
  uint8_t gpi; // GPI id
  bool value = 0;
  uint32_t lastChange = 0;
};

struct ADC {
  uint8_t gpi;
  uint32_t sum = 0;
  uint32_t value = 0;
  uint16_t avValues[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
};

struct LED {
  uint8_t gpi;
};

struct WS_LED {
    uint8_t led; // Index of LED in WS2812 chain
    uint8_t mode = 0; // WS2812_MODE
    uint8_t r1 = 100; // Preset red value
    uint8_t g1 = 100; // Preset green value
    uint8_t b1 = 0; // Preset blue value
    uint8_t r2 = 10; // Preset red value
    uint8_t g2 = 10; // Preset green value
    uint8_t b2 = 0; // Preset blue value
    uint32_t value = 0; // Current value 0x00rrggbb or 0|1 for flash
    bool dir = true; // Direction for  pulse
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
      switchValues ^= (-state ^ switchValues) & (1UL << i);
    }
  }
}

// Process ADCs
void processAdcs(uint32_t now) {
  for (uint16_t i = 0; i < ADCS; ++i) {
    uint32_t value = analogRead(adcs[i].gpi);
    adcs[i].sum -= adcs[i].avValues[avCount];
    adcs[i].avValues[avCount] = value;
    adcs[i].sum += value;
    adcs[i].value = adcs[i].sum / 16;
  }
  if (++avCount > 15)
    avCount = 0;
}

// Process WS2812 animation
void processWs2812(uint32_t now) {
  bool doFlash = scheduledFlash < now;
  bool doFastFlash = scheduledFastFlash < now;
  bool doPulse = scheduledPulse < now;
  bool doFastPulse = scheduledFastPulse < now;
  if (doFlash)
    scheduledFlash += 500;
  if (doFastFlash)
    scheduledFastFlash += 100;
  if (doPulse)
    scheduledPulse += 10;
  if (doFastPulse)
    scheduledFastPulse += 2;
  for (uint16_t i = 0; i < WSLEDS; ++i) {
      if(wsleds[i].mode == WS2812_MODE_OFF) {
          ws2812_set(wsleds[i].led, 0, 0, 0);
          wsleds[i].mode = WS2812_MODE_IDLE;
      } else if (wsleds[i].mode == WS2812_MODE_ON) {
          ws2812_set(wsleds[i].led, wsleds[i].r1, wsleds[i].g1, wsleds[i].b1);
          wsleds[i].mode = WS2812_MODE_IDLE;
      } else if (wsleds[i].mode == WS2812_MODE_SLOW_FLASH && doFlash || wsleds[i].mode == WS2812_MODE_FAST_FLASH && doFastFlash) {
          if (wsleds[i].value) {
            ws2812_set(wsleds[i].led, wsleds[i].r2, wsleds[i].g2, wsleds[i].b2);
            wsleds[i].value = 0;
          } else {
            ws2812_set(wsleds[i].led, wsleds[i].r1, wsleds[i].g1, wsleds[i].b1);
            wsleds[i].value = 1;
          }
      } else if (wsleds[i].mode == WS2812_MODE_SLOW_PULSE && doPulse || wsleds[i].mode == WS2812_MODE_FAST_PULSE && doFastPulse) {
          uint8_t r = wsleds[i].value >> 16;
          uint8_t g = wsleds[i].value >> 8;
          uint8_t b = wsleds[i].value;
          wsleds[i].dir = !wsleds[i].dir;
          if(wsleds[i].dir) {
            // fade out
            if (r > wsleds[i].r2) {
              --r;
              wsleds[i].dir = false;
            }
            if (g > wsleds[i].g2) {
              --g;
              wsleds[i].dir = false;
            }
            if (b > wsleds[i].b2) {
              --b;
              wsleds[i].dir = false;
            }
          } else {
            // fade in
            if (r < wsleds[i].r1) {
              ++r;
              wsleds[i].dir = true;
            }
            if (g < wsleds[i].g1) {
              ++g;
              wsleds[i].dir = true;
            }
            if (b < wsleds[i].b1) {
              ++b;
              wsleds[i].dir = true;
            }
        }
        wsleds[i].value = r << 16 | g << 8 | b;
        ws2812_set(wsleds[i].led, r, g, b);
      }
  }
  ws2812_refresh();
}

// Reset and listen for new I2C address
void reset() {
  Serial.println("RESET");
  Wire.begin(LEARN_I2C_ADDR, false);
  Wire.onRequest(onI2Crequest);
  Wire.onReceive(onI2Creceive);
  configured = false;
}

// Initialise module
void setup(){
  Serial.begin(9600);
  //Disable next module
  pinMode(NEXT_MODULE_PIN, OUTPUT);
  digitalWrite(NEXT_MODULE_PIN, LOW);

  uint8_t switch_gpis[] = GPI_PINS;
  for (uint16_t i = 0; i < SWITCHES; ++i) {
    switches[i].gpi = switch_gpis[i];
    pinMode(switch_gpis[i], INPUT_PULLUP);
  }
  uint8_t adc_gpis[] = ADC_PINS;
  for (uint16_t i = 0; i < ADCS; ++i) {
    adcs[i].gpi = adc_gpis[i];
  }
  ws2812_init(WSLEDS);
  for (uint16_t i = 0; i < WSLEDS; ++ i) {
    wsleds[i].led = i;
  }

  reset();
  pinMode(PC13, OUTPUT);
  delay(2000);
  Serial.printf("Module 0x%04X initalised\n", MODULE_TYPE);
}

// Main process loop
void loop() {
  static bool led_on = false;
  static int uptime = 0;
  static int next_second = 0;
  static int loop_count = 0;
  static bool next_reset = false;

  static int selected_led = 0;
  static uint8_t mode = 0;

  ++loop_count;
  uint32_t now = millis();

  if(now >= next_second) {
    next_second = now + 1000;
    //Serial.printf("Uptime: %d cycles/sec: %d\n", ++uptime, loop_count);
    if (configured)
      led_on = (uptime % 4) == 0;
    else
      led_on = !led_on;
    digitalWrite(PC13, !led_on);
    loop_count = 0;
  }
  if (Serial.available()) {
    char c = Serial.read();
    if (c > '0' && c < '9' && c - '0' < WSLEDS)
        selected_led = c - '0';
    else
      switch(c) {
        case '!':
          reset();
          break;
        case '?':
          Serial.printf("Module: 0x%04X Uptime: %d\n", MODULE_TYPE, now);
          break;
        case 'o':
          wsleds[selected_led].mode = WS2812_MODE_ON;
          break;
        case 'O':
          wsleds[selected_led].mode = WS2812_MODE_OFF;
          break;
        case 'f':
          wsleds[selected_led].mode = WS2812_MODE_SLOW_FLASH;
          break;
        case 'F':
          wsleds[selected_led].mode = WS2812_MODE_FAST_FLASH;
          break;
        case 'p':
          wsleds[selected_led].mode = WS2812_MODE_SLOW_PULSE;
          break;
        case 'P':
          wsleds[selected_led].mode = WS2812_MODE_FAST_PULSE;
          break;
      }
  }
  processSwitches(now);
  processAdcs(now);
  processWs2812(now);
}

// Handle received I2C data
void onI2Creceive(int count) {
  //Serial.printf("onI2Creceive(%d)\n", count);
  if (count) {
    i2cCommand = Wire.read();
    //Serial.printf("  Command: 0x%02X\n", i2cCommand);
  }
  switch(i2cCommand) {
    case 0xfe:
      // Set I2C address
      if (!configured && count == 2) {
        uint8_t addr = Wire.read();
        //Serial.printf("  Set I2C address >> 0x%02X\n", addr);
        if (addr >= 0x10 && addr < 0x77) {
          Wire.begin(addr, true);
          Wire.onRequest(onI2Crequest);
          configured = true;
          //Serial.printf("  Changed I2C address to %02x\n", i2cOffset);
          digitalWrite(NEXT_MODULE_PIN, HIGH);
        }
      }
      break;
    case 0xff:
      // Reset
      reset();
      break;
    case 0x03:
      // Set WS2812 mode and colour
      if (count > 2) {
        uint8_t led = Wire.read();
        if (led < WSLEDS) {
          wsleds[led].mode = Wire.read();
        }
        if (count > 5) {
          wsleds[led].r1 = Wire.read();
          wsleds[led].g1 = Wire.read();
          wsleds[led].b1 = Wire.read();
        }
        //Serial.printf("  LED %d mode %d colour (%d,%d,%d)\n", led, wsleds[led].mode, wsleds[led].r1, wsleds[led].g1, wsleds[led].b1);
      }
      break;
    case 0x04:
      // Set WS2812 mode and colour 2
      if (count > 2) {
        uint8_t led = Wire.read();
        if (led < WSLEDS) {
          wsleds[led].mode = Wire.read();
        }
        if (count > 5) {
          wsleds[led].r2 = Wire.read();
          wsleds[led].g2 = Wire.read();
          wsleds[led].b2 = Wire.read();
        //Serial.printf("  LED %d mode %d colour2 (%d,%d,%d)\n", led, wsleds[led].mode, wsleds[led].r2, wsleds[led].g2, wsleds[led].b2);
        }
      }
      break;
  }
  Wire.flush();
}

// Handle I2C request for data - assume command was set before request
void onI2Crequest() {
  //Serial.printf("onI2Crequest cmd: 0x%02X\n", i2cCommand);
  static uint32_t response;
  response = 0;

  if (i2cCommand == 0xF0)
    response = MODULE_TYPE;
  else if (i2cCommand < ADCS) {
    response = adcs[i2cCommand].value;
  }
  else if (i2cCommand == 32) {
    // Get first bank of 32 switches
    response = switchValues;
  }
  //Serial.printf("Responding with value 0x%08X\n", response);
  Wire.write((uint8_t*)&response, 4);
  delayMicroseconds(100); //!@todo Why is this delay required - without it the response gives spurious results
}
