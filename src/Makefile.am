bin_PROGRAMS = pipanel

pipanel_CFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)/pipanel"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/pipanel/ui"\" \
	-DPACKAGE_BIN_DIR=\""$(bindir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DGREETER_CONFIG_FILE=\""$(sysconfdir)/lightdm/pi-greeter.conf"\" \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

pipanel_SOURCES = pipanel.c

pipanel_includedir = $(includedir)/pipanel

pipanel_include_HEADERS =

pipanel_LDFLAGS = $(DYNAMIC_FLAGS)
pipanel_DEPENDENCIES_EXTRA = $(BUILTIN_PLUGINS)

pipanel_LDADD = \
		$(BUILTIN_PLUGINS) \
		$(PACKAGE_LIBS) \
		$(X11_LIBS) \
		$(INTLLIBS)

EXTRA_DIST =
