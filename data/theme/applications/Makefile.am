themedir = $(PANEL_APPLICATIONS_THEMEDIR)
dist_theme_DATA = \
	apps-coloured.png \
	apps-normal.png \
	panel.css \
	clear-entry.png \
	clear-entry-hover.png \
	launcher-button-fav-toggle-active.png \
	launcher-button-fav-toggle-checked.png \
	launcher-button-fav-toggle-hover.png \
	launcher-button-fav-toggle.png \
	launcher-hover.png \
	launcher-search-bg.png \
	$(NULL)

# Manage mutter-dawati's texture cache.

if ENABLE_CACHE
texture_cache = $(DESTDIR)$(pkgdatadir)/mx.cache
CACHE_GEN = $(Q:@=@echo '  GEN   '$(texture_cache);)

install-data-hook:
	$(CACHE_GEN)$(MX_CREATE_IMAGE_CACHE) $(DESTDIR)$(pkgdatadir)

uninstall-hook:
	rm -f $(texture_cache)
	$(CACHE_GEN)$(MX_CREATE_IMAGE_CACHE) $(DESTDIR)$(pkgdatadir)
endif

include $(top_srcdir)/build/autotools/Makefile.am.panels
