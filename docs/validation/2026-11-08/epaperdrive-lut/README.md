# EPaperDrive 波形适配与实屏确认

2026-11-08，用户授权参考 EPaperDrive 尝试三色屏局刷。研究源码在隔离工作树；主项目源码和稳定0.1.8发布保持，分区和NVS凭据不变。

70字节 `LUTDefault_part_dke42` 首次测试在8010ms时BUSY仍高，随后自动停用并全刷恢复，不能算成功。通过双向SDA读回波形寄存器后，完整取得233字节数据；写入70字节后，其余内容不变，末6字节为221741a83250。此布局与上游GDEY042Z98分支的227字节主波形加6参数匹配。不能由此或状态01唯一识别实际芯片型号。

改用 `LUTDefault_part_GDEY042Z98` 的227字节主波形，保留面板原生6参数，C0上电、CF黑白模式2激活，禁止OTP重新加载覆盖自定义表，边框Hi-Z；当前帧与旧帧显式同步。波形来源为 [HalfSweetStudio/EPaperDrive](https://github.com/HalfSweetStudio/EPaperDrive)，AGPL-3.0授权，许可文本与完整实验源码均保留。

`epaper-dashboard-0.1.9-lut227` 上传校验通过，应用1285689字节、静态RAM88208字节；应用SHA256为7d7d137769c80b6f69a1c0c93157a0b4d73713700ccf4eebd51b2de9aa091774。首次正常全刷20763ms后，第一轮6次窗口请求约932–933ms完成；第二次请求后关闭串口，后续计数继续增长。用户要求重新触发后，再次完成6次，累计partial=12、full=1，最后一次934ms，无显示错误。窗口272,80,96,72，每轮另停3秒，起始间隔约4秒，不是每秒刷新。

用户实屏确认：“可以局部刷新,但是背景有一点点的红色, 还可以优化吗”。据此将窗口隔离标记为用户确认，背景泛红仍需优化，不把BUSY和速度作为颜色质量验收。时分在实验中冻结，6次后暂停，此版本不是正常时钟发布。NVS原显示设置仍为clock、color_full、partialEnabled=false、120秒，实验模式只覆盖RAM。

长表工作基线的固件在long227-firmware.zip、完整源码在long227-source.zip；70字节失败试验及无显示激活的寄存器诊断单独保留。readback-incomplete-lines.log为首次串口长行截断记录，完整读回以readback-serial.log和readback-analysis.json为准。此目录不含私有访问配置；SHA256SUMS用于核验。
