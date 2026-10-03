/*
 * render.c - PSP GU 2D 渲染（经典 homebrew 模板）
 *
 * 使用 PSP homebrew 社区最稳定的 GU 2D 模板：
 *   - 标准 3D 变换管线（GU_TRANSFORM_3D，默认值，不传 flag）
 *   - Ortho 投影矩阵映射 (0,0)→(480,272) 到屏幕
 *   - 纯色矩形 + 位图像素点
 *   - depth test / cull face 关闭
 *   - blend 开启（支持半透明状态层）
 *
 * 顶点布局（PSP GU 官方规范）：
 *   GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF
 *   => u(4), v(4), color(4), x(4), y(4), z(4) = 24 bytes / vertex
 */
#include "game.h"
#include <stdio.h>
#include <string.h>

// ========= 批量像素缓冲（文字） =========
// 用 GU_SPRITES 而不是 GU_POINTS：
//   手机 GPU 的 GLES 驱动对 gl_PointSize 支持很差，点图元经常被整体丢弃
//  （桌面 PPSSPP 正常、手机 PPSSPP 文字全丢就是这个原因）。
//   GU_SPRITES 每对顶点是一个轴对齐矩形，所有后端和真机都稳定支持。
// 顶点布局：GU_COLOR_8888 | GU_VERTEX_32BITF => color(4), x(4), y(4), z(4) = 16B
struct PointV {
    unsigned int color;
    float x, y, z;
} __attribute__((packed));

#define MAX_PT 8192  // 一帧所有文字像素块上限 8K，保守
// 必须 16 字节对齐：GE 按 DMA 块读顶点缓冲
static struct PointV pt_buf[MAX_PT * 2] __attribute__((aligned(16)));
static int pt_count = 0;

// 入队一个 size×size 的像素块（两个顶点表示一个精灵）
static inline void add_pt(float x, float y, float size, unsigned int color) {
    if (pt_count < MAX_PT) {
        // 写入走非缓存别名（真机 GE DMA 直接读物理内存）
        struct PointV *v = &((struct PointV *)UNCACHED(pt_buf))[pt_count * 2];
        unsigned int c = FIXC(color);
        v[0].color = c; v[0].x = x;        v[0].y = y;        v[0].z = 0.0f;
        v[1].color = c; v[1].x = x + size; v[1].y = y + size; v[1].z = 0.0f;
        pt_count++;
    }
}

// 提交所有文字像素 —— 放在 Render_EndFrame 最后调（保证文字在最上层）
static void flush_pts(void) {
    if (pt_count == 0) return;
    sceGuDrawArray(GU_SPRITES,
                   GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   pt_count * 2, NULL, pt_buf);
    pt_count = 0;
}

