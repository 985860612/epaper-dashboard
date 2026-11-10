// One-shot experiment, NOT a production red-partial-refresh driver.
// References: SSD1683 Rev 1.0 sections 6.5-6.7; native panel LUT readback;
// https://github.com/Nicolai-Electronics/esp32-component-ssd1619 (readback idea).
// Keeps native voltage/end/frame parameters. Never writes OTP or NVS.
#include <SPI.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeSans9pt7b.h>
#include "native_control.h"

static constexpr int WIDTH=400, HEIGHT=300, BYTES=15000;
static uint8_t nativeLut[233], candidate[227], readback[233];
static bool baselineReady=false, attempted=false, failed=false;
static NativeControlInfo stageInfo;

class ProbePanel : public GxEPD2_420c_GDEY042Z98 {
public:
  using GxEPD2_420c_GDEY042Z98::GxEPD2_420c_GDEY042Z98;
  void configure() { _busy_timeout=45000000; }
  void readLut(uint8_t* out) {
    SPI.end();
    pinMode(12,OUTPUT);digitalWrite(12,LOW);
    pinMode(11,OUTPUT);digitalWrite(13,LOW);digitalWrite(10,LOW);
    for(int bit=7;bit>=0;--bit) {
      digitalWrite(11,(0x33>>bit)&1);delayMicroseconds(2);
      digitalWrite(12,HIGH);delayMicroseconds(2);digitalWrite(12,LOW);
    }
    digitalWrite(13,HIGH);pinMode(11,INPUT_PULLUP);
    for(int i=0;i<233;++i) {
      out[i]=0;
      for(int bit=0;bit<8;++bit) {
        delayMicroseconds(2);digitalWrite(12,HIGH);delayMicroseconds(2);
        out[i]=(out[i]<<1)|(digitalRead(11)?1:0);digitalWrite(12,LOW);
      }
    }
    digitalWrite(10,HIGH);SPI.begin(12,-1,11,10);
    selectSPI(SPI,SPISettings(4000000,MSBFIRST,SPI_MODE0));
  }
  void invalidate() { _init_display_done=false; }
  void emergencyReset() {
    // Stop further drive after an invalid BUSY response; no repeated activation.
    digitalWrite(7,LOW);delay(20);digitalWrite(7,HIGH);delay(20);
    _writeCommand(0x12);delay(20);_power_is_on=false;_init_display_done=false;
    if(digitalRead(4)==LOW) { _writeCommand(0x10);_writeData(0x11);_hibernating=true; }
  }
  void loadCandidate() {
    _writeCommand(0x32);
    for(int i=0;i<227;++i)_writeData(candidate[i]);
  }
  void window() {
    _writeCommand(0x11);_writeData(0x03);
    _writeCommand(0x44);_writeData(0);_writeData(49);
    _writeCommand(0x45);_writeData(0);_writeData(0);_writeData(0x2b);_writeData(1);
    _writeCommand(0x4e);_writeData(0);
    _writeCommand(0x4f);_writeData(0);_writeData(0);
  }
  void writeMasks() {
    for(uint8_t command : {uint8_t(0x24),uint8_t(0x26)}) {
      window();_writeCommand(command);_startTransfer();
      for(int y=0;y<HEIGHT;++y)for(int bx=0;bx<50;++bx)_transfer(heldMaskByte(command,bx,y));
      _endTransfer();
    }
  }
  bool activateOnce() {
    _writeCommand(0x01);_writeData(0x2b);_writeData(1);_writeData(0);
    _writeCommand(0x21);_writeData(0);_writeData(0);
    _writeCommand(0x3c);_writeData(0xc0); // border high impedance
    _busy_timeout=30000000;
    uint32_t start=millis();
    // Mode 1; no OTP load to overwrite candidate; analog+clock OFF at end.
    _writeCommand(0x22);_writeData(0xc7);_writeCommand(0x20);
    delay(2);bool sawBusy=digitalRead(4)==HIGH;
    _waitWhileBusy("COMPLETE_NATIVE_HOLD_ONESHOT",30000);
    uint32_t elapsed=millis()-start;_power_is_on=false;
    bool valid=sawBusy && digitalRead(4)==LOW && elapsed>=15000 && elapsed<30000;
    Serial.printf("PROBE_RESULT elapsed_ms=%lu busy=%d sawBusy=%d valid=%d visual=UNCONFIRMED\n",(unsigned long)elapsed,digitalRead(4),sawBusy,valid);
    if(valid)hibernate();else emergencyReset();
    return valid;
  }
};
static GxEPD2_3C<ProbePanel,300> display(ProbePanel(10,13,7,4));

