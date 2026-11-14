#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <initializer_list>

constexpr int ENV_SDA=6, ENV_SCL=5;
constexpr uint32_t ENV_INTERVAL_MS=60000;
struct EnvironmentReading {
  bool ahtValid=false, bmpValid=false;
  float temperature=NAN, humidity=NAN, pressureHpa=NAN;
  uint8_t bmpAddress=0;
  const char* ahtError="starting";
  const char* bmpError="starting";
  uint32_t attempts=0, sampledMs=0;
};
extern EnvironmentReading environment;
void initEnvironment();
void serviceEnvironment();

inline bool environmentDue(bool started,uint32_t now,uint32_t last,int64_t minute,int64_t lastMinute) {
  return !started || (minute>0 ? minute!=lastMinute : uint32_t(now-last)>=ENV_INTERVAL_MS);
}
inline uint8_t ahtCrc(const uint8_t* bytes,size_t count) {
  uint8_t crc=0xff;
  for(size_t i=0;i<count;++i) {
    crc^=bytes[i];
    for(int b=0;b<8;++b)crc=(crc&0x80)?uint8_t((crc<<1)^0x31):uint8_t(crc<<1);
  }
  return crc;
}
inline const char* decodeAht(const uint8_t* data,EnvironmentReading& value) {
  if(data[0]&0x80)return "busy_timeout";
  if(!(data[0]&8))return "not_calibrated";
  if(ahtCrc(data,6)!=data[6])return "crc_error";
  uint32_t h=(uint32_t(data[1])<<12)|(uint32_t(data[2])<<4)|(data[3]>>4);
  uint32_t t=(uint32_t(data[3]&15)<<16)|(uint32_t(data[4])<<8)|data[5];
  float temperature=t*(200.0f/1048576.0f)-50.0f,humidity=h*(100.0f/1048576.0f);
  if(temperature< -40 || temperature>85)return "out_of_range";
  value.temperature=temperature;value.humidity=humidity;value.ahtValid=true;
  return "ok";
}
inline uint16_t envU16(const uint8_t* p) {return uint16_t(p[0])|(uint16_t(p[1])<<8);}
inline int16_t envS16(const uint8_t* p) {return static_cast<int16_t>(envU16(p));}
// Bosch BMP280 datasheet rev 1.26, section 8.1 floating-point compensation.
// Calibration is little endian; T1 and P1 are unsigned, all other words signed.
inline bool compensateBmp(const uint8_t* c,const uint8_t* raw,float& hpa) {
  uint32_t p=(uint32_t(raw[0])<<12)|(uint32_t(raw[1])<<4)|(raw[2]>>4);
  uint32_t t=(uint32_t(raw[3])<<12)|(uint32_t(raw[4])<<4)|(raw[5]>>4);
  if(p==0x80000 || t==0x80000 || envU16(c+6)==0 || envU16(c+6)==0xffff)return false;
  double v1=(double(t)/16384.0-double(envU16(c))/1024.0)*envS16(c+2);
  double v2=double(t)/131072.0-double(envU16(c))/8192.0;
  v2=v2*v2*envS16(c+4);
  const int32_t fine=static_cast<int32_t>(v1+v2);
  double temp=(v1+v2)/5120.0;
  if(temp< -40 || temp>85)return false;
  v1=double(fine)/2.0-64000.0;
  v2=v1*v1*envS16(c+16)/32768.0;
  v2+=v1*envS16(c+14)*2.0;
  v2=v2/4.0+double(envS16(c+12))*65536.0;
  v1=(double(envS16(c+10))*v1*v1/524288.0+double(envS16(c+8))*v1)/524288.0;
  v1=(1.0+v1/32768.0)*envU16(c+6);
  if(v1==0)return false;
  double pressure=(1048576.0-p-v2/4096.0)*6250.0/v1;
  v1=double(envS16(c+22))*pressure*pressure/2147483648.0;
  v2=pressure*envS16(c+20)/32768.0;
  pressure+=(v1+v2+envS16(c+18))/16.0;
  if(!isfinite(pressure) || pressure<30000 || pressure>110000)return false;
  hpa=pressure/100.0;return true;
}

