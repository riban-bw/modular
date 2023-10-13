#include "universal.h"
#include <TFT_eSPI.h>
#include <Wire.h>
#include "detect.h"

#define RESET_PIN 15

TFT_eSPI tft = TFT_eSPI();

uint32_t w = 134;
uint32_t h = 239;
bool run = false;
uint32_t i2cValue = 0; // Received I2C value to read / write
uint8_t i2cCommand = 0; // Received I2C command
uint8_t selectedMenu = 0;
int8_t selectedMenuItem = 0;
uint8_t installedModuleCount = 0; //Quantity of installed modules
uint8_t selectedModule = 0;
bool updatePending = false; // True when a change of modules needs to be communicated to the brain
uint8_t srcButton = 0; // Index of selected source

SWITCH encSwitch(25);
SWITCH backSwitch(33);
ENCODER encoder(27, 26);
MODULE* available_modules[3];
MODULE* installedModules[] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
char menuEntries[8][13];

void drawMenu() {
  uint8_t maxEntry = 0;
  if (selectedMenu == MENU_REMOVE) {
    tft.fillScreen(TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("REMOVE", tft.width() / 2, 100, 4);
    tft.drawString(installedModules[selectedModule]->name, tft.width() / 2, 130, 4);
    tft.drawString("?", tft.width() / 2, 160, 4);
    tft.setTextDatum(TL_DATUM);
    return;
  }
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_LIGHTGREY);
  tft.fillRect(0, 0, tft.width(), 25, TFT_DARKGREEN);
  char title[12];
  switch (selectedMenu) {
    case MENU_HOME:
      sprintf(title, "HOME");
      for (maxEntry = 0; maxEntry < installedModuleCount; ++maxEntry) {
          sprintf(menuEntries[maxEntry], installedModules[maxEntry]->name);
      }
      if (maxEntry < 8)
        sprintf(menuEntries[maxEntry++], "+Module");
      break;
    case MENU_MODULES:
      sprintf(title, "ADD");
      for (maxEntry = 0; maxEntry < 3; ++maxEntry) {
          sprintf(menuEntries[maxEntry], available_modules[maxEntry]->name);
      }
      break;
    case MENU_MODULE:
      sprintf(title, installedModules[selectedModule]->name);
      sprintf(menuEntries[0], "INPUTS");
      sprintf(menuEntries[1], "OUTPUTS");
      sprintf(menuEntries[2], "PARAMS");
      sprintf(menuEntries[3], "REMOVE");
      maxEntry = 4;
      break;
    case MENU_SRCS:
      sprintf(title, "OUTPUTS");
      for (maxEntry = 0; maxEntry < installedModules[selectedModule]->nSrcs; ++maxEntry) {
        if (maxEntry > 7)
          break;
        sprintf(menuEntries[maxEntry], installedModules[selectedModule]->srcNames[maxEntry]);
      }
      break;
    case MENU_DESTS:
      sprintf(title, "INPUTS");
      for (maxEntry = 0; maxEntry < installedModules[selectedModule]->nDests; ++maxEntry) {
        if (maxEntry > 7)
          break;
        sprintf(menuEntries[maxEntry], installedModules[selectedModule]->dstNames[maxEntry]);
      }
      break;
    case MENU_PARAMS:
      sprintf(title, "PARAMS");
      for (maxEntry = 0; maxEntry < installedModules[selectedModule]->nParams; ++maxEntry) {
        if (maxEntry > 7)
          break;
        sprintf(menuEntries[maxEntry], installedModules[selectedModule]->paramNames[maxEntry]);
      }
      break;
  }

  tft.setTextDatum(TC_DATUM);
  tft.drawString(title, tft.width() / 2, 2, 4);
  tft.setTextDatum(TL_DATUM);

  if (selectedMenuItem >= maxEntry)
    selectedMenuItem = maxEntry - 1;
  if (selectedMenuItem < 0)
    selectedMenuItem = 0;

  tft.fillRect(0, 26 + selectedMenuItem * 26, 134, 26, TFT_LIGHTGREY);

  for (uint32_t i = 0; i < maxEntry; ++i) {
    if (i == selectedMenuItem)
      tft.setTextColor(TFT_BLACK, TFT_LIGHTGREY);
    else
      tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(menuEntries[i], 2, 26 + i * 26, 4);
  }
}

