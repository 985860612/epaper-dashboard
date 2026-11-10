# 历史像素字库来源

本目录保留0.1.2–0.1.3像素字实验的素材与授权，当前字体方案见[项目入口](../../README.md)。

中文使用 [Fusion Pixel Font](https://github.com/TakWolf/fusion-pixel-font)，版本`2026.10.25`的12px等宽简体字形。保留上游OTF、[OFL授权](OFL.txt)及`LICENSES/`中的来源字库授权。

原始发布文件：`fusion-pixel-font-12px-monospaced-otf-v2026.10.25.zip`，来自[上游发布页](https://github.com/TakWolf/fusion-pixel-font/releases/tag/2026.10.25)，归档SHA-256：`6e4cd37ad32ccdab76858759397a01c3ec372ac37fdf3acf0705da7b44281302`。

`tools/build_fonts.py`按原始12px网格生成1位字模（ascent10、descent2），中文界面以2倍整数像素绘制，保留原始点阵笔画，不做轮廓字加粗或灰度缩放。固件只嵌入字符子集的压缩字模，不加载整个OTF。

数字使用Adafruit GFX内置标准5×7像素字形，价格5倍、时间9倍、小数字2倍。运行时不依赖字体服务或电脑。


## 0.4.9字体对照

用户提供的GenJyuuGothic-Medium-2.ttf和NotoSansCJK-Regular-1.otf仅生成20px对照子集，不替换全局字库。原文件的SHA-256、版权和内嵌许可证声明记录在对应metadata.json。前者为源柔黑Medium，声明OFL1.1且缺顺/义；后者为Noto Sans CJK Regular，该旧版声明Apache2.0，包含顺/义。分别附许可证，不能将这份Noto的许可误记为OFL。系统Arial、苹方和SF Mono不包含原字体文件。详情见../../docs/font-comparison.md。


## 0.4.10方正字体本地比较

方正晶黑（FZJH/W2）和方正晶细黑（FZXHGB_YS/Regular）按用户提供的压缩包在本机生成20px对照字模。晶黑缺顺/义，晶细黑覆盖。字体原文件在忽略的local/fonts/，未随源码ZIP分发；assets/fonts/仅记录这两份的名称、版权与SHA-256元数据。压缩包及字体中没有附分发许可，不据此认定为开放字体。
