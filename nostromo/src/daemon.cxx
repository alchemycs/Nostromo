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

/**
 * @file daemon.cxx
 **/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/input.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <pwd.h>
#include <time.h>
#include <netdb.h>
#include <syslog.h>
#include <pthread.h>
#include <gtk/gtk.h>

#define XK_MISCELLANY
#include <X11/keysymdef.h>
#include <X11/extensions/XTest.h>
#include <X11/X.h>

#include "nost_data.h"

#define BELKIN_VENDOR_ID 0x050d     /**< Belkin's vendor ID */
#define NOSTROMO_N50_ID 0x0805      /**< n50 USB ID */
#define NOSTROMO_N52_ID 0x0815      /**< n52 USB ID */

#define PIDFILE "/tmp/nostromo_n50.pid"

nost_config_data* cfg;
nost_data* all_cfg = NULL;

Display* display;

mode_type current_mode;
mode_type last_mode;

/*** jimbo - for nostromo state ***/
#define NOSTROMO_KEY_OFFSET 304
typedef struct {
    unsigned char dpad[2];
    unsigned char wheel;
    unsigned char leds[3];      //bitfields
    int shift_state;
    int indev;
    int id;                 /**< Type of device (n50/n52) */
} nostromo_state;

#define NOSTROMO_X_AXIS 1
#define NOSTROMO_Y_AXIS 0
#define NOSTROMO_HORIZONTAL_INDEX ( nost->dpad[ NOSTROMO_Y_AXIS ] / 127) * 4
#define NOSTROMO_VERTICAL_INDEX   ( nost->dpad[ NOSTROMO_X_AXIS ] / 127)
#define NOSTROMO_WHEEL_THRESH_LOW 15
#define NOSTROMO_WHEEL_THRESH_HIGH 246
#define INDEFAULT "/dev/input/event0"
#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)    ((array[LONG(bit)] >> OFF(bit)) & 1)

int mode = 0;

void set_nostromo_leds(nostromo_state * nost);
void send_key_sequence(nostromo_state* nost, int key, int release, int from_timer = 0);

/** 
 * n52's have 2 devices, and only one of them accepts
 * commands for the LEDs. 
 **/
nostromo_state* n52_LEDs = NULL;

int shift_keycode;  /**< SHIFT keycode (shouldn't change, but I'm paranoid) */
int control_keycode;/**< CTRL keycode (shouldn't change, but I'm paranoid) */
int meta_keycode;   /**< ALT keycode (shouldn't change, but I'm paranoid) */

int sockfd = 0; /**< Socket (if connected) to send keystrokes to */
int srvfd = 0;  /**< Handle of server socket we're listening on */

int open_readers();

/**
 * Send a fake key hit, either to the local server
 * or the remote socket.
 **/
void send_key(int key, int flags, int remote)
{
    key_stroke_type type = STROKE_KEY;
    if(sockfd > 0 && remote) {
        printf("Sending key hit to remote\n");
        if(send(sockfd, &type, sizeof(type), 0) < 0 ||
           send(sockfd, &key, sizeof(key), 0) < 0 ||
           send(sockfd, &flags, sizeof(flags), 0) < 0) {
            syslog(LOG_INFO, "lost remote connection: %m");
            close(sockfd);
            sockfd = 0;
       }
    } else {
        printf("%s(%d, %08x)\n", __FUNCTION__, key, flags);
        XTestFakeKeyEvent(display, key, flags, 0);
        XFlush(display);
    }
}

/**
 * Send a mouse button hit, at whatever the current mouse pos is.
 **/
void send_mouse_click(int button, int press) {
    key_stroke_type type = STROKE_MOUSE;
    if(sockfd > 0) {
        printf("Sending mouse hit to remote\n");
        if(send(sockfd, &type, sizeof(type), 0) < 0 ||
           send(sockfd, &button, sizeof(button), 0) < 0 ||
           send(sockfd, &press, sizeof(press), 0) < 0) {
            syslog(LOG_INFO, "lost remote connection: %m");
            close(sockfd);
            sockfd = 0;
       }
    } else {
        XTestFakeButtonEvent(display, button, press, CurrentTime);
        XFlush(display);
    }
}

