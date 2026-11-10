# 0.4.5 在线省电优化

2026-11-09，用户报告USB／5V输入、ESP32-S3加驱动板和屏幕整机静态电流约50mA，约0.25W。此数值为用户读数，未接入远程电流仪，不能把软件状态当成实际电流测量。

## 本版范围

- CPU从旧版编译默认240MHz改为80MHz，串口status报告实际频率及设置是否成功。
- Wi-Fi从Arduino ESP32-S3默认的WIFI_PS_MIN_MODEM切至WIFI_PS_MAX_MODEM，继续保持连接；不改变发射功率、SSID、密码及网页鉴权。接收延迟可能随路由器信标／监听周期增加。串口status使用esp_wifi_get_ps读取实际模式，值2代表MAX_MODEM。
- 主循环空闲轮询从2ms改为20ms；时间和行情既有调度间隔不变，设备仍保留60秒行情更新。
- 不改刷新波形、屏幕引脚、SPI频率、字模和配色。当前C7／CF／F7结束时已关闭屏幕高压，不因功耗优化额外重置或深睡墨水屏，保留局刷前帧状态。

此处20ms是前台轮询让出CPU时间，不是轻度睡眠。当前Arduino Core 3.3.11所用ESP32-S3 sdkconfig未启用CONFIG_PM_ENABLE／CONFIG_FREERTOS_USE_TICKLESS_IDLE，因此未宣称启用自动Light-sleep。没有用esp_pm_configure失败后伪装成功，也未关闭Wi-Fi进入Deep-sleep。

## 验证和测量

编译、校验上传后读取新版本、实际CPU频率和Wi-Fi省电模式；关闭串口后使用原网页/API检查主循环推进、真实行情拉取、60秒配置及局刷是否保留。至少观察一次跨分钟行情更新。功能验证与电流读数分开记录。

同一USB／5V输入、同一整套硬件及相同使用条件下，等待启动全刷结束，再观察未刷新时的稳定mA；另记包含一分钟行情抓取和屏幕刷新在内的平均电流。连续打开后台状态页会周期访问Wi-Fi，严格比较时应保持前后网页打开状态一致。不要将瞬时最低值当作一分钟平均值。

后续用户反馈：同一USB／5V整机约40mA，较原约50mA下降约20%。这是用户外部表读数，未经代理独立电流测量；还未到用户满意的功耗。整机还包括开发板稳压器、USB相关电路和驱动板指示灯等，实际贡献需要单独测量，尚未量化；软件优化不能承诺整机达到某个mA目标。

若需回退，0.4.4固件与源码独立保留，沿用相同分区及配置，不整片擦除。

## 依据

- 本机Arduino Core：libraries/WiFi/src/WiFiGeneric.cpp，ESP32-S3默认MIN_MODEM；setSleep只有驱动设置成功才更新模式。
- 本机sdkconfig：esp32s3-libs/3.3.11/qio_qspi/include/sdkconfig.h，旧默认CPU 240MHz及PM配置。
- [乐鑫ESP32-S3 Wi-Fi省电模式](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/wifi.html#esp32-s3-wi-fi-power-saving-mode)：MAX_MODEM保持连接，监听周期与收包延迟的关系。
- [乐鑫电源管理](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/power_management.html)：自动轻睡眠的编译与运行条件。
