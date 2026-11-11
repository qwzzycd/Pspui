TARGET = NexusOS
OBJS = main.o

INCDIR =
CFLAGS = -O3 -G0 -Wall -fno-pic -mno-abicalls -D_PSP_FW_VERSION=661
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LDFLAGS =

LIBS = -losl -lpng -lz -lintrafont -lpspgu -lpspgum -lpsppower -lpsprtc -lpspctrl -lpspdisplay -lpspiofilemgr -lpspsysmem -lpsputility -lpspkernel -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = NexusOS
PSP_EBOOT_ICON = NULL
PSP_EBOOT_PIC1 = NULL

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak