#include "app.h"
#include "environment.h"
#include "environment_history.h"
#include <Wire.h>

EnvironmentReading environment;
static bool busReady=false,started=false;
static int64_t lastMinute=0;
struct EnvironmentBus {
  bool write(uint8_t address,const uint8_t* bytes,size_t count) {
    Wire.beginTransmission(address);Wire.write(bytes,count);
    return Wire.endTransmission()==0;
  }
  bool read(uint8_t address,uint8_t* bytes,size_t count) {
    if(Wire.requestFrom(address,count)!=count) {while(Wire.available())Wire.read();return false;}
    for(size_t i=0;i<count;++i)bytes[i]=Wire.read();
    return true;
  }
  bool reg(uint8_t address,uint8_t reg,uint8_t* bytes,size_t count) {
    Wire.beginTransmission(address);Wire.write(reg);
    return Wire.endTransmission(false)==0 && read(address,bytes,count);
  }
  void wait(uint32_t ms) {delay(ms);}
};
void initEnvironment() {
  busReady=Wire.begin(ENV_SDA,ENV_SCL,100000);
  Wire.setTimeOut(20);
}
void serviceEnvironment() {
  uint32_t now=millis();int64_t minute=clockReady()?time(nullptr)/60:0;
  if(!environmentDue(started,now,environment.sampledMs,minute,lastMinute))return;
  started=true;lastMinute=minute;
  if(!busReady)initEnvironment();
  if(busReady) {EnvironmentBus bus;sampleEnvironment(bus,environment,now);}
  else {
    const uint32_t attempts=environment.attempts+1;
    environment=EnvironmentReading{};environment.sampledMs=now;environment.attempts=attempts;
    environment.ahtError=environment.bmpError="bus_init";
  }
  needRefresh=true;
  recordEnvironmentHistory(clockReady()?time(nullptr):0,environment);
  char text[48];environmentText(environment,text,sizeof(text));
  Serial.printf("ENV sample=%lu %s aht=%s bmp=%s address=%02x\n",static_cast<unsigned long>(environment.attempts),text,environment.ahtError,environment.bmpError,environment.bmpAddress);
}
void environmentJson(JsonDocument& d) {
  environmentHistoryJson(d);
  auto e=d["environment"].to<JsonObject>();
  e["ahtValid"]=environment.ahtValid;e["bmpValid"]=environment.bmpValid;
  e["temperature"]=nullptr;e["humidity"]=nullptr;e["pressureHpa"]=nullptr;
  if(environment.ahtValid) {e["temperature"]=environment.temperature;e["humidity"]=environment.humidity;}
  if(environment.bmpValid)e["pressureHpa"]=environment.pressureHpa;
  e["ahtError"]=environment.ahtError;e["bmpError"]=environment.bmpError;
  e["bmpAddress"]=environment.bmpAddress;e["attempts"]=environment.attempts;
  e["sampleAgeMs"]=started?millis()-environment.sampledMs:0;
}
