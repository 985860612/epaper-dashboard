# 横屏墨水屏数据展示

- 状态：active
- 创建日期：2026-11-08

当前固件为 **0.5.11**（2026-11-10）：六个正式页面默认使用方正晶细黑（汉字）与Arial（英数，默认Regular；股票现价用Bold），按16/18/20/24/48/80/112/164px语义档位生成原生字模。年月标题统一为“2026 年  11 月”；主时间112px、大字日期164px；股票名称为24px苹方简体Regular、现价48px Bold，右上角保留20px涨额/涨幅单行；上下刻度仅显示18px Regular价格，不显示百分比或斜线；分时图改为376×120，上方价格刻度移至昨收虚线下方并避让曲线，下方刻度留在底部，去均价线，价格折线按昨收价分为上方红色、下方或相等黑色。局刷窗口随布局更新，原配置、波形和省电逻辑保留。[字体与字号规格](docs/typography.md) · [固件归档](firmware/releases/0.5.11-2026-11-10/README.md)。

0.4.9新增字体对照页，0.4.10扩展为[七组字体对照](docs/font-comparison.md)，包含当前字体、两组Arial＋苹方方案，以及用户提供的源柔黑、Noto Sans CJK Regular、方正晶黑和方正晶细黑。测试页不参与轮播，网页可返回正常显示，重启也会退出；原持久配置保留。

0.4.11按用户选择确定汉字为方正晶细黑（原F），临时页改为五种24px英数候选：Arial Bold、DIN Alternate Bold、Helvetica Neue Medium、Avenir Next Demi Bold、SF Mono Medium。标题汉字使用晶细黑；英数选定后统一应用到正式页面。原七组中文对照保留在0.4.10归档。

0.4.12按“并非所有位置都适合加粗”的反馈，临时页比较Arial Regular与Bold的18/24px文字及40px数字。中文继续确定为晶细黑，正式页面的混合字重方案待确认；屏幕驱动和原配置保留。

0.4.6改为每分钟手动轻睡眠，保留前帧与局刷状态；平时网页离线，短按BOOT开启10分钟配置，启动提供90秒窗口。网页可保持配置或立即省电。自动唤醒、联网、局刷与原配置已核验；新电流待外部读表。USB串口在本次Mac会话睡眠后未恢复，后续USB烧录建议先RESET进入启动窗口。[操作与构建](docs/minute-sleep.md)。

0.4.5加入在线省电：CPU 80MHz、Wi-Fi最大调制解调器省电模式、20ms空闲轮询；保留网页、每分钟行情及现有屏幕波形。用户原报告USB／5V整机待机约50mA，用户随后反馈同口径约40mA，较原约50mA下降约20%；这是用户外部表读数，不根据软件状态推算电流。[省电实现与测量方法](docs/power-optimization.md)。

ESP32-S3 通过 Wi-Fi 独立运行，驱动 HINK-E042A13 4.2 寸 400×300 黑白红墨水屏。非交易时间展示大字时间和北京顺义天气；交易日09:15起展示长江电力，正式开盘后显示当天按昨收价分色的价格分时线与黑色昨收参考线。网页支持修改股票列表、天气位置、网络和显示参数。

五套卡片自`0.3.2`接入并继续保留：大字日历、月历天气、黄历、极简时钟、今日总览，默认每10分钟轮换；交易时段优先股票，其余时间轮播。网页“卡片显示”可选择固定某张卡片并保存，固定模式保持该页；下一张按钮便于浏览。时间按分钟局刷，切换卡片约20秒全刷。大字日期164px、极简时钟112px，按原生字号生成字模，修复实屏中央字偏小的问题。已有股票分时图、字体、局刷驱动及股票页10分钟维护全刷保留。[固件与源码](firmware/releases/0.3.2-2026-11-08/README.md)、[功能与字号验证](docs/validation/2026-11-08/cards-032/README.md)已归档。

黄历使用lunar-python 1.4.9离线生成的2026—2028年数据，显示农历及宜忌节选，标明民俗参考，超出范围提示更新；不再使用预览中的示例宜忌。天气和行情使用现有真实数据，历史行情明确标记。实际设备已验证五页、固定不轮播、自动轮换及重启后配置保留；最终恢复auto/rotate/600秒，Wi-Fi、和风Key、城市、股票和分区保留。夜间未宣称已实测开市行情。运行不依赖串口或电脑常驻。

[原五种主题设计预览](assets/page-themes/index.html)仅为设计参考，里面的时间、温度和宜忌仍是示例；实际设备以当前固件和真实数据为准。主项目编译入口已同步最新源码；0.2.0、0.1.8与早期回放实验均保留归档，未自动提交或推送。

## 入口

| 内容 | 位置 |
| --- | --- |
| Arduino 固件 | [epaper_dashboard](firmware/epaper_dashboard/epaper_dashboard.ino) |
| 网页配置源码 | [web_page.h](firmware/epaper_dashboard/web_page.h) |
| 配网与旧和风凭据导入 | [provision.py](tools/provision.py) |
| 时间、交易日历与数据规则 | [设计说明](docs/design.md) |
| 编译、联网与屏幕验证状态 | [验证记录](docs/validation.md) |
| 当前固件与源码快照 | [0.5.11 发布目录](firmware/releases/0.5.11-2026-11-10/README.md) |
| 已确认的两种版式 | [预览](assets/layout/preview.html) |
| 当前生成字模版式 | [三色股票页](assets/generated-layout/stock-color-400x300.png) · [黑白研究预览](assets/generated-layout/stock-fast-400x300.png) |
| 股票排版调整 | [去除均价图例行、日期间距与前后对比](docs/validation/2026-11-08/stock-layout-spacing/README.md) |
| 红色局刷研究 | [股票两步更新](docs/validation/2026-11-09/stock-red-041/README.md) · [增强擦除实验基线](firmware/experimental/red-native-hold-clean-2026-11-09/README.md) · [首次20秒实验](docs/validation/2026-11-09/red-native-hold/README.md) |
| 早期轮廓字模加粗实验 | [历史对比图](assets/font-contrast/comparison-800x300.png) |

