#include "app.h"
#include <SPI.h>
#include <WiFi.h>
#include <GxEPD2_3C.h>
#include "native_text.h"
#include "styled_text.h"
#include "frame_canvas.h"

class HinkPanel : public GxEPD2_420c_GDEY042Z98 {
public:
  using GxEPD2_420c_GDEY042Z98::GxEPD2_420c_GDEY042Z98;
  void allowLongRefresh() { _busy_timeout = 90000000; }
  void partialTimeout() { _busy_timeout = 8000000; }
};
static GxEPD2_3C<HinkPanel, 32> display(HinkPanel(10, 13, 7, 4));
static NativeText font;
static Adafruit_GFX* painter=&display;
static bool renderBw=false;
static uint32_t busyStart, lastBusyLog, busyCallbacks;

static void label(int x, int y, const String& text, uint16_t color = GxEPD_BLACK, bool right = false) {
  font.setForegroundColor(color);
  if (right) x -= font.getUTF8Width(text.c_str());
  font.setCursor(x, y); font.print(text);
}

static void heading(int x,int y,const String& text,uint16_t color=GxEPD_BLACK,bool right=false) {
  int width=styleWidth(text,Header18);
  if(width<0) { label(x,y,text,color,right);return; }
  styleText(*painter,right?x-width:x,y,text,Header18,color);
}
enum class PixelFace { Axis, Small, Quote };
static void latin(int x,int y,const String& text,PixelFace face,uint16_t color=GxEPD_BLACK,bool right=false,int size=1) {
  if(face!=PixelFace::Quote) { label(x,y,text,color,right);return; }
  const StyleFont& chosen=size>1 ? Clock80 : Quote48;
  int width=styleWidth(text,chosen);
  styleText(*painter,right?x-width:x,y,text,chosen,color);
}

static String clipped(const String& input, int count) {
  int characters = 0, end = 0;
  for (unsigned int i = 0; i < input.length(); ++i) {
    if ((static_cast<uint8_t>(input[i]) & 0xc0) != 0x80) {
      if (characters++ == count) break;
    }
    end = i + 1;
  }
  return input.substring(0, end);
}

static void bigName(const String& text) {
  if(styleWidth(text,Title26)<0) { label(20,73,clipped(text,13));return; }
  styleText(*painter,20,73,text,Title26,GxEPD_BLACK);
}

static String phaseLabel(Phase p) {
  switch (p) {
    case Phase::Auction: return "集合竞价";
    case Phase::Opening: return "待开盘";
    case Phase::Trading: return "交易中";
    case Phase::Closing: return "收盘竞价";
    case Phase::Unknown: return "日历待更新";
    default: return "已休市";
  }
}

static void weatherIcon(int code) {
  if (code < 104) {
    painter->drawCircle(46, 213, 12, GxEPD_BLACK);
    for (int i = 0; i < 8; ++i) {
      float a = i * PI / 4;
      painter->drawLine(46+17*cos(a), 213+17*sin(a), 46+22*cos(a), 213+22*sin(a), GxEPD_BLACK);
    }
  }
  if (code != 100 && code != 150) {
    painter->fillRect(29, 222, 40, 20, GxEPD_WHITE);
    painter->drawCircle(40, 228, 12, GxEPD_BLACK);
    painter->drawCircle(54, 221, 14, GxEPD_BLACK);
    painter->drawCircle(67, 231, 10, GxEPD_BLACK);
    painter->fillRect(33, 228, 40, 12, GxEPD_WHITE);
    painter->drawFastHLine(34, 241, 34, GxEPD_BLACK);
    if (code >= 300 && code < 500) for (int x = 38; x <= 66; x += 12) painter->drawLine(x, 247, x-3, 253, GxEPD_BLACK);
  }
}

