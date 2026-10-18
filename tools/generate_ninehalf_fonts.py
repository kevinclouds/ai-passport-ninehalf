"""Rasterize the application's verified OFL subset into LVGL 4-bit fonts."""

import json
from pathlib import Path

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont


root = Path(__file__).resolve().parents[1]
source = root / "assets/fonts/ninehalf/NotoSansSC.ttf"
text = "".join(path.read_text() for path in (root / "main").glob("ninehalf_*.c"))
text += "鼠牛虎兔龍蛇馬羊猴雞狗豬龙马鸡猪↑↓≥·未出现连续未出现本期揭晓同步失败配网失败等待手机音量亮度保存更新成功"
points = sorted(set(range(32, 127)) | {ord(char) for char in text if ord(char) > 127 and not char.isspace()})
cmap = TTFont(source).getBestCmap()
missing = [hex(point) for point in points if point not in cmap]
assert not missing, missing

output = ['#include "lvgl.h"']
sizes = [8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 19, 20, 24]
scale = 4
font_specs = [(f"nt_font_{size}", size, points) for size in sizes]
font_specs.append(("nt_wifi_font_13", 13, sorted(point for point in cmap if 32 <= point < 65536)))

for name, size, glyph_points in font_specs:
    face = ImageFont.truetype(str(source), size * scale)
    face.set_variation_by_axes([600 if size >= 15 else 500])
    bitmap = bytearray()
    glyphs = ["{0}"]
    descent = 0

    for point in glyph_points:
        char = chr(point)
        left, top, right, bottom = face.getbbox(char, anchor="ls")
        left = left // scale
        top = top // scale
        right = -(-right // scale)
        bottom = -(-bottom // scale)
        width, height = right - left, bottom - top
        descent = max(descent, bottom)

        image = Image.new("L", (width * scale, height * scale))
        ImageDraw.Draw(image).text(
            (-left * scale, -top * scale), char, font=face, fill=255, anchor="ls"
        )
        if width and height:
            image = image.resize((width, height), Image.Resampling.LANCZOS)
        values = [min(15, (value + 8) // 17) for value in image.getdata()]
        index = len(bitmap)
        for offset in range(0, len(values), 2):
            bitmap.append(values[offset] * 16 + (values[offset + 1] if offset + 1 < len(values) else 0))
        glyphs.append(
            "{.bitmap_index=%d,.adv_w=%d,.box_w=%d,.box_h=%d,.ofs_x=%d,.ofs_y=%d}"
            % (index, round(face.getlength(char) * 16 / scale), width, height, left, -bottom)
        )

    output.extend(
        [
            f"static const uint8_t {name}_bitmap[]={{",
            ",\n".join(",".join(map(str, bitmap[i : i + 128])) for i in range(0, len(bitmap), 128)),
            "};",
            f"static const lv_font_fmt_txt_glyph_dsc_t {name}_glyphs[]={{",
            ",\n".join(",".join(glyphs[i : i + 8]) for i in range(0, len(glyphs), 8)),
            "};",
            f"static const uint16_t {name}_unicode[]={{",
            ",\n".join(",".join(str(point - 32) for point in glyph_points[i : i + 64])
                       for i in range(0, len(glyph_points), 64)),
            "};",
            f"static const lv_font_fmt_txt_cmap_t {name}_cmaps[]={{ "
            f"{{.range_start=32,.range_length={max(glyph_points) - 31},.glyph_id_start=1,"
            f".unicode_list={name}_unicode,.list_length={len(glyph_points)},"
            ".type=LV_FONT_FMT_TXT_CMAP_SPARSE_TINY} };",
            f"static const lv_font_fmt_txt_dsc_t {name}_descriptor={{.glyph_bitmap={name}_bitmap,"
            f".glyph_dsc={name}_glyphs,.cmaps={name}_cmaps,.cmap_num=1,.bpp=4,.bitmap_format=0}};",
            f"const lv_font_t {name}={{.get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,"
            f".get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,.line_height={size + 2 + descent},"
            f".base_line={descent},.dsc=&{name}_descriptor}};",
        ]
    )

(root / "main/ninehalf_fonts.c").write_text("\n".join(output))
(root / "assets/fonts/ninehalf/coverage.json").write_text(
    json.dumps({"points": points, "sizes": sizes, "negative": ord("龘")}, ensure_ascii=False) + "\n"
)
assert ord("龘") not in points
print("Font coverage:", len(points), "glyphs,", len(sizes), "sizes")
