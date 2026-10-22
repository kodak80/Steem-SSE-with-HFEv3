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

DOMAIN: Emu
FILE: fdc.cpp
DESCRIPTION: Emulation of the Western Digital WD1772 floppy disk controller.
There are two internal emulations here, one based on agendas (ST/MSA/DIM/STX,
scanline precision) and one based on events (TWD1772: STW/HFE/SCP, cycle
precision).
I/O happens through the global io_write() and io_read() functions and the
TWD1772 object, which "dispatches" to the correct emulation.
Steem also interfaces with two more WD1772/disk emulations through this 
object: Pasti (STX) and CAPS/SPS (IPF/CTR).
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#include <osd.h>
#include <diskman.h>
#include <iolist.h>


#define FLOPPY_IRQ_YES 9
#define FLOPPY_IRQ_ONESEC 10
#define FLOPPY_IRQ_NOW 11
#define DRIVE_HBLS_PER_ROTATION  (ADAT?FloppyDrive[DRIVE].HblsPerRotation():(313*50/5))
#define DRIVE_HBLS_OF_INDEX_PULSE (DRIVE_HBLS_PER_ROTATION/((ADAT)?50:20)) // 4ms =200ms/50

#if USE_PASTI
HINSTANCE hPasti=NULL;
COUNTER_VAR pasti_update_time;
const struct pastiFUNCS *pasti=NULL;
char pasti_file_exts[PASTI_FILE_EXTS_BUFFERSIZE];
WORD pasti_store_byte_access;
#endif//PASTI
bool pasti_active=false;

WORD crc16_table[256]; // overhead only 512 bytes
DWORD hbl_at_ip=0;
int floppy_mediach[2]={0,0};
const BYTE fdc_step_time_to_hbls[4]={94,188,32,47};
WORD floppy_write_track_bytes_done;
BYTE floppy_access_ff_counter=0, floppy_irq_flag=0,fdc_spinning_up=0;
bool floppy_access_started_ff=false;

#if defined(SSE_DISK_STX)
BYTE StartShift,ShiftIn; //record a random shift when reading the track up to the 1st sync byte
#endif

#define LOGSECTION LOGSECTION_FDC


BYTE floppy_current_drive() { // see Psg.CurrentDrive()
  BYTE drive;
  if((psg_reg[PSGR_PORT_A]&BIT_1)==0)  // Drive A
    drive=DRIVE_A;
  else if((psg_reg[PSGR_PORT_A]&BIT_2)==0) // Drive B
    drive=DRIVE_B;
  else
    drive=DRIVE_A; // Neither, guess A 
#if defined(SSE_DRIVE_FREEBOOT)
  if(DiskMan.nFloppyDrives>1 && FloppyDrive[DRIVE_B].bFreeboot)
    drive=!drive;
#endif
  return drive;
}


BYTE floppy_current_side() {
  return ((psg_reg[PSGR_PORT_A]&BIT_0)==0);
}


// reading bytes from floppy
void write_to_dma(BYTE Val,int Num=1) {
  int n=(Num<=0) ? 1 : Num;
  for(int i=0;i<n;i++)
    Dma.AddToFifo(MNGR_STEEM,Val);
}


bool fdc_handle_file_error(BYTE floppyno,bool bWrite,SHORT sector,
                           int PosInSector,bool bFromFormat) {
  static DWORD last_reinsert_time[2]={0,0};
  TSF314 &drive=FloppyDrive[floppyno]; // shorthand
  TFloppyDisk &disk=FloppyDisk[floppyno]; // shorthand
  TRACE_LOG("%s %s\n",((bWrite)?"Write":"Read"),"ERROR");
  bool bWorkingNow=false;
  if(timer>=last_reinsert_time[floppyno]+2000 && drive.DiskInDrive())
  {
    // Over 2 seconds since last failure
    FILE *fpDest=NULL;
    if(bFromFormat)
    {
      if(disk.ReopenFormatFile())
        fpDest=disk.Format_fp;
      else if(drive.ReinsertDisk())
        fpDest=disk.fp;
    }
    if(fpDest)
    {
      if(disk.SeekSector(floppy_current_side(),drive.track,sector,bFromFormat))
      {
        FSEEK(fpDest,PosInSector,SEEK_CUR);
        BYTE temp;
        if(bWrite)
        {
          temp=Dma.GetFifoByte(MNGR_STEEM);
          if(!DiskMan.bDiskProtectImage)
#if defined(SSE_DISK_STX)
          if(!(disk.STX_File&&DiskMan.bDiskProtectImageStx))
#endif
            bWorkingNow=(FWRITE(&temp,1,1,fpDest)>0);
        }
        else
        {
          bWorkingNow=(FREAD(&temp,1,1,fpDest)>0);
          if(DMA_ADDRESS_IS_VALID_W && Dma.Counter) 
            write_to_dma(temp,0);
        }
      }
    }
#if !defined(SSE_LIBRETRONUKE)
    else
      DiskMan.EjectDisk(floppyno);
#endif
  }
  last_reinsert_time[floppyno]=timer;
  return (!bWorkingNow);
}


bool floppy_track_index_pulse_active() {
  // This function is used by Steem native and by WD1772 manager
  TSF314 &drive=FloppyDrive[DRIVE];
  bool bActive=false;
  if(Fdc.StatusType==0)
  {}
  else if(ADAT)
    bActive=(drive.bMotor && drive.BytePosition()<=125); //4ms = +-125 bytes
  else if(DRIVE<DiskMan.nFloppyDrives)
  {
    DWORD hbl_rev=DRIVE_HBLS_PER_ROTATION;
    bActive=((hbl_count%hbl_rev)>=(DWORD)(hbl_rev-DRIVE_HBLS_OF_INDEX_PULSE));
  }
  return bActive;
}


#if defined(SSE_MEGASTE)
bool fdc_check_wrong_density() {
  // we do that check at IRQ, works with TOS 205 and 206
  bool bWrongDensity=(!FloppyDrive[DRIVE].Empty() &&
    ((FloppyDisk[DRIVE].Density==2)^((MegaSte.FdHd&BIT_1)==BIT_1)));
  if(bWrongDensity)
  {
    //ASSERT(IS_MEGASTE);
    TRACE_LOG("Mismatch %cD $FF860E %X\n",(FloppyDisk[DRIVE].Density==1) ? 'D' : 'H',MegaSte.FdHd);
  }
  return bWrongDensity;
}
#endif


void fdc_type1_check_verify() {
  BYTE floppyno=floppy_current_drive();
  BYTE current_side=floppy_current_side();
  TSF314 &drive=FloppyDrive[floppyno];
  BYTE &track=drive.track;
  TFloppyDisk &disk=FloppyDisk[floppyno];
  // Slow drive config (default) -> set up agenda and return
  if(ADAT)
  {
    if(Fdc.cr&Fdc.CR_V) // Verify flag
    {
      int HBLsToNextID=0;
      BYTE NextIDNum=0xFF;
#if defined(SSE_DISK_STX)
      if(disk.STX_File)
      {
        if(ImageSTX[floppyno].LoadTrack(!!current_side,track))
        {
          WORD bytes=drive.BytePosition();
          //TRACE_LOG("STX from %d ",bytes);
          NextIDNum=ImageSTX[floppyno].FindID(bytes,-1,track,-1); // must find one ID with correct track
          HBLsToNextID=drive.BytesToHbls(bytes);
          //TRACE_LOG("NextIDNum %d HBLsToNextID %d\n",NextIDNum,HBLsToNextID);
        }
      }
      else
#endif
        disk.NextID(NextIDNum,HBLsToNextID); // C++ references
      HBLsToNextID+=milliseconds_to_hbl(15); // head settling
/*  When you boot a ST with no disk into the drive, it will hang for
    a long time then show the desktop with two drive icons, the drive
    never stops IIRC. */
      if(drive.Empty())
      {
        TRACE_LOG("No disk %c verify times out\n",'A'+DRIVE);
      }
      else
      {
        Fdc.IndexCounter=0; // 5 REVS to find match
        agenda_add(agenda_fdc_verify,HBLsToNextID,NextIDNum);
      }
    }
    else
    {
      // no verify: delay 'finish' to avoid nasty bugs (European Demos)
      agenda_add(agenda_fdc_verify,2,1); 
    }
    return;
  }
  // Turbo drive -> do it here
  if((Fdc.cr&Fdc.CR_V)==0)
    return; // nothing to do
  // This reads an ID field and checks that track number matches floppy drive head
  // It will fail on an unformatted track or if there is no disk of course
  if(track>FLOPPY_MAX_TRACK_NUM||drive.Empty())
    Fdc.str|=FDC_STR_SE;
  else if(!disk.TrackIsFormatted[current_side][track])
  {
    // If track is formatted then it is okay to seek to it, otherwise do this:
    if(track>=disk.TracksPerSide || current_side>=disk.Sides)
      Fdc.str|=FDC_STR_SE;
  }
}


void fdc_command(BYTE cm) {
  TSF314 &drive=FloppyDrive[DRIVE];
  if(Fdc.str&FDC_STR_BSY)
  {
/*  WD1770/1772 5 1/4 " Floppy Disk Controller/Formatter:
    "This register should not be loaded when the device is busy unless
    the new command is a force interrupt." 
    Later versions of the doc state:
    "This register is not loaded when the device is busy unless the new
    command is a force interrupt."
    The first version is more accurate. It is always possible to change the
    registers but you shouldn't.
    While the disk is still spinning up, it's possible to load a new command (from Hatari).
    Command is decoded after spinup, after 15ms, or almost at once.   
    Ignoring the new command is a simplification (a hack, hence the option).
    Froggies: ignore $17 on $80 (motor on)
    Overdrive: replace $00 (motor off) with $13
*/
    if(fdc_spinning_up || !OPTION_HACKS || Fdc.CommandType()==1 && Fdc.CommandType(cm)==1) // Awesome
    {
      TRACE_LOG("CR %X->%X\n",Fdc.cr,cm);
    }
    else if((cm&0xF0)!=0xD0) // Not force interrupt
    {
      TRACE_LOG("Command %X ignored\n",cm);
      //Fdc.cr=cm; // should but we can't afford in our emu
      return;
    }
  }
  if(Fdc.InterruptCondition!=8) // see note in IORead()
  {
    fdc_irq=false; // Turn off IRQ output
    update_disk_irq();
  }
  Fdc.InterruptCondition=0;
  floppy_irq_flag=0;
  Fdc.cr=cm;
  agenda_delete(agenda_fdc_finished);
  agenda_delete(agenda_fdc_motor_flag_off);
  
  // AFAIK the FDC turns the motor on automatically whenever it receives a command.
  // Normally the FDC delays execution until the motor has reached top speed but
  // there is a bit in type 1, 2 and 3 commands that will make them execute
  // while the motor is in the middle of spinning up (BIT_3).
  bool delay_exec=false;
  // no motor if no drive (drive 1 when 0 is max)
  if(DRIVE<DiskMan.nFloppyDrives && !(Fdc.str&FDC_STR_MO)) 
  {
    if((cm&0xF0)!=0xd0&&!(cm&Fdc.CR_H))  // Not force interrupt, delay not disabled
      delay_exec=true; // Delay command until after spinup
    Fdc.str=FDC_STR_BSY|FDC_STR_MO;
    fdc_spinning_up=(delay_exec) ? 2 : 1;
    drive.Motor(true);
    if(drive.Empty())
    {}  // no IP
    else if(DiskMan.bTurboDrive)
    {
      //TRACE_LOG("add agenda_fdc_spun_up 01\n");
      agenda_add(agenda_fdc_spun_up,milliseconds_to_hbl(100),delay_exec);
    }
    else
    { //  Set up agenda for next IP
      Fdc.IndexCounter=0;
      WORD delay=drive.HblsNextIndex();
      //TRACE_LOG("add agenda_fdc_spun_up 02\n");
      agenda_add(agenda_fdc_spun_up,delay,delay_exec);
    }
  }
  if(!delay_exec)
  {
#if defined(SSE_DRIVE_SOUND)
    drive.SoundCheckCommand(Fdc.cr);
#endif
    fdc_execute();
  }
}


void agenda_fdc_spun_up(int do_exec) {
/*  On the WD1772 all commands, except the Force Interrupt Command,
 are programmed via the h Flag to delay for spindle motor start 
 up time. If the h Flag is not set and the MO signal is low when 
 a command is received, the WD1772 forces MO to a logic 1 and 
 waits 6 revolutions before executing the command. 
 At 300 RPM, this guarantees a one second spindle start up time.
 ->
 We count IP, only if there's a spinning selected drive
 Not emulated: if program changes drive and the new drive is spinning
 too, but IP occurs at a different time!
*/
  TSF314 &drive=FloppyDrive[DRIVE];
  agenda_delete(agenda_fdc_spun_up);
  hbl_at_ip=hbl_count;
  if(ADAT)
  {
    //ASSERT(Fdc.IndexCounter<6);
    if(Psg.CurrentDrive()!=TYM2149::NO_VALID_DRIVE && drive.bMotor) //TODO wd's line
      Fdc.IndexCounter++;
    if(Fdc.IndexCounter<6) // not finished
    {
      //TRACE_LOG("add agenda_fdc_spun_up 03 %d\n",Fdc.IndexCounter);
      agenda_add(agenda_fdc_spun_up,DRIVE_HBLS_PER_ROTATION,do_exec);
      return;
    }
  }
#if defined(SSE_DRIVE_SOUND)
  drive.SoundCheckCommand(Fdc.cr);
#endif
  fdc_spinning_up=0;
  TRACE_LOG("FDC Drive spun\n");
  if(do_exec) 
    fdc_execute();
}


