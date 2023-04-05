/*  riban modular
 *  Copyright riban 2023
 *  
 *  This is the generic module code
 *  Configure with preprocessor defines
 */

#include <Arduino.h>
#include <Wire.h> // I2C inteface
#include <queue>
#include "modular.h"
#include "switch.h"
#include "touch.h"
#include "adc.h"
#include "led.h"

#define NEXT_MODULE_PIN PC14 // GPI connected to next module's reset pin
#define LEARN_I2C_ADDR 0x77 // I2C address to use when reset
#define QUEUE_LEN 20 // Maximum quantity of control readings to queue for I2C Tx

// Define preprocessor directives to configure module type and controls
#ifndef MODULE_TYPE
#define MODULE_TYPE 1
#endif
#ifndef SWITCH_PINS
#define SWITCH_PINS {PB12}
#endif
#ifndef ADC_PINS
#define ADC_PINS {PA0}
#endif
#ifndef LED_PINS
#define LED_PINS {PB6}
#endif
#ifndef CAP_PINS
#define CAP_PINS {PB13}
#endif

const static uint32_t module_type = MODULE_TYPE;
const static uint16_t switch_pins[] = SWITCH_PINS;
const static uint16_t adc_pins[] = ADC_PINS;
const static uint16_t led_pins[] = LED_PINS;
const static uint16_t cap_pins[] = CAP_PINS;

bool configured = false; // True when initialised and I2C address set
uint8_t command = 0; // Received I2C command
int16_t param = 0; // Received I2C parameter
int32_t param_val = 0; // Received I2C value


const static uint16_t num_switches = sizeof(switch_pins) / sizeof(uint16_t);
const static uint16_t num_adcs = sizeof(adc_pins) / sizeof(uint16_t);
const static uint16_t num_leds = sizeof(led_pins) / sizeof(uint16_t);
const static uint16_t num_dests = sizeof(cap_pins) / sizeof(uint16_t);
const static uint16_t num_controls = num_switches + num_adcs + num_leds + num_dests;

Control * controls[num_controls];
std::queue<Control*> send_queue;

// Reset and listen for new I2C address
void reset() {
  Serial.println("RESET");
  Wire.begin(LEARN_I2C_ADDR, false);
  Wire.onRequest(onI2Crequest);
  configured = false;
  for (uint16_t i = 0; i < num_controls; ++i)
    controls[i]->reset();
  while(send_queue.size())
    send_queue.pop();
}

// Initialise module
void setup(){
  Serial.begin(9600);
  //Disable next module
  pinMode(NEXT_MODULE_PIN, OUTPUT);
  digitalWrite(NEXT_MODULE_PIN, LOW);

  uint16_t index = 0;
  for (uint16_t i = 0; i < num_switches; ++i)
    controls[index++] = new Switch(index + 1, switch_pins[i]);
  for (uint16_t i = 0; i < num_adcs; ++i)
    controls[index++] = new ADC(index + 1, adc_pins[i]);
  for (uint16_t i = 0; i < num_leds; ++i)
    controls[index++] = new LED(index + 1, led_pins[i]);
  for (uint16_t i = 0; i < num_dests; ++i)
    controls[index++] = new Touch(index + 1, cap_pins[i]);

  reset();
  Wire.onReceive(onI2CReceive);
  pinMode(PC13, OUTPUT);
}

// Main process loop
void loop(){
  static bool led_on = false;
  static int uptime = 0;
  static int next_second = 0;
  static int loop_count = 0;
  static bool next_reset = false;

  uint32_t  now = millis();
  ++loop_count;
  if(now >= next_second) {
    next_second = now + 1000;
    Serial.printf("Uptime: %d cycles/sec: %d queue size: %d\n", ++uptime, loop_count, send_queue.size());
    loop_count = 0;
    if (configured)
      led_on = (uptime % 4) == 0;
    else
      led_on = !led_on;
    digitalWrite(PC13, !led_on);
  }
  for (uint16_t i = 0; i < num_controls; ++i) {
    //!@todo Ensure higher controls are read when lower controls are noisy, e.g. restart check after last checked control
    if (send_queue.size() >= QUEUE_LEN)
      break;
    if (controls[i]->read(now)) {
      send_queue.push(controls[i]);
      if (i == 1) {
        int val = controls[i]->get_value();
        ((Touch*)(controls[3]))->set_value(val);
        Serial.printf("Changed output to %d\n", val);
      } else if (i == 3)
        Serial.printf("Touch: %d\n", controls[i]->get_value());
    }
  }
}

// Handle received I2C data
void onI2CReceive(int count){
  Serial.printf("onI2cReceive(%d)\n", count);
  if(count == 1) {
    command = Wire.read();
    Serial.printf("Set command %d\n", command);
  } else if (count == 3) {
    // 2-byte read
    command = Wire.read();
    param = Wire.read() + (Wire.read() << 8);
    Serial.printf("Set command %d and register to %d\n", command, param);
    return;
  } else if (count == 7) {
    command = Wire.read();
    param = Wire.read() + (Wire.read() << 8);
    param_val = Wire.read() | (Wire.read() << 8) | (Wire.read() << 16) | (Wire.read() << 24);
    Serial.printf("Set command %d register %d value %d\n", command, param_val, param);
  } else {
    Wire.flush();
    Serial.printf("Malformed I2C message %d long\n", count);
    command = 0;
  }

  switch(command) {
    case 3:
      // Write value to control
      if (param && param <= num_controls)
        controls[param - 1]->set_value(param_val);
      break;
    case 0xfe:
      if(param > 10 && param < 110) {
          Wire.begin(param, true);
          Wire.onRequest(onI2Crequest);
          configured = true;
          Serial.printf("Changed I2C address to %02x\n", param);
          digitalWrite(NEXT_MODULE_PIN, HIGH);
      }
      break;
    case 0xff:
      reset();
      break;
  }
}

// Handle I2C request for data
void onI2Crequest() {
  Serial.println("onI2Crequest");
  static uint8_t buffer[32];
  bzero(buffer, 32);
  switch(command) {
    case 0:
      *(uint16_t*)(buffer) = 0xffff;
      *(uint32_t*)(buffer + 2) = module_type;
      break;
    case 1:
      if (send_queue.size()) {
        Control * control = send_queue.front();
        *(uint16_t*)(buffer) = control->get_index();
        *(int32_t*)(buffer + 2) = control->get_value();
        send_queue.pop();
      }
      break;
    case 2:
      if (param && param <= num_controls) {
        Control * control = controls[param - 1];
        *(uint16_t*)(buffer) = control->get_index();
        *(int32_t*)(buffer + 2) = control->get_value();
        ++param;
        break;
      }
  }
  Wire.write(buffer, 6);
}
