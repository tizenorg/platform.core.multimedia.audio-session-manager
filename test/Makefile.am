bin_PROGRAMS = asm_testsuite

asm_testsuite_SOURCES = asm_testsuite.c

asm_testsuite_CFLAGS = -I$(srcdir)/../include \
				$(AUDIOSESSIONMGR_CFLAGS) \
				$(DLOG_CFLAGS) \
				$(VCONF_CFLAGS)

asm_testsuite_CFLAGS += -fPIC -pie

asm_testsuite_DEPENDENCIES = $(srcdir)/../.libs/libaudio-session-mgr.la

asm_testsuite_LDADD = $(AUDIOSESSIONMGR_LIBS) \
					$(srcdir)/../.libs/libaudio-session-mgr.la \
					$(DLOG_LIBS) \
					$(VCONF_LIBS)


