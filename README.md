# epaper-dashboard · 横屏墨水屏看板

ESP32-S3 + 4.2 寸黑白红墨水屏，通过 Wi-Fi 独立显示时间、和风天气、日历和 A 股分时行情。交易时段自动看股票，其余时间轮播生活卡片，也可以在网页上固定页面。

当前版本：**[v0.5.11](https://github.com/985860612/epaper-dashboard/releases/tag/v0.5.11)** · [固件与校验记录](firmware/releases/0.5.11-2026-11-10/) · [字体与版式](docs/typography.md)

## 界面预览

下面的图片由固件使用的页面代码和字模生成，展示的是**示例数据**，不是设备实拍或实时行情。

<table>
<tr><th>股票分时</th><th>极简时钟与天气</th></tr>
<tr><td><img src="docs/images/stock.png" alt="股票页：分时线按昨收分为红黑两色，上下刻度仅显示价格" width="480"></td><td><img src="docs/images/clock.png" alt="时钟页：大号时间、日期、星期和北京顺义天气" width="480"></td></tr>
</table>

股票页保留大号现价、涨跌金额与涨跌幅。价格线高于昨收画红色，低于或等于昨收画黑色；穿越昨收时在交点分段。刻度只留价格数字，去掉均价线，减少干扰。

<details>
<summary>查看六套页面：五张日常卡片 + 股票分时</summary>

![大字日历、月历天气、黄历、极简时钟、今日总览和股票分时](docs/images/cards.png)

</details>

## 能做什么

- **自动切换**：交易日从 09:15 集合竞价开始显示股票；正式分时线从 09:30 绘制，午休压缩为连续时间轴。非交易时间默认每 10 分钟轮换卡片。
- **五张日常卡片**：大字日历、月历天气、黄历、极简时钟、今日总览。网页支持固定卡片和手动切换。
- **自选股**：支持最多 8 只沪深股票，默认长江电力 `sh600900`；显示当前价、涨跌和当日价格分时线。
- **和风天气**：可配置 API Host、Key 与城市/区县位置，默认北京顺义；天气按 30 分钟调度更新。
- **独立校时**：使用阿里云 NTP，联网后首次同步，此后每小时校准，北京时间运行。
- **网页配置**：管理 Wi-Fi、城市、股票、交易日历、轮播和刷新模式；密码与 Key 不通过配置查询接口回显。
- **按需联网与局刷**：轻睡眠保留前帧，每分钟唤醒；平时网页离线，短按 BOOT 开启 10 分钟配置窗口。

## 硬件与接线

已验证组合：**ESP32-S3 Mini + HINK-E042A13 4.2 寸 400×300 黑白红屏**。

| 墨水屏驱动板 | ESP32-S3 |
| --- | --- |
| GND | GND |
| 3V3 | 3V3 |
| SCK | GPIO12 |
| SDA / DIN / MOSI | GPIO11 |
| CS | GPIO10 |
| DC | GPIO13 |
| RST | GPIO7 |
| BUSY | GPIO4 |
| CS2 | 不接 |

SPI 4MHz，BUSY 高电平表示忙，等待上限 90 秒。供电和逻辑电平使用 3.3V。

固件沿用 `GxEPD2_420c_GDEY042Z98` 兼容驱动及本项目验证过的波形；这不等于确认了实物的控制器型号。其他面板、尺寸或黑白版本不能直接套用本项目三色波形。

## 刷新与省电

| 项目 | 当前行为 |
| --- | --- |
| 卡片轮换 | 非交易时间每 600 秒，支持固定页面 |
| 时间 | 按分钟更新，局刷窗口独立于天气区 |
| 行情 | 网页可配置；已验证的设备配置为每 60 秒抓取 |
| 天气 | 每 30 分钟调度更新 |
| 校时 | 阿里云 NTP，每小时校准 |
| 维护全刷 | 股票页每 10 分钟，其他页面约 15 分钟 |
| 配置窗口 | 启动 90 秒；短按 BOOT 开启 10 分钟 |

本块面板的黑白窗口局刷约 1 秒，部分股票黑白更新约 2 秒；红色局刷约 18–19 秒，整屏刷新约 21 秒。**三色局刷不等于快速 LCD 刷新**，耗时和残影需以实屏确认。

首次使用默认采用保守的整屏刷新，间隔 120 秒；若使用本项目已验证的同一屏幕，可在网页中启用局刷、选择刷新模式并将行情间隔设为 60 秒。已有 NVS 配置不会因更新固件而重置。

当前采用 CPU 80MHz、Wi-Fi 最大调制解调器省电及每分钟手动 Light-sleep。整机电流受开发板、稳压器、指示灯和驱动板影响；仓库没有将软件休眠计数换算成电池续航承诺。详见[省电与 BOOT 配置窗口](docs/minute-sleep.md)。

## 编译与烧录

### 1. 准备环境

需要 Arduino CLI、`uv` 和数据线。已验证依赖：

| 依赖 | 版本 |
| --- | --- |
| ESP32 Arduino Core | 3.3.11 |
| GxEPD2 | 1.6.9 |
| ArduinoJson | 7.4.3 |
| Adafruit GFX Library | 1.13.0 |
| Adafruit BusIO | 1.17.4 |

```sh
git clone https://github.com/985860612/epaper-dashboard.git
cd epaper-dashboard

arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.11 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install "GxEPD2@1.6.9" "ArduinoJson@7.4.3" "Adafruit GFX Library@1.13.0" "Adafruit BusIO@1.17.4"
```

### 2. 编译

```sh
uv run tools/build_firmware.py --build-path "$PWD/build/release"
```

构建器使用 `esp32:esp32:esp32s3:CDCOnBoot=cdc`，仅对 C++ 与链接启用 LTO，PSRAM 关闭，保留默认 4MB Flash 分区。已生成的字模和网页 gzip 头包含在源码中，**直接编译不需要安装原始字体**。

v0.5.11 程序段为 1,310,233 字节，应用二进制为 1,310,480 字节，原应用分区上限为 1,310,720 字节；修改后应重新检查容量。[版本验证与 SHA-256](firmware/releases/0.5.11-2026-11-10/README.md)。

### 3. 上传并校验

先用 `arduino-cli board list` 确认当前端口，再替换示例端口：

```sh
arduino-cli upload \
  --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  --port /dev/cu.usbmodemXXXX \
  --build-path "$PWD/build/release" \
  --verify \
  firmware/epaper_dashboard
```

Windows 可使用对应的 `COM` 端口。保留现有分区与 NVS，更新时不要整片擦除。串口波特率 115200，正常运行不需要电脑或串口监视器常驻。

也可以从 [GitHub Releases](https://github.com/985860612/epaper-dashboard/releases) 下载已验证的固件包，具体偏移以包内 `flash_args` 为准，不要只凭文件名猜写入地址。

## 配网与网页管理

首次未配网或无法联网时，设备会显示设置页并开启 `EPaper-...` 热点；连接信息和临时管理密码在屏幕上显示。连接后访问 `http://192.168.4.1/`，用户名为 `admin`。

配置自己的 Wi-Fi、和风专属 API Host/Key 和天气位置后，设备连接局域网。短按 BOOT 开启配置窗口，再访问设备 IP，即可修改股票列表、城市、刷新模式及卡片。网页有“保持配置 10 分钟”和“立即省电”；休眠期间无法访问网页属于正常行为。

股票使用 `sh600900`、`sz000001` 这样的市场前缀代码。和风天气位置采用区县 LocationID，支持网页搜索。密码与 Key 留空表示保留原值，不会清空。

如已有本机私有配置，也可通过 USB 导入：

```sh
uv run tools/provision.py \
  --port /dev/cu.usbmodemXXXX \
  --source /path/to/private/qweather.env \
  --wifi "$PWD/local/wifi.json"
```

`local/wifi.json` 使用 `ssid`、`wifiPassword` 两个字段；`qweather.env` 使用 `QWEATHER_API_HOST`、`QWEATHER_API_KEY`。导入脚本生成的登录资料存放于 `local/device-access.json`，权限为 0600。`local/` 与 `build/` 已忽略，固件不内置个人网络、和风 Key 或固定管理密码。

## 字体与二次开发

- 中文默认方正晶细黑；当前股票名称“长江电力”使用苹方简体 Regular 24px。
- 英文/数字默认 Arial Regular，主现价使用 Arial Bold 48px；价格刻度为原生 18px Regular。
- 文字按原生字号生成 1-bit 字模，不拉伸、不用网点模拟文字灰阶。未验证本面板原生灰度。
- 名称子集未包含的汉字会整段回退到已有的 18px 基础字库；长文本按像素省略。添加新的股票名称时，可按需扩充子集。

唯一字体规格为 [`tools/typography.json`](tools/typography.json)。重建字模需自行准备规格中对应的本机字体：

```sh
uv run tools/build_fonts.py
uv run tools/check_typography.py
```

`check_typography.py` 从实际 C++ 页面头与字模渲染预览，检查字形、间距、重叠、边界和局刷覆盖。预览图输出到被忽略的 `build/`。

字体文件及其授权相互独立：仓库中的开放字体附有对应许可证，系统字体与本地专有原字体不随源码 ZIP 分发。详见[字体说明](assets/fonts/README.md)、[字体来源元数据](assets/fonts/)和[字体规划](docs/typography.md)。

## 数据与已知边界

- 行情来自现有网络接口，可能延迟或不可用；历史/过期数据会标记状态，不作为交易依据。
- 交易日历仅预置 **2026 年**；跨年须在网页更新，未更新时自动模式显示时间天气并提示，不用工作日近似交易日。
- 黄历数据覆盖 **2026–2028 年**，离线生成，宜忌仅作民俗参考。
- 红色局刷速度、残影和驱动兼容性仍是实验结果，不代表同型号所有批次均有相同表现。
- 管理网页面向可信局域网，没有提供公网部署方案。

## 项目目录与记录

| 位置 | 内容 |
| --- | --- |
| [`firmware/epaper_dashboard/`](firmware/epaper_dashboard/) | 当前 Arduino 固件与嵌入式管理网页 |
| [`firmware/releases/`](firmware/releases/) | 已烧录版本、源码快照、校验和与验证记录 |
| [`tools/`](tools/) | 编译、字模生成、预览检查、配网与黄历数据工具 |
| [`tests/`](tests/) | 交易时间、卡片、功耗调度、字形与分色检查 |
| [`docs/validation/`](docs/validation/) | 实验与设备验证记录 |
| [`docs/images/`](docs/images/) | 本页的当前界面配图 |
| [`experiments/`](experiments/) | 历史研究与实验脚本 |
| [`licenses/`](licenses/) | EPaperDrive、黄历数据工具等第三方许可证 |

更多资料：[设计与数据规则](docs/design.md) · [局刷研究](docs/partial-refresh.md) · [GitHub 发布前开发记录](docs/development-history.md)。
