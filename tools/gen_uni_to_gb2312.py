#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 Unicode → GB2312 一级汉字转换表（C 数组）。

输出:
    src/uni_to_gb2312.h - 两个数组:
        g_uni_to_gb2312[]: 按 Unicode 升序排列的 GB2312 一级汉字 (Unicode 码点)
        g_uni_gb2312_qu[]: 对应区字节
        g_uni_gb2312_wei[]: 对应位字节
    二分查找: uni → (qu, wei)

GB2312 一级汉字: 区 16~55, 位 1~94, 共 3755 字
"""
import sys

def gen_table():
    entries = []  # (uni, qu, wei)
    for qu in range(0xB0, 0xD8):
        for wei in range(0xA1, 0xFF):
            b = bytes([qu, wei])
            try:
                ch = b.decode('gb2312')
            except UnicodeDecodeError:
                continue
            uni = ord(ch)
            if 0x4E00 <= uni <= 0x9FFF:  # 汉字区
                entries.append((uni, qu, wei))

    entries.sort(key=lambda e: e[0])  # 按 Unicode 升序
    return entries

def main():
    entries = gen_table()
    print(f"一级汉字: {len(entries)}")

    out_path = "src/uni_to_gb2312.h"
    lines = []
    lines.append("/**")
    lines.append(" * @file uni_to_gb2312.h")
    lines.append(" * @brief Unicode → GB2312 一级汉字转换表 (自动生成, 勿手改)")
    lines.append(" *")
    lines.append(" * 二分查找: 在 g_uni_to_gb2312[] 中找 uni, 下标 i 对应 qu/wei")
    lines.append(" */")
    lines.append("#ifndef __UNI_TO_GB2312_H")
    lines.append("#define __UNI_TO_GB2312_H")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append(f"#define UNI_TO_GB2312_COUNT  {len(entries)}")
    lines.append("")
    lines.append("/* Unicode 码点, 升序排列 */")
    lines.append("const uint16_t g_uni_to_gb2312[] = {")
    for i in range(0, len(entries), 12):
        chunk = entries[i:i+12]
        lines.append("    " + ", ".join(f"0x{u:04X}" for u,_,_ in chunk) + ",")
    lines.append("};")
    lines.append("")
    lines.append("/* 对应区字节 */")
    lines.append("const uint8_t g_uni_gb2312_qu[] = {")
    for i in range(0, len(entries), 24):
        chunk = entries[i:i+24]
        lines.append("    " + ", ".join(f"0x{q:02X}" for _,q,_ in chunk) + ",")
    lines.append("};")
    lines.append("")
    lines.append("/* 对应位字节 */")
    lines.append("const uint8_t g_uni_gb2312_wei[] = {")
    for i in range(0, len(entries), 24):
        chunk = entries[i:i+24]
        lines.append("    " + ", ".join(f"0x{w:02X}" for _,_,w in chunk) + ",")
    lines.append("};")
    lines.append("")
    lines.append("#endif /* __UNI_TO_GB2312_H */")

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    import os
    print(f"输出: {out_path} ({os.path.getsize(out_path)/1024:.1f} KB)")

if __name__ == "__main__":
    main()
