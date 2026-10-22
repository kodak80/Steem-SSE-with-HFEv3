/*---------------------------------------------------------------------------
PROJECT: Steem SSE
Atari ST emulator
Copyright (C) 2026 by Anthony Hayward and Russel Hayward + SSE

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

DOMAIN: Compilation
FILE: pch.cpp
DESCRIPTION: Generate Precompiled Header
---------------------------------------------------------------------------*/

/*
VS2008 / 2019
Project Configuration Properties -> C/C++ -> Precompiled Headers
Use Precompiled Header (/Yu)
pch.h
default file

This file (pch.cpp)  Configuration Properties -> C/C++ -> Precompiled Headers
Create Precompiled Header (/Yc)
pch.h
default file

If Yu is not set, no error but slower builds

Disable for C files (Not Using Precompiled Headers)
3rdparty/
6301/6301.c
pasti/div68kCycleAccurate.c

And for
3rdparty/CpuUsage/CpuUsage.cpp


VS2008: in current Windows 10 it is unreliable, you can get a mysterious fatal
error, the solution is to reboot, or to recompile pch or to disable pch, or to
use VS2019 instead
https://stackoverflow.com/questions/11854470/what-does-unexpected-precompiled-header-error-mean
https://devblogs.microsoft.com/cppblog/visual-c-precompiled-header-errors-on-windows-7/


GCC: TODO


*/

#include <pch.h>