static void drawClock(const tm& now, bool valid, const Weather& w, const String& city, Phase phase) {
  char date[40], time[8];
  if (valid) { snprintf(date, sizeof(date), "%04d.%02d.%02d", now.tm_year+1900, now.tm_mon+1, now.tm_mday); snprintf(time, sizeof(time), "%02d:%02d", now.tm_hour, now.tm_min); }
  else { strcpy(date, "等待阿里云校时"); strcpy(time, "--:--"); }
  heading(20, 29, date);
  if (valid) { const char* days[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"}; heading(380, 29, days[now.tm_wday], GxEPD_BLACK, true); }
  latin((400-styleWidth(String(time),Clock80))/2, 145, time, PixelFace::Quote, GxEPD_BLACK, false, 2);
  painter->drawFastHLine(20, 169, 360, GxEPD_BLACK);
  weatherIcon(w.valid ? w.icon : 104);
  latin(88, 237, w.valid ? String(w.temp, 0) : "--", PixelFace::Quote);
  String temperature = w.valid ? String(w.temp,0) : "--";
  painter->drawCircle(min(205,88+static_cast<int>(temperature.length())*30+8), 207, 3, GxEPD_BLACK);
  label(220, 215, w.valid ? clipped(w.text, 6) : "天气待更新");
  label(220, 242, clipped(city, 6));
  String footer = "和风天气";
  if (!w.valid) footer += " · 未获取";
  else if (!weatherError.isEmpty() || ::time(nullptr)-w.receivedAt > 3600) footer += " · 缓存";
  label(20, 293, footer);
  if (w.dailyValid) label(380, 293, String(w.low,0)+" / "+String(w.high,0)+" C", GxEPD_BLACK, true);
  if (phase == Phase::Unknown) label(20, 60, "请更新交易日历", GxEPD_RED);
}

static void drawStock(const tm& now, const Stock& s, const Weather& w, const String& city,
                      Phase phase, int index, int total, bool stale) {
  char date[24]; snprintf(date, sizeof(date), "%02d.%02d  %02d:%02d", now.tm_mon+1, now.tm_mday, now.tm_hour, now.tm_min);
  heading(20, 27, date);
  heading(380, 27, clipped(city, 4)+" "+(w.valid ? String(w.temp,0) : "--")+" C", GxEPD_BLACK, true);
  painter->drawFastHLine(20, 38, 360, GxEPD_BLACK);
  bigName(s.valid ? s.name : cfg.stocks[index]);
  char todayBuffer[9]; strftime(todayBuffer,sizeof(todayBuffer),"%Y%m%d",&now);
  String today = todayBuffer;
  bool historical = s.valid && s.stamp.substring(0,8) != today;
  label(380, 69, historical ? "历史行情" : stale ? "数据延迟" : phaseLabel(phase), GxEPD_BLACK, true);
  uint16_t color = !renderBw && s.change > 0 ? GxEPD_RED : GxEPD_BLACK;
  latin(18, 120, s.valid ? String(s.price,2) : "--.--", PixelFace::Quote, color);
  String delta = s.valid ? (s.change > 0 ? "+" : "")+String(s.change,2)+"  "+(s.percent > 0 ? "+" : "")+String(s.percent,2)+"%" : "--  --%";
  latin(380, 110, delta, PixelFace::Small, color, true);
  bool auction = (phase == Phase::Auction || phase == Phase::Opening) && !historical;
  bool hasCurve = s.valid && s.count > 0 && !auction;
  if (hasCurve) {
    const int left = 54, right = 334, top = 160, bottom = 284;
    float lo = s.previous, hi = s.previous;
    for (int i = 0; i < s.count; ++i) {
      lo = min(lo, s.points[i].price); hi = max(hi, s.points[i].price);
      if (isfinite(s.points[i].average)) { lo = min(lo, s.points[i].average); hi = max(hi, s.points[i].average); }
    }
    float margin = max((hi-lo)*0.1f, s.previous*0.002f);
    lo -= margin; hi += margin;
    auto py = [&](float v) { return bottom-static_cast<int>(lround((v-lo)/(hi-lo)*(bottom-top))); };
    for (int xx = left; xx <= right; xx += 8) painter->drawFastHLine(xx, py(s.previous), min(4, right-xx+1), GxEPD_BLACK);
    for (int i = 1; i < s.count; ++i) {
      int x0 = left+s.points[i-1].offset*(right-left)/240, x1 = left+s.points[i].offset*(right-left)/240;
      painter->drawLine(x0,py(s.points[i-1].price),x1,py(s.points[i].price),GxEPD_BLACK);
      if (isfinite(s.points[i-1].average) && isfinite(s.points[i].average) && (!renderBw || i%3!=0)) painter->drawLine(x0,py(s.points[i-1].average),x1,py(s.points[i].average),renderBw?GxEPD_BLACK:GxEPD_RED);
    }
    const Point& last = s.points[s.count-1];
    label(20, 145, isfinite(last.average) ? "均价 "+String(last.average,2) : "均价暂无", GxEPD_BLACK);
    painter->drawFastHLine(260,137,12,GxEPD_BLACK); label(278,145,"价格");
    if(renderBw) { for(int x=326;x<338;x+=4)painter->drawFastHLine(x,137,2,GxEPD_BLACK); } else painter->drawFastHLine(326,137,12,GxEPD_RED); label(344,145,"均价");
    label(50, top+6, String(hi,hi>=1000 ? 0 : 2), GxEPD_BLACK, true);
    label(50, bottom+4, String(lo,lo>=1000 ? 0 : 2), GxEPD_BLACK, true);
    label(395, top+6, String((hi/s.previous-1)*100,2)+"%", renderBw?GxEPD_BLACK:GxEPD_RED, true);
    label(395, bottom+4, String((lo/s.previous-1)*100,2)+"%", GxEPD_BLACK, true);
  } else {
    label(30, 184, auction ? "集合竞价 · 等待正式开盘" : "等待有效分时数据");
    label(30, 215, stockError.length() ? clipped(stockError,14) : "分时线从09:30开始绘制");
  }
}

static void busyCallback(const void*) {
  ++busyCallbacks; serviceWeb();
  if (millis()-lastBusyLog > 5000) {
    lastBusyLog = millis();
    Serial.printf("DISPLAY_BUSY elapsed_ms=%lu BUSY=%d\n", static_cast<unsigned long>(millis()-busyStart), digitalRead(4));
  }
  delay(1);
}

void initScreen() {
  SPI.begin(12, -1, 11, 10);
  display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.epd2.allowLongRefresh(); display.epd2.setBusyCallback(busyCallback);
  display.init(115200);
  display.setRotation(0); display.epd2.selectFastFullUpdate(false);
  font.begin(display); font.setScale(1);
}

static bool partialFailed=false;
static bool baselineValid=false;
static int probeStage=0;
static String previousTag,previousLayout,previousWeather;
static uint32_t previousRevision=0,lastFull=0,oldHeaderHash=0,oldBodyHash=0,oldClockHash=0,oldRedHash=0;

static bool partialRegion(FrameCanvas& frame,int x,int y,int w,int h,const char* kind) {
  if(frame.hasRed(x,y,w,h)) return false;
  display.epd2.partialTimeout();
  uint32_t start=millis(), callbacksBefore=busyCallbacks;
  display.epd2.writeImagePartToCurrent(frame.black,x,y,400,300,x,y,w,h);
  display.epd2.refresh_bw(x,y,w,h);
  uint32_t elapsed=millis()-start;
  display.epd2.allowLongRefresh();
  if(digitalRead(4)==HIGH || elapsed>6000 || elapsed<100 || busyCallbacks==callbacksBefore) {
    partialFailed=true; partialStatus="unsupported_or_timeout";
    probeStage=0;
    displayError="局刷未通过，已停用并准备全刷恢复";
    Serial.printf("PARTIAL_FAILED region=%s elapsed_ms=%lu BUSY=%d\n",kind,static_cast<unsigned long>(elapsed),digitalRead(4));
    return false;
  }
  display.epd2.writeImagePartToPrevious(frame.black,x,y,400,300,x,y,w,h);
  partialStatus="busy_passed_visual_pending"; ++partialRefreshCount;
  Serial.printf("PARTIAL_DONE region=%s elapsed_ms=%lu ready_busy=%d\n",kind,static_cast<unsigned long>(elapsed),digitalRead(4));
  return true;
}

void refreshScreen() {
  static uint32_t lastBlockedLog=0;
  if(partialFailed && !baselineValid && digitalRead(4)==HIGH) {
    if(millis()-lastBlockedLog>10000) { lastBlockedLog=millis();Serial.println("DISPLAY_WAITING_FOR_READY_AFTER_PARTIAL_FAILURE"); }
    return;
  }
  if(partialProbeRequested) {
    partialProbeRequested=false;
    if(cfg.partialEnabled && cfg.refreshMode=="bw_partial" && !partialFailed)probeStage=1;
    else Serial.println("PARTIAL_PROBE_REJECTED: enable bw_partial first");
  }
  time_t epoch=time(nullptr);tm now{};localtime_r(&epoch,&now);
  const bool online=WiFi.status()==WL_CONNECTED,valid=clockReady();
  const Phase phase=currentPhase();
  const bool stocks=cfg.view=="stock" || (cfg.view=="auto" && stockPage(phase));
  const uint32_t revision=settingsRevision;
  char clock[32];strftime(clock,sizeof(clock),"%Y%m%d%H%M",&now);
  String layout=String(stocks)+"|"+(stocks?cfg.stocks[activeStock]:String("clock"))+"|"+String(online)+"|"+String(clock).substring(0,8);
  String weatherKey=weather.location+weather.observedAt+String(weather.temp)+weather.text+String(weather.low)+String(weather.high)+weatherError;
  uint32_t pointHash=2166136261u;
  for(int i=0;i<stock.count;++i) {
    const uint8_t* bytes=reinterpret_cast<const uint8_t*>(&stock.points[i]);
    for(size_t j=0;j<sizeof(Point);++j)pointHash=(pointHash^bytes[j])*16777619u;
  }
  String tag=String(clock)+String(revision)+weatherKey+layout+String(stock.price,4)+String(stock.previous,4)+String(pointHash)+stock.stamp.substring(0,8)+stockError+phaseCode(phase)+String(probeStage);
  bool full=!baselineValid || layout!=previousLayout || weatherKey!=previousWeather || revision!=previousRevision || millis()-lastFull>900000UL;
  if(!full && tag==previousTag) { needRefresh=false;return; }
  if((partialFailed || !cfg.partialEnabled) && !full && millis()-lastFull<120000) return;
  FrameCanvas frame;
  if(!frame.valid()) { displayError="绘图内存不足";return; }
  painter=&frame;font.begin(frame);font.setScale(1);
  renderBw=cfg.refreshMode=="bw_partial";
  Stock snapshot=stock;Weather w=weather;
  if(!online && apActive) {
    label(20,45,"墨水屏联网设置");label(20,95,"请连接设备热点后打开");label(20,125,"http://192.168.4.1");
    label(20,165,"热点: EPaper-"+String(static_cast<uint32_t>(ESP.getEfuseMac()),HEX));label(20,195,"密码: "+cfg.adminPassword);label(20,225,"网页用户: admin");full=true;
  } else if(stocks && valid) drawStock(now,snapshot,w,cfg.city,phase,activeStock,cfg.stockCount,!stockError.isEmpty() || (stock.valid && time(nullptr)-stock.receivedAt>cfg.refreshSeconds*2));
  else drawClock(now,valid,w,cfg.city,phase);
  if(probeStage==1 && stocks)label(96,276,"局刷测试");
  // The fixed red accent is outside every monochrome partial region.
  frame.fillRect(8,12,4,16,GxEPD_RED);
  const uint32_t headerHash=frame.hash(frame.black,16,0,192,36);
  const uint32_t bodyHash=frame.hash(frame.black,0,40,400,260);
  const uint32_t clockHash=frame.hash(frame.black,40,64,320,96);
  const uint32_t redHash=frame.hash(frame.red,0,0,400,300);
  bool headerChanged=stocks && headerHash!=oldHeaderHash;
  bool bodyChanged=stocks && bodyHash!=oldBodyHash;
  bool clockChanged=!stocks && clockHash!=oldClockHash;
  full=full || !cfg.partialEnabled || partialFailed || redHash!=oldRedHash;
  if(!full && bodyChanged && frame.hasRed(0,40,400,260))full=true;
  if(!full && !headerChanged && !bodyChanged && !clockChanged) { previousTag=tag;painter=&display;font.begin(display);return; }
  rendering=true;needRefresh=false;busyStart=lastBusyLog=millis();
  bool success=true;
  int readyBusy=0;
  if(full) {
    display.epd2.allowLongRefresh();
    display.epd2.writeImage(frame.black,frame.red,0,0,400,300);
    display.epd2.refresh(false);
    readyBusy=digitalRead(4);
    if(readyBusy==HIGH) { success=false;displayError="全刷BUSY未就绪"; }
    else {
      ++fullRefreshCount;lastFull=millis();
      if(cfg.partialEnabled && !partialFailed) {
        // SSD1683 requires matching old/new BW outside each differential window.
        display.epd2.writeImageToCurrent(frame.black,0,0,400,300);
        display.epd2.writeImageToPrevious(frame.black,0,0,400,300);
      } else display.hibernate();
      displayError="";
    }
    lastFrameKind="full";
  } else {
    if(headerChanged)success=partialRegion(frame,16,0,192,36,"header");
    if(success && bodyChanged)success=partialRegion(frame,0,40,400,260,"stock");
    if(success && clockChanged)success=partialRegion(frame,40,64,320,96,"clock");
    lastFrameKind="partial";
    readyBusy=digitalRead(4);
    if(success)displayError="";
  }
  lastFrameElapsedMs=millis()-busyStart;lastRefreshMs=millis();rendering=false;
  if(success) {
    baselineValid=true;previousTag=tag;previousLayout=layout;previousWeather=weatherKey;previousRevision=revision;
    oldHeaderHash=headerHash;oldBodyHash=bodyHash;oldClockHash=clockHash;oldRedHash=redHash;
    if(probeStage==1)probeStage=2;else if(probeStage==2)probeStage=0;
  } else baselineValid=false;
  Serial.printf("DISPLAY_DONE kind=%s elapsed_ms=%lu ready_busy=%d sleep_busy=%d full=%lu partial=%lu free_heap=%u\n",lastFrameKind.c_str(),static_cast<unsigned long>(lastFrameElapsedMs),readyBusy,digitalRead(4),static_cast<unsigned long>(fullRefreshCount),static_cast<unsigned long>(partialRefreshCount),ESP.getFreeHeap());
  painter=&display;font.begin(display);
}
