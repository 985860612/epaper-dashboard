#include "app.h"
#include "power_schedule.h"
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

uint32_t sleepCount=0,sleepTotalMs=0,lastSleepMs=0;
String powerError;
static uint32_t configStarted=0,configDuration=90000;
static bool buttonWasLow=false,sleepFault=false;

void initPowerManager() { pinMode(0,INPUT_PULLUP);configStarted=millis(); }
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
