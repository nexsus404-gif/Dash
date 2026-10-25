ARCHS = arm64
TARGET = iphone:clang:latest:12.0
INSTALL_TARGET_PROCESSES = Dash

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = Dash

Dash_FILES = \
	src/main.m \
	src/AppDelegate.m \
	src/RootViewController.m \
	src/Database.c \
	src/PackagesParser.c

Dash_FRAMEWORKS = UIKit CoreGraphics

# -Os: оптимизация по размеру (кэш инструкций A8 = 64KB)
# -fno-objc-arc: ручное управление памятью в hot paths
# -fvisibility=hidden: меньше символов → быстрее dyld
# -Wl,-dead_strip: убрать мёртвый код из бинарника
Dash_CFLAGS = \
	-Os \
	-fno-objc-arc \
	-fvisibility=hidden \
	-std=c11 \
	-Wall -Wextra

Dash_OBJCFLAGS = \
	-Os \
	-fno-objc-arc \
	-fvisibility=hidden

Dash_LDFLAGS = \
	-lsqlite3 \
	-Wl,-dead_strip

include $(THEOS_MAKE_PATH)/application.mk

after-install::
	install.exec "uicache -p /Applications/Dash.app" || true
