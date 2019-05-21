bin_PROGRAMS = rainbowpath
rainbowpath_SOURCES = rainbowpath.c

if USE_CURSES
rainbowpath_SOURCES += curses.c
rainbowpath_CFLAGS = $(CURSES_CFLAGS)
rainbowpath_LDFLAGS = $(CURSES_LIBS)
else
rainbowpath_SOURCES += builtin.c
endif
