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

#ifndef NEWMAPWINDOW_H
#define NEWMAPWINDOW_H
#include <stdlib.h>
#include <FL/Fl_Window.H>
#include <FL/Fl_Browser.H>

/**
 * Dialog type to enter a new string of keys.  I wanted a window that
 * would shove keystrokes into the right data structures, and that I 
 * could still arrange/manage through FLUID.
 **/
class NewMapWindow : public Fl_Window
{
  public:
    NewMapWindow(int width, int height, const char* txt = NULL);

  protected:
    int handle(int event);

};

#endif // NEWMAPWINDOW_H