uint8_t processSwitch(SWITCH& sw, uint32_t now) {
  bool state = !digitalRead(sw.pin);
  uint8_t pressType = SWITCH_IDLE;

  if (state != sw.state && now > sw.time + DEBOUNCE_TIME) {
    sw.state = state;
    if (!state)
      if (now - sw.time > LONG_PRESS_TIME)
        pressType = SWITCH_LONG;
      else
        pressType = SWITCH_SHORT;
    else
      pressType = SWITCH_PRESS;
    sw.time = now;
  }
  return pressType;
}

void reset() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("riban", tft.width() / 2, 15, 4);
  tft.drawWedgeLine(18, 20, 18, 30, 3, 3, TFT_WHITE, TFT_BLACK);
  tft.drawWedgeLine(28, 15, 28, 35, 3, 3, TFT_WHITE, TFT_BLACK);
  tft.drawWedgeLine(115, 20, 115, 30, 3, 3, TFT_WHITE, TFT_BLACK);
  tft.drawWedgeLine(105, 15, 105, 35, 3, 3, TFT_WHITE, TFT_BLACK);
  tft.drawString("connecting", tft.width() / 2, 100, 4);
  tft.setTextDatum(TL_DATUM);
  Wire.end();
  initDetect();
  run = false;
  for (uint8_t i = 0; i < 10; ++i) {
    delete installedModules[i];
    installedModules[i] = nullptr;
  }
  installedModuleCount = 0;
}

void setup() {
  available_modules[0] = new MODULE(15, "VCA");
  available_modules[1] = new MODULE(11, "VCO");
  available_modules[2] = new MODULE(2, "MIDI");

  tft.init();
  tft.setRotation(2);
  pinMode(RESET_PIN, INPUT);
  reset();
  //Serial.begin(9600);
}

void loop() {
  static uint32_t next_sec = 0;
  static char s[10];

  // Wait for module to be detected by brain
  if (detect())
    return;

  // Handle reset
  if (!digitalRead(RESET_PIN)) {
    reset();
    return;
  }

  // Init after reset
  if (!run) {
    // First loop after reset
    startI2c();
    run = true;
    drawMenu();
  }

  uint32_t now = millis();

  // Check encoder button
  switch(processSwitch(encSwitch, now)) {
    case SWITCH_SHORT:
      // Short press release
      switch (selectedMenu) {
        case MENU_HOME:
          if (selectedMenuItem == installedModuleCount) {
            // Last item is "Add module"
            selectedMenuItem = 0;
            selectedMenu = MENU_MODULES;
          } else {
            selectedMenu = MENU_MODULE;
            selectedModule = selectedMenuItem;
            selectedMenuItem = 0;
          }
          break;
        case MENU_MODULE:
          if (selectedMenuItem == 0)
            selectedMenu = MENU_DESTS;
          else if (selectedMenuItem == 1)
            selectedMenu = MENU_SRCS;
          else if (selectedMenuItem == 2)
            selectedMenu = MENU_PARAMS;
          else if (selectedMenuItem == 3)
            selectedMenu = MENU_REMOVE;
          selectedMenuItem = 0;
          break;
        case MENU_SRCS:
          srcButton = selectedMenuItem + 1;
          break;
        case MENU_MODULES:
          // Add module
          installedModules[installedModuleCount++] = new MODULE(available_modules[selectedMenuItem]->type, available_modules[selectedMenuItem]->name);
          selectedMenu = MENU_HOME;
          selectedMenuItem = installedModuleCount - 1;
          updatePending = true;
          break;
        case MENU_REMOVE:
          delete installedModules[selectedModule];
          for (uint8_t i = selectedModule; i < 9; ++i)
            installedModules[i] = installedModules[i + 1];
          installedModules[9] = nullptr;
          --installedModuleCount;
          --selectedMenuItem;
          selectedMenu = MENU_HOME;
          updatePending = true;
          break;
      }
      drawMenu();
      break;
    case SWITCH_LONG:
    // Long press release
    tft.fillScreen(TFT_BLACK);
    sprintf(s, "I2C: 0x%02X", getI2cAddress());
    tft.drawString(s, 10, 100, 4);
    delay(1500);
    break;
  }

  // Check BACK button
  switch (processSwitch(backSwitch, now)) {
    case SWITCH_SHORT:
      switch (selectedMenu) {
        case MENU_MODULE:
          selectedMenuItem = selectedModule;
          selectedMenu = MENU_HOME;
          break;
        case MENU_MODULES:
          selectedMenuItem = 0;
          selectedMenu = MENU_HOME;
          break;
        case MENU_SRCS:
          selectedMenuItem = 1;
          selectedMenu = MENU_MODULE;
          break;
        case MENU_DESTS:
          selectedMenuItem = 0;
          selectedMenu = MENU_MODULE;
          break;
        case MENU_PARAMS:
          selectedMenuItem = 2;
          selectedMenu = MENU_MODULE;
          break;
        case MENU_REMOVE:
          selectedMenuItem = 3;
          selectedMenu = MENU_MODULE;
          break;
      }
      drawMenu();
      break;
    case SWITCH_LONG:
      selectedMenuItem = 0;
      selectedMenu = MENU_HOME;
      drawMenu();
      break;
  }

  // Check for encoder rotation
  int8_t dir = 0;
  bool clk = !digitalRead(encoder.pinClk);
  if (clk || encoder.code) {
    bool data = !digitalRead(encoder.pinData);
    encoder.code <<= 2;
    if (data)
      encoder.code |= 2;
    if (clk)
      encoder.code |= 1;
    encoder.code &= 0x0f;
    if (ENC_VALID[encoder.code]) {
      encoder.count <<= 4;
      encoder.count |= encoder.code;
      if (encoder.count == 0xd4)
        dir = +1;
      else if (encoder.count == 0x17)
        dir = -1;
      if (dir) {
        if (now >= encoder.time + ACCEL_THRES)
          encoder.value += dir; // Slow rotation
        else
          encoder.value += dir * ACCEL_FACTOR;
        encoder.time = now;
        encoder.code = 0;
        selectedMenuItem += encoder.value;
        encoder.value = 0;
        drawMenu();
      }
    }
  }

  // Test - remove whe I2C handler implemented
  if (now > next_sec) {
    next_sec = now + 1000;
    /*
    sprintf(s, "I2C: %d", getI2cAddress());
    tft.drawString(s, 5, 40, 4);
    sprintf(s, "%04d", now / 1000);
    tft.drawString(s, 2, 2, 4);
    */
  }
}

