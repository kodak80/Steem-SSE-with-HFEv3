FloppyBridge client library
===========================

These files are the client side of Rob Smith's FloppyBridge, taken from
https://github.com/RobSmithDev/FloppyDriveBridge (folder floppybridge,
commit 710fa15). They are released into the public domain (Unlicense, see
the header of each file).

One local change: a (char*) cast in FloppyBridgeAPI::setComPort(), without
which the non-Unicode (MultiByte) build that Steem uses doesn't compile.

They don't contain any drive code: floppybridge_lib.cpp loads the FloppyBridge
driver library at run time (FloppyBridge.dll / FloppyBridge_x64.dll on
Windows), which talks to the Greaseweazle, DrawBridge or SuperCard Pro.
That DLL is not part of Steem: download it from
https://amiga.robsmithdev.co.uk/winuae and put it next to Steem.exe.

Steem uses it through steem/floppybridge_drive.cpp (SSE_FLOPPYBRIDGE).