static pthread_mutex_t timer_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t timer_cnd = PTHREAD_COND_INITIALIZER;

/**
 * Actions that we can schedule to happen in the future.  They are scheduled
 * in response to user actions (key presses) and delayed until wanted.
 **/
typedef enum {
    TIMER_REPEAT_KEY,   /**< Repeat key sequence when timer expires */
    TIMER_PRESS_KEY,    /**< Press/release single key when timer expires */
    TIMER_MOUSE_CLICK,  /**< Press/release mouse button when timer expires */
} timer_type;

/**
 * A simple way to schedule things to happen in the future.
 **/
typedef struct timer_entry {
    struct timer_entry* next;
    struct timeval expires;
    int id;
    timer_type type;
    int arg;
    int flag;
    int delay;
    int remote;
} timer_entry;

/**
 * A list of timers that the timer_thread is waiting for.
 **/
timer_entry* timer_list = NULL;

/**
 * Make something happen sometime in the future.
 **/
void add_timer(timer_type type, int id, int arg, int flag, int delay, int remote)
{
  timer_entry *e = NULL;
  timer_entry *l = NULL;
  timer_entry *last = NULL;
  struct timeval now;

  gettimeofday(&now, NULL);
  e = (timer_entry*)calloc(1, sizeof(timer_entry));

  if(e) {
    /* Set the wakeup time */
    e->expires.tv_sec = now.tv_sec + delay / 1000;
    e->expires.tv_usec = now.tv_usec + (delay % 1000) * 100;
    e->type = type;
    e->arg = arg;
    e->flag = flag;
    e->delay = delay;
    e->id = id;
    e->remote = remote;

    /* Add to timer list */
    pthread_mutex_lock(&timer_mtx);

    /* Insert in order sorted by wakeup time */
    last = NULL;
    for(l = timer_list; l; l = l->next) {
      if(timercmp(&l->expires, &e->expires, >=)) {
        if(l == timer_list) {
          e->next = timer_list;
          timer_list = e;
        } else {
          last->next = e;
          e->next = l;
        }
        break;
      }
      last = l;
    }

    if(l == NULL) {
      if(last) {                /* Past end, back up and append */
        last->next = e;
      } else {                  /* No list head, make one */
        timer_list = e;
      }
    }

    pthread_cond_signal(&timer_cnd);
    pthread_mutex_unlock(&timer_mtx);
  }
}

/**
 * Clear a set of timers with matching ids out of the list we 
 * are monitoring.
 **/
void remove_timer(int id)
{
    timer_entry *e = NULL;
    timer_entry *tmp = NULL;
    timer_entry *last = NULL;

    pthread_mutex_lock(&timer_mtx);
    e = timer_list;
    while(e) {
        if(e->id == id) {
            if(e == timer_list) {
                timer_list = e->next;
                free(e);
                last = NULL;
                e = timer_list;
            } else {
                last->next = e->next;
                free(e);
                e = last->next;
            }
        } else {
            last = e;
            e = e->next;
        }
    }
    pthread_cond_signal(&timer_cnd);
    pthread_mutex_unlock(&timer_mtx);
}

/**
 * Waits for each timer in its queue to expire, then executes the
 * required action.
 **/
