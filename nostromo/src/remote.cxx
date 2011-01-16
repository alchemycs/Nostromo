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
 * @file remote.cxx
 * This is the remote half, for situations when you want to run
 * the Nostromo physically connected to one machine, but feeding
 * keystrokes to a different one.
 * (I need this personally as WineX chokes hard on my dual-CPU
 * machine.  Since I play more Linux games than Win32-emulated 
 * games, the Nostromo stays hooked up to the dual CPU machine
 * and it ships keystrokes to the single-CPU box running WineX.)
 **/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <pwd.h>
#include <syslog.h>

#include <signal.h>
#include <netdb.h>
#include <sys/socket.h>

#define XK_MISCELLANY
#include <X11/keysymdef.h>
#include <X11/extensions/XTest.h>
#include <X11/X.h>

#include "nost_data.h"

#define PIDFILE "/tmp/nostromo_n50_remote.pid"

Display* display;
nost_data* all_cfg = NULL;

/**
 * There can be only one.
 **/
int ensure_singleton()
{
    int pid;
    int pidfd;

    /* Create our pid file */
    pidfd = open(PIDFILE, O_CREAT | O_EXCL | O_RDWR, 0600);
    if(pidfd < 0) {
        if(errno == EEXIST) {
            /* there's already a file, see if it's a valid process */
            pidfd = open(PIDFILE, O_RDWR, 0600);
            if(pidfd < 0) {
                fprintf(stderr, "Unable to open file in /tmp - do you own it?\n");
                perror(PIDFILE);
                exit(-1);
            }
            if(read(pidfd, &pid, sizeof(pid)) < sizeof(pid)) {
                fprintf(stderr, "Unable to read pid.\n");
                perror(PIDFILE);
                exit(-1);
            }
            if(!kill(pid, 0)) {
                fprintf(stderr, "Already running as process %d\n", pid);
                exit(0);
            } else {
                fprintf(stderr, "Removing stale pidfile.\n");
                pidfd = open(PIDFILE, O_RDWR | O_CREAT | O_TRUNC, 0600);
            }
        } else {
            fprintf(stderr, "Unable to create file in /tmp\n");
            perror(PIDFILE);
            exit(-1);
        }
    }
    return pidfd;
}

/**
 * Load our configuration information.
 **/
void load()
{
    /* Load our keymap settings */
    char fname[PATH_MAX+1];
    struct passwd* pw = getpwuid(getuid());
    int count, current, n;
    nost_data* oldcfg = all_cfg;

    sprintf(fname, "%s/.nostromorc", (pw ? pw->pw_dir : "."));
    printf("Loading configs from %s\n", fname);
    all_cfg = load_configs(fname);
}

/**
 * Main loop for silent console driver
 **/
int main(int argc, char *argv[])
{
    int pid, pidfd = 0;
    int sockd;
    int key, flags;
    key_stroke_type type;
    struct sockaddr_in s;
    struct hostent* h;
    char* hostname;
    short port;

    load();

    hostname = all_cfg->server;
    port = all_cfg->port;

    if(!all_cfg->network_enabled || !all_cfg->server || !all_cfg->port) {
        fprintf(stderr, "Networking not enabled or not configured.  Please be sure\n" 
                        "that settings are correct between client and server and resetart.\n"
                        "Look in the Options/Preferences dialog in the Nostromo\n"
                        "n50 configuration GUI for settings.\n"
                        "Exiting.\n");
        exit(-1);
    }
    pidfd = ensure_singleton();

    daemon(0, 0);

    h = gethostbyname(hostname);
    s.sin_family = AF_INET;
    s.sin_addr.s_addr = *((unsigned long*)h->h_addr);
    s.sin_port = htons(port);
    sockd = socket(PF_INET, SOCK_STREAM, 0);
    if(connect(sockd, (struct sockaddr*)&s, sizeof(s)) < 0) {
        perror(hostname);
        exit(errno);
    }

    /* Set up X necessaries */
    display = XOpenDisplay(NULL);

    pid = getpid();
    if(write(pidfd, &pid, sizeof(pid)) != sizeof(pid)) {
      syslog(LOG_ERR, "Failed to write pid");
    }
    close(pidfd);

    /* Do our thing */
    while(1) {
        /* Read a key or mouse data from the socket */
        if(recv(sockd, &type, sizeof(type), 0) < 0 || 
           recv(sockd, &key, sizeof(key), 0) < 0 || 
          recv(sockd, &flags, sizeof(flags), 0) < 0) {
            break;
        }

        printf("%d/%d/%d\n", type,key, flags);
        
        /* Shove it to X */
        if(type == STROKE_KEY) {
            XTestFakeKeyEvent(display, key, flags, 0);
        } else if(type == STROKE_MOUSE) {
            XTestFakeButtonEvent(display, key, flags, CurrentTime);
        }
        XFlush(display);
    }
}