// Set module I2C address
void startI2c() {
  Wire.begin(getI2cAddress());
  Wire.onRequest(onI2Crequest);
  Wire.onReceive(onI2Creceive);
}

// Handle received I2C data
void onI2Creceive(int count) {
  //Serial.printf("I2C Rx %d\n", count);
  if (count) {
    i2cCommand = Wire.read();
    --count;
  }
  switch(i2cCommand) {
    case 0xff: // Reset
      reset();
      break;
    case 0xF1: // Set WS2812 mode and primary colour
    case 0xF2: // Set WS2812 mode and secondary colour
      break;
    case 0xF3: // Turn off all LEDs temporarily
      break;
  }
  return;
  while (count) {
    // Flush the read buffer
    Wire.read();
    --count;
  }
}

// Handle I2C request for data - assume command was set before request
void onI2Crequest() {
  static uint32_t response; // Response is always 3 bytes: 23:16=addr, 15:0=value
  static uint8_t readSessionPos = 0; // Index of session last read value
  static uint64_t readMask;
  uint8_t useResponse = false;

  response = i2cCommand << 16;
  if (i2cCommand == 0) {
    if (updatePending) {
      response = 0x010000;
      updatePending = false;
    } else if (srcButton)
      response = 0x100000 | srcButton;
    useResponse = true;
    // Get next changed value
  } else if (i2cCommand == 0x01) {
    uint8_t r[] = {0,0,0,0,0,0,0,0};
    for (uint8_t i = 0; i < installedModuleCount; ++i)
      r[i] = installedModules[i]->type;
    Wire.write(r, 8);
  } else if (i2cCommand < 0x10) {
    // General commands
  } else if (i2cCommand < 0x20) {
    // Get first bank of 32 switches
    if (srcButton)
      response |= 1 << srcButton;
    srcButton = 0;
//  } else if (i2cCommand - 0x20 < nAdcs) {
  } else if (i2cCommand == 0xF0) {
    response |= PANEL_TYPE;
    useResponse = true;
  } else if (i2cCommand == 0xF1) {
    //response = ((sizeof(BRAND) - 1) << 16) | ((sizeof(PLUGIN) - 1) << 8 | sizeof(MODEL) - 1);
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
    Wire.slaveWrite((uint8_t*)&response, 3);
  
  //Serial.printf("I2C Req %08X\n", response);
}
