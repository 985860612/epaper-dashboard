#include "../firmware/epaper_dashboard/environment.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

struct FakeBus {
  bool ahtPresent=true,bmpPresent=true,bmpWrong=false,calibrated=true;
  bool ahtBusy=false,bmpBusy=false,badCrc=false,shortAht=false,badCalibration=false;
  bool failWrite=false,bmpStuckAfterTrigger=false,triggered=false;
  uint8_t bmpAddress=0x76;
  int writes=0,reads=0,waited=0;
  uint8_t calibration[24]{};
  FakeBus() {
    int coeff[]={27504,26435,-1000,36477,-10685,3024,2855,140,-7,15500,-14600,6000};
    for(int i=0;i<12;++i) {calibration[2*i]=coeff[i]&255;calibration[2*i+1]=(coeff[i]>>8)&255;}
  }
  bool write(uint8_t addr,const uint8_t* bytes,size_t n) {
    ++writes;if(failWrite)return false;
    if(addr==0x38) {
      assert(n==3);assert(bytes[0]==0xbe || bytes[0]==0xac);
      if(bytes[0]==0xbe)calibrated=true;
      return ahtPresent;
    }
    assert(n==2 && bytes[0]==0xf4 && bytes[1]==0x25);triggered=true;
    return bmpPresent;
  }
  bool read(uint8_t address,uint8_t* data,size_t n) {
    ++reads;assert(address==0x38 && n==7);
    if(!ahtPresent || shortAht)return false;
    // Exact 25 C and 50 %: raw T=393216, raw RH=524288.
    const uint8_t frame[]={uint8_t(ahtBusy?0x88:8),0x80,0,6,0,0};
    memcpy(data,frame,6);data[6]=0xa6^(badCrc?1:0);return true;
  }
  bool reg(uint8_t addr,uint8_t reg,uint8_t* data,size_t n) {
    ++reads;
    if(addr==0x38) {assert(reg==0x71 && n==1);*data=calibrated?8:0;return ahtPresent;}
    if(!bmpPresent || addr!=bmpAddress)return false;
    switch(reg) {
      case 0xd0: assert(n==1);*data=bmpWrong?0x60:0x58;break;
      case 0xf3: assert(n==1);*data=(bmpBusy || (triggered && bmpStuckAfterTrigger))?8:0;break;
      case 0x88: assert(n==24);memcpy(data,calibration,24);if(badCalibration)data[6]=data[7]=0;break;
      case 0xf7: {
        assert(n==6);uint32_t p=415148,t=519888;
        data[0]=p>>12;data[1]=p>>4;data[2]=p<<4;
        data[3]=t>>12;data[4]=t>>4;data[5]=t<<4;break;
      }
      default:assert(false);
    }return true;
  }
  void wait(uint32_t ms) {waited+=ms;}
};
int main() {
  const uint8_t crcFrame[]={8,0x80,0,6,0,0};
  assert(ahtCrc(crcFrame,6)==0xa6);
  FakeBus b;EnvironmentReading v;sampleEnvironment(b,v,100);
  assert(v.ahtValid && v.bmpValid && v.attempts==1 && v.sampledMs==100);
  assert(v.temperature==25 && v.humidity==50);
  // Bosch published reference vector: 100653.26 Pa, tolerance 0.02 hPa.
  assert(fabs(v.pressureHpa-1006.5326)<0.02);
  char text[48];environmentText(v,text,sizeof(text));assert(strcmp(text,"IN 25.0C 50% 1007hPa")==0);
  b.badCrc=true;sampleEnvironment(b,v,60100);
  assert(!v.ahtValid && isnan(v.temperature) && isnan(v.humidity) && v.bmpValid);
  assert(strcmp(v.ahtError,"crc_error")==0 && v.attempts==2);
  b.badCrc=false;b.bmpPresent=false;sampleEnvironment(b,v,120100);
  assert(v.ahtValid && !v.bmpValid && isnan(v.pressureHpa) && !v.bmpAddress);
  b.bmpPresent=true;b.bmpAddress=0x77;b.calibrated=false;sampleEnvironment(b,v,180100);
  assert(v.ahtValid && v.bmpValid && v.bmpAddress==0x77);
  b.ahtBusy=true;b.reads=b.waited=0;sampleEnvironment(b,v,240100);
  assert(!v.ahtValid && strcmp(v.ahtError,"busy_timeout")==0 && v.bmpValid);
  assert(b.reads<25 && b.waited<=180);
  b.ahtBusy=false;b.bmpBusy=true;sampleEnvironment(b,v,300100);
  assert(v.ahtValid && !v.bmpValid && strcmp(v.bmpError,"busy_timeout")==0);
  b.bmpBusy=false;b.bmpStuckAfterTrigger=true;b.triggered=false;b.reads=b.waited=0;
  sampleEnvironment(b,v,330100);
  assert(v.ahtValid && !v.bmpValid && strcmp(v.bmpError,"busy_timeout")==0);
  assert(b.reads<30 && b.waited<=140);
  b.bmpStuckAfterTrigger=false;b.bmpWrong=true;sampleEnvironment(b,v,360100);
  assert(!v.bmpValid && strcmp(v.bmpError,"wrong_chip_id")==0);
  b.bmpWrong=false;b.badCalibration=true;sampleEnvironment(b,v,420100);
  assert(!v.bmpValid && strcmp(v.bmpError,"invalid_data")==0);
  b.badCalibration=false;b.shortAht=true;sampleEnvironment(b,v,480100);
  assert(!v.ahtValid && v.bmpValid && strcmp(v.ahtError,"short_read")==0);
  b.shortAht=false;b.failWrite=true;sampleEnvironment(b,v,540100);
  assert(!v.ahtValid && !v.bmpValid && isnan(v.temperature) && isnan(v.pressureHpa));
  b.failWrite=false;b.ahtPresent=b.bmpPresent=false;sampleEnvironment(b,v,600100);
  assert(!v.ahtValid && !v.bmpValid);environmentText(v,text,sizeof(text));assert(strcmp(text,"IN --C --% --hPa")==0);
  b.ahtPresent=b.bmpPresent=true;sampleEnvironment(b,v,660100);assert(v.ahtValid && v.bmpValid);
  for(float temp:{-40.0f,-0.1f,0.0f,85.0f}) {
    v.temperature=temp;v.humidity=100;v.pressureHpa=1100;environmentText(v,text,sizeof(text));
    assert(strlen(text)*6<=288);if(temp==-0.1f)assert(strstr(text,"-0.1C"));
  }
  assert(environmentDue(false,0,0,0,0));
  assert(!environmentDue(true,59999,0,0,0));assert(environmentDue(true,60000,0,0,0));
  assert(environmentDue(true,1000,900,101,100));assert(!environmentDue(true,61000,0,100,100));
  assert(environmentDue(true,40000,UINT32_MAX-30000,0,0));
  assert(!environmentDue(true,1000,UINT32_MAX-30000,0,0));
  assert(environmentDue(true,100,0,99,100)); // NTP clock correction.
  std::cout<<"PASS: sensor reference values, CRC, busy bounds, calibration, IDs/addresses, read/write errors, disconnect/recovery, display rounding and minute/wrap scheduling\n";
}
