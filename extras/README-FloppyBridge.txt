Steem SSE HFEv3 - real floppy drive through FloppyBridge (test build)
=====================================================================

Steem can use a real floppy drive connected to a Greaseweazle (or a
DrawBridge or SuperCard Pro) as drive A: or B:, original copy-protected
disks included. It works through Rob Smith's FloppyBridge, the same driver
WinUAE and DiskFlashback use.

Setup
-----
1. Download FloppyBridge from https://amiga.robsmithdev.co.uk/winuae and put
   FloppyBridge_x64.dll (for Steem64) or FloppyBridge.dll (for Steem32) in
   the same folder as the Steem exe.
2. Only one program can use the Greaseweazle at a time: close DiskFlashback,
   or release the drive from it, first.
3. Copy Greaseweazle.hfe (included) to your disks folder. It is a small text
   file, not an image: edit it if needed (port, drive A or B).
4. In Steem's disk manager, insert Greaseweazle.hfe in A: like a disk image.
   The drive motor starts and Steem reads the real disk.

How it works
------------
Each track is read from the drive the first time the ST needs it (Steem
pauses for a moment, the ST doesn't notice), then kept until the disk is
removed. FloppyBridge measures how fast each byte goes past the head, and
Steem uses that like the density of an HFEv3 image, so timing-based
protections such as Rob Northen Copylock see the real disk's timing.

The log
-------
With log=on (the default) Steem writes Greaseweazle.log next to
Greaseweazle.hfe: one line per track read, with how long it took, the speed
range and the sectors found and checked, eg
  track  0 side 0: 100114 bits, 412 ms, 1 rev, speed 994-1006 | 9 IDs
  (trk 0 side 0, sectors 1 2 3 4 5 6 7 8 9), data OK 9, bad 0
If a disk doesn't boot, the log shows whether the drive read it correctly.

Limits of this first version
----------------------------
- Read-only: the disk is shown as write-protected, nothing is written to it.
- Swapping the real disk: eject and re-insert Greaseweazle.hfe in Steem so
  the ST sees a disk change.
- One revolution per track is kept, so protections that need weak bits to
  read differently each time may not pass yet.
- Windows only.
