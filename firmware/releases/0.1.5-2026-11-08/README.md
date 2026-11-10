# 0.1.5 稳定生成字库版

Header18px，正文16px常规字重，名称26px、现价48px、时钟80px分别生成字模。保留扩大图线区域，底部时间刻度、行情日期与页码已移除。

应用1297197字节，上传哈希校验通过。设备确认版本0.1.5，原Wi-Fi、和风Key与手动股票模式保留；242点分时、天气与阿里云SNTP正常，校时间隔3600秒。整屏刷新20648ms，显示错误为空。

本HINK屏的兼容驱动局刷实验未通过，默认禁用。源码保留差分及回退处理，修正休眠BUSY误判；不能将此固件记为已实现快速局刷。

源代码见source.zip，源码及二进制SHA-256见manifest.json，校验命令：`shasum -a 256 -c SHA256SUMS`。

从项目根目录恢复（替换端口）：

```sh
arduino-cli upload --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  --port /dev/cu.usbmodemXXXX --input-dir "$PWD/firmware/releases/0.1.5-2026-11-08" \
  --verify "$PWD/firmware/epaper_dashboard"
```

禁止整片擦除。固件无Key或Wi-Fi密码，恢复后沿用epaperdash NVS，首次设置另用USB导入脚本。
