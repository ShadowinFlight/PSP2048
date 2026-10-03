#include "game.h"

// 全局游戏实例
Game g_game;

// 阻止 libcglue 引入 sceNetInet（真机未加载网络模块时静态导入会 8002013C）
// 本游戏不做任何网络操作，给失败桩即可
#include <errno.h>
#include <sys/types.h>
int __socket_close(int s) { (void)s; return -1; }
ssize_t recv(int s, void *buf, size_t len, int flags) {
    (void)s; (void)buf; (void)len; (void)flags; return -1;
}
ssize_t send(int s, const void *buf, size_t len, int flags) {
    (void)s; (void)buf; (void)len; (void)flags; return -1;
}
// libcglue 的 __set_errno 是强符号且内部调 sceNetInetGetErrno。
// 在目标文件里先定义它，归档里的 __set_errno.o 就不会被抽出。
int __set_errno(int psp_err) {
    if ((psp_err & 0x80010000) != 0x80010000) return psp_err;  // 非错误码原样返回
    errno = EIO;
    return -1;
}

// GU 命令列表缓冲区，必须 16 字节对齐。
// PSP 1000 只有 32MB，帧缓冲约 1.7MB、深度缓冲 0.5MB，list 用 2MB 刚好
unsigned int __attribute__((aligned(16))) list[524288];   // 2MB

// 模块信息
PSP_MODULE_INFO("PSP2048", PSP_MODULE_USER, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(1024);   // 1MB 堆足够（PSP 1000 只有 32MB）

// ---------- 退出回调 ----------
static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1; (void)arg2; (void)common;
    sceKernelExitGame();
    return 0;
}

static int CallbackThread(SceSize args, void *argp) {
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int SetupCallbacks(void) {
    int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
    return thid;
}

// ---------- 图形初始化 ----------
// PSP homebrew 最简单的 GU 2D 模板：
//   - 所有 DrawArray 都用 GU_TRANSFORM_2D（跳过矩阵，顶点直接送光栅化）
//   - 不设任何矩阵（GU_TRANSFORM_2D 不需要）
//   - 纯色矩形/像素，不用纹理
void InitGU(void) {
    void *fbp0 = (void*)0x40000000;
    void *fbp1 = (void*)(0x40000000 + BUFFER_WIDTH * SCREEN_HEIGHT * 4);
    void *zbp  = (void*)(0x40000000 + BUFFER_WIDTH * SCREEN_HEIGHT * 8);

    sceGuInit();
    sceGuStart(GU_DIRECT, list);

    sceGuDrawBuffer(GU_PSM_8888, fbp0, BUFFER_WIDTH);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, fbp1, BUFFER_WIDTH);
    sceGuDepthBuffer(zbp, BUFFER_WIDTH);

    sceGuOffset(2048 - (SCREEN_WIDTH / 2), 2048 - (SCREEN_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);

    // 所有测试关掉 —— 2D 绘制靠 z 值和顺序控制（其实 GU_TRANSFORM_2D 跳过深度）
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_DITHER);
    sceGuDisable(GU_FOG);

    // 混合：支持半透明状态层（pause/win/gameover overlay）
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuFrontFace(GU_CW);

    sceGuClearColor(FIXC(0xFFfaf8ef));  // 米黄背景
    sceGuClearDepth(0);
    sceGuClearStencil(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    // GU_TRANSFORM_2D 下矩阵不用，但初始化 identity 总是安全的
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGumUpdateMatrix();

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

// ---------- 游戏初始化 ----------
void Game_Init(void) {
    // 启动日志——真机跑起来就写，方便排查 8002013C
    // 如果真机上这个文件不存在，说明模块加载阶段就崩了
    FILE *log = fopen("ms0:/PSP2048_LOG.txt", "w");
    if (log) {
        fprintf(log, "PSP2048 boot OK\n");
        fclose(log);
    }

    // 不用 memset —— PPSSPP HLE 可能不设 v0，编译器 -O2 会复用
    Game *g = &g_game;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) g->grid[i][j] = 0;
    g->score = 0;
    g->best_score = 0;
    g->has_won = false;
    g->keep_playing = false;
    g->state = STATE_TITLE;
    g->combo_streak = 0;
    g->combo_count = 0;
    g->combo_base = 0;
    g->last_bonus = 0;
    g->max_combo = 0;

    // 初始化控制器（只需要数字按键）
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    // 加载最高分
    Save_LoadBest();

    // 初始化随机种子
    u64 tick;
    sceRtcGetCurrentTick(&tick);
    srand((unsigned int)(tick & 0xFFFFFFFF));
}

// ---------- 主函数 ----------
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    SetupCallbacks();
    InitGU();
    Game_Init();

    // 主循环：输入 → 更新 → 渲染
    unsigned int prev_buttons = 0;
    while (1) {
        sceCtrlReadBufferPositive(&g_game.pad, 1);
        unsigned int cur = g_game.pad.Buttons;
        unsigned int pressed = cur & ~prev_buttons;
        prev_buttons = cur;

        // △ 键全局重启——任何界面都能一键重来
        if (pressed & PSP_CTRL_TRIANGLE) {
            Board_Reset();
            g_game.state = STATE_PLAYING;
            continue;
        }

        // 按状态处理其他按键
        switch (g_game.state) {
            case STATE_TITLE:
                if (pressed & (PSP_CTRL_UP | PSP_CTRL_DOWN |
                               PSP_CTRL_LEFT | PSP_CTRL_RIGHT |
                               PSP_CTRL_CIRCLE)) {
                    Board_Reset();
                    g_game.state = STATE_PLAYING;
                }
                break;

            case STATE_PLAYING:
                if (pressed & (PSP_CTRL_UP | PSP_CTRL_DOWN |
                               PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) {
                    Board_HandleInput(pressed);
                }
                if (pressed & PSP_CTRL_CIRCLE) {
                    g_game.state = STATE_PAUSED;
                }
                break;

            case STATE_PAUSED:
                if (pressed & PSP_CTRL_CIRCLE) {
                    g_game.state = STATE_PLAYING;
                }
                break;

            case STATE_WIN:
                if (pressed & PSP_CTRL_CIRCLE) {
                    g_game.keep_playing = true;
                    g_game.state = STATE_PLAYING;
                }
                break;

            case STATE_GAMEOVER:
                // 没有额外按键——△ 全局重启已在外面处理
                break;

            default: break;
        }

        // 渲染
        Render_BeginFrame();

        // 先绘制棋盘背景（始终可见）
        Render_DrawHeader();
        Render_DrawBoard();
        Render_DrawFooter();

        // 状态叠加层
        switch (g_game.state) {
            case STATE_TITLE:    Render_DrawTitle();    break;
            case STATE_PAUSED:   Render_DrawPaused();   break;
            case STATE_WIN:      Render_DrawWin();      break;
            case STATE_GAMEOVER: Render_DrawGameOver(); break;
            default: break;
        }

        Render_EndFrame();

        // 垂直同步
        sceDisplayWaitVblankStart();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
