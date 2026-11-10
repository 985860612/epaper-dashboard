# HINK 三色屏红色局刷：调研与一次性测试

2026-11-09。**已准备并编译测试代码，未烧录、未发送屏幕测试命令，未验证红色效果或长期可靠性。** 设备保留正式版 `epaper-dashboard-0.3.2`。用户要求先准备，明早自行烧录。

## 调研结果与方案选择

| 参考 | 已确认的内容 | 本次用途／限制 |
| --- | --- | --- |
| [平平 xly95 上位机](https://xly95.github.io/)及[网页仓库](https://github.com/XLY95/xly95.github.io) | 确有红色局刷校准入口，网页发送 `E6 + 参数` | `E6` 是蓝牙固件协议，不是屏幕 SPI 命令。公开网页没有设备端波形，不能直接移植。仍是后续最值得追的配套固件线索。 |
| [SSD1683 Rev 1.0](https://files.seeedstudio.com/wiki/Other_Display/42-epaper/IC%20Driver%20SSD1683%20Datasheet.PDF)，第13—17页 | 三色模式4张表，227字节主波形和6字节电压／结束参数；各阶段可编程 | 与本屏此前233字节读回的布局相符，仅说明寄存器兼容，不据此认定实际芯片型号。用于解码和构造候选。 |
| [Nicolai SSD1619 驱动](https://github.com/Nicolai-Electronics/esp32-component-ssd1619) | 提供波形读回、缩短原波形阶段的研究方法；作者验证对象为2.9寸屏 | 参考方法，不复制其其他格式的字节偏移、电压或时间参数。没有把它当成已验证的4.2寸红色局刷。 |
| [GxEPD2 作者说明](https://github.com/ZinggJM/GxEPD2/discussions/140)及[佳显产品说明](https://buy-lcd.com/products/42-inch-e-paper-display-morochrome-spi-e-ink-for-digital-price-tags-gdey042t91-443) | GDEY042Z98 的快速局刷明确为黑白 | 当前黑白 Mode 2 把第二块RAM用于前一帧，不能直接往里面写红色就得到红色局刷。 |
| [AgentMing 三色局刷实验](https://github.com/AgentMing/zkc42v-ai-usage) | 有允许红色变化的开关；作者明确标注面板波形和实机仍待验证 | 不是已验证红色LUT，本次不直接套用其 `0xFF` 激活序列。 |

选择：**从这块屏自身的三色原生波形提取一次短阶段，验证“写红、擦红、保持不变”能否同时成立。** 这是按手册推导的候选实验，不是已找到的厂家红色局刷方案。暂不接入股票或生产循环。

## 候选的数据来源与限制

本屏2026-11-08已归档的原生读回：主表227字节，尾部 `22 17 41 A8 32 50`。正式测试时会重新全刷并连续读回两次，必须一致；拒绝尾部参数、帧率或选定阶段结构不匹配的面板。

从原生 Mode 1 的 Group 4 提取四张表，关闭其余阶段；重复数从3减到1，阶段长度不增加；保留原生50Hz及电压／结束参数。历史原始数据的四段如下，每行7字节：

```text
VCOM   03 05 04 0F 22 01 01
RED    03 05 84 0F E2 01 01
WHITE  03 05 84 8F 22 01 01
BLACK  03 45 04 0F 22 01 01
```

候选将前三行重复数改为1，BLACK改成源极输出零的保持类。RAM映射：`R/BW=10`写红，`01`擦白，`00`保持。总计58帧，名义驱动约1160毫秒，不含上电及结束过程；**实际耗时待测**。写入后再读回，主表及6字节原生参数必须匹配，随后仅用一次 `0x22/C7` 激活 Mode 1，不加载OTP覆盖候选；不写OTP，不改NVS。

**这里仍扫描全屏，变化像素掩码限制在两个40×40色块；不是硬件保证的局部窗口。保持类的源极输出零，不代表电场为零：原生VCOM仍存在，因此可能泛红、闪动或影响静态内容。** 删去完整波形的其他阶段也没有经过长期电荷平衡验证。保留原生电压只能减少变量，不能保证无损。

## 测试画面与通过标准

- 左侧 `ERASE`：原先红色40×40，尝试擦成白色。
- 中间 `ADD`：原先白底黑框40×40，尝试变成红色。
- 右侧 `KEEP`：原先红色40×40，必须保持；标题、外框和背景也必须保持。
- 小方块变黑、左侧擦不干净、右侧变浅、背景泛红或全屏闪动，任一项都不能算成功。
- BUSY完成、读回匹配、耗时变短只是电气／协议证据；红色和局部隔离必须看实屏，建议录视频。

## 明早手动操作

本文件中的烧录命令**本次均未执行**。下面使用本机现有Arduino CLI及已验证接线；端口上次为 `/dev/cu.usbmodem2101`，操作前重新确认只有目标设备。

```sh
cd '/Users/wangxiaojie/xj-studio/projects/epaper-dashboard/experiments/red_partial_probe'
arduino-cli board list
shasum -a 256 -c build/SHA256SUMS
```

烧录已编译的独立实验固件，保留默认4MB分区和NVS；不要整片擦除：

```sh
arduino-cli upload --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  --port /dev/cu.usbmodem2101 --build-path "$PWD/build/firmware" --verify "$PWD"
```

上电只初始化，不自动刷屏、不联网。此时暂停原有天气／股票服务是预期行为；配置仍在NVS。不要同时打开Arduino IDE串口监视器和下列脚本。

先建立原厂三色全刷画面，约20秒，完成后读回并检查波形：

```sh
uv run console.py --port /dev/cu.usbmodem2101 --action baseline --log build/baseline.log
```

只有打印 `ARMED=1 double_read_match=1` 才进入下一步。如果不匹配，直接恢复正式版，不自行改参数绕过。

看着屏幕／开始录像后，手动触发一次：

```sh
uv run console.py --port /dev/cu.usbmodem2101 --action try --log build/probe.log
```

脚本不会自动建立基准、重试或重复刷新；当前上电会话只允许尝试一次。完成后进入休眠并保持最后画面，等你观察；`valid=1` 仅表示BUSY时序合格，仍会标注 `visual=UNCONFIRMED`。5秒超时后停止并尝试复位，不继续驱动；若持续异常、BUSY不退出或无法恢复，断电停止，不反复重刷。

## 恢复0.3.2

测试结束无论成功与否，先恢复正式版，不把本实验留作常用固件。关闭串口程序，执行：

```sh
arduino-cli upload --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  --port /dev/cu.usbmodem2101 --build-path "$PWD/build/restore-0.3.2" --verify \
  '/Users/wangxiaojie/xj-studio/projects/epaper-dashboard/firmware/epaper_dashboard'
```

恢复应用SHA-256：`0ac8f6e82a1a937cd4d06bbe09741b75f640262674c9f93fda72d9ebb4d2ff67`。该目录保留原引导、分区、应用、源码压缩包；无整片擦除。它是保存的正式固件，并非本次从设备读取的Flash/NVS备份。恢复后检查网页显示版本0.3.2、原配置仍在，并等待原厂全刷完成。恢复固件也不能保证消除未知波形可能造成的损伤。

## 重编译与离线验证

`red_stage_lut.h`为纯C++候选构造及拒绝条件；`checks/test_red_stage.cpp`使用历史真实233字节快照，覆盖时序、电压、帧率、错误表长和过长驱动预算的拒绝。测试不能替代实屏验证。

```sh
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  --build-path "$PWD/build/firmware" --jobs 0 "$PWD"
xcrun clang++ -std=c++17 \
  -isystem /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1 \
  -Wall -Wextra -Werror checks/test_red_stage.cpp -o build/test-red-stage
build/test-red-stage
```

2026-11-09离线结果：实验固件编译成功，Sketch341746字节，静态RAM54664字节；上述候选测试及原项目交易时段／卡片调度测试通过。无本次实机读回、BUSY或屏幕照片证据。若明早失败，保留日志、录像及左右三个色块变化，再决定下一种波形，不连续扫参数。
