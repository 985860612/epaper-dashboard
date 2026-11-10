#include "app.h"
#include "power_schedule.h"
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "battery.h"
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>

BatteryReading battery;
static adc_oneshot_unit_handle_t batteryAdc=nullptr;
static adc_cali_handle_t batteryCalibration=nullptr;
static bool batteryAdcReady=false;
static void initBattery() {
  adc_oneshot_unit_init_cfg_t unit{};unit.unit_id=ADC_UNIT_1;
  adc_oneshot_chan_cfg_t channel{};channel.atten=ADC_ATTEN_DB_12;channel.bitwidth=ADC_BITWIDTH_12;
  adc_cali_curve_fitting_config_t calibration{};
  calibration.unit_id=ADC_UNIT_1;calibration.chan=ADC_CHANNEL_2;
  calibration.atten=channel.atten;calibration.bitwidth=channel.bitwidth;
  esp_err_t error=adc_oneshot_new_unit(&unit,&batteryAdc);
  if(error==ESP_OK)error=adc_oneshot_config_channel(batteryAdc,ADC_CHANNEL_2,&channel);
  if(error==ESP_OK)error=adc_cali_create_scheme_curve_fitting(&calibration,&batteryCalibration);
  batteryAdcReady=error==ESP_OK;
  if(!batteryAdcReady)Serial.printf("POWER_ADC_INIT error=%d\n",error);
}
void serviceBattery() {
  static uint32_t lastSample=0;
  if(!batteryAdcReady || (lastSample && millis()-lastSample<10000))return;
  lastSample=millis();
  // Calibrated ADC mV, 16-sample trimmed mean. GPIO3 stays an analog input.
  uint32_t sum=0,low=UINT32_MAX,high=0;
  for(int i=0;i<16;++i) {
    int raw=0,mv=0;esp_err_t error=adc_oneshot_read(batteryAdc,ADC_CHANNEL_2,&raw);
    if(error==ESP_OK)error=adc_cali_raw_to_voltage(batteryCalibration,raw,&mv);
    if(error!=ESP_OK) {battery={};Serial.printf("POWER_ADC_READ error=%d\n",error);return;}
    sum+=mv;low=min(low,uint32_t(mv));high=max(high,uint32_t(mv));
  }
  battery=batteryFromAdc((sum-low-high+7)/14);
}

uint32_t sleepCount=0,sleepTotalMs=0,lastSleepMs=0;
String powerError;
static uint32_t configStarted=0,configDuration=90000;
static bool buttonWasLow=false,sleepFault=false;

void initPowerManager() {
  pinMode(0,INPUT_PULLUP);configStarted=millis();
  initBattery();serviceBattery();
}
uint32_t configRemainingSeconds() { return (remainingMs(millis(),configStarted,configDuration)+999)/1000; }
void keepPowerAwake() { configStarted=millis();configDuration=600000; }
void closePowerWindow() { configStarted=millis();configDuration=1500; }
void servicePowerButton() {
  bool low=digitalRead(0)==LOW;
  if(low && !buttonWasLow) { keepPowerAwake();Serial.println("POWER_CONFIG button=BOOT seconds=600"); }
  buttonWasLow=low;
}
bool powerSleepAllowed() { return !sleepFault && configRemainingSeconds()==0 && !rendering && !apActive && displayError.isEmpty(); }

bool enterMinuteSleep(uint32_t durationMs) {
  if(durationMs<1500 || !powerSleepAllowed())return false;
  esp_err_t error=gpio_wakeup_enable(GPIO_NUM_0,GPIO_INTR_LOW_LEVEL);
  if(error==ESP_OK)error=esp_sleep_enable_gpio_wakeup();
  if(error==ESP_OK)error=esp_sleep_enable_timer_wakeup(uint64_t(durationMs)*1000);
  if(error!=ESP_OK) {
    powerError="无法配置休眠唤醒";sleepFault=true;keepPowerAwake();return false;
  }
  server.stop();WiFi.setAutoReconnect(false);
  bool stopped=WiFi.mode(WIFI_OFF);delay(50);
  const gpio_num_t pins[]={GPIO_NUM_7,GPIO_NUM_10,GPIO_NUM_11,GPIO_NUM_12,GPIO_NUM_13};
  unsigned held=0;
  if(!stopped)error=ESP_FAIL;
  while(error==ESP_OK && held<5) { error=gpio_hold_en(pins[held]);if(error==ESP_OK)++held; }
  uint32_t start=millis();
  if(error==ESP_OK) {
    Serial.printf("POWER_SLEEP ms=%lu count=%lu\n",static_cast<unsigned long>(durationMs),static_cast<unsigned long>(sleepCount));
    delay(20);start=millis();error=esp_light_sleep_start();
  }
  lastSleepMs=millis()-start;
  for(unsigned i=0;i<held;++i)gpio_hold_dis(pins[i]);
  auto cause=esp_sleep_get_wakeup_cause();
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);gpio_wakeup_disable(GPIO_NUM_0);
  if(error==ESP_OK) {
    ++sleepCount;sleepTotalMs+=lastSleepMs;
    if(cause==ESP_SLEEP_WAKEUP_GPIO)keepPowerAwake();
  } else {
    powerError="休眠异常，保持配置模式";sleepFault=true;keepPowerAwake();
  }
  Serial.printf("POWER_WAKE cause=%d elapsed_ms=%lu count=%lu error=%d\n",static_cast<int>(cause),static_cast<unsigned long>(lastSleepMs),static_cast<unsigned long>(sleepCount),error);
  return true; // network was stopped; caller must restore it even after failure
}