void* timer_thread(void*)
{
    struct timespec timeout;
    struct timeval now;
    timer_entry* t;

    while(1) {
        pthread_mutex_lock(&timer_mtx);
        if(timer_list) {
            timeout.tv_sec = timer_list->expires.tv_sec;
            timeout.tv_nsec = timer_list->expires.tv_usec * 1000;
        } else {
            gettimeofday(&now, NULL);
            timeout.tv_sec = now.tv_sec + 3;
            timeout.tv_nsec = now.tv_usec * 1000;
        }

        pthread_cond_timedwait(&timer_cnd, &timer_mtx, &timeout);

        t = NULL;
        if(timer_list) {
            gettimeofday(&now, NULL);
            /* See if our earliest timer has expired */
            if(timercmp(&now, &timer_list->expires, >=)) {
                t = timer_list;
                timer_list = timer_list->next;
            }
        }
        pthread_mutex_unlock(&timer_mtx);

        if(t) {
            switch(t->type) {
                case TIMER_REPEAT_KEY:
                    printf("TIMER_REPEAT_KEY: repeating %d\n", t->arg);
                    send_key_sequence((nostromo_state*)t->flag, t->arg, 0, 1);
                    break;

                case TIMER_PRESS_KEY:
                    printf("TIMER_PRESS_KEY:  %d (flag:%d)\n", t->arg, t->flag);
                    send_key(t->arg, t->flag, t->remote);
                    break;

                case TIMER_MOUSE_CLICK:
                    printf("TIMER_MOUSE_CLICK:  %d (flag:%d)\n", t->arg, t->flag);
                    send_mouse_click(t->arg, t->flag);
                    break;
            }
            free(t);
        }
    }
}

/**
 * Set a key to be pressed after some delay (or 0 for now)
 **/
void enqueue_key(int id, int key, int flags, int delay, int remote) {
    add_timer(TIMER_PRESS_KEY, id, key, flags, delay, remote);
}

/**
 * Set a key to be pressed after some delay (or 0 for now)
 **/
void enqueue_mouse_click(int id, int button, int flags, int delay, int remote) {
    add_timer(TIMER_MOUSE_CLICK, id, button, flags, delay, remote);
}

/**
 * Send the 'modifier' keycodes, e.g. shift, control, alt.
 **/
void enqueue_modifiers(int key, int state, int pressed, int delay, int remote)
{
    /* Press necessary modifiers */
    if(state & ShiftMask) {
        enqueue_key(key, shift_keycode, pressed, delay, remote);
    }
    if(state & ControlMask) {
        enqueue_key(key, control_keycode, pressed, delay, remote);
    }
    if(state & Mod1Mask) {
        enqueue_key(key, meta_keycode, pressed, delay, remote);
    }
}

/**
 * Set the input mode to the given value.  Value should be one
 * of the MODE constants.
 **/
void change_mode(nostromo_state* nost, mode_type mode) 
{
    current_mode = mode;

    nost->leds[0] =
    nost->leds[1] =
    nost->leds[2] = 0;

    switch(mode) {
        case NORMAL_MODE:
            break;
        case BLUE_MODE:
            nost->leds[2] = 1;
            break;
        case GREEN_MODE:
            nost->leds[1] = 1;
            break;
        case RED_MODE:
            nost->leds[0] = 1;
            break;
    }

    set_nostromo_leds(nost);
}

/**
 * User hit one of the keys on the Nostromo, we need to pump
 * all of the inputs that correspond to whatever the chosen
 * key was.
 **/
