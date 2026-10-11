# NineHalf 字体

[English](README.md)

NotoSansSC.ttf 复用已有相邻应用的 Noto Sans SC 字体，原始来源 https://github.com/notofonts/noto-cjk。OFL.txt 保留 SIL Open Font License。

使用 Pillow 和 fontTools 运行 tools/generate_ninehalf_fonts.py，以4倍采样生成4位 LVGL 应用子集。coverage.json 记录字形与字号，U+9F98 为已知缺字反例。生成源码 main/ninehalf_fonts.c；文案变化后需重新生成。实机显示另行验收。