// Bus operations have a timeout; sensor busy waits are bounded as well.
// Kept independent of Arduino to exercise the real protocol with a fake bus.
template<class Bus> const char* readAht(Bus& bus,EnvironmentReading& out) {
  uint8_t status=0;
  if(!bus.reg(0x38,0x71,&status,1))return "no_response";
  if(status&0x80)return "busy_timeout";
  if(!(status&8)) {
    const uint8_t init[]={0xbe,0x08,0x00};
    if(!bus.write(0x38,init,3))return "init_write";
    bus.wait(10);
    if(!bus.reg(0x38,0x71,&status,1) || !(status&8))return "not_calibrated";
  }
  const uint8_t trigger[]={0xac,0x33,0};
  if(!bus.write(0x38,trigger,3))return "trigger_write";
  bus.wait(80);
  uint8_t bytes[7];
  for(int i=0;i<9;++i) {
    if(!bus.read(0x38,bytes,7))return "short_read";
    if(!(bytes[0]&0x80))return decodeAht(bytes,out);
    if(i<8)bus.wait(10);
  }
  return "busy_timeout";
}
template<class Bus> const char* readBmp(Bus& bus,EnvironmentReading& out) {
  uint8_t id=0,address=0;
  bool wrongId=false;
  for(uint8_t candidate:{uint8_t(0x76),uint8_t(0x77)}) {
    if(bus.reg(candidate,0xd0,&id,1)) {
      if(id==0x58) {address=candidate;break;}
      wrongId=true;
    }
  }
  if(!address)return wrongId?"wrong_chip_id":"no_response";
  out.bmpAddress=address;
  uint8_t status=0;
  if(!bus.reg(address,0xf3,&status,1))return "status_read";
  if(status&9)return "busy_timeout";
  uint8_t calib[24];
  if(!bus.reg(address,0x88,calib,24))return "calibration_read";
  const uint8_t trigger[]={0xf4,0x25}; // T/P x1, forced mode; returns to sleep.
  if(!bus.write(address,trigger,2))return "trigger_write";
  bus.wait(10);
  for(int i=0;i<10;++i) {
    if(!bus.reg(address,0xf3,&status,1))return "status_read";
    if(!(status&9)) {
      uint8_t raw[6];
      if(!bus.reg(address,0xf7,raw,6))return "short_read";
      if(!compensateBmp(calib,raw,out.pressureHpa))return "invalid_data";
      out.bmpValid=true;return "ok";
    }
    if(i<9)bus.wait(5);
  }
  return "busy_timeout";
}
template<class Bus> void sampleEnvironment(Bus& bus,EnvironmentReading& out,uint32_t now) {
  const uint32_t attempts=out.attempts+1;
  out=EnvironmentReading{}; // Discard old values on every attempt, including errors.
  out.attempts=attempts;out.sampledMs=now;
  out.ahtError=readAht(bus,out);
  out.bmpError=readBmp(bus,out);
}
// ASCII status fits y=2..8 without adding a large font; IN distinguishes indoors.
inline void environmentText(const EnvironmentReading& value,char* text,size_t size) {
  char t[12]="--",h[8]="--",p[8]="--";
  if(value.ahtValid) {
    int tenths=static_cast<int>(lroundf(value.temperature*10));
    snprintf(t,sizeof(t),"%s%d.%d",tenths<0?"-":"",abs(tenths)/10,abs(tenths)%10);
    snprintf(h,sizeof(h),"%d",static_cast<int>(lroundf(value.humidity)));
  }
  if(value.bmpValid)snprintf(p,sizeof(p),"%d",static_cast<int>(lroundf(value.pressureHpa)));
  snprintf(text,size,"IN %sC %s%% %shPa",t,h,p);
}
