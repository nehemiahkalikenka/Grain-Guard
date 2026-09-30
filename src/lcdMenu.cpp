#include "lcdMenu.h"
#include "WIFIdriver.h"
#include "CommPrefs.h"
#include <ItemCommand.h>

extern float currentTemp;
extern float currentHumidity;
extern int   currentMoisturePercent;

LiquidCrystal_I2C lcd(0x27, 16, 2);
LiquidCrystal_I2CAdapter lcdAdapter(&lcd);
CharacterDisplayRenderer renderer(&lcdAdapter, 16, 2, 0x7E, 0x7F);
LcdMenu menu(renderer);

char wifiIPStr[16] = "No WiFi";

Button upButton(UP_PIN);
Button downButton(DOWN_PIN);
Button enterButton(ENTER_PIN);

ButtonAdapter upButtonA   (&menu, &upButton,    UP, 0, 0);
ButtonAdapter downButtonA (&menu, &downButton,  DOWN, 0, 0);
ButtonAdapter enterButtonA(&menu, &enterButton, ENTER);

// Forward declarations for MenuScreens
extern MenuScreen* welcomeScreen;
extern MenuScreen* optionsScreen;
extern MenuScreen* statusScreen;
extern MenuScreen* settingsScreen;
extern MenuScreen* aboutScreen;
extern MenuScreen* commModeScreen;

// ─── Comm Mode Lambdas ─────────────────────────────────────────────────────
auto setWiFiOnly = []() {
  commPrefs.save(CommMode::WIFI_ONLY);
  Serial.printf("[LCD] Comm mode set: %s\n", commModeLabel(CommMode::WIFI_ONLY));
};

auto setGSMOnly = []() {
  commPrefs.save(CommMode::GSM_ONLY);
  Serial.printf("[LCD] Comm mode set: %s\n", commModeLabel(CommMode::GSM_ONLY));
};

auto setBoth = []() {
  commPrefs.save(CommMode::BOTH);
  Serial.printf("[LCD] Comm mode set: %s\n", commModeLabel(CommMode::BOTH));
};

auto setAuto = []() {
  commPrefs.save(CommMode::AUTO);
  Serial.printf("[LCD] Comm mode set: %s\n", commModeLabel(CommMode::AUTO));
};

// ─── Menu Screens ──────────────────────────────────────────────────────────
MENU_SCREEN(statusScreen, statusItems,
    ITEM_VALUE("Temp",     currentTemp,            "%.1f C"),
    ITEM_VALUE("Humidity", currentHumidity,        "%.1f %%"),
    ITEM_VALUE("Moisture", currentMoisturePercent, "%d %%"),
    ITEM_BASIC(wifiIPStr),
    ITEM_SUBMENU("Back", optionsScreen)
);

MENU_SCREEN(aboutScreen, aboutItems,
    ITEM_BASIC("GrainGuard V1.0"),
    ITEM_SUBMENU("Back", optionsScreen)
);

MENU_SCREEN(commModeScreen, commModeItems,
    ITEM_BASIC("COMM MODE"),
    ITEM_COMMAND("WiFi Only",  setWiFiOnly),
    ITEM_COMMAND("GSM Only",   setGSMOnly),
    ITEM_COMMAND("WiFi + GSM", setBoth),
    ITEM_COMMAND("Auto",       setAuto),
    ITEM_SUBMENU("Back", optionsScreen)
);

MENU_SCREEN(settingsScreen, settingsItems,
    ITEM_BASIC("SETTINGS"),
    ITEM_COMMAND("Reset WiFi", []() { requestWiFiReset(); }),
    ITEM_SUBMENU("Back", optionsScreen)
);

MENU_SCREEN(optionsScreen, optionItems,
    ITEM_BASIC("OPTIONS"),
    ITEM_SUBMENU("Status", statusScreen),
    ITEM_SUBMENU("Comm Mode", commModeScreen),
    ITEM_SUBMENU("Settings", settingsScreen),
    ITEM_SUBMENU("Back", welcomeScreen)
);

MENU_SCREEN(welcomeScreen, welcomeItems,
    ITEM_BASIC("WELCOME GRAIN-"),
    ITEM_BASIC("GUARD"),
    ITEM_SUBMENU("Options", optionsScreen),
    ITEM_SUBMENU("About",   aboutScreen)
);

// ─── Initialization Functions ──────────────────────────────────────────────
void lcdMenuSetup() {
    lcd.init();
    lcd.backlight();
    renderer.begin();
    menu.setScreen(welcomeScreen);
    upButton.begin();
    downButton.begin();
    enterButton.begin();
}

void buttonListener() {
    upButtonA.observe();
    downButtonA.observe();
    enterButtonA.observe();
}