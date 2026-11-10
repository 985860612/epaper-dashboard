#include "app.h"
#include <WiFi.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include "power_schedule.h"
#include <sys/time.h>

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Select ESP32-S3 with USB CDC enabled"
#endif
SET_LOOP_TASK_STACK_SIZE(32768);

bool needRefresh=true, needFetch=true, needReconnect=false;
bool apActive=false, rendering=false;
bool redPartialReady=false;
bool fontTestActive=false;
uint32_t loopCount=0, screenCheckCount=0, weatherFetchCount=0, stockFetchCount=0;
time_t lastDisplayedEpoch=0;
String displayedView="pending";
int activeStock=0;
uint32_t lastRefreshMs=0;
uint32_t settingsRevision=0,lastFrameElapsedMs=0,fullRefreshCount=0,partialRefreshCount=0;
String lastFrameKind="none",partialStatus="pending";
String weatherError, stockError, displayError;
time_t lastNtpSync=0;
static volatile bool syncNotification=false;
static uint32_t wifiAttempt=0, lastWeatherMs=0, lastStockMs=0, lastRotateMs=0;
static bool wasOnline=false, ntpStarted=false, sleepReconnect=false;
static time_t lastStockMinute=0;
static Phase previousPhase=Phase::Unknown;

bool clockReady() { return time(nullptr)>1767225600; }
String dateCompact() {
  time_t epoch=time(nullptr); tm now{}; localtime_r(&epoch,&now);
  char date[9]; strftime(date,sizeof(date),"%Y%m%d",&now); return date;
}
Phase currentPhase() {
  time_t epoch=time(nullptr); tm t{}; localtime_r(&epoch,&t);
  bool day=isTradingDate(t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_wday,cfg.calendarYear,cfg.holidays.c_str());
  return marketPhase(clockReady(),t.tm_year+1900==cfg.calendarYear,day,t.tm_hour*60+t.tm_min);
}
const char* phaseCode(Phase p) {
  switch(p) {
    case Phase::Auction:return "auction";
    case Phase::Opening:return "opening";
    case Phase::Trading:return "trading";
    case Phase::Closing:return "closing_auction";
    case Phase::Unknown:return "calendar_or_clock_unknown";
    default:return "closed";
  }
}

static uint8_t activeCardIndex=0;
static uint32_t cardStartedMs=0;
static bool cardHasStarted=false;
static uint32_t stockPreviewUntil=0;
uint32_t stockPreviewSeconds() {
  if(!stockPreviewUntil)return 0;int32_t left=static_cast<int32_t>(stockPreviewUntil-millis());
  if(left<=0) {stockPreviewUntil=0;return 0;}return (static_cast<uint32_t>(left)+999)/1000;
}
bool showingStock() {
  if(fontTestActive)return false;
  if(cfg.view=="auto" && cfg.cardMode=="rotate" && stockPreviewSeconds())return true;
  return stockTakesPriority(cfg.view=="stock",cfg.view=="auto",cfg.cardMode=="fixed",stockPage(currentPhase()));
}
CardId selectedCard() {
  if(cfg.view=="clock")return CardId::MinimalClock;
  if(cfg.cardMode=="fixed")return static_cast<CardId>(cardIndex(cfg.fixedCard.c_str()));
  return static_cast<CardId>(activeCardIndex);
}
void advanceCard() {
  activeCardIndex=nextCardIndex(activeCardIndex);cardStartedMs=millis();needRefresh=true;
}
void noteCardShown() { cardHasStarted=true;cardStartedMs=millis(); }
uint32_t nextCardSeconds() {
  if(!cardHasStarted || showingStock() || cfg.cardMode!="rotate" || cfg.view=="clock")return 0;
  uint32_t elapsed=(millis()-cardStartedMs)/1000;
  return elapsed>=static_cast<uint32_t>(cfg.cardRotationSeconds)?0:cfg.cardRotationSeconds-elapsed;
}

static void startWifi() {
  WiFi.persistent(false);
  WiFi.mode(apActive ? WIFI_AP_STA : WIFI_STA);
  // Keep association and the configuration web server available between fetches.
  bool powerSave=WiFi.setSleep(WIFI_PS_MAX_MODEM);
  Serial.printf("POWER_WIFI requested=max_modem applied=%d\n",powerSave);
  WiFi.setAutoReconnect(true);
  if(cfg.ssid.length()) WiFi.begin(cfg.ssid.c_str(),cfg.wifiPassword.c_str());
  wifiAttempt=millis();
}
static void startAP() {
  if(apActive)return;
  WiFi.mode(WIFI_AP_STA);
  String name="EPaper-"+String(static_cast<uint32_t>(ESP.getEfuseMac()),HEX);
  apActive=WiFi.softAP(name.c_str(),cfg.adminPassword.c_str());
  Serial.println(apActive ? "SETUP_AP_READY" : "SETUP_AP_FAILED");
  needRefresh=true;
}

static void serialCommands() {
  static String line;
  while(Serial.available()) {
    char c=Serial.read();
    if(c=='\n') {
      JsonDocument d;
      auto err=deserializeJson(d,line); line="";
      if(err) { Serial.println("{\"event\":\"invalid_command\"}"); continue; }
      String cmd=d["command"].as<String>();
      if(cmd=="provision") {
        String error;
        if(applySettings(d["config"].as<JsonObjectConst>(),error,true)) {
          Serial.println("{\"event\":\"provisioned\"}");
          needReconnect=true;
        } else { JsonDocument response; response["event"]="provision_failed"; response["error"]=error; serializeJson(response,Serial); Serial.println(); }
      } else if(cmd=="status") {
        JsonDocument response;
        response["event"]="status"; response["version"]=VERSION;
        response["ip"]=WiFi.status()==WL_CONNECTED ? WiFi.localIP().toString() : "";
        response["clockReady"]=clockReady(); response["lastNtpSync"]=lastNtpSync;
        response["phase"]=phaseCode(currentPhase()); response["ap"]=apActive;
        response["freeHeap"]=ESP.getFreeHeap();
        response["cpuMHz"]=getCpuFrequencyMhz();
        wifi_ps_type_t ps=WIFI_PS_NONE;
        response["wifiPowerSave"]=esp_wifi_get_ps(&ps)==ESP_OK?static_cast<int>(ps):-1;
        response["idlePollMs"]=20;
        response["sleepCount"]=sleepCount;response["sleepTotalMs"]=sleepTotalMs;
        response["configSeconds"]=configRemainingSeconds();response["powerError"]=powerError;
        response["weatherValid"]=weather.valid; response["weatherError"]=weatherError;
        response["stockValid"]=stock.valid; response["stockError"]=stockError;
        response["stockPoints"]=stock.count; response["quoteStamp"]=stock.stamp;
        response["lastRefreshMs"]=lastRefreshMs; response["displayError"]=displayError;
        serializeJson(response,Serial); Serial.println();
      } else if(cmd=="preview_stock") {stockPreviewUntil=millis()+600000UL;++settingsRevision;needRefresh=true;Serial.println("stock_preview_10min");
      } else if(cmd=="font_test") { fontTestActive=d["enabled"]|true;++settingsRevision;needRefresh=true;Serial.println("{\"event\":\"font_test\"}");
      } else if(cmd=="power_awake") { keepPowerAwake();Serial.println("{\"event\":\"power_awake\"}"); }
      else if(cmd=="power_sleep") { closePowerWindow();Serial.println("{\"event\":\"power_sleep\"}"); }
      else Serial.println("{\"event\":\"unknown_command\"}");
    } else if(c!='\r') {
      if(line.length()<8192) line+=c; else line="";
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setRxBufferSize(8192);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(1500);
  Serial.printf("\nSTART %s\n",VERSION);
  const bool cpuReduced=setCpuFrequencyMhz(80);
  Serial.printf("POWER_CPU requested_mhz=80 actual_mhz=%lu applied=%d\n",static_cast<unsigned long>(getCpuFrequencyMhz()),cpuReduced);
  setenv("TZ","CST-8",1); tzset();
  loadSettings(); loadWeatherCache();
  needReconnect=false;initPowerManager();
  startWifi(); setupWeb(); initScreen();
  // Automatic clock sync is handled by the SNTP task, independently of screen refresh.
  esp_sntp_set_time_sync_notification_cb([](struct timeval*) { syncNotification=true; });
}

void loop() {
  ++loopCount;
  serialCommands(); serviceWeb();
  if(needReconnect) {
    needReconnect=false;
    if(apActive) { WiFi.softAPdisconnect(true); apActive=false; }
    WiFi.disconnect(); wasOnline=false; startWifi();
    lastWeatherMs=lastStockMs=0;
  }
  bool online=WiFi.status()==WL_CONNECTED;
  if(online && !wasOnline) {
    Serial.printf("WIFI_CONNECTED ip=%s\n",WiFi.localIP().toString().c_str());
    if(!ntpStarted) {
      configTzTime("CST-8","ntp1.aliyun.com","ntp2.aliyun.com","ntp3.aliyun.com");
      esp_sntp_set_sync_interval(3600000UL); ntpStarted=true;
    } else if(!lastNtpSync || time(nullptr)-lastNtpSync>=3600)esp_sntp_restart();
    if(!sleepReconnect)needFetch=true;
    sleepReconnect=false;
  }
  wasOnline=online;
  if(!online && millis()-wifiAttempt>30000) { startAP(); startWifi(); }
  if(syncNotification) {
    syncNotification=false; lastNtpSync=time(nullptr);
    Serial.printf("NTP_SYNC_OK interval_ms=%lu\n",static_cast<unsigned long>(esp_sntp_get_sync_interval()));
    needFetch=needRefresh=true;
  }
  Phase phase=currentPhase();
  if(phase!=previousPhase) { previousPhase=phase; needRefresh=needFetch=true; }
  bool showStocks=showingStock();
  bool rotating=!fontTestActive && cfg.cardMode=="rotate" && cfg.view!="clock" && cfg.view!="stock";
  if(showStocks || (!online && !sleepReconnect) || !clockReady() || !rotating)cardStartedMs=millis();
  else if(cardHasStarted && cardRotationDue(millis(),cardStartedMs,cfg.cardRotationSeconds))advanceCard();
  bool wantsStock=!fontTestActive && (showStocks || selectedCard()==CardId::Overview || rotating);
  uint32_t now=millis();
  if(showStocks && cfg.stockCount>1 && lastRotateMs && now-lastRotateMs>=static_cast<uint32_t>(cfg.rotationSeconds)*1000) {
    activeStock=(activeStock+1)%cfg.stockCount; stock=Stock(); needFetch=true;
    lastRotateMs=now;
  }
  if(!lastRotateMs) lastRotateMs=now;
  if(online && clockReady()) {
    bool forced=needFetch;
    needFetch=false;
    if(forced || !lastWeatherMs || now-lastWeatherMs>=1800000UL) {
      weatherError="";
      ++weatherFetchCount;
      bool ok=fetchWeather(weatherError);
      lastWeatherMs=now;
      if(!ok) lastWeatherMs=now-1740000UL; // Retry after one minute; preserve valid cache.
      needRefresh=true;
      Serial.printf("WEATHER_FETCH ok=%d free_heap=%u\n",ok,ESP.getFreeHeap());
      serviceWeb();
    }
    bool minuteSample=cfg.refreshSeconds==60 && time(nullptr)/60!=lastStockMinute;
    if(wantsStock && (forced || minuteSample || !lastStockMs || now-lastStockMs>=static_cast<uint32_t>(cfg.refreshSeconds)*1000)) {
      stockError="";
      ++stockFetchCount;
      bool ok=fetchStock(cfg.stocks[activeStock],stockError);
      lastStockMs=now;lastStockMinute=time(nullptr)/60;needRefresh=true;
      Serial.printf("STOCK_FETCH ok=%d points=%d free_heap=%u\n",ok,stock.count,ESP.getFreeHeap());
    }
  }
  // Do not draw the same stale frame before NTP or Wi-Fi has a chance to connect.
  bool ready=online && (clockReady() || millis()>30000);
  static uint32_t lastScreenCheck=0;
  if((ready || apActive) && millis()-lastScreenCheck>=1000) {
    lastScreenCheck=millis(); ++screenCheckCount; refreshScreen();
  }
  bool minuteDrawReady=!cfg.partialEnabled || cfg.refreshMode=="color_full" || sameClockMinute(time(nullptr),lastDisplayedEpoch);
  bool minuteQuoteReady=!wantsStock || cfg.refreshSeconds!=60 || sameClockMinute(time(nullptr),lastStockMinute*60);
  if(online && clockReady() && minuteDrawReady && minuteQuoteReady && !needFetch && !needRefresh && !needReconnect && powerSleepAllowed()) {
    timeval wall{};gettimeofday(&wall,nullptr);
    uint32_t wait=minuteWakeDelayMs(wall.tv_sec%60,wall.tv_usec);
    now=millis();
    if(wantsStock && cfg.refreshSeconds!=60)wait=min(wait,remainingMs(now,lastStockMs,cfg.refreshSeconds*1000UL));
    wait=min(wait,remainingMs(now,lastWeatherMs,1800000));
    wait=min(wait,screenMaintenanceDelayMs());
    if(rotating && !showStocks && cardHasStarted)wait=min(wait,remainingMs(now,cardStartedMs,cfg.cardRotationSeconds*1000UL));
    if(showStocks && cfg.stockCount>1)wait=min(wait,remainingMs(now,lastRotateMs,cfg.rotationSeconds*1000UL));
    if(enterMinuteSleep(wait)) {
      sleepReconnect=true;wasOnline=false;
      startWifi();server.begin();
    }
  }
  delay(20);
}
