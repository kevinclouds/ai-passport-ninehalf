# NineHalf Fonts

[简体中文](README.zh_CN.md)

NotoSansSC.ttf is Noto Sans SC, copied from the existing sibling application's licensed font asset. Original upstream: https://github.com/notofonts/noto-cjk. OFL.txt preserves the SIL Open Font License.

Run tools/generate_ninehalf_fonts.py with Pillow and fontTools to rasterize the verified application subset at 4-bit coverage with 4x supersampling. coverage.json records code points and sizes, with U+9F98 as a known-missing negative case. Generated firmware source: main/ninehalf_fonts.c. Re-run when fixed UI text changes. On-device rendering remains a separate acceptance check.