void fdc_execute() {
  // We need to do something here to make the command take more time
  // if the disk spinning up (fdc_spinning_up).
  int floppyno=floppy_current_drive();
  int current_side=floppy_current_side();
  TSF314 &drive=FloppyDrive[floppyno];
  TFloppyDisk &disk=FloppyDisk[floppyno];
  BYTE &track=drive.track;
  TRACE_LOG2("fdc_execute byte position %d\n",drive.BytePosition());
  floppy_irq_flag=FLOPPY_IRQ_YES;
  int hbls_to_interrupt=64;
  agenda_delete(agenda_fdc_seek);
  agenda_delete(agenda_fdc_readwrite_sector);
  agenda_delete(agenda_fdc_read_address);
  agenda_delete(agenda_fdc_read_track);
  agenda_delete(agenda_fdc_write_track);
  agenda_delete(agenda_fdc_verify);
  agenda_delete(agenda_fdc_spun_up);
/*
The 177x accepts 11 commands.  Western Digital divides these commands
into four categories, labeled I,II, III, and IV.

COMMAND SUMMARY
     +------+----------+-------------------------+
     !	    !	       !	   BITS 	 !
     ! TYPE ! COMMAND  !  7  6	5  4  3  2  1  0 !
     +------+----------+-------------------------+
     !	 1  ! Restore  !  0  0	0  0  h  v r1 r0 !
     !	 1  ! Seek     !  0  0	0  1  h  v r1 r0 !
     !	 1  ! Step     !  0  0	1  u  h  v r1 r0 !
     !	 1  ! Step-in  !  0  1	0  u  h  v r1 r0 !
     !	 1  ! Step-out !  0  1	1  u  h  v r1 r0 !
     !	 2  ! Rd sectr !  1  0	0  m  h  E  0  0 !
     !	 2  ! Wt sectr !  1  0	1  m  h  E  P a0 !
     !	 3  ! Rd addr  !  1  1	0  0  h  E  0  0 !
     !	 3  ! Rd track !  1  1	1  0  h  E  0  0 !
     !	 3  ! Wt track !  1  1	1  1  h  E  P  0 !
     !	 4  ! Forc int !  1  1	0  1 i3 i2 i1 i0 !
     +------+----------+-------------------------+
*/

  if((Fdc.cr & BIT_7)==0)  // Type 1 commands
  {
/*
Type I commands are Restore, Seek, Step, Step In, and Step Out.

The following table is a bit map of the values to store in the Command
Register.
Command      Bit 7     B6     B5     B4     B3     B2     B1     Bit 0
--------     -----     --     --     --     --     --     --     -----
Restore      0         0      0      0      h      V      r1     r0
Seek         0         0      0      1      h      V      r1     r0
Step         0         0      1      u      h      V      r1     r0
Step in      0         1      0      u      h      V      r1     r0
Step out     0         1      1      u      h      V      r1     r0

Flags:

u (Update Track Register) - If this flag is set, the 177x will update
the track register after executing the command.  If this flag is
cleared, the 177x will not update the track register.

h (Motor On) - If the value of this bit is 1, the controller will
disable the motor spin-up sequence.  Otherwise, if the motor is off
when the chip receives a command, the chip will turn the motor on and
wait 6 revolutions before executing the command.  At 300 RPM, the
6-revolution wait guarantees a one-second start time. If the 177x is
idle for 9 consecutive disk revolutions, it turns off the drive motor.
If the 177x receives a command while the motor is on, the controller
executes the command immediately.

V (Verify) - If this flag is set, the head settles after command
execution.  The settling time is 15 000 cycles for the 1772 and 30 000
cycles for the 1770.  The FDDC will then verify the track position of
the head.  The 177x reads the first ID field it finds and compares the
track number in that ID field against the Track Register.  If the
track numbers match but the ID field CRC is invalid, the 177x sets the
CRC Error bit in the status register and reads the next ID field.  If
the 177x does not find a sector with valid track number AND valid CRC
within 5 disk rotations, the chip sets the Seek Error bit in the
status register.

r (Step Time) - This bit pair determines the time between track steps
according to the following table:

r1       r0            1770                                        1772
--       --            ----                                        ----
0        0             6000 CPU clock cycles                       6000 cycles
0        1             12000 cycles                                12000 cycles
1        0             20 000 cycles                               2000 cycles
1        1             30 000 cycles                               3000 cycles

  SS 
  ST: 1772 r1 r0 11 -> 3000 cycles
  commands starting with 1 = seek  h  V r1 r0

13                                 0  0  1  1  seek
17                                 0  1  1  1  seek with Verify

53  = step in with update track register
*/
    Fdc.str|=FDC_STR_SU;
    fdc_spinning_up=0;
    hbls_to_interrupt=fdc_step_time_to_hbls[Fdc.cr&(Fdc.CR_I1|Fdc.CR_I0)];
    switch(Fdc.cr&0xF0) {
    case 0x00: //restore to track 0
/*
Restore:
If the FDDC receives this command when the drive head is at track
zero, the chip sets its Track Register to $00 and ends the command.
[SS: according to the flow chart, this is tested only after 6 IPs]
If the head is not at track zero, the FDDC steps the head carriage
until the head arrives at track 0.  The 177x then sets its Track
Register to $00 and ends the command.  If the chip's track-zero input
does not activate after 255 step pulses AND the V bit is set in the
command word, the 177x sets the Seek Error bit in the status register
and ends the command.
*/
      if((Fdc.cr&Fdc.CR_V) && drive.Empty()) //no disk
        Fdc.str=FDC_STR_SE|FDC_STR_MO|FDC_STR_BSY;
      else
      {
        Fdc.tr=255,Fdc.dr=0; // like in CAPSimg
        floppy_irq_flag=0;
        if(track==0 && floppyno<DiskMan.nFloppyDrives)
          Fdc.tr=0;
        agenda_add(agenda_fdc_seek,1,0); //1 scanline
        Fdc.str=FDC_STR_MO|FDC_STR_BSY;
        Fdc.StatusType=1;
      }
      break;
    case 0x10: //seek to track number in data register
/*
Seek:
The CPU must load the desired track number into the Data Register
before issuing this command.  The Seek command causes the 177x to step
the head to the desired track number and update the Track Register.
*/
      agenda_add(agenda_fdc_seek,2,0);
      Fdc.str=FDC_STR_MO|FDC_STR_BSY;
      floppy_irq_flag=0;
      Fdc.StatusType=1;
      break;
    default: {//step, step in, step out
/*
Step:
The 177x issues one step pulse to the mechanism, then delays one step
time according to the r flag.

Step in:
The 177x issues one step pulse in the direction toward Track 76 and
waits one step time according to the r flag.  [Transcriber's Note:
Western Digital assumes in this paragraph that disks do not have more
than 77 tracks.]

Step out:
The 177x issues one step pulse in the direction toward Track 0 and
waits one step time according to the r flag.

The chip steps the drive head in the same direction it last stepped
unless the command changes the direction.  Each step pulse takes 4
cycles.  The 177x begins outputting a direction signal to the drive 24
cycles before the first stepping pulse.
*/
#if defined(SSE_DRIVE_SOUND)
      if(OPTION_DRIVE_SOUND)
        drive.SoundStep();
#endif
      Fdc.str=FDC_STR_MO|FDC_STR_BSY;
      char d=1; //step direction, default is inwards
      if(drive.Empty()||Psg.CurrentDrive()==TYM2149::NO_VALID_DRIVE) //Japtro
      {
        if(Fdc.cr&TWD1772::CR_V)
          Fdc.str|=FDC_STR_SE;
      }
      else
      {
        switch(Fdc.cr&TWD1772::CR_STEPPING) {
        case TWD1772::CR_STEP: 
          if(!Fdc.Lines.direction)
            d=-1; 
          break;
        case TWD1772::CR_STEP_OUT: 
          d=-1; 
          break;
        }
        Fdc.Lines.direction=(d==1);
        if(Fdc.cr & TWD1772::CR_U) //U flag, update track register
          Fdc.tr+=d;
        if(d==-1 && track==0)   //trying to step out from track 0
          Fdc.tr=0; //here we set the track register
        else 
        { //can step
          track+=d;
          if(ADAT)
            floppy_irq_flag=0; // IRQ set in fdc_type1_check_verify()
          fdc_type1_check_verify();
        }
        Fdc.StatusType=1;
        //          TRACE_LOG("Step %d (TR %d CYL %d)\n",d,Fdc.tr,FloppyDrive[floppyno].track);
      }
    }//case
    }//sw
  }
  else // bit7 on: types 2-4
  {
    Fdc.StatusType=0;
    Fdc.str&=~FDC_STR_WP;
    switch(Fdc.cr&(BIT_7|BIT_6|BIT_5|BIT_4)) {
      // Type 2
    case 0x80:case 0xa0: // Read/write single sector
    case 0x90:case 0xb0: // Read/write multiple sectors
/*
Type II commands are Read Sector and Write Sector.

Command          Bit 7     B6     B5     B4     B3     B2     B1     Bit 0
------------     -----     --     --     --     --     --     --     -----
Read Sector      1         0      0      m      h      E      0      0
Write Sector     1         0      1      m      h      E      P      a0

Flags:

m (Multiple Sectors) - If this bit = 0, the 177x reads or writes
("accesses") only one sector.  If this bit = 1, the 177x sequentially
accesses sectors up to and including the last sector on the track.  A
multiple-sector command will end prematurely when the CPU loads a
Force Interrupt command into the Command Register.

h (Motor On) - If the value of this bit is 1, the controller will
disable the motor spin-up sequence.  Otherwise, if the motor is off
when the chip receives a command, the chip will turn the motor on and
wait 6 revolutions before executing the command.  At 300 RPM, the
6-revolution wait guarantees a one- second start time.  If the 177x is
idle for 9 consecutive disk revolutions, it turns off the drive motor.
If the 177x receives a command while the motor is on, the controller
executes the command immediately.

E (Settling Delay) - If this flag is set, the head settles before
command execution.  The settling time is 15 000 cycles for the 1772
and 30 000 cycles for the 1770.

P (Write Precompensation) - On the 1770-02 and 1772-00, a 0 value in
this bit enables automatic write precompensation.  The FDDC delays or
advances the write bit stream by one-eighth of a cycle according to
the following table.

Previous          Current bit           Next bit
bits sent         sending               to be sent       Precompensation
---------         -----------           ----------       ---------------
x       1         1                     0                Early
x       0         1                     1                Late
0       0         0                     1                Early
1       0         0                     0                Late

Programmers typically enable precompensation on the innermost tracks,
where bit shifts usually occur and bit density is maximal.  A 1 value
for this flag disables write precompensation.

a0 (Data Address Mark) - If this bit is 0, the 177x will write a
normal data mark.  If this bit is 1, the 177x will write a deleted
data mark.

Read Sector:
The controller waits for a sector ID field that has the correct track
number, sector number, and CRC.  The controller then checks for the
Data Address Mark, which consists of 43 copies of the second byte of
the CRC.  If the controller does not find a sector with correct ID
field and address mark within 5 disk revolutions, the command ends.
Once the 177x finds the desired sector, it loads the bytes of that
sector into the data register.  If there is a CRC error at the end of
the data field, the 177x sets the CRC Error bit in the Status Register
and ends the command regardless of the state of the "m" flag.

Write Sector:
The 177x waits for a sector ID field with the correct track number,
sector number, and CRC.  The 177x then counts off 22 bytes from the
CRC field.  If the CPU has not loaded a byte into the Data Register
before the end of this 22-byte delay, the 177x ends the command.
Assuming that the CPU has heeded the 177x's data request, the
controller writes 12 bytes of zeroes.  The 177x then writes a normal
or deleted Data Address Mark according to the a0 flag of the command.
Next, the 177x writes the byte which the CPU placed in the Data
Register, and continues to request and write data bytes until the end
of the sector.  After the 177x writes the last byte, it calculates and
writes the 16-bit CRC.  The chip then writes one $ff byte.  The 177x
interrupts the CPU 24 cycles after it writes the second byte of the
CRC.
*/
      if(drive.Empty() || track>FLOPPY_MAX_TRACK_NUM || (floppyno>=DiskMan.nFloppyDrives))
      { //no drive etc.
        TRACE_LOG("Drive empty or track %d overshoot\n",track);
        if(drive.Empty()||(floppyno>=DiskMan.nFloppyDrives))
          return; // Chaos Engine
        Fdc.str=FDC_STR_MO|FDC_STR_BSY;
        floppy_irq_flag=FLOPPY_IRQ_ONESEC;  //end command after 1 second
        break;
      }
      // compute crc for debug info
      Fdc.CrcLogic.Reset();
      Fdc.CrcLogic.Add(0xFB); //dam  TODO or...
#ifdef SSE421R1 // change test order to give native STX + turbo a chance
#if defined(SSE_DISK_STX)
      if(disk.STX_File)
      {
        WORD start=drive.BytePosition(), bytes=start;
        Fdc.str=FDC_STR_MO|FDC_STR_BSY;
        if(ImageSTX[floppyno].GetSector(!!current_side,track,Fdc.sr,bytes))
        {
          int nhbls=drive.BytesToHbls(bytes); // to first byte of data
          agenda_delete(agenda_fdc_readwrite_sector);
          agenda_add(agenda_fdc_readwrite_sector,nhbls,MAKELONG(0,start));
          floppy_irq_flag=0;
        }
        else
        {
          Fdc.str|=FDC_STR_RNF;
          floppy_irq_flag=FLOPPY_IRQ_ONESEC;
        }
      }
#endif
      else if(DiskMan.bTurboDrive) 
      {
        int param=MAKELONG(0,Fdc.cr);
        agenda_add(agenda_fdc_readwrite_sector,hbls_to_interrupt,param);
        Fdc.str=FDC_STR_MO|FDC_STR_BSY;
        floppy_irq_flag=0;
      }
#else//#ifdef SSE421R1
      if(DiskMan.bTurboDrive) 
      {
        int param=MAKELONG(0,Fdc.cr);
        agenda_add(agenda_fdc_readwrite_sector,hbls_to_interrupt,param);
        Fdc.str=FDC_STR_MO|FDC_STR_BSY;
        floppy_irq_flag=0;
      }
#if defined(SSE_DISK_STX)
      else if(disk.STX_File)
      {
        WORD start=drive.BytePosition(), bytes=start;
        Fdc.str=FDC_STR_MO|FDC_STR_BSY;
        if(ImageSTX[floppyno].GetSector(!!current_side,track,Fdc.sr,bytes))
        {
          int nhbls=drive.BytesToHbls(bytes); // to first byte of data
          agenda_delete(agenda_fdc_readwrite_sector);
          agenda_add(agenda_fdc_readwrite_sector,nhbls,MAKELONG(0,start));
          floppy_irq_flag=0;
        }
        else
        {
          Fdc.str|=FDC_STR_RNF;
          floppy_irq_flag=FLOPPY_IRQ_ONESEC;
        }
      }
#endif
#endif//#ifdef SSE421R1
      else
      {
        TWD1772IDField IDList[30];
        int SectorIdx=-1;
        //this should work with STW too as this has been modded:
        int nSects=disk.GetIDFields(current_side,track,IDList);
        for(int n=0;n<nSects;n++) 
        {
          if(IDList[n].track==Fdc.tr && IDList[n].num==Fdc.sr) 
          {
            SectorIdx=n;
            break;
          }
        }
        if(SectorIdx>-1&&nSects>0)
        {
          BYTE num=Fdc.sr;
          WORD CurrentByte=drive.BytePosition();
          int BytesToNextID=disk.BytesToID(num);
          // 12+3: to ID; 16: will be counted in agenda function
          int pre_data_gap=disk.PreDataGap()-12-3-16;
          WORD start=(CurrentByte+BytesToNextID+pre_data_gap)%disk.TrackBytes;
          DWORD HBLOfSectorStart=hbl_count+drive.BytesToHbls(BytesToNextID+pre_data_gap);
          HBLOfSectorStart-=2+1; //see below, Steem's way, + hbl_count is ++ right after comparison
          agenda_delete(agenda_fdc_readwrite_sector);
          int nhbls=(HBLOfSectorStart-hbl_count)+2;
          agenda_add(agenda_fdc_readwrite_sector,nhbls,MAKELONG(0,start));
          floppy_irq_flag=0;
          Fdc.str=FDC_STR_MO|FDC_STR_BSY;
        }
        else 
        {
          TRACE_LOG("SectorIdx %d nSects %d\n",SectorIdx,nSects);
          floppy_irq_flag=FLOPPY_IRQ_ONESEC;
          Fdc.str=FDC_STR_MO|FDC_STR_BSY;
        }
      }
      break;
      // Type 3
/*
Type III commands are Read Address, Read Track, and Write Track.

Command          Bit 7     B6     B5     B4     B3     B2     B1     Bit 0
------------     -----     --     --     --     --     --     --     -----
Read Address     1         1      0      0      h      E      0      0      C0
Read Track       1         1      1      0      h      E      0      0      E0
Write Track      1         1      1      1      h      E      P      0      F0

Flags:

h (Motor On) - If the value of this bit is 1, the controller will
disable the motor spin-up sequence.  Otherwise, if the motor is off
when the chip receives a command, the chip will turn the motor on and
wait 6 revolutions before executing the command.  At 300 RPM, the
6-revolution wait guarantees a one- second start time.  If the 177x is
idle for 9 consecutive disk revolutions, it turns off the drive motor.
If the 177x receives a command while the motor is on, the controller
executes the command immediately.

E (Settling Delay) - If this flag is set, the head settles before
command execution.  The settling time is 15 000 cycles for the 1772
and 30 000 cycles for the 1770.

P (Write Precompensation) - On the 1770-02 and 1772-00, a 0 value in
this bit enables automatic write precompensation.  The FDDC delays or
advances the write bit stream by one-eighth of a cycle according to
the following table.

Previous          Current bit           Next bit
bits sent         sending               to be sent       Precompensation
---------         -----------           ----------       ---------------
x       1         1                     0                Early
x       0         1                     1                Late
0       0         0                     1                Early
1       0         0                     0                Late

Programmers typically enable precompensation on the innermost tracks,
where bit shifts usually occur and bit density is maximal.  A 1 value
for this flag disables write precompensation.
*/
    case 0xc0: //read address
/*
Read Address:
The 177x reads the next ID field it finds, then sends the CPU the
following six bytes via the Data Register:
Byte #     Meaning                |     Sector length code     Sector length
------     ------------------     |     ------------------     -------------
1          Track                  |     0                      128
2          Side                   |     1                      256
3          Sector                 |     2                      512
4          Sector length code     |     3                      1024
5          CRC byte 1             |
6          CRC byte 2             |

[Transcriber's Note:  | is the vertical bar character.]

The 177x copies the track address into the Sector Register.  The chip
sets the CRC Error bit in the status register if the CRC is invalid.
*/
      if(drive.Empty())
        floppy_irq_flag=0;  //never cause interrupt, timeout
      else
      {
        TWD1772IDField IDList[255];
#if defined(SSE_DISK_STX)
        if(FloppyDisk[floppyno].STX_File)
          ImageSTX[floppyno].LoadTrack(!!current_side,track);
#endif
        DWORD nSects=disk.GetIDFields(current_side,track,IDList);
        if(nSects && !(drive.bSingleSided&&CURRENT_SIDE==1))
        {
          int HBLsToNextSector=0;
          BYTE NextIDNum; // it's index 0 - #sectors-1
          disk.NextID(NextIDNum,HBLsToNextSector); //references
          agenda_delete(agenda_fdc_read_address);
          agenda_add(agenda_fdc_read_address,HBLsToNextSector,NextIDNum);
          floppy_irq_flag=0;
          Fdc.str=FDC_STR_MO|FDC_STR_BSY;
        }
        else 
        {
          Fdc.str=FDC_STR_MO|FDC_STR_SE|FDC_STR_BSY;  //sector not found
          floppy_irq_flag=FLOPPY_IRQ_ONESEC; //Antago
        }
      }
      break;
    case 0xe0:  //read track
/*
Read Track:
This command dumps a raw track, including gaps, ID fields, and data,
into the Data Register.  The FDDC starts reading with the leading edge
of the first index pulse it finds, and stops reading with the next
index pulse.  During this command, the FDDC does not check CRCs.  The
address mark detector is on during the entire command.  (The address
mark detector detects ID, data and index address marks during read and
write operations.)  Because the address mark detector is always on,
write splices or noise may cause the chip to look for an address mark.
[Transcriber's Note: I do not know how the programmer can tell that
the AM detector has found an address mark.]  The chip may read gap
bytes incorrectly during write-splice time because of synchronization.
*/
      Fdc.str=FDC_STR_MO|FDC_STR_BSY;
      floppy_irq_flag=0;
      if(drive.DiskInDrive()) 
      {
#if defined(SSE_DISK_STX)
        if(FloppyDisk[floppyno].STX_File)
        {
          bool reload=(Fdc.old_cr==Fdc.cr && ImageSTX[floppyno].CurrentSide==current_side
            && ImageSTX[floppyno].CurrentTrack==track);
          ImageSTX[floppyno].LoadTrack(!!current_side,track);
          StartShift=(reload&&SSEOptions.RandomizeTrack)?rand()%5:0;
          // If no FirstSync, try to guess it (dangerous)
          if(!ImageSTX[floppyno].FirstSync && ImageSTX[floppyno].pSectorDesc)
            if(ImageSTX[floppyno].pSectorDesc[0].bitPosition/8>15)
              ImageSTX[floppyno].FirstSync=ImageSTX[floppyno].pSectorDesc[0].bitPosition/8-15;
          TRACE_LOG("track shift %d to %d\n",StartShift,ImageSTX[floppyno].FirstSync);
        }
#endif
        agenda_delete(agenda_fdc_read_track);
        DWORD DiskPosition=hbl_count%DRIVE_HBLS_PER_ROTATION;
        int nhbls=DRIVE_HBLS_PER_ROTATION-DiskPosition;
        agenda_add(agenda_fdc_read_track,nhbls,0);
      }
      break;
    case 0xf0:  //write (format) track
/*
Write Track:
This command is the means of formatting disks.  The drive head must be
over the correct track BEFORE the CPU issues the Write Track command.
Writing starts with the leading edge of the first index pulse which
the 177x finds.  The 177x stops writing when it encounters the next
index pulse.  The 177x sets the Data Request bit immediately after
receiving the Write Track command, but does not start writing until
the CPU loads the Data Register.  If the CPU does not send the 177x a
byte within three byte times after the first index pulse, the 177x
ends the command.  The 177x will write all data values from $00 to $f4
(inclusive) and from $f8 to $ff (inclusive) unaltered.  Data values
$f5, $f6, and $f7, however, have special meanings.  The value $f5
means to write an $a1 to the disk.  The $a1 value which the 177x
writes to the disk will lack an MFM clock transition between bits 4
and 5.  This missing clock transition indicates that the next normally
written byte will be an address mark.  In addition, a Data Register
value of $f5 will reset the 177x's CRC generator.  A Data Register
value of $f6 will not reset the CRC generator but will write a pre-
address-mark value of $c2 to the disk.  The written $c2 will lack an
MFM clock transition between bits 3 and 4.  A Data Register value of
$f7 will write a two-byte CRC to the disk.
*/
      floppy_irq_flag=0;
      Fdc.str=FDC_STR_MO|FDC_STR_BSY;
      if(drive.DiskInDrive() && !disk.WriteProtect && track<=FLOPPY_MAX_TRACK_NUM)
      {
        if(disk.Format_fp==NULL) 
          disk.OpenFormatFile();
        if(disk.Format_fp) 
        {
          agenda_delete(agenda_fdc_write_track);
          DWORD DiskPosition=hbl_count % DRIVE_HBLS_PER_ROTATION;
          int nhbls=DRIVE_HBLS_PER_ROTATION-DiskPosition;
          agenda_add(agenda_fdc_write_track,nhbls,0);
          floppy_write_track_bytes_done=0;
#if defined(SSE_DISK_STX)
          if(FloppyDisk[floppyno].STX_File)
            ImageSTX[floppyno].LoadTrack(!!current_side,track);  // ? TODO
#endif
        }
      }
      break;
    case 0xd0:        //force interrupt
/*
The Type IV command is Force Interrupt.

Force Interrupt:
Programmers use this command to stop a multiple-sector read or write
command or to ensure Type I status in the Status Register.  The format
of this command is %1101(I3)(I2)00.  If flag I2 is set, the 177x will
acknowledge the command at the next index pulse.  If flag I3 is set,
the 177x will immediately stop what it is doing and generate an
interrupt.  If neither I2 nor I3 are set, the 177x will not interrupt
the CPU, but will immediately stop any command in progress.  After the
CPU issues an immediate interrupt command ($d8), it MUST write $d0
(Force Interrupt, I2 clear, I3 clear) to the Command Register in order
to shut off the 177x's interrupt output.  After any Force Interrupt
command, the CPU must wait 16 cycles before issuing any other command.
If the CPU does not wait, the 177x will ignore the previous Force
Interrupt command.  Because the 177x is microcoded, it will
acknowledge Force Interrupt commands only between micro- instructions.
*/
/*  On the common $90-$D0 sequence, the command is still active when it is 
    interrupted, and STR isn't 'type I'. 
*/
      if(!(Fdc.str&FDC_STR_BSY))
        Fdc.StatusType=2; // PYM/BPOC doesn't like 'track0' 
      fdc_spinning_up=0; // v4, was the real problem with Froggies
      agenda_delete(agenda_fdc_finished);
      if(Fdc.str&FDC_STR_BSY)
        Fdc.str&=~FDC_STR_BSY;
      else // read str is type I if FDC wasn't busy when interrupted (doc)
      {
        Fdc.StatusType=1;
        Fdc.str&=~(FDC_STR_CRC|FDC_STR_LD|FDC_STR_RT|FDC_STR_RNF);
      }
      if(Fdc.cr&b1100) 
      {
/*  "The lower four bits of the command determine the conditional 
interrupt as follows:
- i0,i1 = Not used with the WD1772
- i2 = Every Index Pulse
- i3 = Immediate Interrupt
The conditional interrupt is enabled when the corresponding bit 
positions of the command (i3-i0) are set to a 1. When the 
condition for interrupt is met the INTRQ line goes high signifying
 that the condition specified has occurred. If i3-i0 are all set
 to zero (Hex D0), no interrupt occurs but any command presently
 under execution is immediately terminated. When using the immediate
 interrupt condition (i3 = 1) an interrupt is immediately generated
 and the current command terminated. Reading the status or writing
 to the Command Register does not automatically clear the interrupt.
 The Hex D0 is the only command that enables the immediate interrupt
 (Hex D8) to clear on a subsequent load Command Register or Read 
 Status Register operation. Follow a Hex D8 with D0 command."

  -> D4 (IRQ every index pulse) wasn't implemented yet in Steem. 
*/          
        if(Fdc.cr&b1000) // D8
        {
          Fdc.InterruptCondition=8;
          agenda_fdc_finished(0); // Interrupt CPU immediately
        }
        else // D4
        {
          Fdc.InterruptCondition=4; // IRQ at every index pulse
          floppy_irq_flag=0;
          WORD hbls_to_next_ip=drive.HblsNextIndex();
          agenda_add(agenda_fdc_finished,hbls_to_next_ip,0);
        }
      }
      else
      { //D0
        floppy_irq_flag=0;
        Fdc.InterruptCondition=0;
        agenda_add(agenda_fdc_motor_flag_off,DRIVE_HBLS_PER_ROTATION*9,0);
      }
      break;
    }//sw
  }//if
  if(!ADAT && (Fdc.str&FDC_STR_MO) && !drive.Empty())
    agenda_add(agenda_fdc_motor_flag_off,milliseconds_to_hbl(1800),0);
  // Don't need to add agenda if it has happened
  if(floppy_irq_flag && (floppy_irq_flag!=FLOPPY_IRQ_NOW)) 
  {
    if(floppy_irq_flag==FLOPPY_IRQ_ONESEC) 
    {
      TRACE_LOG("Error - 5REV - IRQ in %d HBLs\n",DRIVE_HBLS_PER_ROTATION*5);
      agenda_add(agenda_fdc_finished,DRIVE_HBLS_PER_ROTATION*5,0);
    }
    else
      agenda_add(agenda_fdc_finished,hbls_to_interrupt,0);
  }
}


