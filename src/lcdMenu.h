#pragma once

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <LcdMenu.h>
#include <MenuScreen.h>
#include <ItemValue.h>
#include <ItemCommand.h>
#include <ItemSubMenu.h>
#include <display/LiquidCrystal_I2CAdapter.h>
#include <renderer/CharacterDisplayRenderer.h>
#include <Button.h>
#include <input/ButtonAdapter.h>

// 1. Give the raw pin definitions unique names
constexpr uint8_t UP_PIN    = 12;
constexpr uint8_t DOWN_PIN  = 14;
constexpr uint8_t ENTER_PIN = 26;

// 2. Clear out implementation parameters from your extern templates
extern LiquidCrystal_I2C lcd;
extern LiquidCrystal_I2CAdapter lcdAdapter;
extern CharacterDisplayRenderer renderer;
extern LcdMenu menu;
// Add with the other externs
extern char wifiIPStr[16];   // "192.168.xxx.xxx\0" — max 15 chars + null



// 3. Declare the actual Button objects as extern
extern Button upButton;
extern Button downButton;
extern Button enterButton;

extern ButtonAdapter upButtonA;
extern ButtonAdapter downButtonA;
extern ButtonAdapter enterButtonA;

void lcdMenuSetup();
void buttonListener();
