SUBDIRS = . \
         test

AM_CFLAGS = $(AUDIOSESSIONMGR_CFLAGS)

lib_LTLIBRARIES = libaudio-session-mgr.la
libaudio_session_mgr_la_SOURCES = $(srcdir)/src/audio-session-mgr.c 

libaudio_session_mgr_la_LIBADD = $(AUDIOSESSIONMGR_LIBS) $(VCONF_LIBS) $(MMCOMMON_LIBS)
libaudio_session_mgr_la_LDFLAGS = -Wl,-init, __ASM_init_module
libaudio_session_mgr_la_LDFLAGS += -Wl,-fini, __ASM_fini_module

libaudio_session_mgr_la_CPPFLAGS = -I$(srcdir)/include -DUSE_VCONF $(VCONF_CFLAGS) $(MMCOMMON_CFLAGS)

if USE_SECURITY
libaudio_session_mgr_la_CPPFLAGS += $(SECURITY_CFLAGS) -DUSE_SECURITY
libaudio_session_mgr_la_LIBADD += $(SECURITY_LIBS)
endif

libaudio_session_mgr_la_CPPFLAGS += $(MMLOG_CFLAGS) -DMMF_LOG_OWNER=0x00800000 -DMMF_DEBUG_PREFIX=\"MMF-SESSIONMGR\"
libaudio_session_mgr_la_LIBADD += $(MMLOG_LIBS)

audiosessionmgrincludedir = $(includedir)/mmf
audiosessionmgrinclude_HEADERS = \
		$(srcdir)/include/audio-session-manager-types.h \
		$(srcdir)/include/audio-session-manager.h


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = audio-session-mgr.pc

