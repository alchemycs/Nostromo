/*
    Nostromo_n50 configuration tools to support Belkin's Nostromo n50
    Copyright (C) 2003 Paul Bohme and others

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef UI_SUPPORT_H
#define UI_SUPPORT_H

#include "nost_data.h"
#include <string.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <FL/Fl_Menu.H>
#include <FL/Fl_Button.H>

extern void add_new_configuration(const char* newtxt, model_type model, int cfg);
extern void startup();
extern void shutdown();
extern void set_current_key(int index);
extern void set_current_configuration(int cfg);
extern void change_key_mapping_name(const char* txt, int cfg, int key);
extern void change_key_repeat_flag(int value, int cfg, int key);
extern void change_key_remote_flag(int value, int cfg, int key);
extern void change_key_repeat_delay(int value, int cfg, int key);
extern void set_current_key_mapping_type(int type);
extern void set_current_mode(int type);
extern void rename_current_configuration_done(const char* txt);
extern void delete_current_configuration_done();

extern Fl_Menu_Item* configuration_list;
extern nost_data* nost_cfg;
extern int current_mode;
extern int current_key;
extern int current_keystroke;
extern void save();

const char* create_key_display(const XKeyEvent* event, int delay);
#endif // UI_SUPPORT_H
