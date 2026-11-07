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
   Nothing happens until the ST reads the disk (press Play): the real
   motor turns on and off with the emulated one.

How it works
------------
Each track is read from the drive the first time the ST needs it (Steem
pauses for a moment, the ST doesn't notice), then kept until the disk is
removed. Loading takes about as long as on a real ST. With cache=on in
Greaseweazle.hfe, FloppyBridge reads the whole disk ahead in the background
so loading is faster, but the drive then also runs while Steem is stopped. FloppyBridge measures how fast each byte goes past the head, and
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

Writing to the disk (write=on)
------------------------------
By default the real disk is read-only: it shows as write-protected and
nothing is written to it. With write=on in Greaseweazle.hfe, Steem writes
what the ST writes (saved games, high scores, a formatted blank disk...).
TRY IT ON A COPY OR A BLANK DISK FIRST - never on an original you care about.

- The Greaseweazle can't start writing at a given sector, so a track the ST
  changed is rewritten whole, from the index hole: the track as Steem read
  it, with the ST's changes. The join falls in the track's largest gap.
- It's written when the head moves to another track, when the ST's drive
  motor stops (a couple of seconds after the last access), when Steem stops
  and when the disk is ejected. Each written track is read back and every
  sector checked; if one doesn't read back the same, the track is written
  again, then reported in the log (WRITE FAILED).
- The track is rewritten at the normal density. If some of its sectors
  were at another density (sectors rewritten earlier by a drive turning at
  a slightly different speed, or a density copy protection such as
  Copylock) they're flattened, and the log says so. A real ST only rewrites
  the sector it changes, so this is where Steem differs: a copy-protected
  original written to this way may not load any more. Use copies.
- Not written (the log says why; the ST still sees its changes until the
  disk is ejected):
  - tracks that couldn't be read, unless the ST formats them (formatting a
    blank disk works);
  - a disk with its write-protect tab set: it also shows as write-protected
    in Steem (re-insert Greaseweazle.hfe after moving the tab).
- Steem pauses while a track is written and checked (about a second).

Swapping disks
--------------
Just swap the real disk in the drive, like on a real ST (games asking for
disk 2...), while the drive is quiet. Steem notices it three ways:
- the drive's disk-change line (pin 34 of a PC drive), if it has one;
- the write-protect sensor, which changes as a disk goes in or out (that's
  how the ST itself sees a disk change);
- after the drive has been idle, the first read checks the disk: a track
  Steem already has is read again from the disk and compared.
Then everything read from the old disk is dropped and the ST gets a disk
change. Tracks the ST wrote to are saved when its drive motor stops, so wait
for the drive to go quiet before taking the disk out. The log shows "disk
check: same disk" / "ANOTHER DISK" and the write-protect sensor changes.

Weak bits
---------
When a sector has a bad CRC, a few more revolutions are read. If the sector
reads correctly on one of them, that copy is used (a weak read). If it's
bad every time and its data changes from one revolution to the next, the
bytes that change are weak bits: they read differently each time the ST
reads them, as on the real disk (protections such as the FTL "fuzzy bits"
of Dungeon Master check this). A sector that's bad but always the same (a
deliberate CRC error, as in Copylock) is left as it is. The log shows
"repaired from another revolution" and "WEAK BITS" lines.
Only the bit cells that change are made weak, so a byte keeps the values
it can really take (Dungeon Master's fuzzy bytes read $68 or $E8, never
anything else). The drive may show only a few bytes of a fuzzy area
changing, so the same cells of the identical bytes around them (the rest of
the run) are made weak too.

Limits
------
- Windows only.
