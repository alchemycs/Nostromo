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
#ifndef NOST_DATA_H
#define NOST_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>

#define CFG_FILE_NAME ".nostromorc"

//! The number of available 'modes'
#define MAX_MODES 4

//! The number of keys on the N50 device
#define MAX_N50_KEYS 14

//! The number of keys on the N52 device [including 3 for the mouse wheel] (and our global max)
#define MAX_KEYS 23

//! How many keystrokes per key to map (for convenience)
#define MAX_KEYSTROKES 32

/**
 * Key mapping types, sets the behavior of the key 
 * when it's hit.
 **/
typedef enum
{
    SINGLE_KEY,     /**< Maps 1:1 nostromo->keyboard key */
    MULTI_KEY,      /**< Map multiple keys per nostromo button */
    NORMAL_SHIFT,   /**< Flip to "normal"/no lights mode while pressed */
    NORMAL_LOCK,    /**< Flip to "normal"/no lights mode */
    BLUE_SHIFT,     /**< Flip to "blue" mode while pressed */
    BLUE_LOCK,      /**< Flip to "red" mode */
    GREEN_SHIFT,    /**< Flip to "green" mode while pressed */
    GREEN_LOCK,     /**< Flip to "green" mode */
    RED_SHIFT,      /**< Flip to "red" mode while pressed */
    RED_LOCK,       /**< Flip to "red" mode */
    SHIFT_KEY,      /**< Hold Shift while key is held */
    CONTROL_KEY,    /**< Hold Control while key is held */
    ALT_KEY,        /**< Hold Alt while key is held */
} key_map_type;

/**
 * Model designations.
 **/
typedef enum {
    N50 = 0,
    N52
} model_type;

/**
 * Key modes for the daemon.
 **/
typedef enum {
    NULL_MODE = -1,     /**< NULL state */
    NORMAL_MODE = 0,    /**< No lights on */
    BLUE_MODE,          /**< Blue light on */
    GREEN_MODE,         /**< Green light on */
    RED_MODE            /**< Red light on */
} mode_type;

/**
 * Whether a key_stroke_data describes a keystroke or a mouse
 * button.
 **/
typedef enum {
    STROKE_KEY = 0, 
    STROKE_MOUSE
} key_stroke_type;

/**
 * One keystroke in a series that gets mapped to a single nostromo key.
 **/
typedef struct
{
    key_stroke_type type; /**< Keystroke or mouse hit? */
    int code;             /**< Raw scan code of the key */
    int state;            /**< State flags */
    char* display;        /**< Display string for this keystroke */
    int delay;            /**< Delay until next keystroke */
} nost_key_stroke_data;

/**
 * Info for a single nostromo key
 **/
typedef struct 
{
    char* name;         /**< Display name */
    key_map_type type;  /**< What the key does when hit */
    short key_count;    /**< Number of keystrokes mapped */
    nost_key_stroke_data data[MAX_KEYSTROKES]; /**< Keystrokes mapped */
    int repeat;         /**< Whether to repeat when key is held down */
    int repeat_delay;   /**< Amount of time to delay between keystrokes. */
    int remote;         /**< Whether to ship this to the remote node or not */

    int pressed;        /**< Used by daemon to eat extra events when n52 keys held */
} nost_key_config_data;

/**
 * One set of key mappings.
 **/
typedef struct 
{
  char* name;                                     /**< Display name */
  model_type model;                               /**< Device model */
  nost_key_config_data keys[MAX_MODES][MAX_KEYS]; /**< keystrokes for each key */
} nost_config_data;

typedef struct
{
  int network_enabled;
  int port;
  char* server;
  int num_configs;
  int current_config;
  nost_config_data* configs;
} nost_data;

void save_configs(const char* fname, const nost_data* data);
nost_data* load_configs(const char* fname);

#ifdef __cplusplus
}
#endif

#endif // NOST_DATA_H
