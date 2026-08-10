# Metro Rush — C99 software-rendered port
# Targets:
#   make              — Linux (libX11)
#   make static       — Linux with statically linked libgcc/libstdc; X11 still dynamic
#   make win98        — Windows 98 / Win32 PE via MinGW i686

CC ?= gcc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Iinclude
LDFLAGS_LINUX = -lX11 -lm -lrt
# Fully static libc+libm where possible; X11 remains a system shared lib.
LDFLAGS_STATIC = -static-libgcc -lm -lrt -lX11

WIN32_CC ?= i686-w64-mingw32-gcc
WIN32_CFLAGS = -std=c99 -O2 -Wall -Wextra -Iinclude -DMETRO_WIN32 \
	-DWINVER=0x0400 -D_WIN32_WINDOWS=0x0400 -D_WIN32_WINNT=0x0400 \
	-fno-stack-protector
WIN32_LDFLAGS = -mwindows -lgdi32 -luser32 -static -static-libgcc \
	-Wl,--major-subsystem-version,4,--minor-subsystem-version,0 \
	-Wl,--major-os-version,4,--minor-os-version,0 \
	-Wl,--disable-nxcompat,--disable-dynamicbase

GAME_SRCS = \
	src/math.c \
	src/game_state.c \
	src/update_game.c \
	src/systems_player.c \
	src/systems_spawn.c \
	src/systems_collision.c \
	src/systems_score.c \
	src/systems_world.c \
	src/systems_input.c \
	src/render.c \
	src/storage.c \
	src/main.c

.PHONY: all clean static win98 win32 linux

all: linux

linux: metro-rush

metro-rush: $(GAME_SRCS) src/platform_x11.c
	$(CC) $(CFLAGS) -DMETRO_LINUX $(GAME_SRCS) src/platform_x11.c -o $@ $(LDFLAGS_LINUX)

static: $(GAME_SRCS) src/platform_x11.c
	$(CC) $(CFLAGS) -DMETRO_LINUX $(GAME_SRCS) src/platform_x11.c -o metro-rush $(LDFLAGS_STATIC)

win98 win32: metro-rush.exe

metro-rush.exe: $(GAME_SRCS) src/platform_win32.c
	$(WIN32_CC) $(WIN32_CFLAGS) $(GAME_SRCS) src/platform_win32.c -o $@ $(WIN32_LDFLAGS)

clean:
	rm -f metro-rush metro-rush.exe metro-rush-libx11 *.o src/*.o