void send_key_sequence(nostromo_state* nost, int key, int release, int from_timer /* = 0 */)
{
    int n;
    int id = key;

    /* Only timer-created keystrokes have IDs that match the parent */
    if(!from_timer) {
        id = 0;
    }

    printf("%ld: %s(%p, %d, %d)\n", time(NULL), __FUNCTION__, nost, key, release);

    if(release && last_mode != NULL_MODE) {
        switch(cfg->keys[last_mode][key].type) {
            case NORMAL_SHIFT:
            case BLUE_SHIFT:
            case GREEN_SHIFT:
            case RED_SHIFT:
                change_mode(nost, last_mode);
                last_mode = NULL_MODE;
                return;
        }
    }

    /* Only press keys when it's not a key release being sent */
    switch(cfg->keys[current_mode][key].type) {
        case SINGLE_KEY:
            for(n = 0; n < cfg->keys[current_mode][key].key_count; n++) {
                enqueue_modifiers(id, cfg->keys[current_mode][key].data[n].state, 1, cfg->keys[current_mode][key].data[n].delay, cfg->keys[current_mode][key].remote);
                if(cfg->keys[current_mode][key].data[n].type == STROKE_KEY) {
                    enqueue_key(id, cfg->keys[current_mode][key].data[n].code, !release, cfg->keys[current_mode][key].data[n].delay, cfg->keys[current_mode][key].remote);
                } else if(cfg->keys[current_mode][key].data[n].type == STROKE_MOUSE) {
                    enqueue_mouse_click(id, cfg->keys[current_mode][key].data[n].code, !release, cfg->keys[current_mode][key].data[n].delay, cfg->keys[current_mode][key].remote);
                }
                enqueue_modifiers(id, cfg->keys[current_mode][key].data[n].state, 0, cfg->keys[current_mode][key].data[n].delay, cfg->keys[current_mode][key].remote);
            }
            break;
        case MULTI_KEY:
            /* Nostromo key was pressed, send all the corresponding mapped keys */
            if(!release) {
                int delay = 0;
                for(n = 0; n < cfg->keys[current_mode][key].key_count; n++) {
                    /* Note: The small 'fudge' on each delay forces them into the right order in the timing queue. */
                    enqueue_modifiers(id, cfg->keys[current_mode][key].data[n].state, 1, delay + cfg->keys[current_mode][key].data[n].delay, cfg->keys[current_mode][key].remote);
                    if(cfg->keys[current_mode][key].data[n].type == STROKE_KEY) {
                        enqueue_key(id, cfg->keys[current_mode][key].data[n].code, 1, delay + cfg->keys[current_mode][key].data[n].delay + 1, cfg->keys[current_mode][key].remote);
                        enqueue_key(id, cfg->keys[current_mode][key].data[n].code, 0, delay + cfg->keys[current_mode][key].data[n].delay + 2, cfg->keys[current_mode][key].remote);
                    } else if(cfg->keys[current_mode][key].data[n].type == STROKE_MOUSE) {
                        enqueue_mouse_click(id, cfg->keys[current_mode][key].data[n].code, 1, cfg->keys[current_mode][key].data[n].delay + 3, cfg->keys[current_mode][key].remote);
                        enqueue_mouse_click(id, cfg->keys[current_mode][key].data[n].code, 0, cfg->keys[current_mode][key].data[n].delay + 4, cfg->keys[current_mode][key].remote);
                    }
                    enqueue_modifiers(id, cfg->keys[current_mode][key].data[n].state, 0, delay + cfg->keys[current_mode][key].data[n].delay + 5, cfg->keys[current_mode][key].remote);
                    delay += cfg->keys[current_mode][key].data[n].delay + 10;
                }

                if(cfg->keys[current_mode][key].repeat) {
                    add_timer(TIMER_REPEAT_KEY, key, key, (int)(long long)nost, cfg->keys[current_mode][key].repeat_delay + delay, cfg->keys[current_mode][key].remote);
                }
            } else {
                if(cfg->keys[current_mode][key].repeat) {
                    remove_timer(key);
                }
            }
        break;

        case NORMAL_SHIFT:
        case BLUE_SHIFT:
        case GREEN_SHIFT:
        case RED_SHIFT:
            if(!release) {
                last_mode = current_mode;
                switch(cfg->keys[current_mode][key].type) {
                    case NORMAL_SHIFT:
                        change_mode(nost, NORMAL_MODE);
                        break;
                    case BLUE_SHIFT:
                        change_mode(nost, BLUE_MODE);
                        break;
                    case GREEN_SHIFT:
                        change_mode(nost, GREEN_MODE);
                        break;
                    case RED_SHIFT:
                        change_mode(nost, RED_MODE);
                        break;
                }
            }
            break;

        case NORMAL_LOCK:
            if(!release) {
                change_mode(nost, NORMAL_MODE);
            }
            break;
        case BLUE_LOCK:
            if(!release) {
                change_mode(nost, BLUE_MODE);
            }
            break;
        case GREEN_LOCK:
            if(!release) {
                change_mode(nost, GREEN_MODE);
            }
            break;
        case RED_LOCK:
            if(!release) {
                change_mode(nost, RED_MODE);
            }
            break;
        case SHIFT_KEY:
            printf("shift_key: %d\n", (!release ? 1 : 0));
            enqueue_key(shift_keycode, shift_keycode, !release, 0, cfg->keys[current_mode][key].remote);
            break;
        case CONTROL_KEY:
            printf("control_key: %d\n", (!release ? 1 : 0));
            enqueue_key(control_keycode, control_keycode, !release, 0, cfg->keys[current_mode][key].remote);
            break;
        case ALT_KEY:
            enqueue_key(meta_keycode, meta_keycode, !release, 0, cfg->keys[current_mode][key].remote);
            break;
    }

}

