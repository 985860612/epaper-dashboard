#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <time.h>
#include "market_clock.h"
#include "card_schedule.h"

constexpr const char* VERSION = "epaper-dashboard-0.5.22";
constexpr int MAX_STOCKS = 8, MAX_POINTS = 245;
constexpr uint32_t STOCK_FULL_REFRESH_INTERVAL_MS = 10UL*60UL*1000UL;
constexpr bool stockFullRefreshDue(uint32_t nowMs,uint32_t lastFullMs) {
  return static_cast<uint32_t>(nowMs-lastFullMs)>=STOCK_FULL_REFRESH_INTERVAL_MS;
}

struct Settings {
  String ssid, wifiPassword, apiHost, apiKey, adminPassword;
  String location = "101010400", city = "北京顺义", view = "auto";
  String refreshMode = "color_full";
  String cardMode="rotate", fixedCard="big_calendar";
  int cardRotationSeconds=600;
  bool partialEnabled = false;
  String stocks[MAX_STOCKS] = {"sh600900"};
  int stockCount = 1, refreshSeconds = 120, rotationSeconds = 300;
  int calendarYear = 2026;
  String holidays = "20260101,20260102,20260216,20260217,20260218,20260219,20260220,20260223,20260406,20260501,20260504,20260505,20260619,20260925,20261001,20261002,20261005,20261006,20261007";
};

struct Weather {
  bool valid = false, dailyValid = false;
  float temp = 0, low = 0, high = 0;
  int icon = 100;
  String text, observedAt, location;
  time_t receivedAt = 0;
};

struct Point { int offset; float price, average; };
struct Stock {
  bool valid = false;
  String symbol, name, stamp, minuteDate;
  float price = 0, previous = 0, open = 0, change = 0, percent = 0;
  time_t receivedAt = 0;
  Point points[MAX_POINTS];
  int count = 0;
};

extern Settings cfg;
extern Weather weather;
extern Stock stock;
extern WebServer server;
extern bool needRefresh, needFetch, needReconnect;
extern bool apActive, rendering;
extern int activeStock;
extern String weatherError, stockError, displayError;
extern uint32_t lastRefreshMs;
extern uint32_t settingsRevision, lastFrameElapsedMs, fullRefreshCount, partialRefreshCount;
extern String lastFrameKind, partialStatus;

bool clockReady();
Phase currentPhase();
String dateCompact();
const char* phaseCode(Phase p);
void loadSettings();
bool applySettings(JsonObjectConst input, String& error, bool serial = false);
void settingsJson(JsonDocument& doc, bool secrets = false);
void loadWeatherCache();
void saveWeatherCache();
bool fetchWeather(String& error);
bool fetchStock(const String& symbol, String& error);
bool lookupCities(const String& query, JsonDocument& doc, String& error);
void setupWeb();
void serviceWeb();
void initScreen();
void refreshScreen();

extern uint32_t loopCount, screenCheckCount, weatherFetchCount, stockFetchCount;
extern time_t lastDisplayedEpoch;
extern String displayedView;
bool showingStock();
CardId selectedCard();
void advanceCard();
void noteCardShown();
uint32_t nextCardSeconds();

extern bool redPartialReady;

void initPowerManager();
void servicePowerButton();
void keepPowerAwake();
void closePowerWindow();
uint32_t configRemainingSeconds();
bool powerSleepAllowed();
bool enterMinuteSleep(uint32_t durationMs);
uint32_t screenMaintenanceDelayMs();
extern uint32_t sleepCount,sleepTotalMs,lastSleepMs;
extern String powerError;

extern bool fontTestActive;

uint32_t stockPreviewSeconds();

void environmentJson(JsonDocument& doc);
void environmentHistoryJson(JsonDocument& doc,bool samples=false);
