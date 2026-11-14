#include "app.h"
#include <SPI.h>
#include <WiFi.h>
#include <GxEPD2_3C.h>
#include "typography.h"
#include "layout_regions.h"
#include "frame_canvas.h"
#include "red_waveform.h"
#include "red_transition.h"
#include "battery_draw.h"
#include "environment.h"

// Adapted from HalfSweetStudio/EPaperDrive (AGPL-3.0):
// WAVEFORM_SETTING_LUT.h, LUTDefault_part_GDEY042Z98.
// https://github.com/HalfSweetStudio/EPaperDrive
// A register readback established a 227-byte waveform + 6 parameter layout.
// Only the volatile main waveform is copied; the panel-native voltage/end
// parameters stay unchanged. The upstream command byte is sent separately.
static const uint8_t ssd1619PartialLut[] = {
  0x01,0x19,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x01,0x5e,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x01,0x9e,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x02,0x00,0x00,
 };
static_assert(sizeof(ssd1619PartialLut)==227,"Long-format LUT must contain 227 bytes");

class HinkPanel : public GxEPD2_420c_GDEY042Z98 {
public:
  using GxEPD2_420c_GDEY042Z98::GxEPD2_420c_GDEY042Z98;
  void allowLongRefresh() { _busy_timeout = 90000000; }
  void partialTimeout() { _busy_timeout = 8000000; }
  void normalGate() {
    _writeCommand(0x01);_writeData(0x2b);_writeData(0x01);_writeData(0x00);
    _writeCommand(0x3c);_writeData(0x05);
  }
  void restoreColorRegisters() { _init_display_done=false; }
  void readRegister(uint8_t command,uint8_t* bytes,size_t count) {
    // SSD1619 reads on bidirectional SDA. Release hardware SPI before changing
    // MOSI to input, then restore the established SPI pins/settings afterward.
    SPI.end();
    pinMode(12,OUTPUT);digitalWrite(12,LOW);
    pinMode(11,OUTPUT);digitalWrite(13,LOW);digitalWrite(10,LOW);
    for(int bit=7;bit>=0;--bit) {
      digitalWrite(11,(command>>bit)&1);delayMicroseconds(2);
      digitalWrite(12,HIGH);delayMicroseconds(2);digitalWrite(12,LOW);
    }
    digitalWrite(13,HIGH);pinMode(11,INPUT_PULLUP);
    for(size_t i=0;i<count;++i) {
      bytes[i]=0;
      for(int bit=0;bit<8;++bit) {
        delayMicroseconds(2);digitalWrite(12,HIGH);delayMicroseconds(2);
        bytes[i]=static_cast<uint8_t>((bytes[i]<<1)|(digitalRead(11)?1:0));
        digitalWrite(12,LOW);
      }
    }
    digitalWrite(10,HIGH);
    SPI.begin(12,-1,11,10);
    selectSPI(SPI,SPISettings(4000000,MSBFIRST,SPI_MODE0));
  }
  uint8_t nativeRed[233]{}, redLut[227]{};
  NativeControlInfo redInfo;
  bool cacheRedWaveform() {
    uint8_t second[233];
    readRegister(0x33,nativeRed,233);readRegister(0x33,second,233);
    redPartialReady=memcmp(nativeRed,second,233)==0 && makeNativeHold(nativeRed,233,redLut,227,redInfo);
    Serial.printf("RED_PROFILE ready=%d nominal_ms=%u tail=%02X\n",redPartialReady,redInfo.nominalMilliseconds,nativeRed[230]);
    return redPartialReady;
  }
  bool refreshRed(const FrameCanvas& before,const FrameCanvas& after) {
    if(!redPartialReady || digitalRead(4)==HIGH)return false;
    _writeCommand(0x32);for(uint8_t value:redLut)_writeData(value);
    uint8_t check[233];readRegister(0x33,check,233);
    if(memcmp(check,redLut,227) || memcmp(check+227,nativeRed+227,6)) {
      redPartialReady=false;return false; // no activation on incompatible readback
    }
    for(uint8_t command:{uint8_t(0x24),uint8_t(0x26)}) {
      _writeCommand(0x11);_writeData(3);
      _writeCommand(0x44);_writeData(0);_writeData(49);
      _writeCommand(0x45);_writeData(0);_writeData(0);_writeData(0x2b);_writeData(1);
      _writeCommand(0x4e);_writeData(0);_writeCommand(0x4f);_writeData(0);_writeData(0);
      _writeCommand(command);_startTransfer();
      for(size_t i=0;i<FrameCanvas::BYTES;++i) {
        auto delta=redTransition(before.red[i],after.red[i]);
        _transfer(command==0x24?delta.bw:delta.red);
      }
      _endTransfer();
    }
    normalGate();_writeCommand(0x21);_writeData(0);_writeData(0);
    _writeCommand(0x3c);_writeData(0xc0);_busy_timeout=30000000;
    uint32_t start=millis();
    _writeCommand(0x22);_writeData(0xc7);_writeCommand(0x20);
    delay(2);bool sawBusy=digitalRead(4)==HIGH;
    _waitWhileBusy("STOCK_RED_HOLD",30000);_power_is_on=false;
    uint32_t elapsed=millis()-start;
    bool ok=sawBusy && digitalRead(4)==LOW && elapsed+1000>=redInfo.nominalMilliseconds && elapsed<redInfo.nominalMilliseconds+3000;
    Serial.printf("RED_PARTIAL_DONE elapsed_ms=%lu valid=%d\n",static_cast<unsigned long>(elapsed),ok);
    allowLongRefresh();return ok;
  }
  bool refreshBwWindow(int x,int y,int w,int h) {
    if(x<0 || y<0 || w<8 || h<1 || x+w>400 || y+h>300 || x%8 || w%8)return false;
    _busy_timeout=8000000;
    normalGate();
    _writeCommand(0x21);_writeData(0x00);_writeData(0x00);
    _writeCommand(0x3c);_writeData(0xc0); // border Hi-Z
    _writeCommand(0x22);_writeData(0xc0);_writeCommand(0x20);
    _waitWhileBusy("CUSTOM_LUT_POWER_ON",100);
    if(digitalRead(4)==HIGH)return false;
    // Preserve native gate/source/VCOM voltages and end/frame parameters.
    // Only the 227 main waveform bytes are transferred, not the six extras.
    _writeCommand(0x32);
    for(uint8_t value:ssd1619PartialLut)_writeData(value);
    _writeCommand(0x11);_writeData(0x03);
    _writeCommand(0x44);_writeData(x/8);_writeData((x+w-1)/8);
    _writeCommand(0x45);_writeData(y&255);_writeData(y>>8);_writeData((y+h-1)&255);_writeData((y+h-1)>>8);
    _writeCommand(0x4e);_writeData(x/8);
    _writeCommand(0x4f);_writeData(y&255);_writeData(y>>8);
    uint32_t start=millis();
    // CF activates black/white Mode 2 WITHOUT loading OTP over the custom LUT.
    _writeCommand(0x22);_writeData(0xcf);_writeCommand(0x20);
    _waitWhileBusy("LONG_LUT_CUSTOM_PARTIAL",8000);
    _power_is_on=false;
    bool ready=digitalRead(4)==LOW;
    Serial.printf("LUT_WINDOW_DONE x=%d y=%d w=%d h=%d bytes=227 update=CF elapsed_ms=%lu BUSY=%d\n",x,y,w,h,static_cast<unsigned long>(millis()-start),digitalRead(4));
    return ready;
  }
};
static GxEPD2_3C<HinkPanel, 32> display(HinkPanel(10, 13, 7, 4));
static Adafruit_GFX* painter=&display;
static bool renderBw=false;
static uint32_t busyStart, lastBusyLog, busyCallbacks;

