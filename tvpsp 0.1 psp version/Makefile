# Makefile for TVPSP
TARGET = tvpsp
OBJS = jmemsrc.o tvpsp.o

LIBS = -lstdc++ -ljpeg -lmad -lz -lm \
	-lpsphprm -lpspusb -lpspusbstor -lpsppower -lpspdisplay -lpspgu -lpspge -lpspctrl -lpspaudiolib -lpspaudio -lpspwlan

INCDIR = 
CFLAGS = -O2 -G0 -g -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

#SDL specific
PSPBIN = $(PSPSDK)/../bin
#CFLAGS += `$(PSPBIN)/sdl-config --cflags`
#LIBS += `$(PSPBIN)/sdl-config --libs`
CFLAGS += $(shell $(PSPBIN)/sdl-config --cflags | sed s/-Dmain=SDL_main//)
LIBS += $(shell $(PSPBIN)/sdl-config --libs | sed s/-lSDLmain//)

LIBDIR =
LDFLAGS =

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = TVPSP
#PSP_EBOOT_ICON = pbpstuff/Icon0.png
#PSP_EBOOT_PIC1 = pbpstuff/Pic1.png

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
