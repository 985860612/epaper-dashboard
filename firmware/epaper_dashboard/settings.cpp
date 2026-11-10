#include "app.h"
#include <Preferences.h>

Settings cfg;
Weather weather;
Stock stock;

static bool digits(const String& s) {
  for (char c : s) if (!isDigit(c)) return false;
  return s.length() > 0;
}

void settingsJson(JsonDocument& doc, bool secrets) {
  doc["ssid"] = cfg.ssid;
  doc["apiHost"] = cfg.apiHost;
  doc["location"] = cfg.location;
  doc["city"] = cfg.city;
  doc["view"] = cfg.view;
  doc["cardMode"]=cfg.cardMode;doc["fixedCard"]=cfg.fixedCard;doc["cardRotationSeconds"]=cfg.cardRotationSeconds;
  if(!secrets) { auto choices=doc["cardOptions"].to<JsonArray>();
    for(const auto& c:CARD_CHOICES) { auto item=choices.add<JsonObject>();item["code"]=c.code;item["name"]=c.name; } }
  doc["refreshMode"] = cfg.refreshMode;
  doc["partialEnabled"] = cfg.partialEnabled;
  doc["refreshSeconds"] = cfg.refreshSeconds;
  doc["rotationSeconds"] = cfg.rotationSeconds;
  doc["calendarYear"] = cfg.calendarYear;
  doc["holidays"] = cfg.holidays;
  JsonArray list = doc["stocks"].to<JsonArray>();
  for (int i = 0; i < cfg.stockCount; ++i) list.add(cfg.stocks[i]);
  doc["apiKeyConfigured"] = !cfg.apiKey.isEmpty();
  doc["wifiPasswordConfigured"] = !cfg.wifiPassword.isEmpty();
  if (secrets) {
    doc["apiKey"] = cfg.apiKey;
    doc["wifiPassword"] = cfg.wifiPassword;
    doc["adminPassword"] = cfg.adminPassword;
  }
}

static bool validate(const Settings& s, String& error) {
  if (s.ssid.length() > 32 || s.wifiPassword.length() > 63) error = "Wi-Fi配置长度不正确";
  else if (s.adminPassword.length() < 12 || s.adminPassword.length() > 64) error = "管理密码需12至64字符";
  else if (s.apiKey.length() > 256) error = "API Key过长";
  else if (s.apiHost.length() && !s.apiHost.endsWith(".qweatherapi.com")) error = "请填写和风专属API Host";
  else if (!s.apiHost.isEmpty() && (s.apiHost.indexOf('/') >= 0 || s.apiHost.indexOf(':') >= 0 || s.apiHost.indexOf('@') >= 0)) error = "API Host不包含协议、路径或账号";
  else if (!digits(s.location) || s.location.length() != 9) error = "天气位置需9位LocationID";
  else if (s.city.isEmpty() || s.city.length() > 60) error = "城市名称不能为空且不超过60字节";
  else if (s.view != "auto" && s.view != "clock" && s.view != "stock" && s.view != "cards") error = "显示模式无效";
  else if(s.cardMode!="rotate" && s.cardMode!="fixed")error="卡片播放方式无效";
  else if(cardIndex(s.fixedCard.c_str())<0)error="卡片类型无效";
  else if(s.cardRotationSeconds<60 || s.cardRotationSeconds>86400)error="卡片轮换间隔需60至86400秒";
  else if (s.refreshMode != "color_full" && s.refreshMode != "bw_partial" && s.refreshMode != "color_partial") error = "刷新模式无效";
  else if (s.refreshSeconds < (s.partialEnabled ? (s.refreshMode=="bw_partial" ? 15 : s.refreshMode=="color_partial" ? 30 : 120) : 120) || s.refreshSeconds > 3600) error = "黑白局刷至少15秒，红色局刷至少30秒，全刷至少120秒";
  else if (s.rotationSeconds < 120 || s.rotationSeconds > 3600) error = "轮播间隔需120至3600秒";
  else if (s.stockCount < 1 || s.stockCount > MAX_STOCKS) error = "请配置1至8只股票";
  else if (s.calendarYear < 2026 || s.calendarYear > 2100) error = "交易日历年份无效";
  else if (s.holidays.length() > 2048) error = "休市日期过多";
  if (!error.isEmpty()) return false;
  for (int i = 0; i < s.stockCount; ++i) {
    String symbol = s.stocks[i];
    if (symbol.length() != 8 || (!symbol.startsWith("sh") && !symbol.startsWith("sz")) || !digits(symbol.substring(2))) {
      error = "股票格式示例：sh600900、sz000001"; return false;
    }
    for (int j = 0; j < i; ++j) if (symbol == s.stocks[j]) { error = "股票代码重复"; return false; }
  }
  if (!s.holidays.isEmpty()) {
    int start = 0;
    while (start < static_cast<int>(s.holidays.length())) {
      int end = s.holidays.indexOf(',', start);
      if (end < 0) end = s.holidays.length();
      String item = s.holidays.substring(start, end);
      int m = item.substring(4, 6).toInt(), d = item.substring(6).toInt();
      if (item.length() != 8 || !digits(item) || item.substring(0, 4).toInt() != s.calendarYear || m < 1 || m > 12 || d < 1 || d > 31) {
        error = "休市日期需为所选年份的YYYYMMDD，以逗号分隔"; return false;
      }
      start = end + 1;
    }
  }
  return true;
}

