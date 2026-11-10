#include "app.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <miniz.h>
#include "trust_roots.h"

static bool getJson(const String& url, JsonDocument& output, String& error, bool qweather = false) {
  if (!clockReady()) { error = "等待阿里云校时"; return false; }
  WiFiClientSecure client;
  client.setCACert(TRUST_ROOTS);
  client.setHandshakeTimeout(10);
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(12000);
  http.useHTTP10(true);
  if (!http.begin(client, url)) { error = "HTTP初始化失败"; return false; }
  if (qweather) http.addHeader("X-QW-Api-Key", cfg.apiKey);
  http.addHeader("Accept-Encoding", "gzip");
  int status = http.GET();
  if (status != 200) {
    error = "HTTP " + String(status);
    http.end(); client.stop(); return false;
  }
  if (http.getSize() > 65536) { error = "响应过大"; http.end(); return false; }
  String raw = http.getString();
  http.end(); client.stop();
  if (raw.isEmpty() || raw.length() > 65536) { error = "响应为空或过大"; return false; }
  const uint8_t* data = reinterpret_cast<const uint8_t*>(raw.c_str());
  uint8_t* expanded = nullptr;
  size_t length = raw.length();
  if (length >= 2 && data[0] == 0x1f && data[1] == 0x8b) {
    if (length < 18 || data[2] != 8 || (data[3] & 0xe0)) { error = "gzip头无效"; return false; }
    size_t pos = 10;
    const uint8_t flags = data[3];
    if (flags & 4) {
      if (pos + 2 > length - 8) { error = "gzip扩展字段无效"; return false; }
      size_t extra = data[pos] | (data[pos + 1] << 8); pos += 2 + extra;
    }
    if (flags & 8) { while (pos < length - 8 && data[pos++]) {} }
    if (flags & 16) { while (pos < length - 8 && data[pos++]) {} }
    if (flags & 2) pos += 2;
    if (pos >= length - 8) { error = "gzip内容截断"; return false; }
    uint32_t expected = static_cast<uint32_t>(data[length-4]) | (static_cast<uint32_t>(data[length-3]) << 8)
        | (static_cast<uint32_t>(data[length-2]) << 16) | (static_cast<uint32_t>(data[length-1]) << 24);
    if (expected > 65536 || expected == 0) { error = "gzip解压长度无效"; return false; }
    expanded = static_cast<uint8_t*>(malloc(expected));
    if (!expanded) { error = "解压内存不足"; return false; }
    size_t result = tinfl_decompress_mem_to_mem(expanded, expected, data + pos, length - pos - 8,
                                                TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    if (result != expected) { free(expanded); error = "gzip解压失败"; return false; }
    data = expanded; length = result;
  }
  auto jsonError = deserializeJson(output, data, length);
  free(expanded);
  if (jsonError) { error = "JSON解析失败"; return false; }
  if (qweather && output["code"].as<String>() != "200") {
    error = "和风返回 " + output["code"].as<String>(); return false;
  }
  return true;
}

bool lookupCities(const String& query, JsonDocument& output, String& error) {
  if (cfg.apiKey.isEmpty() || cfg.apiHost.isEmpty()) { error = "请先配置和风凭据"; return false; }
  String encoded;
  for (unsigned char c : query) {
    if (isalnum(c) && c < 128) encoded += static_cast<char>(c);
    else { char hex[4]; snprintf(hex, sizeof(hex), "%%%02X", c); encoded += hex; }
  }
  return getJson("https://" + cfg.apiHost + "/geo/v2/city/lookup?range=cn&number=10&location=" + encoded, output, error, true);
}

bool fetchWeather(String& error) {
  if (cfg.apiHost.isEmpty() || cfg.apiKey.isEmpty()) { error = "尚未配置和风凭据"; return false; }
  JsonDocument d;
  String base = "https://" + cfg.apiHost;
  if (!getJson(base + "/v7/weather/now?location=" + cfg.location + "&lang=zh", d, error, true)) return false;
  const char* raw = d["now"]["temp"];
  if (!raw) { error = "天气缺少气温"; return false; }
  char* end;
  float value = strtof(raw, &end);
  if (end == raw || *end || !isfinite(value) || value < -90 || value > 65) { error = "天气气温无效"; return false; }
  Weather next = weather;
  next.valid = true; next.temp = value;
  next.text = d["now"]["text"].as<String>(); next.icon = d["now"]["icon"].as<int>();
  next.observedAt = d["now"]["obsTime"].as<String>(); next.receivedAt = time(nullptr); next.location = cfg.location;
  d.clear();
  String dailyError;
  if (getJson(base + "/v7/weather/3d?location=" + cfg.location + "&lang=zh", d, dailyError, true)) {
    auto day = d["daily"][0];
    if (day["tempMin"].is<const char*>() && day["tempMax"].is<const char*>()) {
      next.low = day["tempMin"].as<float>(); next.high = day["tempMax"].as<float>();
      next.dailyValid = isfinite(next.low) && isfinite(next.high) && next.low <= next.high;
    }
  } else {
    next.dailyValid = false;
    error = "实时天气可用；预报失败：" + dailyError;
  }
  weather = next;
  saveWeatherCache();
  return true;
}

bool fetchStock(const String& symbol, String& error) {
  JsonDocument d;
  if (!getJson("https://web.ifzq.gtimg.cn/appstock/app/minute/query?code=" + symbol, d, error)) return false;
  if (d["code"].as<int>() != 0) { error = "分时接口返回异常"; return false; }
  JsonObject data = d["data"][symbol];
  JsonArray quote = data["qt"][symbol];
  if (quote.size() < 33) { error = "股票代码无效或行情缺失"; return false; }
  Stock next;
  next.symbol = symbol; next.name = quote[1].as<String>();
  next.price = quote[3].as<float>(); next.previous = quote[4].as<float>();
  next.open = quote[5].as<float>();
  if(!isfinite(next.open) || next.open<0)next.open=0;
  next.stamp = quote[30].as<String>();
  if (!(next.price > 0) || !(next.previous > 0) || next.stamp.length() != 14 || !isfinite(next.price) || !isfinite(next.previous)) {
    error = "行情价格或时间戳无效"; return false;
  }
  next.change = next.price - next.previous;
  next.percent = next.change / next.previous * 100;
  next.minuteDate = data["data"]["date"].as<String>();
  int lastOffset = -1;
  if (next.minuteDate == next.stamp.substring(0, 8)) {
    for (JsonVariant v : data["data"]["data"].as<JsonArray>()) {
      if (next.count >= MAX_POINTS) break;
      int hhmm; float price; double volume, amount;
      const char* row = v.as<const char*>();
      if (!row || sscanf(row, "%d %f %lf %lf", &hhmm, &price, &volume, &amount) != 4) continue;
      int offset = minuteOffset(hhmm);
      if (offset < 0 || offset < lastOffset || !(price > 0) || !isfinite(price)) continue;
      float average = volumeWeightedPrice(amount, volume);
      if (isfinite(average) && (average < price * 0.5f || average > price * 2.f)) average = NAN;
      next.points[next.count++] = {offset, price, average};
      lastOffset = offset;
    }
  }
  next.valid = true; next.receivedAt = time(nullptr);
  stock = next;
  return true;
}