void agenda_fdc_motor_flag_off(int) {
/*
 "If after finishing the command, the device remains idle for
 9 revolutions, the MO signal goes back to a logic 0."
  ->
  To ensure 9 revs, the controller counts 10 IP.
  We do the same as before, using our new variable IndexCounter

  We count IP, only if there's a spinning selected drive
  Not emulated: if program changes drive and the new drive is spinning
  too, but IP occurs at different time!

  Cases Symic; Treasure Trap -FOF

*/
  int floppyno=floppy_current_drive();
  TSF314 &drive=FloppyDrive[floppyno];
  agenda_delete(agenda_fdc_motor_flag_off);
  hbl_at_ip=hbl_count;
  if(ADAT)
  {
    if(Psg.CurrentDrive()!=TYM2149::NO_VALID_DRIVE && drive.bMotor && !drive.Empty())
      Fdc.IndexCounter++;
    if(Fdc.IndexCounter<10 && drive.ImageType.Manager==MNGR_STEEM)
    {
      agenda_add(agenda_fdc_motor_flag_off,DRIVE_HBLS_PER_ROTATION,0);
      return;
    }
    Fdc.IndexCounter=0; 
  }
  Fdc.str&=~FDC_STR_MO;
  TRACE_LOG("FDC1 Drive %c: motor off\n",'A'+floppyno);
  drive.bMotor=false;
}


void agenda_fdc_verify(int) {
  agenda_delete(agenda_fdc_verify);
  if(Fdc.StatusType && (Fdc.cr&Fdc.CR_V))
  {
    // This reads an ID field and checks that track number matches FloppyDrive's track
    // It will fail on an unformatted track or if there is no disk of course
    BYTE floppyno=floppy_current_drive();
    BYTE current_side=floppy_current_side();
    TFloppyDisk &disk=FloppyDisk[floppyno];
    BYTE &track=FloppyDrive[floppyno].track;
    if(track>FLOPPY_MAX_TRACK_NUM || track!=Fdc.tr) // track not correct
      Fdc.str|=FDC_STR_SE;
    else if(!disk.TrackIsFormatted[current_side][track])
    {
      // If track is formatted then it is okay to seek to it, otherwise do this:
      if(track>=disk.TracksPerSide || current_side>=disk.Sides)
        Fdc.str|=FDC_STR_SE;
    }
    if(Fdc.str & FDC_STR_SE)
    {
      TRACE_LOG("Verify error TR %d CYL %d\n",Fdc.tr,track);
    }
  }
  agenda_fdc_finished(0);
}


void agenda_fdc_finished(int flags) {
  BYTE floppyno=floppy_current_drive();
  TRACE_LOG2("byte position %d\n",FloppyDrive[floppyno].BytePosition());
  agenda_delete(agenda_fdc_finished);
  DiskEmu.Update(true);
#if defined(SSE_DRIVE_SOUND)
  if(OPTION_DRIVE_SOUND)
    FloppyDrive[DRIVE].SoundCheckIrq();
#endif
  if(floppy_irq_flag==FLOPPY_IRQ_ONESEC)
    Fdc.str|=FDC_STR_SE;
#if defined(SSE_MEGASTE)
  if(IS_MEGASTE && fdc_check_wrong_density())
    Fdc.str|=FDC_STR_SE;
#endif
  floppy_irq_flag=FLOPPY_IRQ_NOW;
  fdc_irq=true; // Sets bit in GPIP low (and it stays low)
  update_disk_irq();
  Fdc.str&=~FDC_STR_BSY; // Turn off busy bit
#if defined(SSE_DISK_STX)
  //  !	 2  ! Rd sectr !  1  0	0  m  h  E  0  0 !
  if(FloppyDisk[floppyno].STX_File && ImageSTX[floppyno].pSectorDesc && (Fdc.cr&0xe0)==0x80)
    Fdc.str|=ImageSTX[floppyno].SectorFlags;
#endif
#if defined(SSE_DISK_GHOST)
  if(!(OPTION_GHOST_DISK && Fdc.Lines.CommandWasIntercepted)) // spurious Lost Data
#endif
  {
    if(Fdc.StatusType)
    {
      Fdc.str&=~FDC_STR_T0;
      if(CURRENT_TRACK==0 && floppyno<DiskMan.nFloppyDrives)
        Fdc.str|=FDC_STR_T0;
      Fdc.StatusType=2;
      // SU
      if(fdc_spinning_up) //should be up-to-date for WD1772 emu too
        Fdc.str&=~FDC_STR_SU;
      else
        Fdc.str|=FDC_STR_SU;
    }
  }
  Fdc.str|=flags;
#if defined(SSE_ENABLE_TRACE_LOG)
  if(TRACE_LOG_ENABLED(LOGSECTION_FDC))
    DiskEmu.TraceRegs(); // -> "IRQ" trace (including 'busy')
#endif
/*  Command $D4 will trigger an IRQ at each index pulse.
    Motor keeps running.
    It's a way to measure rotation time (Panzer)
*/
  if(Fdc.InterruptCondition==4)
  {
//    TRACE_LOG("D4 prepare next IP at %d\n",hbl_count+DRIVE_HBLS_PER_ROTATION);
    agenda_add(agenda_fdc_finished,DRIVE_HBLS_PER_ROTATION,0);    
  } 
  else if(ADAT)
  {
    //  Set up agenda for next IP
    agenda_delete(agenda_fdc_motor_flag_off); //3.6.2
    Fdc.IndexCounter=0;
    DWORD delay=(ADAT) ? FloppyDrive[floppyno].HblsNextIndex() : 2;
    agenda_add(agenda_fdc_motor_flag_off,delay,0);
  }
}


void agenda_fdc_seek(int) {
  //TRACE_LOG("do agenda_fdc_seek\n");
  agenda_delete(agenda_fdc_seek); // this one is necessary, the others I don't know
  BYTE floppyno=floppy_current_drive();
/*
"SEEK
This command assumes that the track register contains the track number of the
current position of the Read/Write head and the Data Register contains the
desired track number. The WD1770 will update the Track Register and issue
stepping pulses in the appropiate direction until the contents of the Track
Register are equal to the contents of the Data Register (the desired Track
location). A verification operation takes place if the V flag is on. The h
bit allows the Motor On option at the start of the command. An interrupt is
generated at the completion of the command. Note: When using mutiple drives,
the track register must be updated for the drive selected before seeks are
issued."
  ->
  Seek works with DR and TR, not DR and disk track.
*/
  if(ADAT)
  {
    TRACE_LOG2("seek %c TR %d DR%d CYL %d\n",'A'+floppyno,Fdc.tr,Fdc.dr,FloppyDrive[floppyno].track);
    if(Fdc.tr==Fdc.dr)
    {
      fdc_type1_check_verify();
      return;
    }
    else if(Fdc.tr>Fdc.dr)
    {
      Fdc.tr--;
      if(FloppyDrive[floppyno].track)
        FloppyDrive[floppyno].track--;    
      if(!FloppyDrive[floppyno].track && floppyno<DiskMan.nFloppyDrives)
      {
        if(!(Fdc.cr&0xF0)) // condition?
          Fdc.tr=0; // this is how RESTORE works
        Fdc.str|=FDC_STR_T0;
      }
/*  
If the TR00* input does not go active low after 255 stepping pulses,
the WD1772 terminates operation, interrupts, and sets the Seek Error
status bit, providing the v flag is set.
->
If RESTORE fails to set TR00 in the status byte, TOS knows there's no
2nd drive.
*/
      else if(!(Fdc.cr&0xF0) && !Fdc.tr)
      {
        if(Fdc.cr&Fdc.CR_V)
          Fdc.str|=FDC_STR_SE;
        agenda_fdc_finished(0); //IRQ
      }
    }
    else // Fdc.tr<Fdc.dr
    {
      Fdc.tr++;
      if(FloppyDrive[floppyno].track<FLOPPY_MAX_TRACK_NUM-1)
        FloppyDrive[floppyno].track++;
    }
#if defined(SSE_GUI_STATUS_BAR)
    if(OPTION_STATUS_BAR && (DRIVE<DiskMan.nFloppyDrives)
#if !defined(SSE_GUI_STATUS_BAR_CLICKABLE)
      && (!OPTION_DRIVE_INFO) // not if on OSD
#else
      && (SSEConfig.StatusBarMask&(1<<SB_PART_CAPS))
#endif
      )
    {
#ifdef SSE_DEBUG // add current command (CR)
      sprintf(DiskEmu.sTrackinfo,"%2X-%C:%d-%02d-%02d",Fdc.cr,DRIVE,CURRENT_SIDE,
        FloppyDrive[floppyno].track,Fdc.sr);
#else
      sprintf(DiskEmu.sTrackinfo,"%C:%u-%02u-%02u",DRIVE,CURRENT_SIDE,
        FloppyDrive[floppyno].track,Fdc.sr);
#endif
#if defined(SSE_GUI_STATUS_BAR_DRAW_CAPS)
      strcpy(status_bar_text[SB_PART_CAPS],DiskEmu.sTrackinfo);
      DiskEmu.StatusBar=true;
      UPDATE_STATUS_BAR_PART(SB_PART_CAPS);
#else
      PostMessage(hStatusBar,SB_SETTEXT,SB_PART_CAPS,(LPARAM)DiskEmu.sTrackinfo);
#endif
    }
#endif//#if defined(SSE_GUI_STATUS_BAR)
  }
  else // !ADAT or not defined
  {
/*  In Steem 3.2, seek was directly using disk track instead of TR
    It still works that way in fast mode.
    It was already using an agenda for each step.
*/
    if(FloppyDrive[floppyno].track==Fdc.dr)
    {
      Fdc.tr=Fdc.dr;
      fdc_type1_check_verify();
      agenda_fdc_finished(0);
      return;
    }
    if(FloppyDrive[floppyno].track>Fdc.dr)
      FloppyDrive[floppyno].track--;
    else if(FloppyDrive[floppyno].track<Fdc.dr)
      FloppyDrive[floppyno].track++;
  }
  int hbls_to_interrupt=fdc_step_time_to_hbls[Fdc.cr & (Fdc.CR_I1|Fdc.CR_I0)];
  if(DiskMan.bTurboDrive)
    hbls_to_interrupt>>=5;
#if defined(SSE_DRIVE_SOUND)
  if(OPTION_DRIVE_SOUND && DiskEmu.InterTrack==1)
    FloppyDrive[floppyno].SoundStep();
#endif
  agenda_add(agenda_fdc_seek,hbls_to_interrupt,0);
}


void agenda_fdc_readwrite_sector(int Data) {
  agenda_delete(agenda_fdc_readwrite_sector);
  BYTE floppyno=floppy_current_drive();
  BYTE current_side=floppy_current_side();
  TSF314 &drive=FloppyDrive[floppyno];
  TFloppyDisk &disk=FloppyDisk[floppyno];
  BYTE &track=drive.track;
  BYTE Command=Fdc.cr; //shouldn't change...
  bool FromFormat=(track<=FLOPPY_MAX_TRACK_NUM)?disk.TrackIsFormatted[current_side][track]:false;
  BYTE WriteProtectBit=((Command&Fdc.CR_TYPEII_WRITE) && disk.WriteProtect)?FDC_STR_WP:0; 
  if(drive.Empty())
  {
    TRACE_LOG("No disk %c\n",floppyno+'A');
    Fdc.str=WriteProtectBit | FDC_STR_MO | /*FDC_STR_SE | */FDC_STR_BSY;
    agenda_add(agenda_fdc_finished,DRIVE_HBLS_PER_ROTATION*((Glue.VideoFreq==MONO_HZ)?11:5),0);
    return; // Don't loop
  }
  Fdc.str|=FDC_STR_BSY | FDC_STR_MO | WriteProtectBit;
  floppy_irq_flag=0;
  if(FLOPPY_ACCESS_FF(floppyno))
    floppy_access_ff_counter=FLOPPY_FF_VBL_COUNT;
  if(!ADAT)
  {
    agenda_delete(agenda_fdc_motor_flag_off);
    agenda_add(agenda_fdc_motor_flag_off,milliseconds_to_hbl(1800),0);
  }
instant_sector_access_loop:
  WORD Part=LOWORD(Data);
  WORD SectorStage=(Part % 71); // 0=seek, 1-64=read/write, 65=end of sector, 66-70=gap
  if(SectorStage==0) 
  {
    if(!disk.SeekSector(current_side,track,Fdc.sr,FromFormat)) 
    {
      // Error seeking sector, it doesn't exist
      TRACE_LOG("H%d T%d RNF %d\n",current_side,track,Fdc.sr);
      floppy_irq_flag=FLOPPY_IRQ_ONESEC;  //end command after 1 second
    }
#ifdef ONEGAME
    else{
      if (Command & 0x20){ // Write
        OGWriteSector(floppy_current_side(),FloppyDrive[floppyno].track,Fdc.sr,floppy->BytesPerSector);
      }
    }
#endif
#if defined(SSE_DISK_STX)
    if(disk.STX_File && ImageSTX[floppyno].pSectorDesc)
      Fdc.str|=(ImageSTX[floppyno].SectorFlags&FDC_STR_RT);
#endif
  }
  else if(SectorStage<=64) 
  {
    FILE *fp=(FromFormat ? disk.Format_fp : disk.fp);
    int BytesPerStage=16; // constant
    int PosInSector=(SectorStage-1)*BytesPerStage;
    BYTE Temp=0;
    if(Command&Fdc.CR_TYPEII_WRITE)
    { // Write
      if(disk.WriteProtect)
      {
        floppy_irq_flag=FLOPPY_IRQ_NOW; //interrupt with write-protect flag
#ifdef SSE_OSD_DRIVELED
        FDCCantWriteDisplayTimer=timer+3000;
#endif
      }
      else
      {
        if(!disk.ReadOnly && !DiskMan.bDiskProtectImage)
#if defined(SSE_DISK_STX)
        if(!(disk.STX_File&&DiskMan.bDiskProtectImageStx))
#endif
          disk.WrittenTo=true;
#ifdef SSE_OSD_DRIVELED
        if(disk.IsZip()) 
          FDCCantWriteDisplayTimer=timer+5000; // Writing will be lost!
#endif
        // byte per byte, we write 16 bytes
        for(int bb=BytesPerStage;bb>0;bb--)
        {
          Temp=Dma.GetFifoByte(MNGR_STEEM); // from RAM to disk
          Fdc.CrcLogic.Add(Temp); // debug info
          if(!disk.ReadOnly && !DiskMan.bDiskProtectImage)
#if defined(SSE_DISK_STX)
          if(!(disk.STX_File&&DiskMan.bDiskProtectImageStx))
#endif
            if(fp==NULL||FWRITE(&Temp,1,1,fp)==0)
            {
              if(fdc_handle_file_error(floppyno,true,Fdc.sr,PosInSector,FromFormat)) 
              {
                floppy_irq_flag=FLOPPY_IRQ_ONESEC;  //end command after 1 second
                break;
              }
            }
          PosInSector++;
        }
      }
    }
    else
    { // Read
      for(int bb=BytesPerStage;bb>0;bb--) 
      {	// int BytesPerStage=16;
        if(fp==NULL||FREAD(&Temp,1,1,fp)==0)
        {
#if defined(SSE_DISK_STX)
          if(disk.STX_File && disk.SeekSector(current_side,track,Fdc.sr,FromFormat))
          {}
          else
#endif
          if(fdc_handle_file_error(floppyno,false,Fdc.sr,PosInSector,FromFormat))
          {
            floppy_irq_flag=FLOPPY_IRQ_ONESEC;  //end command after 1 second
            break;
          }
        }
#if defined(SSE_DISK_STX)
        if(disk.STX_File) // Fuzzy bits
        {
          if(SSEOptions.FuzzyBits && ImageSTX[floppyno].bFuzzySector 
            && (DWORD)PosInSector<ImageSTX[floppyno].pTrackDesc->fuzzyCount)
          {
            Temp=(Temp&ImageSTX[floppyno].pFuzzyTable[PosInSector]) // sure 0 sure 1
              |(rand() & ~ImageSTX[floppyno].pFuzzyTable[PosInSector]); // maybe 0 maybe 1
          }
          Fdc.dr=Temp;
        }
#endif
        if(Dma.Counter) //game Sabotage
          Dma.AddToFifo(MNGR_STEEM,Temp); // disk to RAM
        //else
          //Fdc.str|=FDC_STR_LD; // nope: Super Hang-On
        Fdc.CrcLogic.Add(Temp);
        PosInSector++;
      }
    }
    if(PosInSector>=((FromFormat) ? disk.FormatLargestSector : disk.BytesPerSector))
    {
      //TRACE_LOG("Part %d->%d crc %04X\n",Part,64,Fdc.CrcLogic.crc);
      Part=64; // Done sector, last part
#if defined(SSE_DISK_STX)
      BYTE status=(disk.STX_File && ImageSTX[floppyno].pSectorDesc)?ImageSTX[floppyno].SectorFlags:0;
      if(status&FDC_STR_CRC)
        floppy_irq_flag=FLOPPY_IRQ_NOW;
      else if(Command&Fdc.CR_M)
      { // Multiple sectors, this happens in short order after the sector has been R/W
        Fdc.sr++;
        if(!Fdc.sr)
          Fdc.sr++; // CAPS: 255 wraps to 1 (due to add with carry)
      }
#endif
    }
  }
  else if(SectorStage==65)
  {
    floppy_irq_flag=FLOPPY_IRQ_NOW;
    if(Command&Fdc.CR_M)
    { // Multiple sectors
#if !defined(SSE_DISK_STX)
      Fdc.sr++;
#endif
      floppy_irq_flag=0;
      {
#if defined(SSE_DISK_STX)
        if(disk.STX_File)
        {
          agenda_delete(agenda_fdc_readwrite_sector);
          WORD start=drive.BytePosition();
          WORD bytes=start;
          if(ImageSTX[floppyno].GetSector((BYTE)current_side,track,Fdc.sr,bytes))
          {
            int nhbls=drive.BytesToHbls(bytes);
            agenda_add(agenda_fdc_readwrite_sector,nhbls,MAKELONG(0,start));
            return;
          }
          else
            floppy_irq_flag=FLOPPY_IRQ_ONESEC;
        }
        else
#else
        if(!Fdc.sr)
          Fdc.sr++; // CAPS: 255 wraps to 1 (due to add with carry)
        TRACE_LOG("SR->%d\n",Fdc.sr);
#endif
        {
          // The controller must find next sector
          int param=MAKELONG(++Part,Command);
          int nhbls=(ADAT) ? drive.BytesToHbls(disk.SectorGap()) : 1;
          agenda_add(agenda_fdc_readwrite_sector,nhbls,param);
          return;
        }
      }
    }
  }
  Part++;
  switch(floppy_irq_flag) {
  case FLOPPY_IRQ_NOW:
    Fdc.str|=WriteProtectBit|FDC_STR_MO;
    // delay IRQ: crc computing (Test Drive)
    agenda_add(agenda_fdc_finished,drive.BytesToHbls(3),0);
    return; // Don't loop
  case FLOPPY_IRQ_ONESEC:
    // sector not found
    TRACE_LOG("Sector %d not found, IRQ in 1 sec\n",Fdc.sr);
    agenda_add(agenda_fdc_finished,DRIVE_HBLS_PER_ROTATION
      *((Glue.VideoFreq==MONO_HZ) ? 11 : 5),FDC_STR_RNF);
    return; // Don't loop
  }
  if(DiskMan.bTurboDrive)
  {  // Fast disk
    Data=MAKELONG(Part,Command); // Part has been ++, Command is unchanged
    goto instant_sector_access_loop; // goto considered harmful
  }
#if defined(SSE_DISK_STX)
  else if(disk.STX_File)
  {
    WORD start=HIWORD(Data),bytes=16;
    if(ImageSTX[floppyno].bMacrodos) // Macrodos/Speedlock: density varies across one sector
    {
      if(Part>=10&&Part<18) // 1st part normal, 2d part slow
      {
        bytes++; // gross
        DiskEmu.BitRate=470;
      }
      else if(Part>=18&&Part<26) // 3d part fast, 4th part normal
      {
        bytes--; // gross
        DiskEmu.BitRate=530;
      }
    }
    int nhbls=drive.BytesToHbls(bytes);
    int d=ImageSTX[floppyno].SectorTiming-NStx::NORMAL_TIMING;
    if(abs(d)>=10 && abs(d)<200) // constant but abnormal density
    {
      nhbls+=d/20;
      DiskEmu.BitRate=500-(WORD)(d/2);
    }
    agenda_add(agenda_fdc_readwrite_sector,nhbls,MAKELONG(Part,start)); 
  }
#endif
  else
  { //Slow disk
/*  Correct drift due to hbl system imprecision. 
    With packs of 16 bytes it can accumulate, and at the end of the sector, 
    we're off by a couple of HBL, enough to miss next ID. */
    //ASSERT(Part!=64||FloppyDrive[DRIVE].ImageType.Extension==EXT_STT); // 32 -> 65
    WORD bytes=(Part==65) ? 19 : 16;
    WORD start=HIWORD(Data);
    if(Part<65)
    {
      WORD current_byte=drive.BytePosition();
      WORD theory_byte=start+((WORD)Part-1)*16;
#if 1 // limit correction!
      SHORT drift=current_byte-theory_byte;
      if(drift<0)
        bytes+=2;
      else if(drift>0)
        bytes-=2;
#else
      if(current_byte-theory_byte)
      {
        //TRACE_LOG("Drift start %d theory %d byte %d Part %d\n",start,start+(Part-1)*16,FloppyDrive[DRIVE].BytePosition(),Part);
        bytes-=2*(current_byte-theory_byte);
      }
#endif
      if(Part==64)
        bytes+=3; //v4: crc, ff
    }
    WORD nhbls=drive.BytesToHbls(bytes);
    agenda_add(agenda_fdc_readwrite_sector,nhbls,MAKELONG(Part,start)); 
  }
}