static void dump(const char* name,const uint8_t* bytes,int count) {
  for(int i=0;i<count;i+=24) {
    char line[100];int used=snprintf(line,sizeof(line),"%s offset=%03d hex=",name,i);
    for(int j=i;j<count && j<i+24;++j)used+=snprintf(line+used,sizeof(line)-used,"%02X",bytes[j]);
    Serial.println(line);delay(8);
  }
}
static void pattern(Adafruit_GFX& g,bool after) {
  g.fillScreen(GxEPD_WHITE);g.setFont(&FreeSans9pt7b);g.setTextColor(GxEPD_BLACK);
  g.setCursor(20,35);g.print("RED HOLD - NATIVE LUT");
  g.drawRect(8,8,384,284,GxEPD_BLACK);g.drawFastHLine(20,53,360,GxEPD_BLACK);
  g.setCursor(63,117);g.print("ERASE");g.setCursor(163,117);g.print("ADD");g.setCursor(286,117);g.print("KEEP");
  if(!after)g.fillRect(72,136,40,40,GxEPD_RED);
  if(after)g.fillRect(160,136,40,40,GxEPD_RED);
  else g.drawRect(160,136,40,40,GxEPD_BLACK);
  g.fillRect(288,136,40,40,GxEPD_RED);
  g.setCursor(20,222);g.print("Complete red + held background");
  g.setCursor(20,250);g.print("Outside targets should stay");
}
static bool buildCandidate() {
  return makeNativeHold(nativeLut,sizeof(nativeLut),candidate,sizeof(candidate),stageInfo);
}
static void baseline() {
  if(attempted || failed) { Serial.println("REJECT baseline_after_attempt; flash production to restore");return; }
  display.epd2.invalidate();display.epd2.configure();display.setFullWindow();
  uint32_t start=millis();
  display.firstPage();do { pattern(display,false); }while(display.nextPage());
  if(digitalRead(4)!=LOW) { failed=true;display.epd2.emergencyReset();Serial.println("BASELINE_FAILED busy");return; }
  Serial.printf("BASELINE_DONE elapsed_ms=%lu busy=%d\n",(unsigned long)(millis()-start),digitalRead(4));
  display.epd2.readLut(nativeLut);display.epd2.readLut(readback);
  bool same=memcmp(nativeLut,readback,233)==0;dump("NATIVE",nativeLut,233);
  baselineReady=same && buildCandidate();if(baselineReady)dump("CANDIDATE",candidate,227);
  Serial.printf("ARMED=%d double_read_match=%d attempt_limit=1 native_voltages_unchanged=1 frames=%u nominal_ms=%u\n",baselineReady,same,stageInfo.frames,stageInfo.nominalMilliseconds);
}
static void attempt() {
  if(!baselineReady || attempted || failed) { Serial.println("REJECT not_armed_or_already_attempted");return; }
  attempted=true; // fail closed, even if a later verification fails
  display.epd2.loadCandidate();display.epd2.readLut(readback);
  bool mainMatch=memcmp(candidate,readback,227)==0;
  bool extrasMatch=memcmp(nativeLut+227,readback+227,6)==0;
  Serial.printf("CANDIDATE_READBACK main_match=%d native_tail_match=%d\n",mainMatch,extrasMatch);
  if(!mainMatch || !extrasMatch) { failed=true;display.epd2.emergencyReset();return; }
  display.epd2.writeMasks();
  Serial.println("PROBE_START mode=1 update=C7 waveform=complete_native_rgb_hold trial_local_isolation=1 red_code=11");delay(20);
  failed=!display.epd2.activateOnce();
  Serial.println("PROBE_STOPPED no_automatic_repeat; production_restore_required");
}
void setup() {
  Serial.begin(115200);Serial.setTxTimeoutMs(0);
  SPI.begin(12,-1,11,10);display.epd2.selectSPI(SPI,SPISettings(4000000,MSBFIRST,SPI_MODE0));
  display.epd2.configure();display.init(0);display.setRotation(0);display.epd2.selectFastFullUpdate(false);
  // No display activation on boot. User/host must explicitly issue BASELINE.
}
void loop() {
  static String command;
  while(Serial.available()) {
    char ch=Serial.read();
    if(ch=='\n') {
      command.trim();
      if(command=="BASELINE")baseline();
      else if(command=="TRY")attempt();
      else if(command=="STATUS")Serial.printf("PROBE_STATUS version=red-native-complete-hold armed=%d attempted=%d failed=%d busy=%d uptime_ms=%lu\n",baselineReady,attempted,failed,digitalRead(4),(unsigned long)millis());
      command="";
    }else if(command.length()<32)command+=ch;
  }
  delay(5);
}
