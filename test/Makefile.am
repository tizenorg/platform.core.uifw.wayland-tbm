TESTS = 			\
	tbm-server-test	\
	tbm-client-test \
	tbm-subcomp-test

#bin_PROGRAMS = $(TESTS)
#check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

SERVER_TEST_CFLAGS = -I$(top_srcdir)/src/ $(AM_CFLAGS) $(WL_TBM_SERVER_CFLAGS)
SERVER_TEST_LDADD = ../src/libwayland-tbm-server.la $(WL_TBM_COMMON_LIBS) $(WL_TBM_SERVER_LIBS)
CLIENT_TEST_CFLAGS = -I$(top_srcdir)/src/ $(AM_CFLAGS) $(WL_TBM_CLIENT_CFLAGS)
CLIENT_TEST_LDADD = ../src/libwayland-tbm-client.la $(WL_TBM_COMMON_LIBS) $(WL_TBM_CLIENT_LIBS)

tbm_server_test_LDADD = $(SERVER_TEST_LDADD)
tbm_server_test_CFLAGS = $(SERVER_TEST_CFLAGS)
tbm_server_test_SOURCES = tbm-server-test.c

tbm_client_test_LDADD = $(CLIENT_TEST_LDADD)
tbm_client_test_CFLAGS = $(CLIENT_TEST_CFLAGS)
tbm_client_test_SOURCES = tbm-client-test.c

tbm_subcomp_test_LDADD = $(CLIENT_TEST_LDADD) $(SERVER_TEST_LDADD)
tbm_subcomp_test_CFLAGS = $(CLIENT_TEST_CFLAGS) $(SERVER_TEST_CFLAGS)
tbm_subcomp_test_SOURCES = tbm-subcomp-test.c
