# 0.1.8 中文内框笔画修复

修正小号“日、口、和、周”的横竖粗细不匀：中文16px正文与18px Header使用苹方简体Light字面，经原生单色栅格化输出，不再对灰度覆盖率直接切阈值。统一正文基线，Header用实际单色墨迹范围裁切，避免单色字模边缘被灰度度量裁掉。没有对字图再缩放，没有加粗小字。

实测字模：Header“日”的左右笔画由1px/2px变为1px/1px；正文“和”内框由竖画2px、横画1px变为统一1px。“口、周”和“日”的两个白色内框也通过闭合检查。回归脚本检查相关内框的四边主笔画宽度，不把其他部件交叉处的空隙当作口字内框。

4533个字符全部保留，未新增空字模；英文数字、Header西文、大号名称、价格和时间字模逐字节保持不变，布局及折线区域不变。glyph-gridfit-comparison.png为整数倍检查图，左0.1.7、右0.1.8；设备按原始尺寸绘制。

编译应用1283357字节，静态RAM88192字节，交易时间边界测试通过。上传哈希校验通过，设备报告0.1.8，时间页clock保留，首帧全刷20651ms、刷新就绪BUSY=0、displayError为空。天气有效；阿里云校时回调2026-11-08 17:59:40，间隔3600秒。实屏观感仍需用户确认。

关闭串口监视器后，2026-11-08 18:02:07观察到第二次定时全刷完成：fullRefreshCount=2、耗时20569ms、显示错误为空。详见periodic-refresh-check.json。

默认三色全刷、局刷关闭，分区表与0.1.7逐字节相同，NVS未擦除。系统原字体文件不随项目分发；源码含已生成字模，可直接编译。

source.zip保留本版源码和生成器；manifest.json与SHA256SUMS用于核验。check-fonts.py可在解压旧版与新版source.zip后独立验证，例如：

```sh
uv run check-fonts.py /tmp/epaper-before /tmp/epaper-after --output /tmp/epaper-font-proof
```

从项目根目录恢复（替换端口）：

```sh
arduino-cli upload --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  --port /dev/cu.usbmodemXXXX --input-dir "$PWD/firmware/releases/0.1.8-2026-11-08" \
  --verify "$PWD/firmware/epaper_dashboard"
```

禁止整片擦除；固件不包含Wi-Fi密码、和风Key或网页管理密码，沿用原NVS。