/* reset the state struct */
void reset_nostromo_state(nostromo_state * nost)
{
    nost->dpad[0] = 128;
    nost->dpad[1] = 128;
    nost->wheel = 0;
    nost->leds[0] = 0;
    nost->leds[1] = 0;
    nost->leds[2] = 0;
    nost->shift_state = 0;
}

/* set the leds to reflect given state */
void set_nostromo_leds(nostromo_state* nost)
{
    struct input_event ev;
    int i, j;
    int dev = nost->indev;

    /* n52's have 2 devices, one of them has the LEDs.. */
    if(nost->id == NOSTROMO_N52_ID && n52_LEDs) {
        dev = n52_LEDs->indev;
    }

    for(j = 0; j < 3; j++) {
        ev.type = EV_LED;
        ev.code = j;
        ev.value = nost->leds[j];
        if(write(dev, &ev, sizeof(struct input_event)) < 0) {
            perror(__FUNCTION__);
        }
    }
}

/**
 * cut out from the main loop of usb2key.c by Matan Ziv-Av
 *
 *  - reads event from usb bus
 *  - sets state in nostromo_state struct
 *  - calls appropriate handler function
 * jimbo
 **/
void handle_nostromo_block(nostromo_state * nost, input_event* ev)
{
    int brk = 0;

    switch (ev->type) {
        case EV_ABS:           /* absolute event (dpad/wheel) */
        {
            switch (ev->code) {
                case 6:
                       /*** wheel event ***/
                    nost->wheel = ev->value;
                    break;
                case 0:
                case 1:
                { 
                    int key = 0;
                    brk = (ev->value == 128) ? 1 : 0;

                    /**
                     * DPAD input events
                     * code   value  meaning
                     * ------------------------------------
                     * 0      0      top of dpad pressed
                     * 0      255    bottom of dpad pressed
                     * 0      128    top/bottom released
                     * 1      0      front of dpad pressed
                     * 1      255    rear of dpad pressed
                     * 1      128    front/rear released
                     **/
                    if(ev->code == NOSTROMO_Y_AXIS) {
                        if(ev->value == 0 || nost->dpad[ev->code] == 0) {
                            key = 10;
                        } else if(ev->value == 255 || nost->dpad[ev->code] == 255) {
                            key = 11;
                        }
                    } else if(ev->code == NOSTROMO_X_AXIS) {
                        if(ev->value == 0 || nost->dpad[ev->code] == 0) {
                            key = 12;
                        } else if(ev->value == 255 || nost->dpad[ev->code] == 255) {
                            key = 13;
                        }
                    }

                    send_key_sequence(nost, key, brk);

                    nost->dpad[ev->code] = ev->value;
                    break;
                }
            }
            break;
        }
        case EV_KEY:
        {
            /* key event(10 finger keys) */
            int key = 0;

            if(ev->code > NOSTROMO_KEY_OFFSET) {
                key = ev->code - NOSTROMO_KEY_OFFSET;
            } else {
                /* n52 runs all buttons/keys through here */
                switch(ev->code) {
                    default: break;
                    case 15: key = 0; break; /* First row */
                    case 16: key = 1; break;
                    case 17: key = 2; break;
                    case 18: key = 3; break;
                    case 19: key = 4; break;

                    case 58: key = 5; break; /* Second row */
                    case 30: key = 6; break;
                    case 31: key = 7; break;
                    case 32: key = 8; break;
                    case 33: key = 9; break;

                    case 105: key = 10; break; /* Dpad top */
                    case 106: key = 11; break; /* Dpad bottom */
                    case 103: key = 12; break; /* Dpad rear/right */
                    case 108: key = 13; break; /* Dpad front/left */

                    case 42: key = 14; break; /* Third row */
                    case 44: key = 15; break;
                    case 45: key = 16; break;
                    case 46: key = 17; break;
                    case 57: key = 18; break; /* Black thumb button */

                    case 56: key = 19; break; /* Orange thumb button */
                    
                    case 274: key = 21; break; /* Push mouse wheel in */

                }
            }
            brk = (ev->value == 0) ? 1 : 0;

            if((cfg->keys[current_mode][key].pressed == 0) != (ev->value == 0)) {
                printf("Sending key sequence for %d brk:%d\n", key, brk);
                send_key_sequence(nost, key, brk);
            }
            cfg->keys[current_mode][key].pressed = ev->value;
            break;
        }
        /* Weirdness note: When the mouse wheel moves, we get an event
         * telling us it moved, which is immediately followed by a EV_SYN
         * event with all 0's for data - not terribly informative.. */
        case EV_REL: /* Mouse wheel movement */
        {
            int key = 0;

            switch(ev->value) {
                case 1: /* Mouse wheel up (forward) */
                    send_key_sequence(nost, 20, 0);
                    send_key_sequence(nost, 20, 1);
                    break;
                case -1: /* Mouse wheel down (back) */
                    send_key_sequence(nost, 22, 0);
                    send_key_sequence(nost, 22, 1);
                    break;
                default:
                    break;
            }
            break;
        }
    }
}