void agenda_fdc_read_address(int idx) {
  agenda_delete(agenda_fdc_read_address);
  BYTE floppyno=floppy_current_drive();
  TWD1772IDField IDList[255];
  int nSects=FloppyDisk[floppyno].GetIDFields(floppy_current_side(),FloppyDrive[floppyno].track,IDList);
  if(idx<nSects) 
  {
    TRACE_LOG("Read address T%d S%d s#%d t%d CRC %02X%02X\n", IDList[idx].track,
      IDList[idx].side,IDList[idx].num,IDList[idx].len,IDList[idx].CRC[0],IDList[idx].CRC[1]);
    // timing of this is not really correct
    Dma.AddToFifo(MNGR_STEEM,IDList[idx].track);
    Dma.AddToFifo(MNGR_STEEM,IDList[idx].side);
    Dma.AddToFifo(MNGR_STEEM,IDList[idx].num);
    Dma.AddToFifo(MNGR_STEEM,IDList[idx].len);
    Dma.AddToFifo(MNGR_STEEM,IDList[idx].CRC[0]);
    Dma.AddToFifo(MNGR_STEEM,IDList[idx].CRC[1]);
    Fdc.str&=~FDC_STR_WP;
    Fdc.str|=FDC_STR_MO;
    //The Track Address of the ID field is written into the sector register so 
    //that a comparison can be made by the user.
    Fdc.sr=IDList[idx].track;
    agenda_fdc_finished(0);
    if(!ADAT) 
    {
      agenda_delete(agenda_fdc_motor_flag_off);
      agenda_add(agenda_fdc_motor_flag_off,milliseconds_to_hbl(1800),0);
    }
  }
}


void agenda_fdc_read_track(int part) {
  //TRACE_LOG2("agenda_fdc_read_track(%d)\n",part);
  agenda_delete(agenda_fdc_read_track);
  WORD &CRC=Fdc.CrcLogic.crc;
  static short BytesRead;
  BYTE floppyno=floppy_current_drive();
  BYTE current_side=floppy_current_side();
  TSF314 &drive=FloppyDrive[floppyno];
  if(drive.Empty()) 
    return; // Stop, timeout
  TFloppyDisk &disk=FloppyDisk[floppyno];
  BYTE &track=drive.track;
  bool bError=false,bFromFormat=false;
  if(track<=FLOPPY_MAX_TRACK_NUM)
    bFromFormat=disk.TrackIsFormatted[current_side][track];
  int RealPart=HIWORD(part); //starts at 0
  part=LOWORD(part);
  if(part==0) 
    BytesRead=0;
  Fdc.str|=FDC_STR_BSY;
  if(FLOPPY_ACCESS_FF(floppyno))
    floppy_access_ff_counter=FLOPPY_FF_VBL_COUNT;
  if(!ADAT)
  {
    agenda_delete(agenda_fdc_motor_flag_off);
    agenda_add(agenda_fdc_motor_flag_off,milliseconds_to_hbl(1800),0);
  }
  int TrackBytes=disk.GetRawTrackData(current_side,track);
  if(TrackBytes) // STT, STX with track image
  {
#if defined(SSE_DISK_STX)
    if(!disk.STX_File)
#endif
      FSEEK(disk.fp,BytesRead,SEEK_CUR);
    int ReinsertAttempts=0;
    BYTE Temp;
    for(int n=0;n<16;n++)
    {
      if(BytesRead>=TrackBytes) 
        break;
      if(FREAD(&Temp,1,1,disk.fp)==0) 
      {
        TRACE_LOG("%s reading file\n","ERROR");
        if(ReinsertAttempts++>2) 
        {
          bError=true;
          break;
        }
        if(drive.ReinsertDisk())
        {
          TrackBytes=disk.GetRawTrackData(current_side,track);
          FSEEK(disk.fp,BytesRead,SEEK_CUR);
          n--;
        }
      }
      else
      {
#if defined(SSE_DISK_STX)
        if(disk.STX_File && BytesRead<ImageSTX[floppyno].FirstSync)
        {
          BYTE Temp2=Temp;
          Temp>>=StartShift;
          Temp|=ShiftIn; // random shift of first bytes on READ TRACK
          ShiftIn=Temp2<<(8-StartShift);
        }
#endif
        if(DMA_ADDRESS_IS_VALID_W && Dma.Counter)
          Dma.AddToFifo(MNGR_STEEM,Temp);
        BytesRead++;
      }
    }
  }
  else // ST, MSA, DIM, STX without track image
  {
    int DDBytes=disk.TrackBytes; //v4
    TWD1772IDField IDList[255];
    int nSects=disk.GetIDFields(current_side,track,IDList);
    if(nSects==0)
    {
      // Unformatted track, read in random values
      TrackBytes=DDBytes;
      for(int bb=0;bb<16;bb++) 
      {
        write_to_dma(rand()&0xFF);
        BytesRead++;
      }
    }
    else
    {
      // Find out if it is a high density track
      TrackBytes=0;
      for(int n=0;n<nSects;n++) 
        TrackBytes+=22+12+3+1+6+22+12+3+1 + (128 << IDList[n].len) + 26;
      if(TrackBytes>DDBytes)
        TrackBytes=DDBytes*2;
      else
        TrackBytes=DDBytes;
      if(part/154<nSects) 
      {
        DWORD IDListIdx=part/154;
        BYTE SectorNum=IDList[IDListIdx].num;
        int SectorBytes=(128 << IDList[IDListIdx].len);
        BYTE pre_sect[200];
        int i=0;
        //for(int n=0;n<(ADAT?nSects<FloppyDisk[floppyno].PostIndexGap():22);n++) //?
        for(int n=0;n<(SectorNum ? 22 : disk.PostIndexGap());n++)  //v4 60,22,10 - 22
          pre_sect[i++]=0x4e;  // gap 1 & 3
/*
Gap 2 Pre ID                    12+3        12+3         3+3     00+A1
*/
        for(int n=0;n<(nSects<11?12:3);n++) 
          pre_sect[i++]=0x00; // gap 2 PLL Lockup time
        for(int n=0;n<3;n++) 
          pre_sect[i++]=0xa1;   // gap 2 Marker
        pre_sect[i++]=0xfe;                         // Start of address mark
        pre_sect[i++]=IDList[IDListIdx].track;
        pre_sect[i++]=IDList[IDListIdx].side;
        pre_sect[i++]=IDList[IDListIdx].num;
        pre_sect[i++]=IDList[IDListIdx].len;
        pre_sect[i++]=IDList[IDListIdx].CRC[0];
        pre_sect[i++]=IDList[IDListIdx].CRC[1];
        for(int n=0;n<22;n++) 
          pre_sect[i++]=0x4e; // gap 3a
        for(int n=0;n<12;n++) 
          pre_sect[i++]=0x00; // gap 3b
        for(int n=0;n<3;n++) 
          pre_sect[i++]=0xa1;  // gap 3b Marker
/*
Data Address Mark                  1           1           1      FB
*/
        pre_sect[i++]=0xfb;                        // Start of data
        int num_bytes_to_write=16;
        int byte_idx=(part%154)*16;
        // Write the gaps/address before the sector
        if(byte_idx<i) 
        {
          while(num_bytes_to_write>0)
          {
            write_to_dma(pre_sect[byte_idx++]);
            num_bytes_to_write--;
            BytesRead++;
            if(byte_idx>=i)
              break;
          }
        }
        byte_idx-=i;
        // Write the sector
/*
Data                             512         512         512
*/
        if(num_bytes_to_write>0&&byte_idx>=0&&byte_idx<SectorBytes)
        {
          if(byte_idx==0)
          {
            CRC=0xffff;
            fdc_add_to_crc(CRC,0xa1);
            fdc_add_to_crc(CRC,0xa1);
            fdc_add_to_crc(CRC,0xa1);
            fdc_add_to_crc(CRC,0xfb);
          }
          if(!disk.SeekSector(current_side,track,SectorNum,bFromFormat)) 
          {
            // Can't seek to sector!
            TRACE_LOG("Argh! Can't seek to sector!\n");
            while(num_bytes_to_write>0) 
            {
              write_to_dma(0x00);
              fdc_add_to_crc(CRC,0x00);
              num_bytes_to_write--;
              BytesRead++;
              byte_idx++;
              if(byte_idx>=SectorBytes)
                break;
            }
          }
          else
          {
            FILE *fp=(bFromFormat) ? disk.Format_fp : disk.fp;
            ASSERTB(fp);
            FSEEK(fp,byte_idx,SEEK_CUR);
            BYTE Temp;
            for(;num_bytes_to_write>0;num_bytes_to_write--)
            {
              if(FREAD(&Temp,1,1,fp)==0) 
              {
                if(fdc_handle_file_error(floppyno,false,SectorNum,byte_idx,bFromFormat)) 
                {
                  Fdc.str=FDC_STR_MO|FDC_STR_SE|FDC_STR_BSY;
                  bError=true;
                  num_bytes_to_write=0;
                  break;
                }
              }
              fdc_add_to_crc(CRC,Temp);
              if(DMA_ADDRESS_IS_VALID_W && Dma.Counter)
                Dma.AddToFifo(MNGR_STEEM,Temp);
              BytesRead++;
              byte_idx++;
              if(byte_idx>=SectorBytes) 
                break;
            }
          }
        }
        byte_idx-=SectorBytes;
        // Write CRC
/*
CRC                                2           2           2
*/
        if(num_bytes_to_write>0&&byte_idx>=0&&byte_idx<2)
        {
          if(byte_idx==0)
          {
            write_to_dma(HIBYTE(CRC));          // End of Data Field (CRC)
            byte_idx++;
            BytesRead++;
            num_bytes_to_write--;
          }
          if(byte_idx==1&&num_bytes_to_write>0)
          {
            write_to_dma(LOBYTE(CRC));          // End of Data Field (CRC)
            byte_idx++;
            BytesRead++;
            num_bytes_to_write--;
          }
        }
        byte_idx-=2;//?
        // Write Gap 4
/*
Gap 4 Post Data                   40          40           1      4E
*/
        int gap4bytes=disk.PostDataGap();
        if(num_bytes_to_write>0&&byte_idx>=0&&byte_idx<gap4bytes) 
        {
          while(num_bytes_to_write>0)
          {
            write_to_dma(0x4e);
            byte_idx++;
            num_bytes_to_write--;
            BytesRead++;
            if(byte_idx>=gap4bytes) 
            {
              // Move to next sector (-1 because we ++ below)
              part=(IDListIdx+1)*154-1;
              break;
            }
          }
        }
      }
      else
      {
        // End of track, read in 0x4e
        BYTE gap5bytes=(nSects>=11 ? 20 : 16); //ProCopy 1.5 Analyze
        // isn't it a bug anyway? More gap with 11 than 9-10 ???
        write_to_dma(0x4e,gap5bytes);
        BytesRead+=gap5bytes;
      }
    }
  }
  part++;
  if(BytesRead>=TrackBytes) 
  {//finished reading in track
    TRACE_LOG2("READ TRACK all fine!\n");
    Fdc.str=FDC_STR_MO;  //all fine!
    agenda_fdc_finished(0);
  }
  else if(bError)
  {
    TRACE_LOG("READ TRACK %s %d\n","ERROR",bError);
  }
  else //if(!bError)
  {   //read more of the track
//    TRACE_LOG("reading on... %d/%d\n",BytesRead,TrackBytes);
    int bytes_per_second=disk.TrackBytes*5;
    int hbls_per_second=HBL_PER_SECOND;
    int n_hbls=hbls_per_second/(bytes_per_second/16);
/*  Correct HBL drift for Read Track.
    Timing is important for ProCopy Analyze.
*/
    WORD current_byte=drive.BytePosition();
    WORD theory_byte=(WORD)RealPart*16;
#if defined(SSE_DISK_STX)
    if(disk.STX_File && abs(ImageSTX[floppyno].TrackTiming-NStx::NORMAL_TIMING)>10) // Bubble Bobble: 999
      DiskEmu.BitRate=(ImageSTX[floppyno].TrackTiming/2); // track with abnormal density
#endif
    if(current_byte>theory_byte)
      n_hbls--;
    else if(current_byte<theory_byte)
      n_hbls++;
    TRACE_LOG2("READ TRACK Drift theory %d byte %d #hbl %d\n",theory_byte,current_byte,n_hbls);
    RealPart++;
    agenda_add(agenda_fdc_read_track,n_hbls,MAKELONG(part,RealPart)); 
  }
}


/*
DISK FORMATTING:
----------------

The 177x formats disks according to the IBM 3740 or System/34
standard.  See the Write Track command for the CPU formatting method.
The recommended physical format for 256-byte sectors is as follows.

Number of Bytes     Value of Byte      Comments
---------------     -------------      --------
60                  $4e                Gap 1 and Gap 3.  Start and end of index
                                       pulse.
12                  $00                Gap 3.  Start of bytes repeated for each
                                       sector.
3                   $a1                Gap 3.  Start of ID field.  See section
                                       on Write Track command.
1                   $fe                ID address mark
1                   track #            $00 through $4c (0 through 76)
1                   side #             0 or 1
1                   sector #           $01 through $10 (1 through 16)
1                   length code        See section on Read Address command.
2                   CRC                End of ID field.  See section on Write
                                       Track command.
22                  $4e                Gap 2.
12                  $00                Gap 2.  During Write Sector commands the
                                       drive starts writing at the start
                                       of this.
3                   $a1                Gap 2.  Start of data field.  See
                                       section on Write Track command.
1                   $fb                data address mark
256                 data               Values $f5, $f6, and $f7 invalid.  See
                                       section on Write Track command.  IBM
                                       uses $e5.
2                   CRC                End of data field.  See section on Write
                                       Track command.
24                  $4e                Gap 4.  End of bytes repeated for each
                                       sector.  During Write Sector
                                       commands the drive stops writing shortly
                                       after the beginning of this.
668                 $4e                Continue writing until the 177x
                                       generates an interrupt.  The listed byte
                                       count is approximate.

Variations in the recommended formats are possible if the following
requirements are met:
(1)  Sector size must be 128, 256, 512, or 1024 bytes.
(2)  All address mark indicators ($a1) must be 3 bytes long.
(3)  The $4e section of Gap 2 must be 22 bytes long.  The $00 section of Gap 2
     must be 12 bytes long.
(4)  The $4e section of Gap 3 must be at least 24 bytes long.  The $00 section
     of Gap 3 must be at least 8 bytes long.
(5)  Gaps 1 and 4 must be at least 2 bytes long.  These gaps should be longer
     to allow for PLL lock time, motor speed variations, and write splice time.

The 177x does not require an Index Address Mark.
*/


