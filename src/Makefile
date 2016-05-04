CC = gcc
LD = gcc

CCFLAGS = -O0 -g -DNDEBUG
CCFLAGS +=  -DKEGS_LITTLE_ENDIAN -DHAVE_SDL -DDISABLE_VIDEO_X11 -DDISABLE_VIDEO_WIN32 -DDISABLE_SOUND_NATIVE -DUSE_CONFIG_FILE

LDFLAGS = -lSDL -lm -flto
PERL = perl
NAME = kegs-sdl.elf

VERSION = 0.64
OBJECTS = adb.o clock.o dis.o engine_c.o joystick_driver.o paddles.o scc.o scc_driver.o iwm.o moremem.o sim65816.o 
OBJECTS += smartport.o sound.o video.o video_driver.o video_driver_sdl.o config_sdl.o configmenu.o sound_driver.o functions.o font.o ki.o rom.o
PROJROOT = .

SPECIALS = 8inst_c 16inst_c size_c

all: $(NAME)

clean: specials_clean
	rm -f *.o kegs-sdl *~ #*#

specials_clean:
	rm -f $(SPECIALS) size_s

$(NAME): $(SPECIALS) $(OBJECTS) compile_time.o
	$(LD) $(LDOPTS) $(OBJECTS) compile_time.o $(LDFLAGS) -o $(NAME) $(XLIBS) $(EXTRA_LIBS)

8inst_c: instable.h
	$(PERL) $(PROJROOT)/make_inst c 8 instable.h > 8inst_c

16inst_c: instable.h
	$(PERL) $(PROJROOT)/make_inst c 16 instable.h > 16inst_c

size_c: size_tab.h
	$(PERL) $(PROJROOT)/make_size c size_tab.h > size_c

engine_c.o: 8inst_c 16inst_c

.c.o:
	$(CC) $(CCFLAGS)  $(OPTS) -c $*.c

compile_time.o: $(OBJECTS)

# DO NOT DELETE