/**
 * Close up open sockets.
 **/
void close_sockets()
{
    close(sockfd);
    close(srvfd);
    sockfd = srvfd = 0;
}

/**
 * Set up sockets, etc.
 **/
void open_sockets()
{
    int pid = getpid();
    struct sockaddr_in sin;

    if(!sockfd && all_cfg->network_enabled) {
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = INADDR_ANY;
        sin.sin_port = htons(all_cfg->port);

        srvfd = socket(AF_INET, SOCK_STREAM, 0);
        if(srvfd < 0 
         || bind(srvfd, (struct sockaddr*)&sin, sizeof(sin)) < 0
         || listen(srvfd, 3) < 0
         || fcntl(srvfd, F_SETOWN, pid) < 0
         || fcntl(srvfd, F_SETFL, FASYNC) < 0) {
           syslog(LOG_NOTICE, "Failed to open socket: %m");
           srvfd = 0;
        }
    }
}

/**
 * Load our configuration information.
 **/
void load()
{
    /* Load our keymap settings */
    char fname[PATH_MAX+1];
    struct passwd* pw = getpwuid(getuid());
    nost_data* oldcfg = all_cfg;

    sprintf(fname, "%s/" CFG_FILE_NAME, (pw ? pw->pw_dir : "."));
    printf("Loading configs from %s\n", fname);
    all_cfg = load_configs(fname);

    if(all_cfg->num_configs <= 0) {
        syslog(LOG_INFO, "No configs to use, exiting.\n");
        exit(0);
    }

    /* Set our global config to the selected one */
    cfg = &all_cfg->configs[all_cfg->current_config];

    /* Handle any changes in networking */
    if(oldcfg) {
        if((oldcfg->network_enabled && !all_cfg->network_enabled) 
        ||(oldcfg->port != all_cfg->port)
        ||(strcmp(oldcfg->server, all_cfg->server))) {
            close_sockets();
        }
        if(all_cfg->network_enabled && !sockfd) {
            open_sockets();
        }
    }
}

char* const* my_argv; 
char* const*my_envp;

/**
 * signal handler - turn off all nostromo leds at exit
 **/
