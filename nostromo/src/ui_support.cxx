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

#include "ui_support.h"
#include "ui.h"

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "nost_data.h"
#include <FL/Fl_Choice.H>
#include <FL/Fl.H>
#include <FL/x.H>

/// Dynamically managed list of configurations
Fl_Menu_Item* configuration_list = NULL;

/// Master configuration data
nost_data* nost_cfg = NULL;

/// Index of current key mode (or color) being manipulated
int current_mode = 0;

/// Index of current key being manipulated
int current_key = -1;

/// Index of keystroke being manipulated
int current_keystroke = -1;

/// Default names of key mappings for new configs
static char* default_key_names[] = {
    "Button 01",
    "Button 02",
    "Button 03",
    "Button 04",
    "Button 05",
    "Button 06",
    "Button 07",
    "Button 08",
    "Button 09",
    "Button 10",
    "DPad up (top)",
    "DPad down (bottom)",
    "DPad right (front)",
    "DPad left (rear)",
    "Button 11",
    "Button 12",
    "Button 13",
    "Button 14",
    "Button 15",
    "Orange button",
    "Mouse wheel up (forward)",
    "Press mouse wheel",
    "Mouse wheel down (back)"
};

/**
 * Clear out a configuration, set names to defaults, etc.
 * @param cfg Pointer to the configuration to initialize
 * @param othercfg Index of configuration to clone, or > nost_cfg->num_configs for none
 **/
void initialize_new_config(nost_config_data* cfg, int othercfg)
{
    nost_config_data* p = NULL;
    int n, m, s;

    if(othercfg < (nost_cfg->num_configs - 1) && othercfg >= 0) {
        p = &nost_cfg->configs[othercfg];

        memcpy(cfg, p, sizeof(nost_config_data));

        /* Now fix the strings */
        cfg->name = strdup(cfg->name);
        for(m = 0; m < MAX_MODES; m++) {
            for(n = 0; n < MAX_KEYS; n++) {
                cfg->keys[m][n].name = strdup(cfg->keys[m][n].name);
                for(s = 0; s < cfg->keys[m][n].key_count; s++) {
                    cfg->keys[m][n].data[s].display = strdup(cfg->keys[m][n].data[s].display);
                }
            }
        }
    } else {
        memset(cfg, 0, sizeof(cfg));

        for(m = 0; m < MAX_MODES; m++) {
            for(n = 0; n < MAX_KEYS; n++) {
                cfg->keys[m][n].name = strdup(default_key_names[n]);
            }
        }
    }
}

/**
 *
 **/
static void populate_configuration_list()
{
    int n;
    Fl_Menu_Item tmp = { 0 };


    if(configuration_list) {
        configuration->menu(&tmp);
        delete [] configuration_list;
        configuration_list = NULL;
    }

    configuration_list = new Fl_Menu_Item[nost_cfg->num_configs + 1];
    memset(configuration_list, 0, sizeof(Fl_Menu_Item) * (nost_cfg->num_configs + 1));
    for(n = 0; n < nost_cfg->num_configs; n++) {
        configuration_list[n].label(nost_cfg->configs[n].name);
    } 
    configuration->menu(configuration_list);
    if(nost_cfg->current_config >= 0) {
        configuration->value(nost_cfg->current_config);
    } else {
        configuration->value(0);
    }
    configuration->redraw();
}

/**
 * Add a blank configuration with the given name.
 * @param newtxt Name for new configuration
 * @param model Model: 0=n50 1=n52
 * @param othercfg Index of configuration to clone into this one, 
 *                 or -1 if this is a new blank configuration
 **/
void add_new_configuration(const char* newtxt, model_type model, int othercfg)
{
    Fl_Menu_Item* old = configuration_list;
    char* txt = strdup(newtxt);

    ++nost_cfg->num_configs;

    /* Open up a new spot in the configuration data */
    nost_config_data* olddata = nost_cfg->configs;
    nost_cfg->configs = new nost_config_data[nost_cfg->num_configs];
    memset(nost_cfg->configs, 0, sizeof(nost_config_data) * nost_cfg->num_configs);
    memcpy(nost_cfg->configs, olddata, sizeof(nost_config_data) * (nost_cfg->num_configs - 1));
    delete [] olddata;
    initialize_new_config(nost_cfg->configs + (nost_cfg->num_configs - 1), othercfg);
    nost_cfg->configs[nost_cfg->num_configs-1].name = txt;
    nost_cfg->configs[nost_cfg->num_configs-1].model = model;
    set_current_configuration(nost_cfg->num_configs-1);

    populate_configuration_list();
}

/**
 * Starting up the app, invoked from main.  Load everything
 * we need and get ready to roll.
 **/
void startup()
{
    /* Load our previous settings */
    char fname[PATH_MAX];
    struct passwd* pw = getpwuid(getuid());
    int n;
    snprintf(fname, sizeof(fname), "%s/%s", (pw ? pw->pw_dir : "."), CFG_FILE_NAME);
    nost_cfg = load_configs(fname);

    populate_configuration_list();
    set_current_configuration(nost_cfg->current_config);
}