// ========= 8x8 位图字体（只定义大写、数字和常见标点） =========
static const unsigned char font_data[128][8] = {
    ['A']={0x3C,0x66,0x66,0x7E,0x66,0x66,0x66,0x00},
    ['B']={0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00},
    ['C']={0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00},
    ['D']={0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00},
    ['E']={0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00},
    ['F']={0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00},
    ['G']={0x3C,0x66,0x60,0x6E,0x66,0x66,0x3E,0x00},
    ['H']={0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00},
    ['I']={0x3E,0x18,0x18,0x18,0x18,0x18,0x3E,0x00},
    ['J']={0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00},
    ['K']={0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00},
    ['L']={0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00},
    ['M']={0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00},
    ['N']={0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00},
    ['O']={0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    ['P']={0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00},
    ['Q']={0x3C,0x66,0x66,0x66,0x6E,0x6C,0x36,0x00},
    ['R']={0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00},
    ['S']={0x3E,0x60,0x60,0x3C,0x06,0x06,0x7C,0x00},
    ['T']={0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    ['U']={0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    ['V']={0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00},
    ['W']={0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    ['X']={0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00},
    ['Y']={0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00},
    ['Z']={0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00},
    ['0']={0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00},
    ['1']={0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00},
    ['2']={0x3C,0x66,0x06,0x0C,0x30,0x60,0x7E,0x00},
    ['3']={0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00},
    ['4']={0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00},
    ['5']={0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00},
    ['6']={0x3C,0x60,0x60,0x7C,0x66,0x66,0x3C,0x00},
    ['7']={0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00},
    ['8']={0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00},
    ['9']={0x3C,0x66,0x66,0x3E,0x06,0x06,0x3C,0x00},
    [' ']={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    [':']={0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00},
    ['-']={0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00},
    ['/']={0x02,0x06,0x0C,0x18,0x30,0x60,0x40,0x00},
    ['!']={0x18,0x18,0x18,0x18,0x00,0x00,0x18,0x00},
    ['?']={0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00},
    ['+']={0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00},
    ['=']={0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00},
    ['#']={0x14,0x7F,0x14,0x7F,0x14,0x7F,0x14,0x00},
    ['.']={0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
};

static const unsigned char *get_glyph(char c) {
    unsigned char uc = (unsigned char)c;
    if (uc >= 128) uc = (unsigned char)'#';  // 越界保护
    const unsigned char *g = font_data[uc];
    if (g[0] == 0 && g[1] == 0 && uc != (unsigned char)' ') return font_data[(unsigned char)'#'];
    return g;
}

static void add_text(const char *s, float x, float y, int scale, unsigned int color) {
    float cx = x;
    float cy = y;
    while (*s) {
        if (*s == '\n') { cx = x; cy += 8 * scale + 2; s++; continue; }
        const unsigned char *glyph = get_glyph(*s);
        for (int row = 0; row < 8; row++) {
            unsigned char bits = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (bits & (0x80 >> col)) {
                    // 一个字体像素 = 一个 scale×scale 的精灵
                    add_pt(cx + col * scale, cy + row * scale, (float)scale, color);
                }
            }
        }
        cx += 8 * scale;
        s++;
    }
}

// ========= GU 帧开始/结束 =========
static int rect_count;              // 定义在后面"矩形"一节（tentative definition）
static void flush_rects(void);

void Render_BeginFrame(void) {
    pt_count = 0;
    rect_count = 0;
    sceGuStart(GU_DIRECT, list);
    sceGuClearColor(FIXC(0xFFfaf8ef));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
}

void Render_EndFrame(void) {
    // 矩形、文字各一次 DrawArray：先矩形后文字（文字在最上层）
    flush_rects();
    flush_pts();
    sceGuFinish();
    sceGuSync(0, 0);
    sceGuSwapBuffers();
}

// ========= 矩形（整帧批量化） =========
// 不能所有矩形共用一个 4 顶点缓冲：GU_DIRECT 模式下 GE 与 CPU 并发执行，
// CPU 复用缓冲时 GE 可能读到被下一块覆盖一半的数据（真机上表现为靠后画的
// 格子丢失/闪烁，PPSSPP 时序不同看不出来）。
// 做法和文字一样：整帧累积到一个静态大缓冲，EndFrame 一次提交。
// GU_SPRITES 每对顶点是一个轴对齐矩形：{c,x0,y0,z}, {c,x1,y1,z} = 32B/矩形
struct RectV {
    unsigned int color; float x, y, z;
} __attribute__((packed, aligned(16)));

#define MAX_RECT 256  // 一帧矩形上限（实际约 40 个，留足余量）
static struct RectV rect_buf[MAX_RECT * 2] __attribute__((aligned(16)));
static int rect_count = 0;

void Render_DrawRect(float x, float y, float w, float h, unsigned int color) {
    if (rect_count < MAX_RECT) {
        // 写入走非缓存别名（真机 GE DMA 直接读物理内存）
        struct RectV *v = &((struct RectV *)UNCACHED(rect_buf))[rect_count * 2];
        unsigned int c = FIXC(color);
        v[0].color = c; v[0].x = x;   v[0].y = y;   v[0].z = 0.5f;
        v[1].color = c; v[1].x = x+w; v[1].y = y+h; v[1].z = 0.5f;
        rect_count++;
    }
}

static void flush_rects(void) {
    if (rect_count == 0) return;
    sceGuDrawArray(GU_SPRITES,
                   GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   rect_count * 2, NULL, rect_buf);
    rect_count = 0;
}

// ========= 方块颜色 =========
static unsigned int tile_color(int value) {
    switch (value) {
        case 0:    return 0xFFcdc1b4;
        case 2:    return 0xFFeee4da;
        case 4:    return 0xFFede0c8;
        case 8:    return 0xFFf2b179;
        case 16:   return 0xFFf59563;
        case 32:   return 0xFFf67c5f;
        case 64:   return 0xFFf65e3b;
        case 128:  return 0xFFedcf72;
        case 256:  return 0xFFedcc61;
        case 512:  return 0xFFedc850;
        case 1024: return 0xFFedc53f;
        case 2048: return 0xFFedc22e;
        default:   return 0xFF3c3a32;
    }
}
static unsigned int tile_text_color(int v) {
    return (v <= 4) ? 0xFF776e65 : 0xFFf9f6f2;
}

// ========= 棋盘 =========
void Render_DrawBoard(void) {
    const float board_w = 240;
    const float board_h = 240;
    const float board_x = (SCREEN_WIDTH - board_w) / 2;
    const float board_y = 30;

    Render_DrawRect(board_x - 4, board_y - 4, board_w + 8, board_h + 8, 0xFFbbada0);

    const float gap  = 6;
    const float cell = (board_w - gap * 5) / BOARD_SIZE;

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            float x = board_x + gap + c * (cell + gap);
            float y = board_y + gap + r * (cell + gap);

            // 空格子背景
            Render_DrawRect(x, y, cell, cell, 0xFFcdc1b4);

            int v = g_game.grid[r][c];
            if (v == 0) continue;

            // 有数字的方块
            Render_DrawRect(x, y, cell, cell, tile_color(v));

            char buf[8];
            snprintf(buf, sizeof(buf), "%d", v);
            int len = (int)strlen(buf);
            int scale = (len <= 3) ? 2 : 1;

            float tw = len * 8 * scale;
            float th = 8 * scale;
            float tx = x + (cell - tw) / 2;
            float ty = y + (cell - th) / 2;

            add_text(buf, tx, ty, scale, tile_text_color(v));
        }
    }
}

// ========= Header（2048 标题 + 分数） =========
void Render_DrawHeader(void) {
    // 2048 色块
    const float bg2048_x = 14;
    const float bg2048_w = 72;
    const float bg2048_h = 20;
    Render_DrawRect(bg2048_x, 6, bg2048_w, bg2048_h, 0xFFedc22e);
    add_text("2048", bg2048_x + (bg2048_w - 4*8*2)/2, 9, 2, 0xFFf9f6f2);

    // SCORE 框
    float bx = SCREEN_WIDTH - 140;
    float by = 4;
    Render_DrawRect(bx, by, 60, 24, 0xFFbbada0);
    add_text("SCORE", bx + 6, by + 2, 1, 0xFFeee4da);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", g_game.score);
    add_text(buf, bx + 6, by + 12, 1, 0xFFFFFFFF);

    // BEST 框
    bx = SCREEN_WIDTH - 74;
    Render_DrawRect(bx, by, 60, 24, 0xFFbbada0);
    add_text("BEST", bx + 6, by + 2, 1, 0xFFeee4da);
    snprintf(buf, sizeof(buf), "%d", g_game.best_score);
    add_text(buf, bx + 6, by + 12, 1, 0xFFFFFFFF);
}

// ========= Footer + 连击提示 =========
void Render_DrawFooter(void) {
    // 底部左：按键提示
    add_text("DIR:MOVE  O:PAUSE  TRI:RESTART",
             14, SCREEN_HEIGHT - 9, 1, 0xFF776e65);

    // 底部右：连击（垂直两行显示，避免太长）
    if (g_game.state == STATE_PLAYING && g_game.combo_count >= 1) {
        char buf[24];

        // 第一行：COMBO xN + 基础分
        snprintf(buf, sizeof(buf), "COMBO x%d + %d",
                 g_game.combo_streak, g_game.combo_base);
        float y1 = SCREEN_HEIGHT - 33;
        add_text(buf, SCREEN_WIDTH - 18 - (int)strlen(buf)*8, y1, 1, 0xFFedc22e);

        // 第二行：奖励分（streak≥3 翻倍时深红强调）
        if (g_game.last_bonus > 0) {
            snprintf(buf, sizeof(buf), "  +%d", g_game.last_bonus);
            float y2 = y1 + 10;
            add_text(buf, SCREEN_WIDTH - 18 - (int)strlen(buf)*8, y2, 1,
                     g_game.combo_streak >= 3 ? 0xFFf65e3b : 0xFFf2b179);
        }
    }
}

// ========= 居中辅助 =========
// 文字宽 = len * 8 * scale；起点 x = center - width/2
#define CX(str, ctr, sc) ((ctr) - ((int)(sizeof(str)-1) * 8 * (sc)) / 2)

void Render_DrawTitle(void) {
    Render_DrawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0xAAfaf8ef);
    add_text("2048", SCREEN_WIDTH/2 - 32, 90, 2, 0xFF776e65);
    add_text("PSP EDITION", SCREEN_WIDTH/2 - 44, 110, 1, 0xFFbbada0);
    add_text("PRESS ANY DIR TO START", SCREEN_WIDTH/2 - 88, 150, 1, 0xFF776e65);
    add_text("USE ARROWS TO MOVE TILES", SCREEN_WIDTH/2 - 96, 170, 1, 0xFF776e65);
}

void Render_DrawPaused(void) {
    Render_DrawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x77000000);
    add_text("PAUSED", SCREEN_WIDTH/2 - 40, 120, 2, 0xFFFFFFFF);
    add_text("PRESS O TO RESUME", SCREEN_WIDTH/2 - 68, 145, 1, 0xFFFFFFFF);
}

void Render_DrawWin(void) {
    Render_DrawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x88f2b179);
    add_text("YOU WIN!", SCREEN_WIDTH/2 - 64, 100, 2, 0xFF776e65);
    char buf[32];
    snprintf(buf, sizeof(buf), "SCORE: %d  MAX COMBO: %d",
             g_game.score, g_game.max_combo);
    add_text(buf, SCREEN_WIDTH/2 - 92, 130, 1, 0xFF776e65);
    snprintf(buf, sizeof(buf), "BEST: %d", g_game.best_score);
    add_text(buf, SCREEN_WIDTH/2 - 28, 148, 1, 0xFF776e65);
    add_text("O:KEEP PLAY  TRI:RESTART",
             SCREEN_WIDTH/2 - 76, 170, 1, 0xFF776e65);
}

void Render_DrawGameOver(void) {
    Render_DrawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x88eee4da);
    add_text("GAME OVER", SCREEN_WIDTH/2 - 72, 100, 2, 0xFF776e65);
    char buf[32];
    snprintf(buf, sizeof(buf), "SCORE: %d  MAX COMBO: %d",
             g_game.score, g_game.max_combo);
    add_text(buf, SCREEN_WIDTH/2 - 92, 130, 1, 0xFF776e65);
    snprintf(buf, sizeof(buf), "BEST: %d", g_game.best_score);
    add_text(buf, SCREEN_WIDTH/2 - 28, 148, 1, 0xFF776e65);
    add_text("PRESS TRI TO RESTART", SCREEN_WIDTH/2 - 80, 170, 1, 0xFF776e65);
}