void signal_handler(int signo)
{
    syslog(LOG_INFO, "Received signal %d", signo);

    switch (signo) {
        case SIGINT:
        case SIGKILL:
        case SIGSEGV:
        case SIGTERM:
        /*** exit cleanly ***/
            close_sockets();
            exit(0);
            break;
        case SIGHUP:
            load();
            break;
        case SIGUSR2:
            open_readers();
            break;
        case SIGIO: {
            struct sockaddr_in sin;
            socklen_t len;
            /* Rest of system will skip the socket if it's < 0 */
            sockfd = accept(srvfd, (struct sockaddr*)&sin, &len);
            if(sockfd) {
                int addr;
                socklen_t size;
                size = sizeof(sin);
                struct hostent* from;

                if(getpeername(sockfd, (struct sockaddr*)&sin, &size) == 0 &&
                   (from = gethostbyaddr((char*)&sin.sin_addr, sizeof(sin.sin_addr), AF_INET)) != NULL) {
                    syslog(LOG_INFO, "accepted connection from %s", from->h_name);
                } else {
                    addr = ntohl(sin.sin_addr.s_addr);
                    syslog(LOG_INFO, "accepted connection from %d.%d.%d.%d",
                        (addr >> 24) & 0xFF, (addr >> 16) & 0xFF,
                        (addr >>  8) & 0xFF, (addr      ) & 0xFF);

                }
                   
            }
            break;
        }
        case SIGPIPE:
            /* they're gone, clean up so we go local again */
            syslog(LOG_INFO, "lost connection to fd:%d", sockfd);
            close(sockfd);
            sockfd = 0;
            break;
        default:
            syslog(LOG_INFO, "nostromo_daemon received signal %d\n", signo);
            break;
    }
}

/* Isn't in a header somewhere? */
struct input_devinfo {
        uint16_t bustype;
        uint16_t vendor;
        uint16_t product;
        uint16_t version;
};

void* reader_thread(void* n)
{
    struct input_event ev;
    int k;
    nostromo_state* nost = (nostromo_state*)n;
    int led_states[][3] = {
        { 0, 0, 0 },
        { 1, 0, 0 },
        { 0, 1, 0 },
        { 0, 0, 1 },
        { 0, 1, 0 },
        { 1, 0, 0 },
        { 0, 0, 0 },
        { -1, -1, -1 },
    };
    int l;

    syslog(LOG_NOTICE, "Starting reader_thread with fd=%d\n", nost->indev);

    reset_nostromo_state(nost);

    /* Do the startup light show */
    for(l = 0; led_states[l][0] != -1; l++) {
        nost->leds[0] = led_states[l][0];
        nost->leds[1] = led_states[l][1];
        nost->leds[2] = led_states[l][2];
        set_nostromo_leds(nost);
        usleep(200000);
    }

    while(1) {
        if(read(nost->indev, &ev, sizeof(struct input_event)) == sizeof(struct input_event)) {
            handle_nostromo_block(nost, &ev);
        } else {
            perror(__FUNCTION__);
            syslog(LOG_WARNING, "reader_thread failed fd=%d %s(%d)\n", nost->indev, strerror(errno), errno);
            break;
        }
    }

    return NULL;
}

/**
 * Kick a reader thread to life.
 **/
pthread_t spawn_reader(int dev, int id)
{
    pthread_t reader;
    nostromo_state* nost = new nostromo_state;

    memset(nost, 0, sizeof(nostromo_state));

    nost->indev = dev;
    nost->id = id;

    /* Look for a device to control LEDs on */
    if(id == NOSTROMO_N52_ID) {
        uint8_t led_bitmask[LED_MAX/8 + 1] = { 0 };
        if(ioctl(dev, EVIOCGBIT(EV_LED, sizeof(led_bitmask)), led_bitmask) >= 0) {
            if(led_bitmask[0]) {
                n52_LEDs = nost;
            } 
        }
    }

    pthread_create(&reader, NULL, reader_thread, nost);

    return reader;
}

/**
 * @todo Can we really rely on this path?
 **/
