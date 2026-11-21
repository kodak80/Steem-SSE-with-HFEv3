/*---------------------------------------------------------------------------
PROJECT: Steem SSE
Atari ST emulator
Copyright (C) 2026 by Anthony Hayward and Russel Hayward + SSE

FILE: isa_picogus.cpp
DESCRIPTION: PicoGUS ISA sound card on an emulated ISA bus window.

The Raven060 (a 68060 Atari clone) has an ISA bus, and the PicoGUS card is
used on it with Raven drivers and tools such as pgusinit. Steem emulates an
ST, so here the ISA I/O space is a window of the 24-bit address space at
$F20000-$F2FFFF, unused on STF/STE/Mega ST, decoded like the Raven's (lane
swapped little endian, "lels"). ISA_BIOS.PRG from the Raven software, with
isa.iobase=0xF20000 and isa.endian=lels in its INF file, installs the
_ISA cookie on it, then the Raven drivers work unmodified.

Like the 68060, the window accepts word accesses at odd addresses (an
ISA_BIOS outpw()/inpw() to an odd port, pgusinit does them).

The card is in 3rdparty/picogus (pgemu.h): the PicoGUS firmware's own GUS,
Sound Blaster and OPL code. There is no ISA DMA or IRQ, as with ISA_BIOS on
machines other than the Raven.

Time: the card runs on emulated time. Its audio is rendered into a ring
indexed by Steem's sound sample time (the PSG's time base), up to the
current time before each access, so register writes take effect at the
right sample, and is mixed into the sound buffer in SoundVBL().
---------------------------------------------------------------------------*/

#include <pch.h>

#if defined(SSE_PICOGUS)

#include <isa_picogus.h>
#include <emulator.h>
#include <computer.h> // Glue
#include <sound.h>
#include <run.h>
#include <gui.h>
#include <debug.h>
#include <picogus/pgemu.h>

#define LOGSECTION LOGSECTION_SOUND

static bool CardOn=false;
static COUNTER_VAR LastSysTime;
static DWORDLONG CardCycles; // emulated cycles since the card was powered on

// audio ring, indexed by sound sample time
#define PG_RING_FRAMES 65536 // power of 2, > 1s at 48kHz
static short PgRing[PG_RING_FRAMES*2];
static DWORD PgRenderedTime; // sample time up to which the ring is rendered
static bool PgAudioSync=false;


static DWORDLONG PicoGusNowUs() {
  COUNTER_VAR t=ABSOLUTE_SYS_TIME;
  COUNTER_VAR d=t-LastSysTime;
  if(d>0) // time never goes back for the card
    CardCycles+=(DWORDLONG)d;
  LastSysTime=t;
  return nSysCyclesPerSecond ? CardCycles*1000000/nSysCyclesPerSecond : 0;
}


static DWORD PicoGusSampleNow() {
  // as psg_set_reg(): position in the sound buffer's time of the current cycle
#if defined(SSE_TIMINGS_US)
  int cpu_cycles_per_vbl=Glue.nFrameCycles;
#else
  if(!VideoFreqAtStartOfVbl)
    return psg_time_of_last_vbl_for_writing;
  int cpu_cycles_per_vbl=nSysCyclesPerSecond/VideoFreqAtStartOfVbl;
#endif
  if(cpu_cycles_per_vbl<=0)
    return psg_time_of_last_vbl_for_writing;
  COUNTER_VAR since_vbl=ABSOLUTE_SYS_TIME-sys_time_of_last_vbl;
  if(since_vbl<0)
    since_vbl=0;
  DWORDLONG a64=(DWORDLONG)since_vbl;
  a64*=psg_n_samples_this_vbl;
  a64/=cpu_cycles_per_vbl;
  return psg_time_of_last_vbl_for_writing+(DWORD)a64;
}


static void PicoGusRenderTo(DWORD target) {
  // render the card's audio up to sample time target
  if(!sound_freq || !UseSound)
  {
    PgAudioSync=false;
    return;
  }
  int ahead=(int)(target-PgRenderedTime);
  if(!PgAudioSync || ahead<0 || ahead>PG_RING_FRAMES/2)
  {
    // first time, sound restarted or a big jump: start from here
    PgRenderedTime=target;
    PgAudioSync=true;
    ZeroMemory(PgRing,sizeof(PgRing));
    return;
  }
  while(PgRenderedTime!=target)
  {
    DWORD pos=PgRenderedTime&(PG_RING_FRAMES-1);
    DWORD n=MIN(target-PgRenderedTime,(DWORD)PG_RING_FRAMES-pos);
    pgemu_render(PgRing+pos*2,n,sound_freq);
    PgRenderedTime+=n;
  }
}


