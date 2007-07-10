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

#include "NewMapWindow.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <FL/Fl.H>
#include <FL/x.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Button.H>
#include <X11/X.h>
#include <X11/Xutil.h>
#define XK_MISCELLANY
#include <X11/keysymdef.h>

#include "ui.h"
#include "ui_support.h"

static const char* button_names[] = {
    "none",
    "Left Button",
    "Middle Button",
    "Right Button"
};

NewMapWindow::NewMapWindow(int width, int height, const char* txt) :
    Fl_Window(width, height, txt)
{
}

int NewMapWindow::handle(int event)
{
    int ret = 0;
    char lbl[256];
    static struct timeval start;

    switch(event) {
        case FL_KEYDOWN:
        {
            int keysym = XLookupKeysym(const_cast<XKeyEvent*>(&fl_xevent->xkey), fl_xevent->xkey.state & ShiftMask);
            switch(keysym) {
                case XK_Shift_L:
                case XK_Shift_R:
                case XK_Shift_Lock:
                case XK_Control_L:
                case XK_Control_R:
                case XK_Meta_L:
                case XK_Meta_R:
                /* Have no idea what these are, but might as well. */
                case XK_Alt_L:
                case XK_Alt_R:
                case XK_Super_L:
                case XK_Super_R:
                case XK_Hyper_L:
                case XK_Hyper_R:
                  ret = 1;
                  break;
                default:
                  /* Stuff the keystroke into the current queue */
                  if(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].key_count < MAX_KEYSTROKES) {
                      int cur = nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].key_count++;
                      if(cur) {
                          struct timeval stop;
                          gettimeofday(&stop, NULL);
                          int delay = (((stop.tv_sec - start.tv_sec) * 1000000) + (stop.tv_usec - start.tv_usec)) / 1000;
                          nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].data[cur].delay = delay;
                      } else {
                          nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].data[cur].delay = 0;
                      }
                      nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].data[cur].type = STROKE_KEY;
                      nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].data[cur].code = fl_xevent->xkey.keycode;
                      nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].data[cur].state = fl_xevent->xkey.state;
                      nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].data[cur].display = strdup(create_key_display(&fl_xevent->xkey, nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].data[cur].delay));
                      create_key_map_browser->add(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].data[cur].display);
                      create_key_map_browser->middleline(create_key_map_browser->size());
                      gettimeofday(&start, NULL);
                  }
                  ret = 1;
                  break;
            }
            break;
        }
        case FL_KEYUP:
            ret = 1;
            break;
        case FL_PUSH:
            if(Fl::event_inside(create_key_map_browser)) {
                /* Stuff the keystroke into the current queue */
                if(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].key_count < MAX_KEYSTROKES) {
                    int cur = nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].key_count++;
                    if(cur) {
                        struct timeval stop;
                        gettimeofday(&stop, NULL);
                        int delay = (((stop.tv_sec - start.tv_sec) * 1000000) + (stop.tv_usec - start.tv_usec)) / 1000;
                        nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].data[cur].delay = delay;
                    } else {
                        nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].data[cur].delay = 0;
                    }
                    nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].data[cur].type = STROKE_MOUSE;
                    nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].data[cur].code = fl_xevent->xbutton.button;
                    sprintf(lbl, "%s\t%dms", button_names[Fl::event_button()], nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].data[cur].delay);
                    nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].data[cur].display = strdup(lbl);
                    create_key_map_browser->add(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].data[cur].display);
                    create_key_map_browser->middleline(create_key_map_browser->size());
                    gettimeofday(&start, NULL);
                }
            } else if(Fl::event_inside(done_box)) {
                set_current_key(current_key);
                delete this;
                return 1;
            } else {
                ret = Fl_Window::handle(event);
            }
            break;
        case FL_RELEASE:
            ret = Fl_Window::handle(event);
            break;
        default:
            ret = Fl_Window::handle(event);
            break;
    }

    return ret;
}

