
SUBDIRS = drivers mixers

INCLUDES=-I$(includedir) 				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DLOCALEDIR=\"$(datadir)/locale\"		\
	$(GNOME_INCLUDEDIR)

bin_PROGRAMS = voodootracker

voodootracker_SOURCES = \
	audio.c audio.h \
	endian-conv.h \
	driver.h \
	envelope-box.c envelope-box.h \
	gtkspinbutton.c gtkspinbutton.h \
	gui-subs.c gui-subs.h \
	gui.c gui.h \
	instrument-editor.c instrument-editor.h \
	main.c main.h \
	mixer.h \
	module-info.c module-info.h \
	notekeys.c notekeys.h \
	poll.c poll.h \
	preferences.c preferences.h \
	recode.c recode.h \
	sample-display.c sample-display.h \
	sample-editor.c sample-editor.h \
	scope-group.c scope-group.h \
	st-subs.c st-subs.h \
	track-editor.c track-editor.h \
	tracker.c tracker.h \
	xm.c xm.h \
	xm-player.c xm-player.h

voodootracker_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) drivers/libdrivers.a mixers/libmixers.a


