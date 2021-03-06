AC_PREREQ(2.53)
AC_INIT(bisho, 0.27.4)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER(src/config.h)
AC_CONFIG_MACRO_DIR([m4])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CANONICAL_HOST
AC_DISABLE_STATIC
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_ISC_POSIX
AC_HEADER_STDC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

AS_AC_EXPAND(BINDIR, $bindir)

AC_SUBST(LIBEXECDIR)

GETTEXT_PACKAGE=bisho
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [The name of the gettext package.])
AC_SUBST(GETTEXT_PACKAGE)

AS_ALL_LINGUAS
AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.40], [no-xml])

PKG_CHECK_MODULES(DEPS, gmodule-export-2.0
                        libsocialweb-client >= 0.24.8
                        libsocialweb-keystore
                        gtk+-2.0
                        gnome-keyring-1
                        libsoup-2.4
                        rest-0.7
                        rest-extras-0.7
                        unique-1.0
                        mx-gtk-1.0)

AC_ARG_ENABLE([capplet],
              [AC_HELP_STRING([--disable-capplet],
                              [Disable the building of the capplet])],
              [enable_cappled=$enableval],
              [enable_capplet=yes])

AM_CONDITIONAL([ENABLE_CAPPLET], [test "x$enable_capplet" = "xyes"])

AS_IF([test "x$enable_capplet" = "xyes"],
      [
      PKG_CHECK_MODULES(CAPPLET,
                        libgnome-control-center-extension)

      EXTENSIONSDIR=$($PKG_CONFIG libgnome-control-center-extension --variable=extensiondir)
      AC_SUBST(EXTENSIONSDIR)
])

MODULESDIR=$libdir/AC_PACKAGE_NAME
AC_SUBST([MODULESDIR])

AM_GCONF_SOURCE_2

AC_OUTPUT([
        bisho.pc
        Makefile
        data/Makefile
        data/bisho.schemas
        src/Makefile
        panes/Makefile
        po/Makefile.in
])