void agenda_fdc_write_track(int part) { // note STW better format for this
  agenda_delete(agenda_fdc_write_track);
  static short SectorLen,nSector=-1;
  BYTE floppyno=floppy_current_drive();
  BYTE current_side=floppy_current_side();
  TSF314 &drive=FloppyDrive[floppyno];
  BYTE &track=drive.track;
  TFloppyDisk &disk=FloppyDisk[floppyno];
  BYTE Data;
  int TrackBytes=disk.TrackBytes; // Double density format only
  if(disk.WriteProtect||disk.STT_File)
  { // STT is no solution for Format
    Fdc.str=FDC_STR_MO|FDC_STR_WP;
    agenda_fdc_finished(0);
    return;
  }
  if(drive.Empty()) 
  {
    Fdc.str=FDC_STR_MO|FDC_STR_SE|FDC_STR_BSY;
    return;
  }
  if(!DiskMan.bDiskProtectImage)
#if defined(SSE_DISK_STX)
  if(!(disk.STX_File&&DiskMan.bDiskProtectImageStx))
#endif
    disk.WrittenTo=true;
  bool Error=false;
  Fdc.str|=FDC_STR_BSY;
  if(FLOPPY_ACCESS_FF(floppyno))
    floppy_access_ff_counter=FLOPPY_FF_VBL_COUNT;
  if(!ADAT)
  {
    agenda_delete(agenda_fdc_motor_flag_off);
    agenda_add(agenda_fdc_motor_flag_off,milliseconds_to_hbl(1800),0);
  }
  if(part==0)
  { // Find data/address marks
    // Find address marks and read SectorLen and nSector from the address
    // Must have [[0xa1] 0xa1] 0xa1 0xfe or [[0xc2] 0xc2] 0xc2 0xfe
    // Find data marks and increase part
    // Must have [[0xa1] 0xa1] 0xa1 0xfb or [[0xc2] 0xc2] 0xc2 0xfb
    for(int n=0;n<16;n++)
    {
      Data=Dma.GetFifoByte(MNGR_STEEM);
      floppy_write_track_bytes_done++;
      if(Data==0xa1||Data==0xf5||Data==0xc2||Data==0xf6)
      { // Start of gap 3
        int Timeout=10;
        do {
          Data=Dma.GetFifoByte(MNGR_STEEM);
          floppy_write_track_bytes_done++;
        } while((--Timeout)>0&&(Data==0xa1||Data==0xf5||Data==0xc2||Data==0xf6));
        if(Data==0xfe)
        { // Found address mark
          if(dma_address+4<himem) 
          {
            nSector=SafePeek(dma_address+2);
            BYTE len=SafePeek(dma_address+3);
            if(len<4)
              SectorLen=128<<len;
            else
              Error=true;
            if(Error) 
            {
              Error=0;
            }
          }
        }
        else if(Data==0xfb)
        {
          part++; // Read next SectorLen bytes of data
          break;
        }
      }
    }
  }
  else
  {
    bool IgnoreSector=true;
    if(nSector>=0)
      IgnoreSector=!disk.SeekSector(current_side,track,nSector,true);
    if(IgnoreSector) 
    {
      dma_address+=SectorLen;
      floppy_write_track_bytes_done+=(WORD)SectorLen;
      part=0;
      nSector=-1;
    }
    else
    {
      FSEEK(disk.Format_fp,(part-1)*16,SEEK_CUR);
      disk.FormatMostSectors=MAX((int)nSector,disk.FormatMostSectors);
      disk.FormatLargestSector=MAX((int)SectorLen,disk.FormatLargestSector);
      disk.TrackIsFormatted[current_side][track]=true; // only place
      for(int bb=0;bb<16;bb++) 
      {
        Data=Dma.GetFifoByte(MNGR_STEEM);
        if(!DiskMan.bDiskProtectImage)
#if defined(SSE_DISK_STX)
        if(!(disk.STX_File&&DiskMan.bDiskProtectImageStx))
#endif
        if(FWRITE(&Data,1,1,disk.Format_fp)==0) 
        {
          Error=true;
          if(disk.ReopenFormatFile())
          {
            disk.SeekSector(current_side,track,nSector,true);
            FSEEK(disk.Format_fp,(part-1)*16+bb,SEEK_CUR);
            Error=(FWRITE(&Data,1,1,disk.Format_fp)==0);
          }
        }
        if(Error) 
          break;
        floppy_write_track_bytes_done++;
      }
      part++;
      if((part-1)*16>=SectorLen) 
      {
        nSector=-1;
        part=0;
      }
    }
  }
  if(floppy_write_track_bytes_done>TrackBytes) 
  {
    Fdc.str=FDC_STR_MO;  //all fine!
    agenda_fdc_finished(0);
    FFLUSH(disk.Format_fp);
  }
  else if(Error)
  {
    Fdc.str=FDC_STR_MO|FDC_STR_SE|FDC_STR_BSY;
  }
  else
  { //write more of the track
    int bytes_per_second=disk.TrackBytes*5;
    int n_hbls=HBL_PER_SECOND/(bytes_per_second/16);
    agenda_add(agenda_fdc_write_track,n_hbls,part);
  }
}


void fdc_make_crc16_table() { // call at init
/*  
    http://www.emcu.it/CRC/CRCuk.html
    
    for reference the table is
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
*/
  WORD POLYNOMIAL = 0x1021;
  WORD /*crc,*/remainder;
#define WIDTH (8 * sizeof(WORD/*crc*/))
#define TOPBIT (1 << (WIDTH - 1))
  /*
  * Compute the remainder of each possible dividend.
  */
  for (WORD dividend = 0; dividend < 256; ++dividend)
  {
    /*
    * Start with the dividend followed by zeros.
    */
    remainder = dividend << (WIDTH - 8);

    /*
    * Perform modulo-2 division, a bit at a time.
    */
    for (BYTE bit = 8; bit > 0; --bit)
    {
      /*
      * Try to divide the current data bit.
      */ 
      if (remainder & TOPBIT)
      {
        remainder = (remainder << 1) ^ POLYNOMIAL;
      }
      else
      {
        remainder = (remainder << 1);
      }
    }

    /*
    * Store the result into the table.
    */
    crc16_table[dividend] = remainder;
  }
#undef WIDTH
#undef TOPBIT
}


#ifdef DEADC0DE
void fdc_add_to_crc(WORD& crc,BYTE* data,WORD n) {
  for(WORD i = 0; i < n; i++)
  {
    WORD index=(crc>>8)^data[i];
    crc=(crc<<8)^crc16_table[index];
  }
}
#endif


void fdc_add_to_crc(WORD &crc,BYTE data) {
  // table is 512byte
  WORD index=(crc>>8)^data;
  crc=(crc<<8)^crc16_table[index];
#ifdef DEADC0DE
  // The CRC polynomial is x^16+x^12+x^5+1 (CRC-16-CCITT polynomial)
  for(int i=0;i<8;i++)
    crc=WORD((crc<<1)^((((crc>>8)^(data<<i))&0x0080)?0x1021:0));
#endif
}


#if USE_PASTI // TODO maybe interface_pasti files

void PASTI_CALLCONV pasti_handle_return(struct pastiIOINFO *pPIOI) {
  pasti_update_time=ABSOLUTE_SYS_TIME+pPIOI->updateCycles*TICKS8;
  BOOL old_irq=((Mfp.reg[MFPR_GPIP]&MFP_GPIP_FDC_MASK)==0);
  if(old_irq!=pPIOI->intrqState) 
  {
#if defined(SSE_DISK_GHOST)
    if(!Fdc.Lines.CommandWasIntercepted)
#endif
    {
      fdc_irq=(pPIOI->intrqState==TRUE);
      if(pasti_active)
        hdc_irq=fdc_irq;
      update_disk_irq();
    }
    if(FLOPPY_ACCESS_FF(DRIVE))
      floppy_access_ff_counter=FLOPPY_FF_VBL_COUNT;
  }
  if(pPIOI->haveXfer)
  {
    dma_address=pPIOI->xferInfo.xferSTaddr;
    if(pPIOI->xferInfo.memToDisk) 
    {
      for(DWORD i=0;i<pPIOI->xferInfo.xferLen;i++)
      {
        if(DMA_ADDRESS_IS_VALID_R)
        {
          ((LPBYTE)pPIOI->xferInfo.xferBuf)[i]=Dma.GetFifoByte(MNGR_PASTI);
        }
      }
    }
    else
    {
      for(DWORD i=0;i<pPIOI->xferInfo.xferLen;i++) 
      {
        if(DMA_ADDRESS_IS_VALID_W) 
        {
          Dma.AddToFifo(MNGR_PASTI,((LPBYTE)pPIOI->xferInfo.xferBuf)[i]);
          //DEBUG_CHECK_WRITE_B(dma_address);
        }
      }
    }
  }
  DiskEmu.Update(old_irq!=pPIOI->intrqState && pPIOI->intrqState);
#if defined(SSE_DEBUG)
  if(TRACE_LOG_ENABLED(LOGSECTION_FDC)&&!old_irq&&old_irq!=pPIOI->intrqState) 
  {
    ASSERTB(DiskEmu.LastManager==MNGR_PASTI);
    DiskEmu.TraceRegs();
  }
#endif
  if(pPIOI->brkHit)
  {
    if(runstate==RUNSTATE_RUNNING) 
    {
      runstate=RUNSTATE_STOPPING;
      SET_WHY_STOP("Pasti breakpoint");
    }
    DEBUG_ONLY(if(debug_in_trace) SET_WHY_STOP("Pasti breakpoint"); )
  }
#if defined(SSE_DRIVE_SOUND)
  if(OPTION_DRIVE_SOUND && pPIOI->intrqState&&!old_irq)
    FloppyDrive[DRIVE].SoundCheckIrq();
#endif
}


#undef LOGSECTION
#define LOGSECTION LOGSECTION_PASTI


void PASTI_CALLCONV pasti_motor_proc(BOOL state) {
  TRACE_LOG("(pasti %c motor %d)\n",'A'+DRIVE,state);
  FloppyDrive[DRIVE].Motor(!!state); // for the trace
}


void PASTI_CALLCONV pasti_log_proc(const char * text) {
  TRACE_LOG("Pasti: %s\n",(char*)text);
}


void PASTI_CALLCONV pasti_warn_proc(const char *text) {
  Alert((char*)text,"Pasti Warning",0);
}

#endif//pasti


#undef LOGSECTION
#define LOGSECTION LOGSECTION_FDC


void TWD1772::Reset() {
  str=FDC_STR_IP; //2
  InterruptCondition=0;
#if defined(SSE_DISK_GHOST)
  Lines.CommandWasIntercepted=false;
#endif
  prg_phase=WD_READY;
  StatusType=1;  //Fdc.StatusType=2;???
  Lines.bMotor=false;
  //tr=sr=dr=0; //? not clear in doc // no: So Watt
  floppy_irq_flag=fdc_spinning_up=0;
}

/*  DmaIO -> WD1772IO
    IO is complicated because there are various WD1772 emulations running,
    we must call the correct one!
*/

BYTE TWD1772::IORead(BYTE Line) {
  ASSERTB( Line<=3 );
  BYTE ior_byte=0xFF; //W4
  // Steem handling
  switch(Line) {
  case 0: // str
#if defined(SSE_DISK_GHOST)
    if(Lines.CommandWasIntercepted)
    {
      ior_byte=str; // just "motor on" $80, but need agenda???
      break;
    }
#endif
    {
      // IP
      if(floppy_track_index_pulse_active())
        str|=FDC_STR_IP;
      else
        str&=~FDC_STR_IP;
      if(StatusType) // type I command status
      {
        // WP
        // disk has just been changed (30 VBL set at SetDisk())
        if(floppy_mediach[DRIVE])
        {
          str&=~FDC_STR_WP;
          if(floppy_mediach[DRIVE]/10!=1) 
            str|=FDC_STR_WP;
        }
        // Permanent status if disk is in
        else if(FloppyDisk[DRIVE].WriteProtect && !FloppyDrive[DRIVE].Empty())
          str|=FDC_STR_WP;
        // SU
        else
        if(fdc_spinning_up) //should be up-to-date for WD1772 emu too
          str&=~FDC_STR_SU;
        else
          str|=FDC_STR_SU;
        // TR0: update line and STR
        Lines.track0=(FloppyDrive[DRIVE].track==0 && DRIVE<DiskMan.nFloppyDrives);
        if(Lines.track0)
          str|=FDC_STR_T0;
        else
          str&=~FDC_STR_T0;
      } 
      if((Mfp.reg[MFPR_GPIP]&MFP_GPIP_FDC_MASK)==0) // IRQ is currently raised
      {
        floppy_irq_flag=0;
/*
"When using the immediate interrupt condition (i3 = 1) an interrupt
 is immediately generated and the current command terminated. 
 Reading the status or writing to the Command Register does not
 automatically clear the interrupt. The Hex D0 is the only command 
 that enables the immediate interrupt (Hex D8) to clear on a subsequent 
 load Command Register or Read Status Register operation. 
 Follow a Hex D8 with D0 command."
 -> More precisely: with D8, for both read str (here) and write cr:
 "clear IRQ if no condition", then "clear condition",
*/
        if(InterruptCondition!=8) // see note in IORead()
        {
          fdc_irq=false; // Turn off IRQ output
          update_disk_irq();
        }
        InterruptCondition=0;
      }
      Lines.irq=false;
      ior_byte=str;
    }
    break;
  case 1:
    ior_byte=tr;
    break;
  case 2:
    ior_byte=sr;
    break;
  case 3:
    ior_byte=dr;
    break;
  default:
    BREAKPOINT(TWD1772::IORead);
  }//sw
  // CAPS handling
#if defined(SSE_DISK_CAPS)
  if(FloppyDrive[DRIVE].ImageType.Manager==MNGR_CAPS)
    ior_byte=(BYTE)Caps.ReadWD1772(Line);
#endif
  return ior_byte;
}


void TWD1772::IOWrite(BYTE Line,BYTE io_src_b) {
  ASSERTB( Line<=3 );
  TSF314 &drive=FloppyDrive[DRIVE];
  BYTE &manager=drive.ImageType.Manager;
  switch(Line) {
  case 0: // Write CR
    if(io_src_b==0xFF)
    {
      TRACE_LOG("FDC ignores $FF\n"); // Infestation
      break;
    }
    DiskEmu.LastManager=manager;
    DiskEmu.Update();
    DiskEmu.AcsiBsy=false;
    DiskEmu.BitRate=500;

#if defined(SSE_DRIVE_SOUND)
    DiskEmu.VBLSoundcheck=false;
    DiskEmu.InterTrack=0;
    if(!(io_src_b&BIT_7)) // type I
    {
      // sound seek starts when spun up, at once if motor on or bit 3 set
      if( (manager==MNGR_PASTI||manager==MNGR_CAPS) && !(str&FDC_STR_MO) && !(io_src_b&CR_H))
        DiskEmu.VBLSoundcheck=true; // because we're not the masters of SU timing
      if(io_src_b&CR_STEPPING)
        DiskEmu.InterTrack=1;
      else if(io_src_b&CR_SEEK) // sound like step if small move
        DiskEmu.InterTrack=(BYTE)abs_quicki((int)tr-(int)dr);
      else // RESTORE - same
        DiskEmu.InterTrack=DiskEmu.track;
      // check direction as in OnUpdate() - we keep that part too because it's
      // a translation of the doc, here it's metadata for drive sound
      if(io_src_b&CR_STEPPING)
      {
        if((io_src_b&CR_STEPPING)==CR_STEP_IN) 
          DiskEmu.direction=true;
        else if((io_src_b&CR_STEPPING)==CR_STEP_OUT) 
          DiskEmu.direction=false; 
      }
      else if(io_src_b&CR_SEEK)
        DiskEmu.direction=(dr>tr);
      else // RESTORE
        DiskEmu.direction=false;
    }
#endif

    old_cr=cr;

#if defined(SSE_DEBUGGER_FAKE_IO) || defined(SSE_LOGSECTIONS_FOR_ALL)
#if defined(SSE_DEBUGGER_FAKE_IO)
    if(TRACE_MASK3 & TRACE_CONTROL_FDCREGS)
#elif defined(SSE_LOGSECTIONS_FOR_ALL)
    if(SSEConfig.TraceFile>=2)
#endif
    {
      if(manager==MNGR_WD1772 && (cr&0xF0)==0x90 && !(str&FDC_STR_RNF))
        TRACE_LOG("\n");
      BYTE drive_char= (psg_reg[PSGR_PORT_A]&6)==6? '?' : 'A'+DRIVE;
      TRACE_LOG("FDC %s CR $%02X %c:%d STR %X ",
                disk_manager[manager],io_src_b,drive_char,CURRENT_SIDE,str);
      DiskEmu.TraceStatus(Fdc.str);
      TRACE_LOG("TR %d CYL %d SR %d DR %d DMA $%X #%d PC $%X\n",
                tr,CURRENT_TRACK,sr,dr,dma_address,Dma.Counter,old_pc);
    }
    Dma.last_act=A_S_T;
#endif//#if defined(SSE_DEBUGGER_FAKE_IO) || defined(SSE_LOGSECTIONS_FOR_ALL)

    if(drive.SectorChecksum)
    {
      TRACE_LOG("%c: bootsector checksum $%X\n",'A'+DRIVE,drive.SectorChecksum); // $1234 means executable
#if defined(SSE_STATS)
      Stats.boot_checksum[DRIVE][CURRENT_SIDE]=drive.SectorChecksum;
#endif
    }
    drive.SectorChecksum=0;

#if defined(SSE_DISK_GHOST)
    Lines.CommandWasIntercepted=false; // reset this at each new whatever command 
    if(OPTION_GHOST_DISK)
    {
      // pasti/sps(caps)/scp or common disk read-only or archived
      if(drive.ImageType.Extension==EXT_STX||manager==MNGR_CAPS|| drive.ImageType.Extension==EXT_SCP
         ||(SSEOptions.GhostDiskRO && (FloppyDisk[DRIVE].ReadOnly||FloppyDisk[DRIVE].IsZip())))
      {
        GhostDisk[DRIVE].CheckCommand(io_src_b); // updates CommandWasIntercepted
      }
    }
#endif//#if defined(SSE_DISK_GHOST)

#if defined(SSE_DRIVE_SOUND)
    if(OPTION_DRIVE_SOUND)
    {
#if defined(SSE_DISK_GHOST)
      if(!Lines.CommandWasIntercepted) //would mess registers, and is instant
#endif
      {
        drive.old_track=DiskEmu.track; //record
        if(!DiskEmu.VBLSoundcheck && (manager==MNGR_PASTI||manager==MNGR_CAPS))
          drive.SoundCheckCommand(io_src_b);
        else
          drive.SoundCheckCommand(0xF0); // only motor start
      }
    }
#endif//#if defined(SSE_DRIVE_SOUND)

#if defined(SSE_DISK_GHOST)
    if((OPTION_GHOST_DISK && Fdc.Lines.CommandWasIntercepted))
    {}
    else
#endif

    if(drive.ImageType.Manager==MNGR_STEEM && !pasti_active)
      fdc_command(io_src_b); // for ST, MSA, DIM, STT

#if defined(SSE_DISK_STW)
    else if(drive.ImageType.Manager==MNGR_WD1772)
      WriteCR(io_src_b); // for STW, SCP, HFE
#endif

#if defined(SSE_TOS_PRG_AUTORUN)
    else if(drive.ImageType.Manager==MNGR_PRG) 
    {
      TRACE_LOG("PRG IRQ\n");
      fdc_irq=true; // hack to avoid long timeout
      update_disk_irq();
    }
#endif

    break;

  case 1: // Write TR

#if defined(SSE_DEBUGGER_FAKE_IO)
    if(TRACE_MASK3 & TRACE_CONTROL_FDCREGS)
      TRACE_LOG("FDC %s TR %d PC %X\n",disk_manager[DiskEmu.LastManager],io_src_b,old_pc);
#endif

    // original doc states "This register should not be loaded when the device 
    // is busy", not that it won't
    tr=io_src_b;
    break;

  case 2: // Write SR

#if defined(SSE_DEBUGGER_FAKE_IO)
    if(TRACE_MASK3 & TRACE_CONTROL_FDCREGS)
      TRACE_LOG("FDC %s SR %d PC %X\n",disk_manager[DiskEmu.LastManager],io_src_b,old_pc);
#endif

    // original doc states "This register should not be loaded when the device 
    // is busy", not that it won't
    sr=io_src_b;
    break;

  case 3: // Write DR

#if defined(SSE_DEBUGGER_FAKE_IO)
    if(TRACE_MASK3 & TRACE_CONTROL_FDCREGS)
      TRACE_LOG("FDC %s DR %d PC %X\n",disk_manager[DiskEmu.LastManager],io_src_b,old_pc);
#endif

    dr=io_src_b;
    break;
  }//sw

#if defined(SSE_DISK_CAPS)
  if(drive.ImageType.Manager==MNGR_CAPS)
#if defined(SSE_DISK_GHOST)
    if(OPTION_GHOST_DISK && Lines.CommandWasIntercepted)
    {
      TRACE_LOG("Ghost - Caps doesn't get command %x\n",io_src_b);
      Caps.fdc.r_command=cr; //update this...
    }
    else
#endif
      Caps.WriteWD1772(Line,io_src_b);
#endif//#if defined(SSE_DISK_CAPS)

}