旧电子工牌 Demo 独立保留在工作台 `archive/epaper-lab/`，本项目只复用已验证驱动参数。

## 硬件与编译

接线：GND/3V3，SCK→12，MOSI→11，CS→10，DC→13，RST→7，BUSY→4；CS2不用。BUSY高忙，4MHz SPI，90秒等待上限。兼容驱动为`GxEPD2_420c_GDEY042Z98`；不据此推断实际控制器型号。正式固件完整刷新约20秒，黑白窗口局刷约1秒；背景和残影观感仍以实屏为准。

已安装依赖：ESP32 Arduino Core 3.3.11、GxEPD2 1.6.9、ArduinoJson 7.4.3、Adafruit GFX 1.13.0、Adafruit BusIO 1.17.4。PSRAM关闭，保留默认4MB分区。

```sh
uv run tools/build_firmware.py --build-path "$PWD/build/release"
```

字模与网页gzip头已生成。使用`uv run tools/build_fonts.py`重建，唯一规格为`tools/typography.json`，需要本机`local/fonts/FangZhengJingXiHei-1.ttf`、Arial Regular、Arial Bold及规格中指定的系统苹方TTC；原字体不随源码ZIP分发。已有字模可直接编译。`uv run tools/check_typography.py`生成实际页面代码的预览并检查字号、字形、边界与局刷窗口。

18px基础字库覆盖GB2312一级汉字及界面用字，20/24px为界面子集。自定义名称有大字号未打包字符时整段使用同字体18px；长文本按像素省略，未收录字显示问号，不偷偷换回旧字体。完整策略和容量边界见[规格](docs/typography.md)。

本块黑白红面板的原生灰度尚未验证。[同名HINK黑白版本的驱动讨论](https://github.com/ZinggJM/GxEPD2/discussions/107)附带4/16灰度波形，不能直接视为适用于三色版；当前固件只使用黑、白、红。网点可模拟照片灰阶，不代表真实灰度，也不会提高文字的实际分辨率。

历史字体实验、局刷研究和恢复二进制继续在`firmware/releases/`独立保留；当前默认字体为晶细黑与Arial Regular，股票名称单独用苹方简体Regular；仅股票现价按用户要求使用Bold。

## 初次设置与使用

确认硬件端口，使用Arduino CLI上传上述构建并校验；不要整片擦除。串口115200，无需保持串口连接。固件不内置Wi-Fi、和风Key或管理密码。

可通过物理USB导入已有和风`.env`与本机私有Wi-Fi配置：

```sh
uv run tools/provision.py --port /dev/cu.usbmodemXXXX \
  --source /path/to/existing/qweather/.env --wifi "$PWD/local/wifi.json"
```

`local/wifi.json`格式为`{"ssid":"网络名称","wifiPassword":"网络密码"}`，权限设为0600；`local/`已忽略，不提交。导入脚本只通过USB发送配置，Key不进入固件二进制，不输出密钥。现有和风项目保持不变。

联网成功后打开设备IP，用户`admin`，管理密码在脚本生成的`local/device-access.json`中。无法联网30秒后设备开启受密码保护的`EPaper-...`热点，热点与网页管理密码显示在设置画面；连接后访问`http://192.168.4.1/`。首次配置保存前，未预配设备的随机密码在重启时可能变化。

网页可搜索区县并选择和风LocationID，或直接输入名称与位置ID；默认北京顺义`101010400`。股票最多8只，使用`sh600900`、`sz000001`等带市场前缀代码，默认长江电力。管理页面不返回已存密码或Key，留空保留旧值。保存Wi-Fi配置会重新联网；管理密码修改后重新登录。

## 校时、刷新与故障

- 阿里云`ntp1.aliyun.com`、`ntp2.aliyun.com`、`ntp3.aliyun.com`：联网后首次校时，SNTP每3600000毫秒再校准，时区`CST-8`（北京时间）。网页显示最近一次成功校时。无需电脑常驻。
- 0.2.0历史参数曾为auto/bw_partial/局刷开启：行情抓取120秒、轮播300秒、天气30分钟，时钟按分钟更新。内容未变化时跳过刷新；股票10分钟、时钟15分钟维护全刷。网页可调整抓取和轮播间隔。
- 已验证的227字节黑白局刷已用于正式循环；早期OTP及短表实验仅作历史记录。出现异常时停用局刷并尝试全刷恢复，避免继续驱动。详见[历史局刷记录](docs/partial-refresh.md)及当前版本验收。
- 未校时不向HTTPS接口发送请求；网络失败保留有效天气缓存并标记。行情日期与当天不一致时标为历史行情，抓取失败或数据陈旧标为延迟；不把旧行情伪装为实时。
- 自动模式按北京时间和交易日历切换；日历仅预置2026年，跨年须在网页更新。未更新时自动模式显示时间天气并提示日历，不用工作日近似交易日。
- 网页配置保存在新的`epaperdash` NVS命名空间，保留其他项目的NVS与分区。

重启只需重新给板子上电。当前没有远程Git仓库，未自动提交或推送。
