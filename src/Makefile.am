bin_PROGRAMS = rainbowpath
rainbowpath_SOURCES = rainbowpath.c \
	args.c \
	styles.c \
	config.c \
	indexer.c \
	parser_common.c \
	style_parser.c \
	config_parser.c \
	list.c \
	bytes.c \
	utils.c

rainbowpath_CFLAGS = -DSYSCONFDIR=\"@sysconfdir@\"

if USE_CURSES
rainbowpath_SOURCES += curses.c
rainbowpath_CFLAGS += $(CURSES_CFLAGS)
rainbowpath_LDADD = $(CURSES_LIBS)
else
rainbowpath_SOURCES += builtin.c
endif