static DWORDLONG PicoGusAccess() {
  // power on if needed, catch up the audio, return the time of the access
  if(!CardOn)
  {
    CardOn=true;
    LastSysTime=ABSOLUTE_SYS_TIME;
    CardCycles=0;
    EasyStr Flash=UsersPath+SLASH+"PicoGUS.ini"; // what pgusinit /save writes
    pgemu_set_flash_file(Flash.Text);
    pgemu_power_on(0);
    PgAudioSync=false;
    TRACE("PicoGUS: power on, mode %d, flash %s\n",pgemu_mode(),Flash.Text);
  }
  DWORDLONG now=PicoGusNowUs();
  PicoGusRenderTo(PicoGusSampleNow());
  return now;
}


static int PgTraceCount=0; // the first ISA accesses go to TRACE.txt
#define PG_TRACE_MAX 200

BYTE PicoGusIsaRead(MEM_ADDRESS ad) {
  DWORDLONG now=PicoGusAccess();
  BYTE x=pgemu_inb((WORD)(ad&0xFFFF),now);
  if(PgTraceCount<PG_TRACE_MAX)
  {
    PgTraceCount++;
    TRACE("PicoGUS: in  %03X = %02X (pc %06X)\n",(int)(ad&0xFFFF),x,(int)(pc&0xFFFFFF));
  }
  return x;
}


void PicoGusIsaWrite(MEM_ADDRESS ad,BYTE x) {
  DWORDLONG now=PicoGusAccess();
  if(PgTraceCount<PG_TRACE_MAX)
  {
    PgTraceCount++;
    TRACE("PicoGUS: out %03X = %02X (pc %06X)\n",(int)(ad&0xFFFF),x,(int)(pc&0xFFFFFF));
  }
  pgemu_outb((WORD)(ad&0xFFFF),x,now);
}


/*  Lane swapped: a 68000 word at address A holds ISA port A in its high
    byte and port A+1 in its low byte. ISA_BIOS swaps the bytes of the
    word, so outpw(port,data) sends the low byte of data to port first,
    the high byte to port+1, as an 8-bit ISA card sees a 16-bit access.
*/

WORD PicoGusIsaReadW(MEM_ADDRESS ad) {
  BYTE hi=PicoGusIsaRead(ad);
  BYTE lo=PicoGusIsaRead(ad+1);
  return (WORD)((hi<<8)|lo);
}


void PicoGusIsaWriteW(MEM_ADDRESS ad,WORD x) {
  PicoGusIsaWrite(ad,HIBYTE(x));
  PicoGusIsaWrite(ad+1,LOBYTE(x));
}


void PicoGusPowerOff() {
  if(CardOn)
    TRACE("PicoGUS: power off\n");
  CardOn=false;
  PgTraceCount=0;
  PgAudioSync=false;
}


void PicoGusMixSound(void *DatAdr[2],DWORD LockLength[2],DWORD write_time) {
  if(!CardOn || !sound_bytes_per_sample)
    return;
  DWORD frames=0;
  for(int n=0;n<2;n++)
    if(DatAdr[n])
      frames+=LockLength[n]/sound_bytes_per_sample;
  // this VBL writes samples from write_time on: render them all
  PicoGusAccess();
  PicoGusRenderTo(write_time+frames);
  if(!PgAudioSync)
    return;
  DWORD t=write_time;
  for(int n=0;n<2;n++)
  {
    if(!DatAdr[n])
      continue;
    DWORD count=LockLength[n]/sound_bytes_per_sample;
    for(DWORD i=0;i<count;i++,t++)
    {
      int rendered=(int)(PgRenderedTime-t);
      if(rendered<=0 || rendered>PG_RING_FRAMES)
        continue; // not rendered (sound restarted)
      DWORD pos=t&(PG_RING_FRAMES-1);
      int s[2]={PgRing[pos*2],PgRing[pos*2+1]};
      if(sound_num_channels==1)
        s[0]=(s[0]+s[1])/2;
      for(int c=0;c<sound_num_channels;c++)
      {
        if(sound_num_bits==8)
        {
          BYTE *pb=(BYTE*)DatAdr[n]+i*sound_bytes_per_sample+c;
          int v=((int)*pb-128)*256+s[c];
          v=(v>SHRT_MAX)?SHRT_MAX:((v<SHRT_MIN)?SHRT_MIN:v);
          *pb=(BYTE)((v>>8)+128);
        }
        else
        {
          short *pw=(short*)((BYTE*)DatAdr[n]+i*sound_bytes_per_sample)+c;
          int v=(int)*pw+s[c];
          *pw=(short)((v>SHRT_MAX)?SHRT_MAX:((v<SHRT_MIN)?SHRT_MIN:v));
        }
      }
    }
  }
}

#undef LOGSECTION

#endif//SSE_PICOGUS
