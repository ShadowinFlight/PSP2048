# PSP 2048 - Makefile
# PSPSDK / pspdev 工具链
# 兼容 PSP 1000/2000/3000 + 6.60 PRO-C2 自制系统

TARGET = psp2048
OBJS = src/main.o src/board.o src/render.o src/save.o

INCDIR =
CFLAGS = -O2 -G0 -Wall -Wno-unused-function
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
# 注意：不要加 -lpspsdk（调试辅助库，真机可能缺依赖导致 8002013C）
#       -lm 在 PSP 上不需要（数学函数在 libpspkernel / libc 里）
LIBS = -lpspgu -lpspgum -lpspctrl -lpspdisplay -lpsprtc -lpspuser

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = 2048

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
