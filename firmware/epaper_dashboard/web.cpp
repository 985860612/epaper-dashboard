#include "app.h"
#include <WiFi.h>
#include "web_gzip.h"
#include "almanac.h"

WebServer server(80);
extern time_t lastNtpSync;

static bool auth() {
  if (server.authenticate("admin", cfg.adminPassword.c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, "epaper-dashboard"); return false;
}
static void sendJson(int code, JsonDocument& data) {
  String body; serializeJson(data, body);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json; charset=utf-8", body);
}
static void fail(int code, const String& error) { JsonDocument d; d["error"] = error; sendJson(code,d); }
static String formatTime(time_t value) {
  if (!value) return "";
  tm t{}; localtime_r(&value,&t); char out[32]; strftime(out,sizeof(out),"%Y-%m-%d %H:%M:%S",&t); return out;
}

void setupWeb() {
  server.on("/", HTTP_GET, [] { if (auth()) { keepPowerAwake();server.sendHeader("Cache-Control","no-store"); server.sendHeader("Content-Encoding","gzip"); server.send_P(200,"text/html; charset=utf-8",reinterpret_cast<const char*>(WEB_GZIP),sizeof(WEB_GZIP)); } });
  server.on("/api/power/awake",HTTP_POST,[] {
    if(!auth())return;
    if(!server.header("Content-Type").startsWith("application/json")) { fail(415,"需要JSON请求");return; }
    keepPowerAwake();JsonDocument d;d["ok"]=true;sendJson(200,d);
  });
  server.on("/api/power/sleep",HTTP_POST,[] {
    if(!auth())return;
    if(!server.header("Content-Type").startsWith("application/json")) { fail(415,"需要JSON请求");return; }
    closePowerWindow();JsonDocument d;d["ok"]=true;sendJson(200,d);
  });
  server.on("/api/font-test",HTTP_POST,[] {
    if(!auth())return;
    if(rendering) {fail(409,"屏幕正在刷新，请稍后切换");return;}
    if(!server.header("Content-Type").startsWith("application/json")) {fail(415,"需要JSON请求");return;}
    JsonDocument body;
    if(deserializeJson(body,server.arg("plain")) || !body["enabled"].is<bool>()) {fail(400,"需要enabled开关");return;}
    fontTestActive=body["enabled"].as<bool>();++settingsRevision;needRefresh=true;
    keepPowerAwake();JsonDocument d;d["ok"]=true;sendJson(200,d);
  });
  server.on("/api/config", HTTP_GET, [] { if (!auth()) return; JsonDocument d; settingsJson(d); sendJson(200,d); });
  server.on("/api/config", HTTP_POST, [] {
    if (!auth()) return;
    if(rendering) { fail(409,"屏幕正在刷新，请稍后保存");return; }
    if (!server.header("Content-Type").startsWith("application/json")) { fail(415,"需要JSON请求"); return; }
    String raw = server.arg("plain");
    if (raw.length()>8192) { fail(413,"配置过大"); return; }
    JsonDocument d;
    if (deserializeJson(d,raw) || !d.is<JsonObject>()) { fail(400,"JSON配置无效"); return; }
    String error;
    if (!applySettings(d.as<JsonObjectConst>(),error)) { fail(400,error); return; }
    keepPowerAwake();JsonDocument response; response["ok"] = true; sendJson(200,response);
  });
  server.on("/api/status", HTTP_GET, [] {
    if (!auth()) return;
    JsonDocument d;
    environmentJson(d);
    d["version"] = VERSION;d["stockPreviewSeconds"]=stockPreviewSeconds();d["fontTestActive"]=fontTestActive;
    d["configSeconds"]=configRemainingSeconds();d["sleepCount"]=sleepCount;d["sleepTotalMs"]=sleepTotalMs;d["lastSleepMs"]=lastSleepMs;d["powerError"]=powerError;
    d["cardMode"]=cfg.cardMode;d["cardCode"]=CARD_CHOICES[static_cast<int>(selectedCard())].code;
    d["cardName"]=CARD_CHOICES[static_cast<int>(selectedCard())].name;d["nextCardSeconds"]=nextCardSeconds();
    d["almanacFirstYear"]=ALMANAC_FIRST_YEAR;d["almanacLastYear"]=ALMANAC_LAST_YEAR;
    d["uptimeMs"]=millis();d["loopCount"]=loopCount;d["screenCheckCount"]=screenCheckCount;
    d["weatherFetchCount"]=weatherFetchCount;d["stockFetchCount"]=stockFetchCount;
    d["configuredView"]=cfg.view;d["displayedView"]=displayedView;d["displayedAt"]=formatTime(lastDisplayedEpoch);
    d["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    d["time"] = clockReady() ? formatTime(time(nullptr)) : "等待阿里云校时";
    d["lastNtpSync"] = formatTime(lastNtpSync); d["ntpIntervalSeconds"] = 3600;
    d["phase"] = phaseCode(currentPhase()); d["rendering"] = rendering;
    d["weatherError"] = weatherError; d["stockError"] = stockError; d["displayError"] = displayError;
    d["freeHeap"] = ESP.getFreeHeap();
    d["lastFrameKind"] = lastFrameKind; d["lastFrameElapsedMs"] = lastFrameElapsedMs;
    d["partialStatus"] = partialStatus;d["redPartialReady"]=redPartialReady;
    d["fullRefreshCount"] = fullRefreshCount; d["partialRefreshCount"] = partialRefreshCount;
    auto w = d["weather"].to<JsonObject>();
    w["valid"] = weather.valid; w["text"] = weather.text; w["temp"] = weather.temp;
    w["observedAt"] = weather.observedAt; w["receivedAt"] = weather.receivedAt;
    auto s = d["stock"].to<JsonObject>();
    s["valid"] = stock.valid; s["symbol"] = stock.symbol; s["name"] = stock.name;
    s["price"] = stock.price; s["previous"] = stock.previous; s["open"] = stock.open; s["stamp"] = stock.stamp;
    s["minuteDate"] = stock.minuteDate; s["receivedAt"] = stock.receivedAt;
    auto points = s["points"].to<JsonArray>();
    for (int i=0;i<stock.count;++i) { auto a=points.add<JsonArray>(); a.add(stock.points[i].offset); a.add(stock.points[i].price); if(isfinite(stock.points[i].average)) a.add(stock.points[i].average); else a.add(nullptr); }
    sendJson(200,d);
  });
  server.on("/api/card/next",HTTP_POST,[] {
    if(!auth())return;
    if(rendering) { fail(409,"屏幕正在刷新，请稍后切换");return; }
    if(cfg.cardMode!="rotate" || cfg.view=="clock" || showingStock()) { fail(409,"当前是固定显示或交易时段股票优先");return; }
    if(!server.header("Content-Type").startsWith("application/json")) { fail(415,"需要JSON请求");return; }
    advanceCard();JsonDocument d;d["message"]="已切换下一张卡片";sendJson(200,d);
  });
  server.on("/api/cities",HTTP_GET,[] {
    if (!auth()) return;
    String q=server.arg("q"); q.trim();
    if(q.isEmpty() || q.length()>80) { fail(400,"请输入城市或区县"); return; }
    if(rendering) { fail(409,"屏幕正在刷新，请稍后搜索"); return; }
    JsonDocument data; String error;
    if(!lookupCities(q,data,error)) { fail(502,error); return; }
    JsonDocument response; auto list=response["locations"].to<JsonArray>();
    for(JsonObject item:data["location"].as<JsonArray>()) {
      auto loc=list.add<JsonObject>();
      for(const char* key:{"id","name","adm1","adm2"}) loc[key]=item[key];
    }
    sendJson(200,response);
  });
  server.on("/api/refresh",HTTP_POST,[] {
    if(!auth()) return;
    if(!server.header("Content-Type").startsWith("application/json")) { fail(415,"需要JSON请求"); return; }
    uint32_t gap=cfg.partialEnabled ? (cfg.refreshMode=="bw_partial" ? 15000 : cfg.refreshMode=="color_partial" ? 30000 : 120000) : 120000;
    static uint32_t lastManualRequest=0;
    if(rendering || (lastManualRequest && millis()-lastManualRequest<gap)) { fail(429,"请等待本轮刷新间隔"); return; }
    lastManualRequest=millis();needFetch=needRefresh=true;
    JsonDocument d; d["message"]="已安排更新，请稍候"; sendJson(200,d);
  });
  const char* headers[]={"Content-Type"}; server.collectHeaders(headers,1);
  server.onNotFound([]{if(auth())fail(404,"页面不存在");});
  server.begin();
}

void serviceWeb() { servicePowerButton();server.handleClient(); }
