AM_CFLAGS = $(AUDIOSESSIONMGR_CFLAGS) 

lib_LTLIBRARIES = libaudio-session-mgr.la
libaudio_session_mgr_la_SOURCES = $(srcdir)/src/audio-session-mgr.c 

libaudio_session_mgr_la_LIBADD = $(AUDIOSESSIONMGR_LIBS) $(MMLOG_LIBS)
libaudio_session_mgr_la_LDFLAGS = -Wl,-init, __ASM_init_module
libaudio_session_mgr_la_LDFLAGS += -Wl,-fini, __ASM_fini_module

libaudio_session_mgr_la_CPPFLAGS = -I$(srcdir)/include -D__DEBUG_MODE__ -DUSE_VCONF $(MMLOG_CFLAGS) -D__USE_LOGMANAGER__

if USE_SECURITY
libaudio_session_mgr_la_CPPFLAGS += $(SECURITY_CFLAGS) -DUSE_SECURITY
libaudio_session_mgr_la_LIBADD += $(SECURITY_LIBS)
endif

audiosessionmgrincludedir = $(includedir)/mmf
audiosessionmgrinclude_HEADERS = \
		$(srcdir)/include/audio-session-manager-types.h \
		$(srcdir)/include/audio-session-manager.h


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = audio-session-mgr.pc

