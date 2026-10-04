#!/usr/bin/env python3
"""生成 PSP2048 游戏菜单封面 ICON: 480x272, 8-bit PNG"""
from PIL import Image, ImageDraw, ImageFont

W, H = 480, 272

# 与游戏内 render.c 一致的 2048 配色
BG       = (0xFA, 0xF8, 0xEF)
BOARD_BG = (0xBB, 0xAD, 0xA0)
CELL_BG  = (0xCD, 0xC1, 0xB4)
TEXT_DARK  = (0x77, 0x6E, 0x65)
TEXT_LIGHT = (0xF9, 0xF6, 0xF2)

TILE_COLORS = {
    0:    CELL_BG,
    2:    (0xEE, 0xE4, 0xDA),
    4:    (0xED, 0xE0, 0xC8),
    8:    (0xF2, 0xB1, 0x79),
    16:   (0xF5, 0x95, 0x63),
    32:   (0xF6, 0x7C, 0x5F),
    64:   (0xF6, 0x5E, 0x3B),
    128:  (0xED, 0xCF, 0x72),
    256:  (0xED, 0xCC, 0x61),
    512:  (0xED, 0xC8, 0x50),
    1024: (0xED, 0xC5, 0x3F),
    2048: (0xED, 0xC2, 0x2E),
}

def text_color(v):
    return TEXT_DARK if v <= 4 else TEXT_LIGHT

def rounded_rect(draw, box, r, fill):
    draw.rounded_rectangle(box, radius=r, fill=fill)

def center_text(draw, cx, cy, s, font, color):
    l, t, r, b = draw.textbbox((0, 0), s, font=font)
    draw.text((cx - (r - l) / 2, cy - (b - t) / 2 - t), s, font=font, fill=color)

img = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(img)

# ---------- 左侧 4x4 棋盘 ----------
cell, gap = 50, 6
board = 4 * cell + 5 * gap
bx, by = 24, (H - board) // 2
rounded_rect(d, (bx, by, bx + board, by + board), 8, BOARD_BG)

grid = [
    [2,    4,    8,   16],
    [32,   64,   128, 256],
    [512,  1024, 0,   2048],
    [4,    2,    8,   0],
]

font_cache = {}
def get_font(size):
    if size not in font_cache:
        font_cache[size] = ImageFont.load_default(size=size)
    return font_cache[size]

for r in range(4):
    for c in range(4):
        x0 = bx + gap + c * (cell + gap)
        y0 = by + gap + r * (cell + gap)
        v = grid[r][c]
        rounded_rect(d, (x0, y0, x0 + cell, y0 + cell), 5, TILE_COLORS[v])
        if v:
            s = str(v)
            fsize = 30 if len(s) <= 2 else (24 if len(s) == 3 else 18)
            center_text(d, x0 + cell / 2, y0 + cell / 2, s, get_font(fsize), text_color(v))

# ---------- 右侧标题区 ----------
# 金色徽章 "2048"
badge_w, badge_h = 150, 90
ex = bx + board + 40          # 288
ey = 40
rounded_rect(d, (ex, ey, ex + badge_w, ey + badge_h), 10, TILE_COLORS[2048])
center_text(d, ex + badge_w / 2, ey + badge_h / 2, "2048", get_font(58), TEXT_LIGHT)

# 副标题
center_text(d, ex + badge_w / 2, ey + badge_h + 42, "JOIN THE NUMBERS", get_font(16), TEXT_DARK)
center_text(d, ex + badge_w / 2, ey + badge_h + 66, "GET THE 2048 TILE", get_font(16), TEXT_DARK)

# 底部操作提示
center_text(d, ex + badge_w / 2, H - 46, "PRESS  O  TO START", get_font(18), TEXT_DARK)

# 装饰小方块
for i, v in enumerate([2, 8, 32, 128]):
    x0 = ex + 15 + i * 32
    y0 = H - 26
    rounded_rect(d, (x0, y0, x0 + 24, y0 + 16), 3, TILE_COLORS[v])

# 保存 XMB 资源：
#   PIC1.PNG —— 选中游戏时的全屏背景 (480x272, 8-bit)
#   ICON0.PNG —— 菜单列表图标 (144x80, 8-bit)
pic1 = img.quantize(colors=256)
pic1.save("/home/zhang/pspGameDev/PSP2048/PIC1.PNG", format="PNG", bitdepth=8)
print("OK: PIC1.PNG", img.size)

# ---------- 144x80 菜单图标：金色 2048 徽章 + 装饰条 ----------
iw, ih = 144, 80
ic = Image.new("RGB", (iw, ih), BG)
dc = ImageDraw.Draw(ic)

# 中央大徽章
bw, bh = 96, 48
bx0, by0 = (iw - bw) // 2, 8
rounded_rect(dc, (bx0, by0, bx0 + bw, by0 + bh), 8, TILE_COLORS[2048])
center_text(dc, iw / 2, by0 + bh / 2, "2048", get_font(34), TEXT_LIGHT)

# 底部一排小方块
for i, v in enumerate([2, 4, 8, 16, 32, 64]):
    x0 = 12 + i * 20
    y0 = ih - 16
    rounded_rect(dc, (x0, y0, x0 + 16, y0 + 12), 2, TILE_COLORS[v])

icon0 = ic.quantize(colors=256)
icon0.save("/home/zhang/pspGameDev/PSP2048/ICON0.PNG", format="PNG", bitdepth=8)
print("OK: ICON0.PNG", ic.size)
