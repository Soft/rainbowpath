bin_PROGRAMS = rainbowpath
rainbowpath_SOURCES = rainbowpath.c styles.c parser.c list.c utils.c

if USE_CURSES
rainbowpath_SOURCES += curses.c
rainbowpath_CFLAGS = $(CURSES_CFLAGS)
rainbowpath_LDADD = $(CURSES_LIBS)
else
rainbowpath_SOURCES += builtin.c
endif

