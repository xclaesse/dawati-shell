AM_CFLAGS = \
	$(PANEL_DEVICES_CFLAGS) \
	-DPKGICONDIR=\"$(PANEL_DEVICES_DATADIR)/icons\" \
	-DI_KNOW_THE_DEVICEKIT_POWER_API_IS_SUBJECT_TO_CHANGE \
	-I$(top_srcdir)/panels/devices/src \
	$(NULL)

LDADD = \
	$(PANEL_DEVICES_LIBS) \
	$(NULL)

tools = \
	test-battery-device \
	test-battery-icon \
	test-conf \
	test-display-device \
	test-disk-tile \
	test-folder-button \
	test-folder-tile \
	test-storage-device \
	test-storage-device-tile \
	$(NULL)

if INSTALL_TEST_TOOLS
testsdir = $(libexecdir)/dawati-shell/tests
tests_PROGRAMS = $(tools)
else
noinst_PROGRAMS = $(tools)
endif

test_battery_device_SOURCES = \
	test-battery-device.c \
	$(top_srcdir)/panels/devices/src/mpd-battery-device.c \
	$(top_srcdir)/panels/devices/src/mpd-gobject.c \
	$(NULL)

test_battery_icon_SOURCES = \
	test-battery-icon.c \
	$(top_srcdir)/panels/devices/src/mpd-battery-icon.c \
	$(NULL)

test_conf_SOURCES = \
	test-conf.c \
	$(top_srcdir)/panels/devices/src/mpd-conf.c \
	$(top_srcdir)/panels/devices/src/mpd-gobject.c \
	$(NULL)

test_display_device_LDADD = \
	$(PANEL_DEVICES_LIBS) \
	$(top_builddir)/gpm/libgpm.la \
	$(NULL)

test_display_device_SOURCES = \
	test-display-device.c \
	$(top_srcdir)/panels/devices/src/mpd-conf.c \
	$(top_srcdir)/panels/devices/src/mpd-gobject.c \
	$(top_srcdir)/panels/devices/src/mpd-display-device.c \
	$(NULL)

test_disk_tile_SOURCES = \
	test-disk-tile.c \
	$(top_srcdir)/panels/devices/src/mpd-storage-device.c \
	$(top_srcdir)/panels/devices/src/mpd-disk-tile.c \
	$(top_srcdir)/panels/devices/src/mpd-gobject.c \
	$(NULL)

test_folder_button_SOURCES = \
	test-folder-button.c \
	$(top_srcdir)/panels/devices/src/mpd-folder-button.c \
	$(top_srcdir)/panels/devices/src/mpd-gobject.c \
	$(NULL)

test_folder_tile_SOURCES = \
	test-folder-tile.c \
	$(top_srcdir)/panels/devices/src/mpd-folder-button.c \
	$(top_srcdir)/panels/devices/src/mpd-folder-tile.c \
	$(top_srcdir)/panels/devices/src/mpd-gobject.c \
	$(NULL)

test_storage_device_SOURCES = \
	test-storage-device.c \
	$(top_srcdir)/panels/devices/src/mpd-gobject.c \
	$(top_srcdir)/panels/devices/src/mpd-storage-device.c \
	$(NULL)

test_storage_device_tile_SOURCES = \
	test-storage-device-tile.c \
	$(top_srcdir)/panels/devices/src/mpd-gobject.c \
	$(top_srcdir)/panels/devices/src/mpd-storage-device.c \
	$(top_srcdir)/panels/devices/src/mpd-storage-device-tile.c \
	$(NULL)
