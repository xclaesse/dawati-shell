SUBDIRS=effects alttab notifications presence

xmldir = $(pkgdatadir)/dbus-xml
xml_DATA = 	mnb-panel-dbus.xml	\
		mnb-toolbar-dbus.xml

pkglibdir=$(MUTTER_PLUGIN_DIR)

AM_CFLAGS = \
	$(MUTTER_PLUGIN_CFLAGS) \
	-DPLUGIN_LOCALEDIR=\"$(localedir)\" \
	-DMX_CACHE=\"$(DAWATI_THEME_DIR)/mx.cache\" \
	-DTHEMEDIR=\"$(DAWATI_RUNTIME_THEME_DIR)\" \
	-DPANELSDIR=\"$(DAWATI_PANELS_DIR)\" \
	-I$(top_srcdir)/libdawati-panel

dbus_h = 	$(srcdir)/$(DBUS_GLUE) 			\
		$(srcdir)/$(DBUS_BINDINGS)

source_h =	$(srcdir)/marshal.h			\
		$(top_srcdir)/libdawati-panel/dawati-panel/mpl-panel-common.h \
		$(srcdir)/dawati-netbook.h		\
		$(srcdir)/dawati-netbook-mutter-hints.h	\
		$(srcdir)/mnb-spinner.h			\
		$(srcdir)/mnb-input-manager.h		\
		$(srcdir)/mnb-statusbar.h               \
		$(srcdir)/mnb-toolbar.h                 \
		$(srcdir)/mnb-toolbar-applet.h          \
		$(srcdir)/mnb-toolbar-background.h      \
		$(srcdir)/mnb-toolbar-button.h          \
		$(srcdir)/mnb-toolbar-clock.h           \
		$(srcdir)/mnb-toolbar-icon.h            \
		$(srcdir)/mnb-toolbar-shadow.h          \
		$(srcdir)/mnb-panel.h			\
		$(srcdir)/mnb-panel-frame.h		\
		$(srcdir)/mnb-panel-oop.h


source_c = 	$(srcdir)/mnb-enum-types.c		\
		$(srcdir)/dawati-netbook.c		\
		$(srcdir)/dawati-netbook-mutter-hints.c	\
		$(srcdir)/mnb-spinner.c			\
		$(srcdir)/marshal.c                   	\
		$(srcdir)/mnb-input-manager.c		\
		$(srcdir)/mnb-statusbar.c               \
		$(srcdir)/mnb-toolbar.c                 \
		$(srcdir)/mnb-toolbar-applet.c          \
		$(srcdir)/mnb-toolbar-background.c      \
		$(srcdir)/mnb-toolbar-button.c          \
		$(srcdir)/mnb-toolbar-icon.c            \
		$(srcdir)/mnb-toolbar-clock.c           \
		$(srcdir)/mnb-toolbar-shadow.c          \
		$(srcdir)/mnb-panel.c         		\
		$(srcdir)/mnb-panel-frame.c         	\
		$(srcdir)/mnb-panel-oop.c

dawati_netbook_la_SOURCES  = 	$(dbus_h)	\
				$(source_h) 	\
				$(source_c)

DBUS_GLUE =	mnb-panel-dbus-glue.h		\
		mnb-toolbar-dbus-glue.h

DBUS_BINDINGS =	mnb-panel-dbus-bindings.h	\
		mnb-toolbar-dbus-bindings.h

%-glue.h: %.xml
	dbus-binding-tool --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

%-bindings.h: %.xml
	dbus-binding-tool --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

STAMPS = stamp-marshal.h stamp-mnb-enum-types.h

ENUMS = mnb-enum-types.h \
	mnb-enum-types.c

mnb-enum-types.h: stamp-mnb-enum-types.h
	@true
stamp-mnb-enum-types.h: $(source_h) Makefile
	$(QUIET_GEN)( $(GLIB_MKENUMS) \
	  	--template $(srcdir)/mnb-enum-types.h.in \
	  $(source_h) ) >> xgen-ceth && \
	(cmp -s xgen-ceth mnb-enum-types.h || cp xgen-ceth mnb-enum-types.h) && \
	rm -f xgen-ceth && \
	echo timestamp > $(@F)

mnb-enum-types.c: mnb-enum-types.h
	$(QUIET_GEN)( $(GLIB_MKENUMS) \
	  	--template $(srcdir)/mnb-enum-types.c.in \
	  $(source_h) ) >> xgen-cetc && \
	cp xgen-cetc mnb-enum-types.c && \
	rm -f xgen-cetc

MARSHALS = marshal.c marshal.h

marshal.h: stamp-marshal.h
	@true
stamp-marshal.h: marshal.list
	$(QUIET_GEN)$(GLIB_GENMARSHAL) \
		--prefix=dawati_netbook_marshal \
		--header \
	  $(srcdir)/marshal.list > xgen-cmh \
	&& (cmp -s xgen-cmh marshal.h || cp xgen-cmh marshal.h ) \
	&& rm -f xgen-cmh \
	&& echo timestamp > $(@F)

marshal.c: marshal.h Makefile
	$(QUIET_GEN)( echo "#include \"marshal.h\"" ; \
	  $(GLIB_GENMARSHAL) \
	  	--prefix=dawati_netbook_marshal \
		--body \
	  $(srcdir)/marshal.list --body ) >> xgen-cmc \
	&& cp xgen-cmc marshal.c \
	&& rm -f xgen-cmc

dawati_netbook_la_LDFLAGS  = $(PLUGIN_LT_LDFLAGS) -module -no-undefined
dawati_netbook_la_LIBADD   = @MUTTER_PLUGIN_LIBS@	\
				effects/libeffects.la	\
				alttab/libalttab.la	\
				presence/libpresence.la	\
				notifications/libnotifications.la \
				$(top_builddir)/libdawati-panel/dawati-panel/libdawati-panel.la

pkglib_LTLIBRARIES = dawati-netbook.la

# post-install hook to remove the .la and .a files we are not interested in
# (There is no way to stop libtool generating static libs locally, and we
# cannot do this globally because of libmetacity-private.so).
install-exec-hook:
	rm -f $(DESTDIR)$(pkglibdir)/*.a
	rm -f $(DESTDIR)$(pkglibdir)/*.la

BUILT_SOURCES = $(DBUS_GLUE) $(DBUS_BINDINGS) $(MARSHALS) $(ENUMS)

CLEANFILES = $(BUILT_SOURCES) $(STAMPS)
EXTRA_DIST =	marshal.list \
		mnb-panel-dbus.xml \
		mnb-toolbar-dbus.xml \
		mnb-enum-types.h.in \
		mnb-enum-types.c.in