bool applySettings(JsonObjectConst input, String& error, bool serial) {
  Settings next = cfg;
  for (const char* key : {"ssid", "apiHost", "location", "city", "view", "holidays", "refreshMode", "cardMode", "fixedCard"}) {
    if (input[key].isNull()) continue;
    String value = input[key].as<String>();
    if (strcmp(key, "ssid") == 0) next.ssid = value;
    if (strcmp(key, "apiHost") == 0) { value.trim(); next.apiHost = value; }
    if (strcmp(key, "location") == 0) next.location = value;
    if (strcmp(key, "city") == 0) next.city = value;
    if (strcmp(key, "view") == 0) next.view = value;
    if (strcmp(key,"cardMode")==0)next.cardMode=value;
    if (strcmp(key,"fixedCard")==0)next.fixedCard=value;
    if (strcmp(key, "refreshMode") == 0) next.refreshMode = value;
    if (strcmp(key, "holidays") == 0) { value.replace("\r", ""); value.replace("\n", ","); value.replace(" ", ""); next.holidays = value; }
  }
  if (!input["partialEnabled"].isNull()) next.partialEnabled = input["partialEnabled"].as<bool>();
  if (!input["apiKey"].isNull() && input["apiKey"].as<String>().length()) next.apiKey = input["apiKey"].as<String>();
  if (!input["wifiPassword"].isNull() && input["wifiPassword"].as<String>().length()) next.wifiPassword = input["wifiPassword"].as<String>();
  if (input["clearWifiPassword"] == true) next.wifiPassword = "";
  if (!input["adminPassword"].isNull() && input["adminPassword"].as<String>().length()) next.adminPassword = input["adminPassword"].as<String>();
  if (!input["refreshSeconds"].isNull()) next.refreshSeconds = input["refreshSeconds"].as<int>();
  if (!input["cardRotationSeconds"].isNull())next.cardRotationSeconds=input["cardRotationSeconds"].as<int>();
  if (!input["rotationSeconds"].isNull()) next.rotationSeconds = input["rotationSeconds"].as<int>();
  if (!input["calendarYear"].isNull()) next.calendarYear = input["calendarYear"].as<int>();
  if (!input["stocks"].isNull()) {
    if (!input["stocks"].is<JsonArrayConst>()) { error = "股票列表必须为数组"; return false; }
    auto list = input["stocks"].as<JsonArrayConst>();
    if (list.size() > MAX_STOCKS) { error = "最多8只股票"; return false; }
    next.stockCount = 0;
    for (JsonVariantConst v : list) { String s = v.as<String>(); s.trim(); s.toLowerCase(); next.stocks[next.stockCount++] = s; }
  }
  if (!validate(next, error)) return false;
  Settings old = cfg;
  cfg = next;
  JsonDocument data;
  settingsJson(data, true);
  String stored;
  serializeJson(data, stored);
  Preferences prefs;
  if (!prefs.begin("epaperdash", false)) { cfg = old; error = "无法打开设备配置存储"; return false; }
  bool saved = prefs.putString("config", stored) == stored.length();
  prefs.end();
  if (!saved) { cfg = old; error = "设备配置保存失败"; return false; }
  if (cfg.location != old.location) { weather = Weather(); saveWeatherCache(); }
  activeStock = 0;
  if (cfg.stocks[0] != stock.symbol) stock = Stock();
  needReconnect = cfg.ssid != old.ssid || cfg.wifiPassword != old.wifiPassword || (apActive && cfg.adminPassword != old.adminPassword);
  needFetch = needRefresh = true;
  ++settingsRevision;
  return true;
}

void loadSettings() {
  Preferences prefs;
  prefs.begin("epaperdash", false);
  String raw = prefs.getString("config", "");
  prefs.end();
  JsonDocument doc;
  if (raw.length() && !deserializeJson(doc, raw)) {
    String error;
    applySettings(doc.as<JsonObjectConst>(), error, true);
    if (error.length()) Serial.println("Stored configuration invalid; setup required");
  }
  if (cfg.adminPassword.isEmpty()) {
    char random[25];
    snprintf(random, sizeof(random), "%08lx%08lx", static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
    cfg.adminPassword = random;
  }
}

void saveWeatherCache() {
  JsonDocument doc;
  doc["valid"] = weather.valid; doc["dailyValid"] = weather.dailyValid;
  doc["temp"] = weather.temp; doc["low"] = weather.low; doc["high"] = weather.high;
  doc["text"] = weather.text; doc["icon"] = weather.icon;
  doc["observedAt"] = weather.observedAt; doc["receivedAt"] = weather.receivedAt; doc["location"] = weather.location;
  String raw; serializeJson(doc, raw);
  Preferences prefs;
  prefs.begin("epaperdash", false); prefs.putString("weather", raw); prefs.end();
}

void loadWeatherCache() {
  Preferences prefs;
  prefs.begin("epaperdash", true); String raw = prefs.getString("weather", ""); prefs.end();
  JsonDocument d;
  if (deserializeJson(d, raw) || d["location"].as<String>() != cfg.location) return;
  weather.valid = d["valid"] | false; weather.dailyValid = d["dailyValid"] | false;
  weather.temp = d["temp"] | 0.f; weather.low = d["low"] | 0.f; weather.high = d["high"] | 0.f;
  weather.text = d["text"].as<String>(); weather.observedAt = d["observedAt"].as<String>();
  weather.icon = d["icon"] | 100; weather.receivedAt = d["receivedAt"] | 0L; weather.location = cfg.location;
}
