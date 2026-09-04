#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 GB2312 一级汉字(3755字) 点阵字模，输出 C 数组。

用法:
    python gen_gb2312_font.py [字体路径] [输出路径] [字号] [--no-bold]

默认:
    字体: C:/Windows/Fonts/simsun.ttc
    输出: src/font_gb2312_16.h

存储格式(Flash 中按此寻址):
    偏移 = ((区-0xB0)*94 + (位-0xA1)) * 32
每字 16x16 点阵 = 16*16/8 = 32 字节
加粗: 渲染后对每个黑色像素向右/向下扩展 1 像素
"""
import sys, os
from PIL import Image, ImageDraw, ImageFont

COMMON_COUNT = 3755   # 全量 GB2312 一级汉字
TOTAL_CHARS = 3755    # GB2312 一级总数

def gb2312_first_level_chars():
    """生成 GB2312 一级汉字的区/位列表，返回 [(区字节, 位字节, 字符), ...]"""
    chars = []
    for qu in range(0xB0, 0xD8):      # 区 16~55
        for wei in range(0xA1, 0xFF):  # 位 1~94
            b = bytes([qu, wei])
            try:
                ch = b.decode('gb2312')
            except UnicodeDecodeError:
                continue
            if not ch:  # 空位
                continue
            chars.append((qu, wei, ch))
    return chars

def render_glyph(font, ch, size, bold):
    """渲染单个字符为 size×size 点阵，返回横向 MSB 优先字节"""
    img = Image.new('1', (size, size), 1)   # 1=白
    draw = ImageDraw.Draw(img)
    draw.text((0, 0), ch, font=font, fill=0)  # 0=黑
    px = img.load()

    if bold:
        # 加粗：复制一份原始像素，对每个黑色像素向右/向下扩展 1 像素
        img2 = Image.new('1', (size, size), 1)
        px2 = img2.load()
        for y in range(size):
            for x in range(size):
                if px[x, y] == 0:
                    px2[x, y] = 0
                    if x + 1 < size:
                        px2[x + 1, y] = 0   # 向右扩展
                    if y + 1 < size:
                        px2[x, y + 1] = 0   # 向下扩展
                    if x + 1 < size and y + 1 < size:
                        px2[x + 1, y + 1] = 0  # 对角扩展
        px = px2

    out = bytearray()
    for y in range(size):
        byte = 0
        for x in range(size):
            if px[x, y] == 0:  # 黑色像素
                byte |= (0x80 >> (x % 8))
            if x % 8 == 7:
                out.append(byte)
                byte = 0
    return bytes(out)

def main():
    font_path = sys.argv[1] if len(sys.argv) > 1 else "C:/Windows/Fonts/simsun.ttc"
    out_path = sys.argv[2] if len(sys.argv) > 2 else "src/font_gb2312_16.h"
    font_size = int(sys.argv[3]) if len(sys.argv) > 3 and not sys.argv[3].startswith("--") else 16
    bold = "--no-bold" not in sys.argv[1:]
    char_bytes = font_size * font_size // 8
    suffix = f"{font_size}" if bold else f"{font_size}_thin"
    macro_prefix = f"GB2312_{suffix.upper()}"
    array_name = f"g_gb2312_{suffix}_font"

    print(f"字体: {font_path}")
    print(f"生成 GB2312 一级汉字 {font_size}x{font_size} {'加粗' if bold else '细'}点阵 (全量)...")

    try:
        font = ImageFont.truetype(font_path, font_size)
    except Exception as e:
        print(f"字体加载失败: {e}")
        sys.exit(1)

    chars = gb2312_first_level_chars()
    total = len(chars)
    print(f"汉字总数: {total}, 常用: {COMMON_COUNT}")

    # 全白占位字模
    blank = bytes([0xFF] * char_bytes)

    all_data = bytearray()
    for idx, (qu, wei, ch) in enumerate(chars):
        if idx < COMMON_COUNT:
            all_data += render_glyph(font, ch, font_size, bold)
        else:
            all_data += blank  # 占位（不常用字不生成，但索引保持）

    print(f"字模数据: {len(all_data)} 字节 ({len(all_data)/1024:.1f} KB)")

    # 生成 C 数组
    lines = []
    lines.append("/**")
    lines.append(f" * @file {os.path.basename(out_path)}")
    lines.append(f" * @brief GB2312 一级汉字 {font_size}x{font_size} {'加粗' if bold else '细'}点阵字模 (自动生成, 勿手改)")
    lines.append(" *")
    lines.append(f" * 全量 3755 字{'加粗' if bold else '细'}字模")
    lines.append(" * 存储: 全量连续存储")
    lines.append(" * 寻址: 偏移 = ((区-0xB0)*94+(位-0xA1)) * 32")
    lines.append(" */")
    guard = f"__{os.path.splitext(os.path.basename(out_path))[0].upper()}_H"
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("/* GB2312 一级汉字区码范围 */")
    lines.append("#define GB2312_Q_MIN    0xB0   /* 16区 */")
    lines.append("#define GB2312_Q_MAX    0xD7   /* 55区 */")
    lines.append("#define GB2312_W_MIN    0xA1   /* 1位 */")
    lines.append("#define GB2312_W_MAX    0xFE   /* 94位 */")
    lines.append(f"#define {macro_prefix}_FONT_W   {font_size}")
    lines.append(f"#define {macro_prefix}_FONT_H   {font_size}")
    lines.append(f"#define {macro_prefix}_CHAR_BYTES {char_bytes}")
    lines.append(f"#define GB2312_COMMON_COUNT {COMMON_COUNT}")
    lines.append("")
    lines.append(f"/* 常用字字模数据: 连续存储, 偏移 = 索引 * {char_bytes} */")
    lines.append(f"const uint8_t {array_name}[] = {{")

    # 只输出常用字的字模
    common_data = bytearray()
    for idx, (qu, wei, ch) in enumerate(chars):
        if idx < COMMON_COUNT:
            common_data += render_glyph(font, ch, font_size, bold)

    for i in range(0, len(common_data), 16):
        chunk = common_data[i:i+16]
        hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
        lines.append(f"    {hex_str},")

    lines.append("};")
    lines.append("")
    lines.append(f"#define {macro_prefix}_FONT_COUNT  {COMMON_COUNT}")
    lines.append("")
    lines.append(f"#endif /* {guard} */")

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    print(f"输出: {out_path} ({os.path.getsize(out_path)/1024:.1f} KB)")
    print(f"常用字字模: {len(common_data)} 字节 ({len(common_data)/1024:.1f} KB)")

if __name__ == "__main__":
    main()