int open_readers()
{
    int i;
    int fd;
    char s[64];
    struct input_devinfo info;
    int found = 0;

    for(i = 0; i < 16; i++) {
        snprintf(s, sizeof(s), "/dev/input/event%d", i);
        fd = open(s, O_RDWR);
        if(fd >= 0) {
            if(ioctl(fd, EVIOCGID, &info) == 0) {
                if(info.vendor == BELKIN_VENDOR_ID &&
                   (info.product == NOSTROMO_N52_ID || info.product == NOSTROMO_N50_ID)) {
                    syslog(LOG_INFO, "Found %04x:%04x at dev %d", info.vendor, info.product, i);
#if defined(EVIOCGRAB)
                    /* Available in 2.6 or patched 2.4 */
                    if(ioctl(fd, EVIOCGRAB, 1) < 0) {
                        syslog(LOG_INFO, "Failed to grab: %s", strerror(errno));
                        perror(__FUNCTION__);
                        continue;
                    }
#else
#warning No EVIOCGRAB interface, n52 support will be severely hampered.
#warning In order to use an n52, you will have to manually unload any
#warning USB keyboard/mouse driver modules - which will of course prevent
#warning the use of USB keyboard/mouse and an n52 at the same time.
#endif
                    found = spawn_reader(fd, info.product);
                } 
            } else {
                perror(s);
            }
        } 
    }

    if(!found) {
        syslog(LOG_INFO, "Nostromo device not found.", info.vendor, info.product, i);
    }

    return found;
}

/**
 * There can be only one.
 * @todo Should be smarter about which process we signal, would
 * hate to have it be the wrong one...
 **/
void ensure_singleton()
{
    int pid;
    int pidfd;

    /* Create our pid file */
    pidfd = open(PIDFILE, O_CREAT | O_EXCL | O_RDWR, 0600);
    if(pidfd < 0) {
        if(errno == EEXIST) {
            /* there's already a file, see if it's a valid process */
            pidfd = open(PIDFILE, O_RDWR);
            if(pidfd < 0) {
                syslog(LOG_ERR, "Unable to open %s - do you own it?", PIDFILE);
                exit(-1);
            }
            if(read(pidfd, &pid, sizeof(pid)) == sizeof(pid) && !kill(pid, SIGHUP)) {
                syslog(LOG_INFO, "Signaling process %d to reload.", pid);
                exit(0);
            } else {
                syslog(LOG_INFO, "Removing stale pidfile.");
                pidfd = open(PIDFILE, O_RDWR | O_CREAT | O_TRUNC, 0600);
            }
        } else {
            syslog(LOG_INFO, "Unable to create file %s: %m", PIDFILE);
            exit(-1);
        }
    }

    /* Record our pid */
    pid = getpid();
    if(write(pidfd, &pid, sizeof(pid)) != sizeof(pid)) {
      syslog(LOG_ERR, "Failed to write pid");
    }
    close(pidfd);
}

extern void daemon_create();
void* docklet_thread(void*)
{
    daemon_create();
    gtk_main();
    
    return NULL;
}

/**
 * Main loop for silent console driver
 **/
int main(int argc, char *argv[], char* envp[])
{
    pthread_t docklet;
    pthread_t reader;
    pthread_t timer;

    my_argv = argv;
    my_envp = envp;

    daemon(0, 0);

    openlog(argv[0], LOG_PID, LOG_USER);

    ensure_singleton();

    gtk_init (&argc, &argv);
    pthread_create(&docklet, NULL, docklet_thread, NULL);

    /* handle forceful exits */
    signal(SIGHUP, signal_handler);
    signal(SIGUSR2, signal_handler);

    /* For network IO handling */
    signal(SIGIO, signal_handler);
    signal(SIGPIPE, signal_handler);

    /* Set up X necessaries */
    display = XOpenDisplay(NULL);

    if(!display) {
       syslog(LOG_NOTICE, "Couldn't connect to X");
       exit(-1);
    }

    shift_keycode = XKeysymToKeycode(display, XK_Shift_L);
    control_keycode = XKeysymToKeycode(display, XK_Control_L);
    meta_keycode = XKeysymToKeycode(display, XK_Meta_L);

    load();

    open_readers();
    open_sockets();

    timer_thread(NULL);
}

