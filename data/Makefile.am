desktopdir = $(datadir)/applications
desktop_in_files = bisho.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

icondir = $(datadir)/icons/hicolor/48x48/apps
dist_icon_DATA = bisho.png

schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = bisho.schemas

install-data-local:
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)" ; then \
		GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) gconftool-2 --makefile-install-rule $(schema_DATA) 2>&1 > /dev/null; \
	fi
endif

CLEANFILES = $(desktop_DATA)
EXTRA_DIST = $(desktop_in_files)
