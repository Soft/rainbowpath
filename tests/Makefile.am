AM_TESTS_ENVIRONMENT = \
	TEST_PARSER='$(abs_top_srcdir)'/tests/test_parser; \
	export TEST_PARSER;
TESTS = run_parser_tests.sh
check_PROGRAMS = test_parser
test_parser_CFLAGS = -I$(abs_top_srcdir)/src -fsanitize=address,undefined
test_parser_SOURCES = test_parser.c \
	$(abs_top_srcdir)/src/style_parser.c \
	$(abs_top_srcdir)/src/config_parser.c \
	$(abs_top_srcdir)/src/parser_common.c \
	$(abs_top_srcdir)/src/list.c \
	$(abs_top_srcdir)/src/bytes.c \
	$(abs_top_srcdir)/src/utils.c \
	$(abs_top_srcdir)/src/styles.c

