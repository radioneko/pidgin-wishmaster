gddir = $(LIBPURPLE_LIBDIR)/purple-2

pidgin_wishmaster_la_LDFLAGS = -module -avoid-version

if PLUGINS

gd_LTLIBRARIES = pidgin-wishmaster.la

pidgin_wishmaster_la_SOURCES = \
	pidgin-wishmaster.c \
	gln_intl.h

pidgin_wishmaster_la_LIBADD = $(DBUS_LIBS) $(GTK_LIBS)

endif

AM_CPPFLAGS = \
	-DLIBDIR=\"$(LIBPURPLE_LIBDIR)/purple-2/\" \
	-DDATADIR=\"$(LIBPURPLE_DATADIR)\" \
	-DLOCALEDIR=\"$(LIBPURPLE_DATADIR)/locale\" \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(PIDGIN_CFLAGS) \
	$(LIBPURPLE_CFLAGS) \
	$(LIBNOTIFY_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GTK_CFLAGS)