#include "page_draw.h"

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
  
}

static bool partialFailed=false, baselineValid=false, fullFailed=false;
static String previousTag, previousLayout;
static uint32_t previousRevision=0,lastFull=0;
static uint32_t oldRegionHash[MAX_REFRESH_REGIONS]={},oldRedHash=0;
static FrameCanvas* previousFrame=nullptr;

static bool partialRegion(FrameCanvas& frame,int x,int y,int w,int h,const char* kind,bool holdRed=false) {
  if(!holdRed && frame.hasRed(x,y,w,h)) return false;
  display.epd2.partialTimeout();
  uint32_t start=millis(),callbacksBefore=busyCallbacks;
  display.epd2.writeImagePartToCurrent(frame.black,x,y,400,300,x,y,w,h);
  bool ready=display.epd2.refreshBwWindow(x,y,w,h);
  uint32_t elapsed=millis()-start;
  display.epd2.allowLongRefresh();
  if(!ready || digitalRead(4)==HIGH || elapsed>8100 || elapsed<100 || busyCallbacks==callbacksBefore) {
    partialFailed=true;partialStatus="partial_failed_recovery_pending";
    displayError="局刷异常，准备全刷恢复";
    Serial.printf("PARTIAL_FAILED region=%s elapsed_ms=%lu BUSY=%d\n",kind,static_cast<unsigned long>(elapsed),digitalRead(4));
    return false;
  }
  display.epd2.writeImagePartToPrevious(frame.black,x,y,400,300,x,y,w,h);
  partialStatus="custom_lut_busy_passed_visual_pending";++partialRefreshCount;
  Serial.printf("PARTIAL_DONE region=%s elapsed_ms=%lu\n",kind,static_cast<unsigned long>(elapsed));
  return true;
}