BYTE TWD1772::CommandType(int command) {
  // return type I-IV of this FDC command or the current command
  if(command==-1) //default: current command
    command=cr;
  BYTE type;
  if(!(command&BIT_7))
    type=1; // Move head
  else if(!(command&BIT_6))
    type=2; // R/W sector(s)
  else if((command&0xF0)==0xD0) //1101xxxx
    type=4; // Interrupt
  else
    type=3; // R/W track
  return type;
}


/*  TWD1772 - STW
    This is yet another WD1772 emulation, specially written to handle
    STW disk images. 
    A goal from the start was to be able to use it for another format
    like SCP as well (v3.7.1), hence we work with a spinning drive and
    a flow of bits or bytes.
    Since v3.7.2, this emu is used for HFE disk image support too, so
    this "rewrite" proved much useful.
    We follow Western Digital flow charts, with some additions and (gasp!) 
    corrections. 
    Because we use Dma.Drq() for each byte, the agenda system is too gross.
    Each time an operation takes time, we set up an event that sends here.
    Otherwise we use recursion to hop to the next phase, it's better than
    goto.
    So it doesn't work with emulation cycles like CapsImg but with timed
    events like Pasti. Still not sure what's the best approach here.
    For a better support of SCP format, we integrated a "data separator"
    inspired by their competitor SPS/CAPS/Kryoflux, as well as a "DPLL"
    inspired by MESS.
*/



TWD1772IDField::TWD1772IDField() {
  memset(this,0,sizeof(TWD1772IDField));
}


WORD TWD1772IDField::nBytes() { // Sector length code to Sector length
  return (1<<((len&3)+7));
}


int TWD1772IDField::GetLen(int bytes) { // Sector length to Sector length code
  return MIN((bytes>>8),3);
}
  

#if defined(SSE_ENABLE_TRACE_LOG)

char* wd_phase_name[]={  // Coooool! note change if change enum !!!!!
  "WD_READY",
  "WD_TYPEI_SPINUP",
  "WD_TYPEI_SPUNUP", // spunup must be right after spinup
  "WD_TYPEI_SEEK",
  "WD_TYPEI_STEP_UPDATE",
  "WD_TYPEI_STEP",
  "WD_TYPEI_STEP_PULSE",
  "WD_TYPEI_CHECK_VERIFY",
  "WD_TYPEI_HEAD_SETTLE",
  "WD_TYPEI_FIND_ID",
  "WD_TYPEI_READ_ID", // read ID must be right after find ID
  "WD_TYPEI_TEST_ID", // test ID must be right after read ID
  "WD_TYPEII_SPINUP",
  "WD_TYPEII_SPUNUP", // spunup must be right after spinup
  "WD_TYPEII_HEAD_SETTLE", //head settle must be right after spunup
  "WD_TYPEII_FIND_ID",
  "WD_TYPEII_READ_ID", // read ID must be right after find ID
  "WD_TYPEII_TEST_ID", // test ID must be right after read ID
  "WD_TYPEII_FIND_DAM",
  "WD_TYPEII_READ_DATA",
  "WD_TYPEII_READ_CRC",
  "WD_TYPEII_CHECK_MULTIPLE",
  "WD_TYPEII_WRITE_DAM",
  "WD_TYPEII_WRITE_DATA",
  "WD_TYPEII_WRITE_CRC",
  "WD_TYPEIII_SPINUP",
  "WD_TYPEIII_SPUNUP", // spunup must be right after spinup
  "WD_TYPEIII_HEAD_SETTLE", //head settle must be right after spunup
  "WD_TYPEIII_IP_START", // start read/write
  "WD_TYPEIII_FIND_ID",
  "WD_TYPEIII_READ_ID", // read ID must be right after find ID
  "WD_TYPEIII_TEST_ID",
  "WD_TYPEIII_READ_DATA",
  "WD_TYPEIII_WRITE_DATA",
  "WD_TYPEIII_WRITE_DATA2", // CRC is 1 byte in RAM -> 2 bytes on disk
  "WD_TYPEIV_4", // $D4
  "WD_TYPEIV_8", // $D8
  "WD_MOTOR_OFF",
};


void TWD1772IDField::Trace(int logsection) {
  if(TRACE_LOG_ENABLED(logsection))
  {
    TRACE("ID T%d S%d N%d L%d CRC%02X%02X\n",track,side,num,len,CRC[0],CRC[1]);
  }
}

#endif

/*  MFM. Correct field must be filled in before calling a function:
    data -> Encode() -> clock and encoded word available 
    encoded word -> Decode() -> data & clock available 
    If mode is FORMAT_CLOCK, the clock byte will have a missing bit
    for bytes $A1 and $C2.
    The STW format could have been done without MFM encoding but it is
    necessary for the HFE format anyway.
*/

void TWD1772MFM::Decode() {
  WORD encoded_shift=encoded;
  data=clock=0; //BYTEs
  for(int i=0;i<8;i++)
  {
    clock|=((encoded_shift&0x8000)!=0);
    if(i<7)
      clock<<=1;
    encoded_shift<<=1;
    data|=((encoded_shift&0x8000)!=0);
    if(i<7)
      data<<=1,encoded_shift<<=1;
  }
}


void TWD1772MFM::Encode(int mode) {
  // 1. compute the clock
  clock=0;
  BYTE previous=data_last_bit;
  BYTE current;
  data_last_bit=data&BIT_0;
  BYTE data_shift=data;
  for(int i=0;i<8;i++)
  {
    current=data_shift&BIT_7;
    if(!previous && !current)
      clock|=1;
    if(i<7)
      clock<<=1;
    data_shift<<=1;
    previous=current;
  }
  if(mode==FORMAT_CLOCK)
  {
    if(data==0xA1) // -> $4489
      clock&=~4; // missing bit 2 of clock
    else if(data==0xC2) // -> $5224
      clock&=~2; // missing bit 1 of clock
  }
  // 2. mix clock & data to create a word
  data_shift=data;
  BYTE clock_shift=clock;
  encoded=0;
  for(int i=0;i<8;i++)
  {
    encoded|=((clock_shift&BIT_7)!=0);
    encoded<<=1; clock_shift<<=1;
    encoded|=((data_shift&BIT_7)!=0);
    if(i<7)
      encoded<<=1; 
    data_shift<<=1;
  }   
}


void TWD1772Crc::Reset() {
/*  Contrary to what the doc states, the CRC Register isn't preset to ones
    ($FFFF) prior to data being shifted through the circuit, but to $CDB4.
    This happens for each $A1 address mark (read or written), so the register
    value after $A1 is the same no matter how many address marks (Dragonflight).
*/
  crc=0xCDB4;
}


void TWD1772Crc::Add(BYTE data) {
  fdc_add_to_crc(crc,data);
}


bool TWD1772Crc::Check(TWD1772IDField* IDField) {
  bool ok=(IDField->CRC[0]==HIBYTE(crc) && IDField->CRC[1]==LOBYTE(crc));
  if(!ok)
  {
    TRACE_LOG2("CRC error - computed: %04X - read: %02X%02X\n",crc,IDField->CRC[0],IDField->CRC[1]);
  }
  return ok;
}


// reset am detector; read returns only on AM detected or clocks elapsed
void TWD1772AmDetector::Enable() {
  Enabled=true;
  nA1=0;
#if defined(SSE_WD1772_LL)
  aminfo|=(CAPSFDC_AI_AMDETENABLE|CAPSFDC_AI_CRCENABLE);
  aminfo&=~(CAPSFDC_AI_CRCACTIVE|CAPSFDC_AI_AMACTIVE);
  amisigmask=CAPSFDC_AI_DSRAM;
#endif
}

void TWD1772AmDetector::Reset() {
#if defined(SSE_WD1772_LL)
  amdatadelay=2;
  ammarkdist=ammarktype=amdataskip=0; 
  amdecode=aminfo=amisigmask=dsr=0; // dword
  dsrcnt=0; // int
#endif
  Enable();
}


// Data Request
void TWD1772::Drq(bool state) {
  Lines.drq=state;
  if(state)
    str|=FDC_STR_DRQ;
  else
    str&=~FDC_STR_DRQ;
  if(state)
    Dma.Drq(MNGR_WD1772);
}


// Interrupt request
void TWD1772::Irq(bool state) {
  Amd.Reset();
  Amd.Enabled=false;
  if(state && !Lines.irq) // so not on "force interrupt"
  {
    IndexCounter=10;
    prg_phase=WD_MOTOR_OFF; // will be changed for $D4
    str&=~FDC_STR_BSY;
    if(CommandType()==2 || CommandType()==3)
      str&=~FDC_STR_DRQ;
#if defined(SSE_MEGASTE)
    if(IS_MEGASTE && fdc_check_wrong_density())
      Fdc.str|=FDC_STR_SE;
#endif
     DiskEmu.Update(true);
#if defined(SSE_DRIVE_SOUND)
    if(OPTION_DRIVE_SOUND)
      FloppyDrive[DRIVE].SoundCheckIrq();
#endif
#if defined(SSE_ENABLE_TRACE_LOG)
    //ASSERT(DiskEmu.LastManager==MNGR_WD1772);
    if(TRACE_LOG_ENABLED(LOGSECTION_FDC))
      DiskEmu.TraceRegs();
#endif
  }
  Lines.irq=fdc_irq=state;
  update_disk_irq();
  // reset drive R/W flags
  FloppyDrive[DRIVE].reading=FloppyDrive[DRIVE].writing=false;
}


void TWD1772::Motor(bool state) {
  if(state!=(bool)FloppyDrive[DRIVE].bMotor)
  {
    TRACE_LOG2("WD motor %d\n",state);
  }
  Lines.bMotor=state;
  if(state)
  {
    str|=FDC_STR_MO;
    agenda_delete(agenda_fdc_motor_flag_off); // eg other "emu" using other drive! (Audio Sculpture)
  }
  else
    str&=~FDC_STR_MO;
  // only on currently selected drive, if any:
  if(Psg.CurrentDrive()!=TYM2149::NO_VALID_DRIVE)
    FloppyDrive[DRIVE].Motor(state); 
  else
  {
    TRACE_LOG2("WD motor %d: no drive\n",state);
  }
}


int TWD1772::MsToCycles(int ms) {
  // STE has a separate 8MHz quartz
  int c=(IS_STE) ? (ms*8000*TICKS8) : (CpuNormalHz*ms/1000);
  return c;
}


void TWD1772::NewCommand(BYTE command) {
  cr=command;
  // reset drive R/W flags
  FloppyDrive[DRIVE].reading=FloppyDrive[DRIVE].writing=false;
  WaitIP=WaitImage=false;
  int type=CommandType(command);
  current_time=A_S_T;
  switch(type) {
  case 1: // I
    // Set Busy, Reset CRC, Seek error, DRQ, INTRQ
    str|=FDC_STR_BSY;
    str&=~(FDC_STR_CRC|FDC_STR_SE |FDC_STR_WP);
    Drq(false); // this takes care of status bit
    if(InterruptCondition!=8)
      Irq(false);
    InterruptCondition=0;
    StatusType=1;
    // should we wait for spinup (H=0)?
    if(!(cr&CR_H) && !Lines.bMotor)
    {
      // Set MO wait 6 index pulses
      Motor(true); // this will create the event at each IP until motor off
      IndexCounter=6;
      prg_phase=WD_TYPEI_SPINUP;
      fdc_spinning_up=true;
    }
    else
    {
      // Doc doesn't state motor is started if it wasn't spinning yet and h=1
      // We assume.
      Motor(true); // but does it make sense?
      fdc_spinning_up=false;
      prg_phase=WD_TYPEI_SPUNUP;
      str|=FDC_STR_SU; // eg ST NICCC 2
      update_time=current_time+256*TICKS8; // 256 My Socks Are Weapons, Suretrip II
    }
    break;
  case 2: // II
    // Set Busy, Reset CRC, DRQ, LD, RNF, WP, Record Type
    str|=FDC_STR_BSY;
    str&=~(FDC_STR_LD|FDC_STR_RNF|FDC_STR_WP|FDC_STR_RT);
    Drq(false);
    if(InterruptCondition!=8)
      Irq(false);
    InterruptCondition=0;
    StatusType=0;
    if(!(cr&CR_H) && !Lines.bMotor)
    {
      Motor(true); 
      IndexCounter=6;
      prg_phase=WD_TYPEII_SPINUP;
      fdc_spinning_up=true;
    }
    else
    {
      Motor(true); 
      fdc_spinning_up=false;
      prg_phase=WD_TYPEII_SPUNUP;
      //current_time=A_S_T;
      OnUpdate();
    }
    break;
  case 3: // III
    str|=FDC_STR_BSY;
    str&=~(FDC_STR_LD|FDC_STR_RNF|FDC_STR_WP|FDC_STR_RT); // we add WP
    Drq(false);
    // we add this:
    if(InterruptCondition!=8)
      Irq(false);
    InterruptCondition=0;
    StatusType=0;

    // we treat the motor / H business as for type II, not as on flow chart
    if(!(cr&CR_H) && !Lines.bMotor)
    {
      Motor(true); 
      IndexCounter=6;
      prg_phase=WD_TYPEIII_SPINUP;
      fdc_spinning_up=true;
    }
    else
    {
      Motor(true); 
      prg_phase=WD_TYPEIII_SPUNUP;
      fdc_spinning_up=false;
      //current_time=A_S_T;
      OnUpdate();
    }
    break;
  case 4: // IV
    Motor(true); // also type IV triggers motor (?)
    if(str&FDC_STR_BSY)
      str&=~FDC_STR_BSY;
    else // read str is type I if FDC wasn't busy when interrupted (doc)
    {
      StatusType=1;
      str&=~(FDC_STR_CRC|FDC_STR_LD|FDC_STR_RT|FDC_STR_RNF);
    }

    if(cr&CR_I3) // immediate, D8
    {
      InterruptCondition=8;
      Irq(true); 
      prg_phase=WD_MOTOR_OFF;
      IndexCounter=10;
    }
    else if(cr&CR_I2) // each IP, D4
    {
      prg_phase=WD_TYPEIV_4;
      InterruptCondition=4;
      IndexCounter=1;
    }
    else // D0, just stop motor in 9 rev
    {
      // no IRQ!
      if(InterruptCondition!=8)
        Irq(false); // but could have to clear it (Wipe-Out)
      prg_phase=WD_MOTOR_OFF;
      IndexCounter=10;
      InterruptCondition=0;
    }
    break;
  }//sw
  prepare_next_event(); //395
}


/*  Drive calls this function at IP if it's selected.
    Whether the WD1772 is waiting for it or not. 
    The WD1772 doesn't know which drive (id) it is operating but we do!
*/
void TWD1772::OnIndexPulse(BYTE id) {
  IndexCounter--; // We set counter then decrement until 0
  if(F7_escaping||prg_phase==WD_TYPEII_WRITE_CRC) // ijor: WD1772 ignores IP when writing CRC
  {
    TRACE_LOG("See no IP! %d %d %X\n",F7_escaping,prg_phase,Mfm.encoded);
    return;
  }
  if(!WaitIP&&(FloppyDrive[id].reading||FloppyDrive[id].writing))
    WaitImage=true;
  WaitIP=false;
  if(FLOPPY_ACCESS_FF(id)) 
    floppy_access_ff_counter=FLOPPY_FF_VBL_COUNT;
  if(prg_phase>WD_READY && prg_phase<=WD_MOTOR_OFF) // anticrash
  {
    TRACE_LOG2("%c: IP #%d (%s) cr %X tr %d sr %d dr %d str %X\n",'A'+id,
      IndexCounter,wd_phase_name[prg_phase],cr,tr,sr,dr,str);
  }
  if(!IndexCounter)
  {
    current_time=time_of_next_event;
    switch(prg_phase) {
    case WD_TYPEI_SPINUP:
      str|=FDC_STR_SU;
      // no break
    case WD_TYPEII_SPINUP:
    case WD_TYPEIII_SPINUP:
      prg_phase++; // we assume next phase is spunup for this optimisation
      fdc_spinning_up=false;
      OnUpdate();
      break;

    case WD_TYPEI_FIND_ID:
    case WD_TYPEI_READ_ID:
    case WD_TYPEII_FIND_ID:
    //case WD_TYPEII_FIND_DAM://?
    case WD_TYPEII_READ_ID:
    case WD_TYPEIII_FIND_ID: // not in doc
    case WD_TYPEIII_READ_ID:
      TRACE_LOG2("Find ID timeout\n");
      str|=FDC_STR_SE; // = FDC_STR_RNF
      Irq(true);      
      break;

    case WD_TYPEIII_IP_START: 
      IndexCounter=1;
      n_format_bytes=0;
      if(cr&CR_TYPEIII_WRITE)
      {
        TRACE_LOG2("Format track %c:%d.%d (DMA %d sectors)\n",'A'+DRIVE,CURRENT_SIDE,CURRENT_TRACK,Dma.Counter);
        prg_phase=WD_TYPEIII_WRITE_DATA;
        F7_escaping=false;
        OnUpdate(); // hop
      }
      else
      {
        TRACE_LOG2("Read track %c:%d.%d  (DMA %d sectors)\n",'A'+DRIVE,CURRENT_SIDE,CURRENT_TRACK,Dma.Counter);
        prg_phase=WD_TYPEIII_READ_DATA;
        Amd.Reset();
        Read();
      }
      break;

    case WD_TYPEIII_WRITE_DATA:
    case WD_TYPEIII_WRITE_DATA2: //! 
#if defined(SSE_DISK_STW2)
      // format command completed: change # bytes
      if(FloppyDrive[DRIVE].ImageType.Extension==EXT_STW && ImageSTW[DRIVE].Version>=0x200)
      {
        ImageSTW[DRIVE].nTrackWords=FloppyDisk[DRIVE].TrackBytes=ImageSTW[DRIVE].Position;
        ImageSTW[DRIVE].nTrackBits=ImageSTW[DRIVE].nTrackWords*16;
        ImageSTW[DRIVE].SourceImage=EXT_STW;
      }
#endif
      // no break
    case WD_TYPEIII_READ_DATA:
      FloppyDrive[DRIVE].reading=FloppyDrive[DRIVE].writing=0; 
      TRACE_LOG2("%d bytes\n",DiskEmu.bytes); //stop list of sector nums
      Irq(true);
      break;

    case WD_TYPEIV_4: // $D4: raise IRQ at each IP until new command
      Irq(true);
      prg_phase=WD_TYPEIV_4;
      IndexCounter=1;
      TRACE_LOG2("%d IP for $D4 interrupt\n",IndexCounter);
      break;

    case WD_MOTOR_OFF:
      Motor(false);
#if defined(SSE_DISK_GHOST)
      Lines.CommandWasIntercepted=false;
#endif
      prg_phase=WD_READY;
      break;

    default: // drive is spinning, WD isn't counting
      OnUpdate(); //just in case... ???
      break; 
    }//sw
  
  }//if
  else
    OnUpdate(); // to trigger Read() or Write() if needed: Delirious 3
}


