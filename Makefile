# kc-flow - Multi-architecture Makefile
# Summary: Build system with per-app artifacts and global toolchains.
#
# Author:  KaisarCode
# Website: https://kaisarcode.com
# License: https://www.gnu.org/licenses/gpl-3.0.html

NAME       = kc-flow
SRC        = src/main.c src/model.c src/load.c src/validate.c src/runtime.c \
             src/process.c src/output.c src/graph.c src/worker.c src/cycle.c \
             src/state.c src/artifact.c src/artifact_map.c
SRC_BASE   = $(notdir $(SRC))
OBJ_NAMES  = $(SRC_BASE:.c=.o)
BIN_ROOT   = bin
TOOLCHAIN_ROOT = /usr/local/share/kaisarcode/toolchains

NDK_VER     = android-ndk-r27c
NDK_HOST    = linux-x86_64
NDK_ROOT    = $(TOOLCHAIN_ROOT)/ndk/$(NDK_VER)
NDK_BIN     = $(NDK_ROOT)/toolchains/llvm/prebuilt/$(NDK_HOST)/bin

CC_x86_64    = gcc
CC_aarch64   = aarch64-linux-gnu-gcc
NDK_API      = 24
CC_arm64_v8a = $(NDK_BIN)/aarch64-linux-android$(NDK_API)-clang
CC_win64     = x86_64-w64-mingw32-gcc

CFLAGS  = -Wall -Wextra -O3 -std=c11
WINSOCK = -lws2_32 -ladvapi32

.PHONY: all clean build_arch x86_64 aarch64 arm64-v8a win64

all: x86_64 aarch64 arm64-v8a win64

x86_64:
	$(MAKE) build_arch ARCH=x86_64 CC="$(CC_x86_64)" EXT=""

aarch64:
	$(MAKE) build_arch ARCH=aarch64 CC="$(CC_aarch64)" EXT=""

arm64-v8a:
	@if [ ! -f "$(CC_arm64_v8a)" ]; then \
		echo "[ERROR] NDK Compiler not found at: $(CC_arm64_v8a)"; \
		exit 1; \
	fi
	$(MAKE) build_arch ARCH=arm64-v8a CC="$(CC_arm64_v8a)" EXT=""

win64:
	$(MAKE) build_arch ARCH=win64 CC="$(CC_win64)" EXT=".exe" \
	CFLAGS="$(CFLAGS) -D_WIN32_WINNT=0x0601"

build_arch:
	@mkdir -p $(BIN_ROOT)/$(ARCH)
	$(eval SYS_LIB = /usr/local/lib/kaisarcode/$(ARCH))
	$(eval RPATH   = -Wl,-rpath,$(SYS_LIB))
	$(foreach src,$(SRC),$(CC) $(CFLAGS) -c $(src) -o $(BIN_ROOT)/$(ARCH)/$(notdir $(src:.c=.o));)
	$(CC) $(CFLAGS) $(addprefix $(BIN_ROOT)/$(ARCH)/,$(OBJ_NAMES)) -o \
	$(BIN_ROOT)/$(ARCH)/$(NAME)$(EXT) \
	$(if $(findstring win64,$(ARCH)),$(WINSOCK),$(RPATH))

clean:
	rm -rf $(BIN_ROOT)
