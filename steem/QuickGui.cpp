/*---------------------------------------------------------------------------
PROJECT: Steem SSE
Atari ST emulator
Copyright (C) 2020 by Anthony Hayward and Russel Hayward + SSE

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see https://www.gnu.org/licenses/.

DOMAIN: All
FILE: QuickGui.cpp
CONDITION: Quick project/script
DESCRIPTION: Amalgamation for much faster compilation
(back to the old Steem way)
---------------------------------------------------------------------------*/

#include "pch.h"
#pragma hdrstop

// Include
#ifdef WIN32
#include "choosefolder.cpp"
#endif
#include "configstorefile.cpp"
#include "di_get_contents.cpp"
#include "dirsearch.cpp"
#include "dynamicarray.cpp"
#include "easycompress.cpp"
#include "easystr.cpp"
#include "easystringlist.cpp"
#include "mymisc.cpp"
#ifdef WIN32
#include "scrollingcontrolswin.cpp"
#endif
#include "wordwrapper.cpp"

// Steem
#include "acc.cpp"
#include "archive.cpp"
#include "associate.cpp"
#include "dataloadsave.cpp"
#include "debug.cpp"
#include "dir_id.cpp"
#ifdef WIN32
#include "directory_tree.cpp"
#endif
#include "diskman.cpp"
#include "diskman_diags.cpp"
#ifdef WIN32
#include "diskman_drag.cpp"
#endif
#include "gui.cpp"
#include "gui_controls.cpp"
#include "harddiskman.cpp"
#include "infobox.cpp"
#ifdef WIN32
#include "input_prompt.cpp"
#endif
#include "key_table.cpp"
#include "loadsave.cpp"
#include "loadsave_emu.cpp"
#include "macros.cpp"
#include "main.cpp"
#include "notifyinit.cpp"
#include "options.cpp"
#include "options_create.cpp"
#include "patchesbox.cpp"
#include "screen_saver.cpp"
#include "shortcutbox.cpp"
#include "Steem.cpp"
#include "steemintro.cpp"
#include "stemdialogs.cpp"
#include "stemwin.cpp"
#include "translate.cpp"

#ifdef UNIX
#include "notwin_mymisc.cpp"
#include <x/hxc.cpp>
#include <x/hxc_alert.cpp>
#include <x/hxc_fileselect.cpp>
#include <x/hxc_popup.cpp>
#include <x/hxc_popuphints.cpp>
#include <x/hxc_dir_lv.cpp>
#include <x/hxc_prompt.cpp>
#include <x/icongroup.cpp>
#include "x/x_mymisc.cpp"
#include "x/x_portio.cpp"
 
#endif

