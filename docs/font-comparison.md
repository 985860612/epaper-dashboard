# 墨水屏字体对照

0.4.9新增临时字体对照页，0.4.10扩展为七组。七组中文和数字均按20px生成1-bit字模，中文黑色、数字红色；右上时间仍是用户喜欢的Arial Bold 24px。未缩放、未网点化，不把显示颜色差别解释为字重差别。截图预览为实际字模的最近邻放大，预览时间12:34是占位值，设备使用当前北京时间。

| 屏幕标签 | 中文 | 数字 |
| --- | --- | --- |
| 当前 | 苹方SC Medium | SF Mono Medium |
| A | 苹方SC Medium | Arial Bold |
| B | 苹方SC Semibold | Arial Bold |
| C | Gen Jyuu Gothic Medium（源柔黑） | 同左 |
| D | Noto Sans CJK Regular | 同左 |
| E | 方正晶黑 W2 | 同左 |
| F | 方正晶细黑 Regular | 同左 |

C来自用户的SiYuanRouHei-Medium.zip。字体自身缺“顺、义”，测试统一使用所有字体都有的“北京 星期一”及0123456789；缺字已在C行标注，没有偷偷换字体补齐。D来自用户提供的NotoSansCJK-Regular-1.otf，包含“顺、义”。前两份原字体、SHA-256和内嵌版权信息保存在assets/fonts/对应metadata.json；C按字体声明保留OFL1.1，D这份旧字体声明的是Apache2.0，分别附许可证。系统字体文件不随项目分发。

短按BOOT开启网页配置窗口，在“省电运行”中点“字体对照页”；显示后按钮变成“返回正常显示”。对照模式仅在RAM中保持，不写持久配置、不进入五张卡片轮播；重启即回到原页面选择。进入测试期间暂停交易页优先和轮播，退出后恢复原规则。顶部时间保留分钟局刷，其余样张保持；仍按现有策略轻睡眠。串口调试命令为JSON行：command=font_test，enabled=true或false。

从源码重建：uv run tools/build_fonts.py；预览和覆盖/边界检查：uv run tools/preview_font_test.py。构建仍用tools/build_firmware.py，保持原分区、NVS和波形。已生成的字模可直接编译，不需要另装字体。

[400×300实际字模预览](../assets/font-comparison/font-test-400x300.png) · [3倍最近邻放大](../assets/font-comparison/font-test-1200x900.png)。视觉效果仍待用户实屏确认。

0.4.10加入用户提供的FangZhengJingHei.zip（E）和FangZhengJingXiHei.zip（F）。E缺顺/义，F包含；共同样张覆盖全部通过。两份压缩包及字体内未提供分发许可，原字体仅放本机被忽略的local/fonts/，不放source.zip。归档保留生成字模和原字体SHA-256/版权元数据。需要重生成时，将对应压缩包内的ttf解压到local/fonts/；直接编译现有字模无需原字体。

七组改为同一行的编号、中文、数字三列，样张仍20px，不缩放。屏幕标签“苹中”为苹方中等，“苹黑”为苹方中黑，“源柔”为源柔黑，“晶黑”和“晶细”为本次两款方正字体。

## 当前选择与英数候选（0.4.11）

2026-11-09用户明确“汉字使用F”：中文确定为方正晶细黑FZXHGB_YS/Regular。英文字母和数字尚待选择，保持这一决定，不重新询问中文方案。当前临时页为五组英数候选，中文标题已用晶细黑；正常日历/股票字体在英数选定后统一应用。

| 编号 | 英数字体 | 对照目的 |
| --- | --- | --- |
| 1 | Arial Bold | 与用户喜欢的右上时间一致，优先比较 |
| 2 | DIN Alternate Bold | 紧凑的数字风格 |
| 3 | Helvetica Neue Medium | 中等字重、简洁字形 |
| 4 | Avenir Next Demi Bold | 较圆润的字形 |
| 5 | SF Mono Medium | 等宽、斜杠零；现有小字数字风格参考 |

英数样张统一24px，黑色AaGgIl .:%与红色0123456789，均按原生字号生成单色字模，不拉伸，不按可见高度另行缩放。小标签仅用于标识候选，不是该组的字体样张。中文标题采用20px晶细黑；右上仍为24px Arial Bold当前时间。字体集合索引已核验：Helvetica Neue Medium为10，Avenir Next Demi Bold为2。系统字体原文件不分发。用户选定后再决定正式页面各字号及数字等宽策略。

五种候选在24px生成后的0—9排版步进均各自一致，包含4：Arial/Helvetica为13px、DIN为12px、Avenir/SF Mono为15px；仅字面自然轮廓不同，没有横向拉伸。检查保存在layout-check.json。

## Arial常规与粗体（0.4.12）

用户反馈并非所有位置都适合加粗，询问非粗体方案。Arial Regular为常规字重，与Arial Bold同家族；候选搭配为小字/说明/次要数值Regular，时间/现价Bold，中文保持已选晶细黑。此分工仍为建议，尚未将所有正式页面替换。

临时测试页比较18px Regular/Bold和24px Regular/Bold，样张相同；底部比较40px的29.78，左右都用黑色避免颜料差别影响字重判断。29.78仅为字形样张，不作为实时行情。中文标题20px晶细黑，右上当前时间24px Arial Bold。使用独立的Arial.ttf和Arial Bold.ttf文件，并校验内部字面名；无轮廓膨胀或人工变细。数字步进各自相同，18px为10px、24px为13px。