/**
 * App is shutting down, save the configs and kill off the main window.
 **/
void shutdown()
{
    /* Cheap and sleazy - tell any nostromo_daemons to reload */
    system("killall -HUP nostromo_daemon");
    delete main_window;
}

/**
 * Color the key buttons that switch modes to reflect that fact.
 **/
static void set_buttons()
{
    int k;

    if(key_buttons[0]->active()) {
        for(k = 0; k < MAX_KEYS; k++) {
            if(nost_cfg->current_config >= 0) {
                switch(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][k].type) {
                    default:
                        key_buttons[k]->color(FL_GRAY, FL_GRAY);
                        break;
                    case NORMAL_SHIFT:
                    case NORMAL_LOCK:
                        key_buttons[k]->color(FL_WHITE, FL_WHITE);
                        break;
                    case BLUE_SHIFT:
                    case BLUE_LOCK:
                        key_buttons[k]->color(FL_BLUE, FL_BLUE);
                        break;
                    case GREEN_SHIFT:
                    case GREEN_LOCK:
                        key_buttons[k]->color(FL_GREEN, FL_GREEN);
                        break;
                    case RED_SHIFT:
                    case RED_LOCK:
                        key_buttons[k]->color(FL_RED, FL_RED);
                        break;
                }
            } else {
                key_buttons[k]->color(FL_GRAY, FL_GRAY);
            }
            key_buttons[k]->redraw();
        }
    }
}

/**
 * Set the current mode, flipping the color on the key_mapping_box
 * to match as a visual indicator.
 **/
void set_current_mode(int mode)
{
    static Fl_Color colors[] = {
        FL_GRAY,
        FL_BLUE,
        FL_GREEN,
        FL_RED
    };

    current_mode = mode;
    key_mappings_box->color(colors[mode]);
    key_mappings_box->redraw();
    set_current_key(current_key);
    set_buttons();
}

/**
 * Set the current key, by index into global arrays.
 * Pass in -1 to clear the current selection to nothing.
 **/
void set_current_key(int key)
{
    int n;

    if(current_key >= 0) {
        key_buttons[current_key]->clear();
    }

    current_key = key;
    if(current_key >= 0) {
        key_buttons[key]->set();

        /* List the key(s) mapped in the browser */
        key_browser->clear();
        if(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][key].key_count > 0) {
            for(n = 0; n < nost_cfg->configs[nost_cfg->current_config].keys[current_mode][key].key_count; n++) {
                key_browser->add(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][key].data[n].display);
            }
        }

        /* Activate the proper controls */
        if(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][key].type == SINGLE_KEY ||
           nost_cfg->configs[nost_cfg->current_config].keys[current_mode][key].type == MULTI_KEY) {
            set_key_mapping_button->activate();
        } else {
            set_key_mapping_button->deactivate();
        }
        key_remote_check_button->value(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][key].remote);
        key_remote_check_button->activate();

        key_mapping_name_input->value(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][key].name);
        key_mapping_name_input->activate();
        key_mapping_type_choice->value((int)nost_cfg->configs[nost_cfg->current_config].keys[current_mode][key].type);
        key_mapping_type_choice->activate();

        /* Set our repeat button/delay input properly */
        if(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][key].type == MULTI_KEY) {
            key_repeat_check_button->activate();
            key_repeat_check_button->value(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][key].repeat);
        } else {
            key_repeat_check_button->deactivate();
        }

        if(key_repeat_check_button->active() && key_repeat_check_button->value()) {
            key_repeat_delay_input->activate();
            key_repeat_delay_input->value(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][key].repeat_delay);
        } else {
            key_repeat_delay_input->deactivate();
        }
    } else {
      /* No key map active, clear all relevant controls */
        key_browser->clear();
        set_key_mapping_button->deactivate();
        key_mapping_name_input->value("");
        key_mapping_name_input->deactivate();
        key_mapping_type_choice->deactivate();
        key_repeat_check_button->deactivate();
        key_repeat_delay_input->deactivate();
        key_remote_check_button->deactivate();
    }

    set_buttons();
}

/**
 * Select a different configuration to edit.
 * Activates or hides the buttons not available on the
 * current device, such as the orange thumb button
 * for the n50.
 **/
void set_current_configuration(int cfg)
{
    int n;
    int max = MAX_KEYS;

    set_current_key(-1);
    nost_cfg->current_config = cfg;

    if(cfg >= 0 && nost_cfg->configs[cfg].model == N50) {
        max = MAX_N50_KEYS;
    }

    for(n = 0; n < MAX_KEYS; n++) {
        if(cfg >= 0 && n < max) {
            key_buttons[n]->activate();
        } else {
            key_buttons[n]->deactivate();
        }
    }

    if(cfg >= 0) {
        mode_choice->activate();
    } else {
        mode_choice->deactivate();
    }

    set_buttons();
}

/**
 * Handle changes to the key repeat delay input
 **/
void change_key_repeat_delay(int value, int cfg, int key)
{
    nost_cfg->configs[cfg].keys[current_mode][key].repeat_delay = value;
}

/**
 * Handle changes to the key repeat check button.
 **/
