#
#
#

AUTOMAKE_OPTIONS = foreign

nostromo_daemondir = $(datadir)/pixmaps

CXXFLAGS = @DEBUG_FLAGS@ @NOSTROMO_CFLAGS@ @FLTK_CXXFLAGS@ -DIMG_PATH=\"$(nostromo_daemondir)\"
CFLAGS = @DEBUG_FLAGS@ @NOSTROMO_CFLAGS@
LDFLAGS =  @NOSTROMO_LIBS@ @FLTK_LIBS@ -lXtst -lpthread @DEBUG_FLAGS@ @LIBS@

bin_PROGRAMS = nostromo_config nostromo_daemon nostromo_remote
nostromo_config_SOURCES = nost_data.h \
                          ui_support.h \
                          ui_support.cxx \
                          save.cxx \
                          load.cxx \
                          NewMapWindow.h \
                          NewMapWindow.cxx

nodist_nostromo_config_SOURCES = ui.h \
                                 ui.cxx

nostromo_config_LDADD = @FLTK_LIBS@

nostromo_daemon_SOURCES = nost_data.h \
                          daemon.cxx \
						  docklet.cxx \
						  eggtrayicon.h \
						  eggtrayicon.c \
                          load.cxx

nostromo_remote_SOURCES = remote.cxx load.cxx

nostromo_remote_LDADD = -lXtst

BUILT_SOURCES = ui.h ui.cxx

nostromo_daemon_DATA = n50_tray.png

EXTRA_DIST = n50_tray.png ui.fl

SUFFIXES = .cxx .h .fl

.fl.cxx:
	fluid -c $<

.fl.h:
	fluid -c $<

ui.cxx : ui.fl

ui.h : ui.fl

