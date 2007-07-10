#
#
#

EXTRA_DIST= \
	color_menu.png \
	key_map_type_menu.png \
	main_window.png \
	new_dialog.png \
	nostromo_n50.html \
	rename_dialog.png 

install-data-local:
	$(mkinstalldirs) $(prefix)/doc/nostromo_n50-$(VERSION)
	$(INSTALL_DATA) $(srcdir)/color_menu.png $(prefix)/doc/nostromo_n50-$(VERSION)/color_menu.png
	$(INSTALL_DATA) $(srcdir)/key_map_type_menu.png $(prefix)/doc/nostromo_n50-$(VERSION)/key_map_type_menu.png
	$(INSTALL_DATA) $(srcdir)/main_window.png $(prefix)/doc/nostromo_n50-$(VERSION)/main_window.png
	$(INSTALL_DATA) $(srcdir)/new_dialog.png $(prefix)/doc/nostromo_n50-$(VERSION)/new_dialog.png
	$(INSTALL_DATA) $(srcdir)/nostromo_n50.html $(prefix)/doc/nostromo_n50-$(VERSION)/nostromo_n50.html
	$(INSTALL_DATA) $(srcdir)/rename_dialog.png $(prefix)/doc/nostromo_n50-$(VERSION)/rename_dialog.png

uninstall-local:
	-rm -f $(prefix)/share/doc/nostromo_n50-$(VERSION)/color_menu.png 
	-rm -f $(prefix)/share/doc/nostromo_n50-$(VERSION)/key_map_type_menu.png 
	-rm -f $(prefix)/share/doc/nostromo_n50-$(VERSION)/main_window.png 
	-rm -f $(prefix)/share/doc/nostromo_n50-$(VERSION)/new_dialog.png 
	-rm -f $(prefix)/share/doc/nostromo_n50-$(VERSION)/nostromo_n50.html 
	-rm -f $(prefix)/share/doc/nostromo_n50-$(VERSION)/rename_dialog.png