void change_key_repeat_flag(int value, int cfg, int key)
{
    nost_cfg->configs[cfg].keys[current_mode][key].repeat = value;
}

/**
 * Handle changes to the key repeat check button.
 **/
void change_key_remote_flag(int value, int cfg, int key)
{
    nost_cfg->configs[cfg].keys[current_mode][key].remote = value;
}

/**
 * Handle changes to the key mapping name input field.
 **/
void change_key_mapping_name(const char* txt, int cfg, int key)
{
    if(nost_cfg->configs[cfg].keys[current_mode][key].name) {
        free(nost_cfg->configs[cfg].keys[current_mode][key].name);
    }
    nost_cfg->configs[cfg].keys[current_mode][key].name = strdup(txt);
}

/**
 * Invoked by changing the key mapping type dropdown.
 **/
void set_current_key_mapping_type(int type)
{
    if(nost_cfg->current_config >= 0 && current_key >= 0) {
        nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].type = (key_map_type)type;
    }
    if(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].type == SINGLE_KEY ||
       nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].type == MULTI_KEY) {
        set_key_mapping_button->activate();
    } else {
        set_key_mapping_button->deactivate();
    }
    /* Enable key repeat check button and delay input if this is a 'keys in sequence' */
    if(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].type == MULTI_KEY) {
        key_repeat_check_button->activate();
        key_repeat_check_button->value(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].repeat);
    } else {
        key_repeat_check_button->deactivate();
    }
    if(key_repeat_check_button->active() && key_repeat_check_button->value()) {
        key_repeat_delay_input->activate();
        key_repeat_delay_input->value(nost_cfg->configs[nost_cfg->current_config].keys[current_mode][current_key].repeat_delay);
    } else {
        key_repeat_delay_input->deactivate();
    }
    set_buttons();
}

/**
 * Change the name of the current configuration to the given text.
 **/
void rename_current_configuration_done(const char* txt)
{
    if(nost_cfg->current_config >= 0) {
        free(nost_cfg->configs[nost_cfg->current_config].name);
        nost_cfg->configs[nost_cfg->current_config].name = strdup(txt);
    }
    configuration_list[nost_cfg->current_config].label(nost_cfg->configs[nost_cfg->current_config].name);
    configuration->redraw();
}

/**
 * Remove the current configuration 
 **/
void delete_current_configuration_done()
{
    int m, k;
    
    if(nost_cfg->current_config >= 0) {
        /* Free up all of the name strings */
        free(nost_cfg->configs[nost_cfg->current_config].name);
        for(m = 0; m < MAX_MODES; m++) {
            for(k = 0; k < MAX_KEYS; k++) {
                if(nost_cfg->configs[nost_cfg->current_config].keys[m][k].name) {
                    free(nost_cfg->configs[nost_cfg->current_config].keys[m][k].name);
                }
            }
        }

        /* If it's not the last config, compress the array down one */
        if(nost_cfg->current_config < nost_cfg->num_configs - 1) {
            memcpy(&nost_cfg->configs[nost_cfg->current_config], 
                   &nost_cfg->configs[nost_cfg->current_config + 1],
                   sizeof(nost_config_data) * (nost_cfg->num_configs - nost_cfg->current_config - 1));
        }

        nost_cfg->num_configs--;

        set_current_configuration(nost_cfg->num_configs - 1);

        populate_configuration_list();
    }
}

#define NSHIFTKEY   "Shift+"
#define NCONTROLKEY "Ctl+"
#define NALTKEY     "Alt+"
#define MAX_KEYDSP_LEN 128

/**
 * Takes in a key event, and spits out a proper string
 * representing that key.  The key state is rendered
 * along the lines:
 * Ctl+Alt+Shift+F1
 **/
const char* create_key_display(const XKeyEvent* xkey, int delay)
{
  int len = 1;
  const char* xdsp;
  static char dsp[MAX_KEYDSP_LEN];

  if(xkey->state & (ShiftMask | LockMask)) {
    len += strlen(NSHIFTKEY);
  }
  if(xkey->state & ControlMask) {
    len += strlen(NCONTROLKEY);
  }
  if(xkey->state & Mod1Mask) {
    len += strlen(NALTKEY);
  }
  xdsp = XKeysymToString(XLookupKeysym(const_cast<XKeyEvent*>(xkey), xkey->state & ShiftMask));
  len += strlen(xdsp);
  snprintf(dsp, MAX_KEYDSP_LEN, "%s%s%s%s\t%dms",
   (xkey->state & (ShiftMask | LockMask)) ? NSHIFTKEY : "", 
   (xkey->state & ControlMask) ? NCONTROLKEY : "",
   (xkey->state & Mod1Mask) ? NALTKEY : "",
   xdsp, delay);

  return dsp;
}

/**
 * Apply button hit, save the config and signal the daemon.
 **/
void save()
{
    char fname[PATH_MAX + 1];
    struct passwd* pw = getpwuid(getuid());

    snprintf(fname, sizeof(fname), "%s/%s", (pw ? pw->pw_dir : "."), CFG_FILE_NAME);

    save_configs(fname, nost_cfg);
}