#if defined(SSE_DISK_SCP) 
// helper function
// reset rev if we're on last rev and not reading

void check_scp_rev(BYTE drive) {
  if(FloppyDrive[drive].ImageType.Extension==EXT_SCP && ImageSCP[drive].rev 
    && ImageSCP[drive].rev==(ImageSCP[drive].file_header.IFF_NUMREVS-1))
  {
    ImageSCP[drive].LoadTrack(CURRENT_SIDE,CURRENT_TRACK);
  }
}

#endif


/*  Core of the lower-level WD1772 emulation. It can work with bytes (STW, HFE)
    or with bits (SCP). It was originally based on the datasheet and started its
    life in Steem SSE in v3.7.0 as working but buggy, it was improved thanks to
    some bug reports.
    It isn't 100%, for example writing the CR register while busy isn't
    correctly emulated.  */

void TWD1772::OnUpdate() {

  BYTE &floppyno=DRIVE;
  TSF314 &drive=FloppyDrive[floppyno];
  BYTE &track=drive.track;
  TFloppyDisk &disk=FloppyDisk[floppyno];

  update_time=current_time+nSysCyclesPerSecond; // put in the future
  if(drive.ImageType.Manager!=MNGR_WD1772)
    return;

  if(WaitIP && CommandType()!=4 && ADAT)
  {
    // wait for IP, generally, block everything! Maybe it's too radical
    // Chimera 79.8, Jupiter's Masterdrive
    TRACE_LOG2("wait IP\n");
    return;
  }

  switch(prg_phase)  {
    
  case WD_TYPEI_SPUNUP:
    // we come here after 6 IP or directly
#if defined(SSE_DRIVE_SOUND)
    drive.SoundCheckCommand(cr);
#endif
    if(cr&CR_STEPPING) // is command a step, step-in, step-out?
    {
      // if step-in or step-out, update DIRC line
      if((cr&CR_STEPPING)==CR_STEP_IN) 
        Lines.direction=true;
      else if((cr&CR_STEPPING)==CR_STEP_OUT) 
        Lines.direction=false; 
      // goto B or C according to flag u
      prg_phase=(cr&CR_U) ? WD_TYPEI_STEP_UPDATE : WD_TYPEI_STEP;
      OnUpdate();
    }
    else  // else it's seek/restore
    {
      prg_phase=WD_TYPEI_SEEK; // goto 'A'
      if((cr&CR_SEEK)==CR_RESTORE) // restore?
      {
        tr=0xFF;
        Lines.track0=(track==0 && floppyno<DiskMan.nFloppyDrives);//?
        if(Lines.track0)
          tr=0;
        dr=0;
      }    
      OnUpdate(); // some recursion is always cool   
    }
    break;

  case WD_TYPEI_SEEK: // 'A'
    dsr=dr;
    if(tr==dsr)
      prg_phase=WD_TYPEI_CHECK_VERIFY;
    else
    {
      Lines.direction=(dsr>tr);
      prg_phase=WD_TYPEI_STEP_UPDATE;
    }
    OnUpdate(); // some recursion is always cool
    break;

  case WD_TYPEI_STEP_UPDATE: // 'B'
    if(Lines.direction)
      tr++;
    else
      tr--;
    prg_phase=WD_TYPEI_STEP;
    OnUpdate(); // some recursion is always cool
    break;

  case WD_TYPEI_STEP: // 'C'
    Lines.track0=(track==0 && floppyno<DiskMan.nFloppyDrives);
    if(Lines.track0 && !Lines.direction)
    {
      tr=0;
      prg_phase=WD_TYPEI_CHECK_VERIFY;
      OnUpdate(); // some recursion is always cool
    }
    else
    {
      StepPulse();
/*
Delay according to r1,r0 field

Command      Bit 7     B6     B5     B4     B3     B2     B1     Bit 0
--------     -----     --     --     --     --     --     --     -----
Restore      0         0      0      0      h      V      r1     r0
Seek         0         0      0      1      h      V      r1     r0
Step         0         0      1      u      h      V      r1     r0
Step in      0         1      0      u      h      V      r1     r0
Step out     0         1      1      u      h      V      r1     r0

r1       r0            1772
--       --            ----
0        0             6000 cycles (?)
0        1             12000 cycles
1        0             2000 cycles
1        1             3000 cycles
*/
      switch(cr&(CR_R1|CR_R0)) {
      case 0:
        update_time=MsToCycles(6);
        break;
      case 1:
        update_time=MsToCycles(12);
        break;
      case 2:
        update_time=MsToCycles(2);
        break;
      case 3:
        update_time=MsToCycles(3);
        break;
      }//sw
      update_time+=current_time;
      prg_phase=WD_TYPEI_STEP_PULSE;
    }
    break;

  case WD_TYPEI_STEP_PULSE:
    // goto 'D' if command is step, 'A' otherwise
    prg_phase=(cr&CR_STEPPING) ? WD_TYPEI_CHECK_VERIFY : WD_TYPEI_SEEK;
    OnUpdate(); // some recursion is always cool
    break;

  case WD_TYPEI_CHECK_VERIFY: // 'D'
    // update str bit 2 (reflects status of the TR00 signal)
    if(Lines.track0)
      str|=FDC_STR_T0;
    else
      str&=~FDC_STR_T0;

    if(cr&CR_V)
    {
      if(drive.ImageType.Manager==MNGR_WD1772 && drive.bDiskInDrive)
      {
        drive.MfmManager->LoadTrack(CURRENT_SIDE,CURRENT_TRACK);
        prg_phase=WD_TYPEI_HEAD_SETTLE; 
        update_time=current_time + MsToCycles(15);
      }
      else if(!ADAT) // like Steem native, not really correct
      {              // but we come fast to GEM
        str|=FDC_STR_SE;
        Irq(true);
      }
    }
    else
      Irq(true); // this updates status bits
    break;

  case WD_TYPEI_HEAD_SETTLE:
    // flow chart is missing head settling
    prg_phase=WD_TYPEI_FIND_ID;
    Amd.Reset();
    n_format_bytes=0;
    Read(); // drive will send word (clock, byte) and set event
    IndexCounter=6; 
    break;

  case WD_TYPEI_FIND_ID:
  case WD_TYPEII_FIND_ID:
  case WD_TYPEIII_FIND_ID:
    CrcLogic.Add(dsr);
#if defined(SSE_WD1772_LL)
    // wait for AM
    if(Amd.aminfo & CAPSFDC_AI_DSRAM)
    {
      // AM detected, read returns on dsr ready
      Amd.amisigmask=CAPSFDC_AI_DSRREADY;
      Amd.nA1=3;
      CrcLogic.Reset(); 
      Amd.Enabled=false; // read IDs OK
    }
    else
#endif
      if(Amd.Enabled && (dsr==0xA1 && !(Mfm.clock&BIT_2))
#if defined(SSE_WD1772_LL) 
      || (Amd.aminfo&CAPSFDC_AI_DSRMA1)
#endif
      )
    {
      Amd.nA1++;
      CrcLogic.Reset(); // only special $A1 resets the CRC logic
    }
    // note: it is strictly 3 A1 syncs
    else if((dsr&0xFF)>=0xFC && Amd.nA1==3) // CAPS: $FC->$FF
    {
      TRACE_LOG2("%X found at %d\n",dsr,FloppyDisk[DRIVE].current_byte);
      n_format_bytes=0; //reset
      prg_phase++; // in type I or type II or III
      Amd.Enabled=false; // read IDs OK
#if defined(SSE_WD1772_LL)
      Amd.amisigmask=CAPSFDC_AI_DSRREADY;
#endif
    }
    else if(Amd.nA1)
      Amd.Reset(); //?
    Read(); // this sets up next event
    break;

  case WD_TYPEI_READ_ID:
  case WD_TYPEII_READ_ID:
  case WD_TYPEIII_READ_ID:
    // fill in ID field
    *(((BYTE*)&IDField)+n_format_bytes)=dsr; //no padding!!!!!!
    if(n_format_bytes<4)
      CrcLogic.Add(dsr);
    if(prg_phase==WD_TYPEIII_READ_ID)
    {
      dr=dsr;
      Drq(true); // read address
    }
    n_format_bytes++;
    if(n_format_bytes==sizeof(TWD1772IDField))
    {
      n_format_bytes=0; 
      prg_phase++; // in type I, II, III
#if defined(SSE_ENABLE_TRACE_LOG)      
      if(TRACE_LOG2_ENABLED)
      {
        TRACE_LOG("At %d:",disk.current_byte); // position
        IDField.Trace(LOGSECTION_FDC);
      }
#endif
      OnUpdate(); // some recursion is always cool
    }
    else
      Read();
    break;

  case WD_TYPEI_TEST_ID:
    //test track and CRC
    if(IDField.track==dr && CrcLogic.Check(&IDField))
    {
      CrcLogic.Reset();
      str&=~FDC_STR_CRC; // WD1772 doc: reset CRC bit
      Irq(true); // verify OK
    }
    else // they should all have correct track, will probably time out
    {
      prg_phase=WD_TYPEI_FIND_ID;
#if defined(SSE_DISK_SCP)
      check_scp_rev(floppyno);
#endif
      if(IDField.track==dr)
        str|=FDC_STR_CRC; // set CRC error if track field was OK
      CrcLogic.Add(dsr); //unimportant
      Amd.Enabled=true; 
      Read(); // this sets up next event
    }
    break;

  case WD_TYPEII_SPUNUP:
  case WD_TYPEIII_SPUNUP:
    prg_phase++;
    if(cr&CR_E) // head settle delay programmed
      update_time=current_time + MsToCycles(15);
    else
      OnUpdate(); // some recursion is always cool
    break;

  case WD_TYPEII_HEAD_SETTLE: // we come directly or after 15ms delay
    // check Write Protect for command write sector
    // note: Lines.write_protect is undefined!
    if((cr&CR_TYPEII_WRITE) && disk.WriteProtect)
    {
      TRACE_LOG2("Can't write on disk\n");
      str|=FDC_STR_WP;
      Irq(true);
    }
    else // read, or write OK
    {
      IndexCounter=5; 
      prg_phase=WD_TYPEII_FIND_ID; // goto '1'
      Amd.Reset();
      n_format_bytes=0;
      Read();
    }
    break;

  case WD_TYPEII_TEST_ID:
    if(IDField.track==tr && IDField.num==sr)
    {
      ByteCount=IDField.nBytes();
      if(CrcLogic.Check(&IDField))
      {
        CrcLogic.Reset();
        str&=~FDC_STR_CRC;
        prg_phase=(cr&CR_TYPEII_WRITE) ? WD_TYPEII_WRITE_DAM : WD_TYPEII_FIND_DAM;
      }
      else
      {
        str|=FDC_STR_CRC;
        CrcLogic.Add(dsr);
        prg_phase=WD_TYPEII_FIND_ID; 
      }
    }
    else // it's no error (yet), the WD1772 must browse the IDs
    {
      prg_phase=WD_TYPEII_FIND_ID;
#if defined(SSE_DISK_SCP)
      check_scp_rev(floppyno);
#endif
    }
    Amd.Reset();
    Read();
    break;

  case WD_TYPEII_FIND_DAM:
    CrcLogic.Add(dsr); //before eventual reset
    n_format_bytes++;
    if(n_format_bytes<27)
    {} // CAPS: first bytes aren't even read
    else if(n_format_bytes==27)
    {
      TRACE_MFM("Enable AMD\n");
      Amd.Enable();
#if defined(SSE_WD1772_LL)
      Amd.amisigmask=CAPSFDC_AI_DSRREADY|CAPSFDC_AI_DSRMA1;
#endif
    }
    else if(n_format_bytes==44+ Amd.nA1) //timed out
    {
      TRACE_LOG2("DAM time out %d in at %d\n",n_format_bytes,disk.current_byte);
      n_format_bytes=0;
      prg_phase=WD_TYPEII_FIND_ID;
      Amd.Enable();
    }
#if defined(SSE_WD1772_LL)
    //if not A1 mark, restart
    else if((Amd.aminfo & CAPSFDC_AI_DSRAM) && !(Amd.aminfo & CAPSFDC_AI_DSRMA1))
    {
      Amd.Enable();
      n_format_bytes=0;
    }
    // wait for AM
    else if ((Amd.aminfo & CAPSFDC_AI_DSRAM))
    {
      TRACE_LOG2("AM found at byte %d (%d in), reset CRC\n",dsr,disk.current_byte,n_format_bytes);
      // AM detected, read returns on dsr ready
      Amd.amisigmask=CAPSFDC_AI_DSRREADY;
      CrcLogic.Reset();
      Amd.nA1=3;
    }
#endif
    else if(dsr==0xA1 && !(Mfm.clock&BIT_2) //stw
#if defined(SSE_WD1772_LL)
      || (Amd.aminfo&CAPSFDC_AI_DSRMA1)
#endif
      ) 
    {
      TRACE_LOG2("%X found at byte %d, reset CRC\n",dsr,disk.current_byte);
      CrcLogic.Reset();
      Amd.nA1++; 
    }
    else if(Amd.nA1==3 && ((dsr&0xFE)==0xF8||(dsr&0xFE)==0xFA)) // DAM found
    {
      TRACE_LOG2("TR%d SR%d %X found at byte %d (%d after ID)\n",tr,sr,dsr,disk.current_byte,n_format_bytes);
      n_format_bytes=0; // for CRC later
      Amd.Enabled=false;
      prg_phase=WD_TYPEII_READ_DATA;
      if((dsr&0xFE)==0xF8)
        str|=FDC_STR_RT; // "record type" set when "deleted data" DAM
    }
    else if(Amd.nA1==3) // address mark but then no FB...
    {
      TRACE_LOG2("%x found after AM: keep looking\n",dsr);
      Amd.Enable();
    }
    Read();    
    break;

  case WD_TYPEII_READ_DATA:
    CrcLogic.Add(dsr);
    dr=dsr;
    Drq(true); // DMA never fails to take the byte
    ByteCount--;
    if(!ByteCount)
      prg_phase=WD_TYPEII_READ_CRC;
    Read();
    break;

  case WD_TYPEII_READ_CRC:
    IDField.CRC[n_format_bytes]=dsr; // and we don't add to CRC
    if(n_format_bytes) //1
    {
      if(!CrcLogic.Check(&IDField))
      {
        TRACE_LOG2("Read sector %c:%d-%d-%d CRC error\n",'A'+floppyno,CURRENT_SIDE,IDField.track,IDField.num);
        str|=FDC_STR_CRC;
        Irq(true);
      }
      else
      {
        TRACE_LOG2("Read sector %c:%d-%d-%d OK CRC %02X%02X\n",'A'+floppyno,CURRENT_SIDE,IDField.track,IDField.num,IDField.CRC[0],IDField.CRC[1]);
        prg_phase=WD_TYPEII_CHECK_MULTIPLE;
        OnUpdate(); // some recursion is always cool
      }
    }
    else
    {
      n_format_bytes++;
      Read(); // next CRC byte
    }
    break;

  case WD_TYPEII_CHECK_MULTIPLE:
#if defined(SSE_DISK_SCP)
    check_scp_rev(floppyno);
#endif
    if(cr&CR_M)
    {
      drive.writing=false;
      sr++;
      if(!sr)
        sr++; // CAPS: 255 wraps to 1 (due to add with carry)
      prg_phase=WD_TYPEII_HEAD_SETTLE; // goto '4'
      OnUpdate();
    }
    else
      Irq(true);
    break;

  case WD_TYPEII_WRITE_DAM:
    n_format_bytes++;
    if(n_format_bytes<23-1) //22 or 23?
      Read();    
    else if(n_format_bytes<23-1+12) // write 12 $0
    {
      Lines.write=Lines.write_gate=true; // those lines don't matter for now
      Mfm.data=0;
      Mfm.Encode(); 
      CrcLogic.Add(Mfm.data); // shouldn't matter
      Write();
    }
    else if(n_format_bytes<23-1+12+3) // write 3x $A1 (missing in flow chart)
    {
      Mfm.data=0xA1;
      Mfm.Encode(TWD1772MFM::FORMAT_CLOCK); 
      CrcLogic.Add(Mfm.data); // before reset   
      CrcLogic.Reset();
      Write();
    }
    else if(n_format_bytes==23-1+12+3) // write DAM acording to A0 field
    {
      //ASSERT(!(cr&CR_A0)); //Amateur versions (>1.50?) of ProCopy use $A1 to copy
      Mfm.data= (cr&CR_A0) ? 0xF9 : 0xFB;
      Mfm.Encode(); 
      TRACE_LOG2("tr %d sr %d write %X at byte %d, %d in\n",tr,sr,Mfm.data,disk.current_byte,n_format_bytes);
      CrcLogic.Add(Mfm.data); // after eventual reset (TODO)        
      Write();     
    }
    else
    {
      n_format_bytes=0;
      prg_phase=WD_TYPEII_WRITE_DATA;
      OnUpdate(); // some recursion is always cool  
    }
    break;

  case WD_TYPEII_WRITE_DATA:
    Drq(true); // normally first DRQ happened much earlier, we simplify
    dsr=dr;
    CrcLogic.Add(dsr);
    Mfm.data=dsr;
    Mfm.Encode();
    ByteCount--;
    if(!ByteCount)
      prg_phase=WD_TYPEII_WRITE_CRC;
    Write();
    break;

  case WD_TYPEII_WRITE_CRC: // CRC + final $FF (?)
    n_format_bytes++;
    if(n_format_bytes==1)
      Mfm.data=HIBYTE(CrcLogic.crc);
    else if(n_format_bytes==2)
      Mfm.data=LOBYTE(CrcLogic.crc);
    else
    {
      n_format_bytes=0;
      prg_phase=WD_TYPEII_CHECK_MULTIPLE;
      Mfm.data=0xFF;
      Lines.write=Lines.write_gate=false; //early - those lines don't matter for now
    }
    Mfm.Encode();
    Write();
    break;

  case WD_TYPEIII_HEAD_SETTLE: // we come directly or after 15ms delay
    Amd.Reset();
#if defined(SSE_WD1772_LL)
    Amd.aminfo&=~CAPSFDC_AI_CRCENABLE;
    Amd.amisigmask=CAPSFDC_AI_DSRREADY;
#endif
    DiskEmu.bytes=0;
    if((cr&0xF0)==CR_TYPEIII_READ_ADDRESS)
    {
      IndexCounter=5; //not documented, see OnIndexPulse()
      prg_phase=WD_TYPEIII_FIND_ID;
      n_format_bytes=0;
      Read();
    }
    // check Write Protect for command write track
     // Lines.write_protect is undefined!
    else if((cr&CR_TYPEIII_WRITE) && disk.WriteProtect)
    {
      TRACE_LOG2("Can't write on disk\n");
      str|=FDC_STR_WP;
      Irq(true);
    }
    else // for read & write track, we start at next IP
    {
      IndexCounter=1;
  //    TRACE_LOG2("%d IP for read or write track\n",IndexCounter);
      prg_phase=WD_TYPEIII_IP_START;
      //ASSERT(drive.motor);
    }
    break;

  case WD_TYPEIII_TEST_ID:
    if(!CrcLogic.Check(&IDField))
      str|=FDC_STR_CRC;
    else
      str&=~FDC_STR_CRC; // guess so
    sr=IDField.track;
    Irq(true);
    break;

  case WD_TYPEIII_READ_DATA:
      if(dsr==0xA1 && !(Mfm.clock&BIT_2)
#if defined(SSE_WD1772_LL)
      || (Amd.aminfo&CAPSFDC_AI_DSRMA1)
#endif
      )
    {
      if(!IMAGE_LL_MFM && CrcLogic.crc!=0xCDB4)
        dsr=0x14; // CAPS-like code produces the $14, it's quite intricate
      CrcLogic.Reset();
    }
    else
      CrcLogic.Add(dsr);
    dr=dsr;
    DiskEmu.bytes++;
    Drq(true);
    Read();
    break;

  case WD_TYPEIII_WRITE_DATA:
    // The most interesting part of STW support, and novelty in ST emulation!
    Drq(true);
    if((dr&0xFE)==0xFE
#if defined(SSE_DEBUGGER_FAKE_IO) 
      && !(TRACE_MASK3&TRACE_CONTROL_FDCMFM)
#endif
      && !SHOULD_DUMP_BYTES)
      n_format_bytes=4; // so we'll trace all written IDs
    // analyse byte in for MFM markers
    if(dr==0xF5 && !F7_escaping) //Write A1 in MFM with missing clock Init CRC
    {
      Mfm.data=dsr=0xA1;
      Mfm.Encode(TWD1772MFM::FORMAT_CLOCK);
      CrcLogic.Reset();
      Write();
      if(n_format_bytes)
      {
        TRACE_LOG2("$%x-",dsr);
        n_format_bytes--;
      }
    }
    else if(dr==0xF6 && !F7_escaping) //Write C2 in MFM with missing clock
    {
      Mfm.data=dsr=0xC2;
      Mfm.Encode(TWD1772MFM::FORMAT_CLOCK);
      CrcLogic.Add(Mfm.data);
      Write();
      if(n_format_bytes)
      {
        TRACE_LOG2("$%x-",dsr);
        n_format_bytes--;
      }
    }
/*  The format code $F7 may be used inside an ID field. The CRC bytes are added
    to the CRC, so that this is correct. This implies that at the receipt of
    $F7, the WD1772 saves the current value of the CRC (at least the lower 
    byte), before it is modified by the upper byte.
    A byte following F7 isn't interpreted as a format byte, it can be considered
    as an escape character, dubious use but complicates emulation (DrCoolZic).*/
    else if(dr==0xF7 && !F7_escaping) //Write 2 CRC Bytes
    {
      Mfm.data=dsr=HIBYTE(CrcLogic.crc); // write 1st byte
      dr=LOBYTE(CrcLogic.crc); // save 2nd byte
      CrcLogic.Add(Mfm.data);
      Mfm.Encode();
      F7_escaping=true;
      Write();
      if(n_format_bytes)
      {
        TRACE_LOG2("%d-",dsr);
        n_format_bytes--;
      }
      prg_phase=WD_TYPEIII_WRITE_DATA2; // for 2nd byte
    }
    else // other bytes ($0, $E5...)
    {
      Mfm.data=dsr=dr;
      Mfm.Encode();
      CrcLogic.Add(dsr);
      F7_escaping=false;
      Write();
      if(n_format_bytes&&dr!=0xFE)
      {
#ifdef DEBUG_BUILD
        TRACE_LOG2("%d ",dsr);
        if(n_format_bytes==1)
          TRACE_LOG2("\n");
#endif
        n_format_bytes--;
      }
    }
    break;

  case WD_TYPEIII_WRITE_DATA2:
    // write 2nd byte of CRC
    Mfm.data=dsr=dr; // as saved
    CrcLogic.Add(Mfm.data);
    Mfm.Encode();
    Write(); 
    if(n_format_bytes)
    {
      TRACE_LOG2("%d-",dsr);
      n_format_bytes--;
    }
    prg_phase=WD_TYPEIII_WRITE_DATA; // go back
    break;

  default:
    update_time=current_time+nSysCyclesPerSecond; 
   break;
  }//sw
  //ASSERT(prg_phase!=WD_TYPEII_HEAD_SETTLE);
  //prepare_next_event();  
}