void refreshScreen() {
  serviceBattery();
  if(fullFailed)return;
  if(partialFailed && !baselineValid && digitalRead(4)==HIGH)return;
  time_t epoch=time(nullptr);tm now{};localtime_r(&epoch,&now);
  const bool online=WiFi.status()==WL_CONNECTED,valid=clockReady();
  const Phase phase=currentPhase();
  const bool stocks=showingStock();
  const CardId card=selectedCard();
  const bool colorPartial=cfg.refreshMode=="color_partial";
  const bool useBw=cfg.partialEnabled && cfg.refreshMode!="color_full" && !partialFailed;
  const bool stockRed=stocks && colorPartial && useBw;
  if(stockRed && !previousFrame)previousFrame=new FrameCanvas;
  const bool canRed=stockRed && previousFrame && previousFrame->valid() && redPartialReady;
  const bool maintenanceFull=baselineValid && (stocks ? stockFullRefreshDue(millis(),lastFull) : millis()-lastFull>=900000UL);
  char clock[24];strftime(clock,sizeof(clock),"%Y%m%d%H%M",&now);
  String layout=String(fontTestActive)+"|"+String(stocks)+"|"+(stocks?cfg.stocks[activeStock]:String(CARD_CHOICES[static_cast<int>(card)].code))+"|"+String(online)+"|"+String(clock).substring(0,8)+"|"+String(phase==Phase::Unknown);
  String weatherKey=weather.location+weather.observedAt+String(weather.temp)+weather.text+String(weather.low)+String(weather.high)+weatherError;
  uint32_t pointHash=2166136261u;
  for(int i=0;i<stock.count;++i) {
    const uint8_t* bytes=reinterpret_cast<const uint8_t*>(&stock.points[i]);
    for(size_t j=0;j<sizeof(Point);++j)pointHash=(pointHash^bytes[j])*16777619u;
  }
  String tag=String(clock)+String(settingsRevision)+weatherKey+layout+String(stock.price,4)+String(stock.previous,4)+String(stock.open,4)+String(pointHash)+stock.stamp.substring(0,8)+stockError+phaseCode(phase);
  tag+="|"+String(battery.displayKey());
  char indoor[48];environmentText(environment,indoor,sizeof(indoor));tag+="|"+String(indoor);
  if(!stocks && card==CardId::EnvironmentHistory)tag+="|"+String(environmentHistoryRevision);
  bool full=!baselineValid || maintenanceFull || layout!=previousLayout || settingsRevision!=previousRevision;
  if(!full && tag==previousTag) { needRefresh=false;return; }
  if(!full && !useBw && millis()-lastFull<120000UL)return;
  FrameCanvas frame;
  if(!frame.valid()) { displayError="绘图内存不足";return; }
  painter=&frame;cardDrawingFailed=false;styleDrawingFailed=false;renderBw=cfg.refreshMode=="bw_partial";
  if(!online && apActive) {
    label(20,45,"墨水屏联网设置");label(20,95,"请连接设备热点后打开");label(20,125,"http://192.168.4.1");
    label(20,165,"热点: EPaper-"+String(static_cast<uint32_t>(ESP.getEfuseMac()),HEX));
    label(20,195,"密码: "+cfg.adminPassword);label(20,225,"网页用户: admin");full=true;
  } else if(fontTestActive && valid) { drawFontTest(now);
  } else if(stocks && valid) {
    drawStock(now,stock,weather,cfg.city,phase,activeStock,!stockError.isEmpty() || (stock.valid && epoch-stock.receivedAt>cfg.refreshSeconds*2));
  } else if(valid) { drawCard(card,now); }
  else drawClock(now,valid,weather,cfg.city,phase);
  if(!(stocks && valid && !fontTestActive))drawBatteryStatus(frame,battery);
  if(cardDrawingFailed || styleDrawingFailed) { displayError="字模解压失败"; painter=&display;needRefresh=true;return; }
  RefreshRegion regions[MAX_REFRESH_REGIONS]{};int count=layoutRegions(regions,fontTestActive,stocks,valid,card);
  uint32_t hashes[MAX_REFRESH_REGIONS]={};bool changed[MAX_REFRESH_REGIONS]={};bool anyChanged=false;
  for(int i=0;i<count;++i) { auto r=regions[i];hashes[i]=frame.hash(frame.black,r.x,r.y,r.w,r.h);changed[i]=hashes[i]!=oldRegionHash[i];anyChanged|=changed[i];if(changed[i] && frame.hasRed(r.x,r.y,r.w,r.h) && !canRed)full=true; }
  const uint32_t redHash=frame.hash(frame.red,0,0,400,300);
  const bool redChanged=redHash!=oldRedHash;
  full=full || !useBw || (redChanged && !canRed);
  if(!full && !anyChanged && !redChanged) { previousTag=tag;needRefresh=false;painter=&display;return; }
  const bool changedLayout=!baselineValid || layout!=previousLayout;
  rendering=true;needRefresh=false;busyStart=lastBusyLog=millis();bool success=true;
  if(full) {
    if(maintenanceFull)Serial.printf("MAINTENANCE_FULL view=%s interval_ms=%lu\n",stocks?"stock":"clock",static_cast<unsigned long>(stocks?STOCK_FULL_REFRESH_INTERVAL_MS:900000UL));
    display.epd2.allowLongRefresh();display.epd2.restoreColorRegisters();display.epd2.normalGate();
    display.epd2.writeImage(frame.black,frame.red,0,0,400,300);display.epd2.refresh(false);
    if(digitalRead(4)==HIGH || millis()-busyStart<1000) {
      success=false;fullFailed=true;displayError="全刷初始化未通过";partialStatus="full_initialization_failed";
    } else {
      ++fullRefreshCount;lastFull=millis();
      if(colorPartial)display.epd2.cacheRedWaveform();
      if(useBw) {
        display.epd2.writeImageToCurrent(frame.black,0,0,400,300);
        display.epd2.writeImageToPrevious(frame.black,0,0,400,300);
        partialStatus="ready";
      } else { display.hibernate();partialStatus=partialFailed?"disabled_after_failure":"disabled"; }
      displayError="";
    }
    lastFrameKind="full";
  } else {
    if(canRed && redChanged) {
      success=display.epd2.refreshRed(*previousFrame,frame);
      if(success) {
        // Red pass uses both RAM planes as masks. Restore differential B/W
        // history, treating red-touched pixels as white after forming/erasing.
        for(size_t i=0;i<FrameCanvas::BYTES;++i)
          previousFrame->black[i]=redTransition(previousFrame->red[i],frame.red[i]).blackAfterRed(previousFrame->black[i]);
        display.epd2.writeImageToPrevious(previousFrame->black,0,0,400,300);
        display.epd2.writeImageToCurrent(frame.black,0,0,400,300);
        // Always finish the stock region: red->black may leave the logical
        // black hash unchanged even though the red pass erased that pixel.
        changed[1]=true;
      } else {
        partialFailed=true;redPartialReady=false;
        displayError="红色局刷异常，准备全刷恢复";
        partialStatus="red_partial_failed_recovery_pending";
      }
    }
    for(int i=0;i<count && success;++i)if(changed[i]) { auto r=regions[i];success=partialRegion(frame,r.x,r.y,r.w,r.h,r.name,canRed); }
    lastFrameKind=canRed && redChanged?"color_partial":"partial";
    if(success)displayError="";
  }
  lastFrameElapsedMs=millis()-busyStart;lastRefreshMs=millis();rendering=false;
  if(success) {
    if(stockRed && previousFrame && previousFrame->valid()) {
      memcpy(previousFrame->black,frame.black,FrameCanvas::BYTES);
      memcpy(previousFrame->red,frame.red,FrameCanvas::BYTES);
    }
    baselineValid=true;previousTag=tag;previousLayout=layout;previousRevision=settingsRevision;
    for(int i=0;i<count;++i)oldRegionHash[i]=hashes[i];oldRedHash=redHash;
    lastDisplayedEpoch=epoch;displayedView=fontTestActive?"font_test":stocks?"stock":CARD_CHOICES[static_cast<int>(card)].code;
    if(!stocks && changedLayout)noteCardShown();
  } else baselineValid=false;
  Serial.printf("DISPLAY_DONE kind=%s view=%s elapsed_ms=%lu full=%lu partial=%lu BUSY=%d\n",lastFrameKind.c_str(),stocks?"stock":"clock",static_cast<unsigned long>(lastFrameElapsedMs),static_cast<unsigned long>(fullRefreshCount),static_cast<unsigned long>(partialRefreshCount),digitalRead(4));
  painter=&display;
}

uint32_t screenMaintenanceDelayMs() {
  if(!baselineValid)return 0;
  uint32_t interval=showingStock()?STOCK_FULL_REFRESH_INTERVAL_MS:900000UL;
  uint32_t elapsed=millis()-lastFull;return elapsed>=interval?0:interval-elapsed;
}
