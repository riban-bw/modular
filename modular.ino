/*  riban modular
 *  Copyright riban 2023
 *  
 *  This is the generic module code
 *  Configure with preprocessor defines
 */
#include <Wire.h> // I2C inteface
#include <queue>

#define NEXT_MODULE_PIN PC14 // GPI connected to next module's reset pin
#define LEARN_I2C_ADDR 0x77 // I2C address to use when reset
#define DEBOUNCE_TIME 20 // Duration to ignore change of switch state (ms)
#define ADC_HYST 2 // Minimum ADC value change to trigger new value
#define ADC_AVERAGE 8 // Quantity of ADC measurements to average over
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
#ifndef DESTINATIONS
#define DESTINATIONS {PB13}
#endif
#ifndef SOURCES
#define SOURCES {}
#endif

const static uint32_t module_type = MODULE_TYPE;
const static uint16_t switch_pins[] = SWITCH_PINS;
const static uint16_t adc_pins[] = ADC_PINS;
const static uint16_t led_pins[] = LED_PINS;
const static uint16_t dest_pins[] = DESTINATIONS;
const static uint16_t src_pins[] = SOURCES;

bool configured = false; // True when initialised and I2C address set
uint8_t command = 0; // Index of register to read

class Control {
  public:
    Control(uint16_t idx, uint8_t gpi) :
      index(idx),
      pin(gpi) {};

    virtual bool read(uint32_t now) {
      return false;
    };

    void write(uint8_t val) {};

    void reset() {
      value = 0;
      last_change = 0;
      pending = false;
    };

    int32_t get_value() { 
      pending = false;
      return value;
    };
    
    uint8_t get_pin() { return pin; };

    uint16_t get_index() { return index; };

  protected:
    int32_t value = 0;
    uint8_t pin;
    uint16_t index;
    uint32_t last_change = 0;
    bool pending = false;
};

// Class representing a switch
class Switch : public Control {
  public:
    Switch(uint16_t idx, uint8_t gpi) : Control(idx, gpi) {
      pinMode(gpi, INPUT_PULLUP);
    };

    bool read(uint32_t now) {
      if (pending)
        return false;
      bool val = digitalRead(pin);
      if (val != value && now > last_change + DEBOUNCE_TIME) {
        value = val;
        last_change = now;
        pending = true;
      }
      return pending;
    };
};

class Touch : public Control {
  public:
    Touch(uint16_t idx, uint8_t gpi) : Control(idx, gpi) {
    }

    bool read(uint32_t now) {
      if (pending)
        return false;
      if (now < last_change + 100)
        return false;
      if (now < sched_start)
        return false;
      if (start_time == 0) {
        start_time = now;
        pinMode(pin, INPUT);
        return false;
      }
      if(digitalRead(pin))
        return false;

      int32_t val = (now > start_time + timeout);
      if (val != value) {
        value = val;
        last_change = now;
        pending = true;
      }
      start_time = 0;
      sched_start = now + 10;
      pinMode(pin, INPUT_PULLUP);
      return pending;
    }

    void set_timeout(uint16_t t) {
      timeout = t;
    }

  private:
    uint32_t sched_start = 0;
    uint32_t start_time = 0;
    uint32_t timeout = 3; //!@todo tune sensitivity
};

// class representing a ADC
class ADC : public Control {
  public:

    ADC(uint16_t idx, uint8_t gpi) : Control(idx, gpi) {
      value = analogRead(gpi);
    };

    bool read(uint32_t now) {
      if (pending)
        return false;
      sum += analogRead(pin);
      if (++count > ADC_AVERAGE) {
        int32_t val = sum / ADC_AVERAGE;
        if (abs(value - val) > ADC_HYST) {
            value = val;
            pending = true;
        }
        count = 0;
        sum = 0;
      }
      return pending;
    };

    void reset(){
      Control::reset();
      sum = 0;
      count = 0;
    };

  private:
    uint32_t sum = 0;
    uint32_t count = 0;
};

class LED : public Control {
  public:

    LED(uint16_t idx, uint8_t gpi) : Control(idx, gpi) {
      pinMode(gpi, OUTPUT);
      pending = false;
    };

    void write(uint32_t val) {
      analogWrite(pin, val);
    };
};

const static uint16_t num_switches = sizeof(switch_pins) / sizeof(uint16_t);
const static uint16_t num_adcs = sizeof(adc_pins) / sizeof(uint16_t);
const static uint16_t num_leds = sizeof(led_pins) / sizeof(uint16_t);
const static uint16_t num_dests = sizeof(dest_pins) / sizeof(uint16_t);
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
    controls[index++] = new Touch(index + 1, dest_pins[i]);

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
        ((Touch*)(controls[3]))->set_timeout(val);
        Serial.printf("Changed output to %d\n", val);
      } else if (i == 3)
        Serial.printf("Touch: %d\n", controls[i]->get_value());
    }
  }
}

// Handle received I2C data
void onI2CReceive(int count){
  Serial.printf("onI2cReceive(%d)\n", count);
  if(count < 1)
    return;
  while(Wire.available()) {
    int rx_data = Wire.read();
    switch(rx_data) {
      case 0xfe:
        if (Wire.available()) {
          rx_data = Wire.read();
          if(rx_data > 10 && rx_data < 110) {
            Wire.begin(rx_data, true);
            Wire.onRequest(onI2Crequest);
            configured = true;
            Serial.printf("Changed I2C address to %02x\n", rx_data);
            digitalWrite(NEXT_MODULE_PIN, HIGH);
          }
        }
        break;
      case 0xff:
        reset();
        break;
      default:
        command = rx_data;
    }
  }
}

// Handle I2C request for data
void onI2Crequest() {
  uint8_t buffer[32];
  bzero(buffer, 32);
  switch(command) {
    case 0:
      *(uint16_t*)(buffer) = 1;
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
  }
  Wire.write(buffer, 6);
}