void TWD1772::Read() {
  if(Psg.CurrentDrive()!=TYM2149::NO_VALID_DRIVE)
  {
    FloppyDrive[DRIVE].Read(); // this gets data and creates event for next byte
    Mfm.Decode();
    if(!IMAGE_LL_MFM) // dsr shouldn't be messed with...
      dsr=Mfm.data;
#ifdef SSE_DEBUG // for SCP it's not perfectly aligned
    TRACE_MFM("%s #%d MFM %04X c $%02X d $%02X\n",wd_phase_name[prg_phase],FloppyDisk[DRIVE].current_byte,Mfm.encoded,Mfm.clock,dsr);
#endif
  }
  else  
    update_time=current_time+nSysCyclesPerSecond;
}


void TWD1772::StepPulse() {
/*
  // useless now, normally it lasts some us, but we're not going to
  // set up events for that
  Lines.step=true; 
  Lines.step=false;
*/
  if(Psg.CurrentDrive()!=TYM2149::NO_VALID_DRIVE)
    FloppyDrive[DRIVE].Step(Lines.direction);
}


void TWD1772::Write() {
/*  Data must be MFM-encoded before.
    We don't do it here because we don't know if we code for special
    format bytes or not.
*/
#ifdef SSE_DEBUG
  TRACE_MFM("%s #%d MFM %04X c $%02X d $%02X\n",wd_phase_name[prg_phase],FloppyDisk[DRIVE].current_byte,Mfm.encoded,Mfm.clock,Mfm.data);
  //ASSERT(Psg.CurrentDrive()!=TYM2149::NO_VALID_DRIVE);
#endif
#if defined(SSE_DISK_STW2)
  if(FloppyDrive[DRIVE].ImageType.RealExtension==EXT_STW) // the WD1772 writes right...
  {
    ImageSTW[DRIVE].CurrentFuzzy=TImageSTW2::DEFAULT_FUZZY; // ...no fuzzy bits
    ImageSTW[DRIVE].CurrentTiming=TImageSTW2::DEFAULT_TIMING; // ...no strange timings
  }
#endif
  if(Psg.CurrentDrive()!=TYM2149::NO_VALID_DRIVE)
    FloppyDrive[DRIVE].Write(); // this writes data and creates event  
  else  
    update_time=current_time+nSysCyclesPerSecond;
}


// TWD1772IO -> WriteCR when manager=MNGR_WD1772


void TWD1772::WriteCR(BYTE const io_src_b) {
  TSF314 &drive=FloppyDrive[DRIVE];
  BYTE NewCommandType=CommandType(io_src_b);
  if(NewCommandType==2 || NewCommandType==3) //or no condition?
  {
    if(drive.ImageType.Manager==MNGR_WD1772 && drive.bDiskInDrive)
      drive.MfmManager->LoadTrack(CURRENT_SIDE,CURRENT_TRACK);
  }
  if(!(str&FDC_STR_BSY)
    ||(io_src_b&0xF0)==0xD0
    || !OPTION_HACKS || fdc_spinning_up
    || CommandType()==1 && NewCommandType==1) // Awesome
  {
    NewCommand(io_src_b); // one more function, more readable
  }
  else
  {
    TRACE_LOG2("FDC command %X ignored\n",io_src_b);
    //cr=io_src_b;//should...
  }
}


#if defined(SSE_WD1772_LL)

/*  This is the correct algorithm for the WD1772 DPLL (digital phase-locked 
    loop) system, as described in patent US 4808884 A.
    It allows us to read low-level (flux level) disk images such as SCP.
    Thx to Olivier Galibert for some inspiration, otherwise the code
    would be a real MESS.
*/

int TWD1772Dpll::GetNextBit(COUNTER_VAR &tm, int drive) {

  int aa=0;

  WORD timing_in_us=0; // us = microsecond

  while(ctime-latest_transition>=0)
  {
#if defined(SSE_DISK_STW2) // made the function virtual
    ASSERTB(FloppyDrive[drive].MfmManager);
    if(FloppyDrive[drive].MfmManager)
      aa=FloppyDrive[drive].MfmManager->GetNextTransition(timing_in_us); // cycles to next 1
    if(drive<2)
#else
#if defined(SSE_DISK_SCP) // add formats here ;)
    aa=ImageSCP[drive].GetNextTransition(timing_in_us); // cycles to next 1
#endif
#endif
    {
      TRACE_MFM("(%d)",timing_in_us);
    }
    if(OPTION_HACKS && timing_in_us>12) // Powerdrome 0-79-10
    {
      short rnd=(rand()%5)+1-3; // -2 +2us
      TRACE_MFM("(NFA%d)",rnd);
      aa+=rnd*8;
    }
    latest_transition+=aa;
  }
  COUNTER_VAR when=latest_transition;

  for(;;) {
    COUNTER_VAR etime = ctime+delays[slot];
    if(transition_time == 0xffff && etime-when >= 0)
      transition_time = counter;

    if(slot < 8) { //SS I don't understand this, why only <8?
      BYTE mask = 1 << slot;
      if(phase_add & mask)
        counter += 226;
      else if(phase_sub & mask)
        counter += 30;
      else
        counter += increment;

      if((freq_add & mask) && increment < 140)
        increment++;
      else if((freq_sub & mask) && increment > 117)
        increment--;
    } else
      counter += increment;

    slot++;
    tm = etime;
    if(counter & 0x800)
      break;
  }

  int bit = transition_time != 0xffff;
  
  if(bit) {
    static const BYTE pha[8] = { 0xf, 0x7, 0x3, 0x1, 0, 0, 0, 0 };
    static const BYTE phs[8] = { 0, 0, 0, 0, 0x1, 0x3, 0x7, 0xf };
    static const BYTE freqa[4][8] = {
      { 0xf, 0x7, 0x3, 0x1, 0, 0, 0, 0 },
      { 0x7, 0x3, 0x1, 0, 0, 0, 0, 0 },
      { 0x7, 0x3, 0x1, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    static const BYTE freqs[4][8] = {
      { 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0x1, 0x3, 0x7 },
      { 0, 0, 0, 0, 0, 0x1, 0x3, 0x7 },
      { 0, 0, 0, 0, 0x1, 0x3, 0x7, 0xf },
    };

    int cslot = transition_time >> 8;
    //ASSERT( cslot<8 );
    phase_add = pha[cslot];
    phase_sub = phs[cslot];
    int way = transition_time & 0x400 ? 1 : 0;
    if(history & 0x80)
      history = way ? 0x80 : 0x83;
    else if(history & 0x40)
      history = way ? history & 2 : (history & 2) | 1;
    freq_add = freqa[history & 3][cslot];
    freq_sub = freqs[history & 3][cslot];
    history = way ? (history >> 1) | 2 : history >> 1;
  } else
    phase_add = phase_sub = freq_add = freq_sub = 0;

  counter &= 0x7ff;
  ctime = tm;
  transition_time = 0xffff;
  slot = 0;
  //ASSERT( bit==0 || bit==1 );
  return bit;
}


void TWD1772Dpll::Reset(COUNTER_VAR const when) {
  counter = 0;
  increment = 128;
  transition_time = 0xffff;
  history = 0x80;
  slot = 0;
  latest_transition= ctime = when;
  phase_add = 0x00;
  phase_sub = 0x00;
  freq_add  = 0x00;
  freq_sub  = 0x00;
  write_position = 0;
  write_start_time = -1;
  SetClock(1); // clock WD1772 = clock CPU, 16 cycles = 2 microseconds
}


void TWD1772Dpll::SetClock(const int &period) {
  for(int i=0; i<42; i++)
    delays[i] = period*(i+1);
}


/*  This is the correct algorithm for the WD1772 data separator.
    It interprets the bit flow from disk images such as SCP, coming
    from the DPLL.
    
    Fluxes -> DPLL -> data separator -> dsr

    Thx to Istvan Fabian for some inspiration otherwise Steem would have lower
    CAPS to read disk images (like those that use the $C2 sync mark).
    Note: my comments in this function marked by SS
*/

bool TWD1772::ShiftBit(int bit) {

  bool byte_ready=false;

  // shift read bit into am decoder
  DWORD amdecode=Amd.amdecode<<1;
  if (bit)
    amdecode|=1;
  Amd.amdecode=amdecode;

  // get am info, clear AM found, A1 and C2 mark signals
  DWORD aminfo=Amd.aminfo & ~(CAPSFDC_AI_AMFOUND|CAPSFDC_AI_MARKA1|CAPSFDC_AI_MARKC2);

  // bitcell distance of last mark detected
  if (Amd.ammarkdist)
    Amd.ammarkdist--;

  // am detector if enabled
  if(Amd.Enabled) {
    //if (aminfo & CAPSFDC_AI_AMDETENABLE) { //SS TODO
    //ASSERT(aminfo & CAPSFDC_AI_AMDETENABLE); //SS asserts...
    // not a mark in shifter/decoder
    BYTE amt=0;

    // check if shifter/decoder has a mark
    // the real hardware probably has two shifters (clocked and fed by data separator) connected to a decoder
    // each bit of the shifter/decoder is for shifter0#0, shifter1#0...shifter0#7, shifter1#7, 
    // only 2 comparisons is needed per cell instead of 8 (A1/0A, C2/14 and 0A/A1, 14/C2)
    switch (amdecode  & 0xffff) {
      // A1 mark only enabled if not overlapped with another A1
      case 0x4489:
        if (!Amd.ammarkdist || Amd.ammarktype!=1)
          amt=1;
        break;

        // C2 mark always enabled
      case 0x5224:
        amt=2;
        break;
    }

    // process mark if found
    if (amt) {
      // we just read the last data bit of a mark, delay by a clock bit to read from decoder#1
      Amd.amdatadelay=1;

      // if overlapped with a different mark
      if (Amd.ammarkdist && Amd.ammarktype!=amt) {
        // dsr value is invalid
        Amd.amdataskip++;

        // delay by an additional data bit (data, clock)
        Amd.amdatadelay+=2;
      }

      // if dsr is empty dsr shouldn't be flushed, dsr value is invalid
      if (!Amd.dsrcnt)
        Amd.amdataskip++;

      // force dsr to flush its current value, since data values start from next data bit
      Amd.dsrcnt=7;

      // 16 bitcells must be read before next mark, otherwise marks are overlapped (8 clock+data bits)
      Amd.ammarkdist=16;

      // save last mark type; only used when marks overlap
      Amd.ammarktype=amt;

      // set mark signal, the shifter/decoder must be connected to the crc logic, not dsr
      if (amt == 1) {
        aminfo|=CAPSFDC_AI_MARKA1|CAPSFDC_AI_MA1ACTIVE;


        // if CRC is enabled, first A1 mark activates the CRC logic
        if (aminfo & CAPSFDC_AI_CRCENABLE) {
          // if CRC logic is not activate yet reset CRC value and counter, 16 cells already available as mark
          if (!(aminfo & CAPSFDC_AI_CRCACTIVE)) {
            aminfo|=CAPSFDC_AI_CRCACTIVE;
            //pc->crc=~0; //SS keep ours
            CrcLogic.crccnt=16; 
          }
        }

      } else
        aminfo|=CAPSFDC_AI_MARKC2;
      TRACE_MFM(" mark %X ",(amdecode  & 0xffff));
    }
  }


  // process CRC if activated
  if (aminfo & CAPSFDC_AI_CRCACTIVE) {
    // process new value at every 16 cells (8 clock/data)
    if (!(CrcLogic.crccnt & 0xf)) { 
      // reset CRC process if less than 3 consecutive A1 marks detected
      if (CrcLogic.crccnt>48 || (aminfo & CAPSFDC_AI_MARKA1)) {
        // 3 consecutive A1 marks found: set AM found signal, disable AM detector
        if (CrcLogic.crccnt == 48) {
          aminfo|=CAPSFDC_AI_AMFOUND|CAPSFDC_AI_AMACTIVE;
          aminfo&=~CAPSFDC_AI_AMDETENABLE;
        }
      } else
        aminfo&=~(CAPSFDC_AI_CRCACTIVE|CAPSFDC_AI_AMACTIVE);
    }

    CrcLogic.crccnt++;
  }



  // wait for data clock cycle plus bitcell delay
  if (!Amd.amdatadelay) {
    // set next delay
    // just read a clock bit here, next cell is data, that gets processed at the clock bit after that
    Amd.amdatadelay=1;

    // clear dsr signals
    aminfo&=~(CAPSFDC_AI_DSRREADY|CAPSFDC_AI_DSRAM|CAPSFDC_AI_DSRMA1);

    // shift data bit into dsr, this is a clock bit, data bit is at decoder#1
    Amd.dsr=( ((Amd.dsr<<1) | ((amdecode>>1) & 1)) ) & 0xff;

    // process data if 8 bits are dsr now, otherwise just increase dsr counter
    if (++Amd.dsrcnt == 8) {
      // reset dsr counter
      Amd.dsrcnt=0;

      // if AM found set dsr signal
      if (aminfo & CAPSFDC_AI_AMACTIVE) {
        TRACE_MFM(" -AM- ");
        aminfo&=~CAPSFDC_AI_AMACTIVE;
        aminfo|=CAPSFDC_AI_DSRAM;
      }

      // if A1 mark found set dsr signal
      if (aminfo & CAPSFDC_AI_MA1ACTIVE) {
        aminfo&=~CAPSFDC_AI_MA1ACTIVE;
        aminfo|=CAPSFDC_AI_DSRMA1;
      }

      // set dsr ready signal, unless data is invalid
      if (!Amd.amdataskip)
      {
        aminfo|=CAPSFDC_AI_DSRREADY;
        dsr=(Amd.dsr&0xFF);
      }
      else
        Amd.amdataskip--;
    }
  } else
  {
    Amd.amdatadelay--;
#ifdef SSE_DEBUG
    Mfm.encoded=(WORD)Amd.amdecode; //wrong byte/clock order for 1st $A1
#endif
  }

  // save new am info
  Amd.aminfo=aminfo;

  // if a byte is complete, break and signal new byte
  if (Amd.aminfo & CAPSFDC_AI_DSRREADY) {
    byte_ready=true;
  }
  return byte_ready;
}

#endif//#if defined(SSE_WD1772_LL)

#undef LOGSECTION
#undef FLOPPY_IRQ_YES
#undef FLOPPY_IRQ_ONESEC
#undef FLOPPY_IRQ_NOW
#undef DRIVE_HBLS_PER_ROTATION
#undef DRIVE_HBLS_OF_INDEX_PULSE
