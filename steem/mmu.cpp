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
FILE: mmu.cpp
DESCRIPTION: High-level MMU (Memory Management Unit) emulation
The MMU (Memory Management Unit) is an Atari custom chip (68 pins on the STF). 
Its memory management capabilities are rudimentary, it's not to be confused 
with a modern MMU. In fact, the limited memory protection is done by the Glue.
The MMU also handles the video and, on the STE, sound memory.
On the STE, the Glue and the Mmu have been merged together, producing the
GSTMCU. In Steem SSE, we do as if they were still separate (even though it's
a tempting inheritance case).
Memory access is handled directly by peeks and pokes, see cpu_ea.cpp
Since Steem SSE uses MMU doc by Christian Zietz, it has become harder
to run the wrong TOS for the ST model (in original Steem, it was OK to run
TOS 1.02 on the STE, now it only works with 4MB, option Hacks).
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <draw.h>
#include <computer.h>

#if defined(SSE_DEBUGGER_FRAME_REPORT)
#include <debug_framereport.h>
#endif
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif


// extern "C"
#ifndef BIG_ENDIAN_PROCESSOR
BYTE *Mem_End=NULL,*Mem_End_minus_1=NULL,*Mem_End_minus_2=NULL,*Mem_End_minus_4=NULL;
#endif
DWORD mem_len=0;

BYTE *STMem=NULL;
MEM_ADDRESS himem=0;

// the reference can't be in the struct in BCC, has to be global
// necessary as long as we want to keep BCC compatibilty (useful to none but me)
MEM_ADDRESS &vbase=Mmu.uVBase.d32;
MEM_ADDRESS &dma_address=Mmu.uDmaCounter.d32;
MEM_ADDRESS &SteSndFrameStart=Mmu.uSoundFrameStart.d32;
MEM_ADDRESS &NextSteSndFrameStart=Mmu.uNextSoundFrameStart.d32;
MEM_ADDRESS &SteSndFrameEnd=Mmu.uSoundFrameEnd.d32;
MEM_ADDRESS &NextSteSndFrameEnd=Mmu.uNextSoundFrameEnd.d32;
MEM_ADDRESS &SteSndFetchAd=Mmu.uSoundFetchAd.d32;


////////////
// MEMORY //
////////////

/*
0       128 KB
1       512 KB
2         2 MB
3         0
4         7 MB (Steem hack)
5         6 MB (MonSTer)
*/
const MEM_ADDRESS bank_length_from_config[N_MEMCONF]=
{128*1024,512*1024,2*MEM_1MB,0,7*MEM_1MB
#if defined(SSE_MMU_MONSTER_ALT_RAM)                  
,6*MEM_1MB
#endif
};


void GetCurrentMemConf(BYTE MemConf[2]) {
  MemConf[0]=MemConf[1]=MEMCONF_512;
  for(BYTE bank=0;bank<2;bank++)
  {
    for(BYTE n=0;n<N_MEMCONF;n++)
    {
      if(SSEConfig.bank_length[bank]==Mmu.BankLength(n)) 
      {
        MemConf[bank]=n;
        break;
      }
    }
  }
}


MEM_ADDRESS TMmu::BankLength(DWORD const conf) {
  return (conf<N_MEMCONF) ? bank_length_from_config[conf] : 0;
}


#define LOGSECTION LOGSECTION_MMU

/*  MMU Confused is when the CONFIG register doesn't match real memory.
    see memdetect.txt in 3rdparty/doc

R=row, C=column
If a line isn't connected, bit is 0.

On MMU configured for 2MB

STF decoding=
0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
C C C C C C C C C C R R R R R R R R R R X
9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 X

STE decoding=
0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
C R C R C R C R C R C R C R C R C R C R X
9 9 8 8 7 7 6 6 5 5 4 4 3 3 2 2 1 1 0 0 X

512K bank: C9, R9 
128K bank: C9, R9, C8, R8 not connected

On MMU configured for 512K 

STF decoding=
8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
C C C C C C C C C R R R R R R R R R X
8 7 6 5 4 3 2 1 0 8 7 6 5 4 3 2 1 0 X

STE decoding=
8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
C R C R C R C R C R C R C R C R C R X
8 8 7 7 6 6 5 5 4 4 3 3 2 2 1 1 0 0 X

128K bank: R8, C8 not connected

*/


MEM_ADDRESS mmu_confused_address(MEM_ADDRESS ad) {
#if defined(SSE_ENABLE_TRACE_LOG)
  MEM_ADDRESS ad1=ad; // save for trace
#endif
  //ASSERT(ad<MEM_4MB);
  int bank=0;
  if(ad>MEM_4MB)
    return 0xffffff;   //bus error
  else if(ad>=Mmu.bank_length[0])
  {
    bank=1;
    ad-=Mmu.bank_length[0];
    if(ad>=Mmu.bank_length[1])
      return 0xfffffe; //gap
  }
  if(SSEConfig.bank_length[bank]==0)
    ad=0xfffffe; //gap
  // MMU configured for 2MB 
  else if(Mmu.bank_length[bank]==MEM_2MB)
  {
    if(SSEConfig.bank_length[bank]==MEM_512KB)
    { //real memory
      if(IS_STF)
        ad&=~(BIT_20|BIT_10);
      else
        ad&=~(BIT_20|BIT_19);
    }
    else if(SSEConfig.bank_length[bank]==MEM_128KB)
    { //real memory
      if(IS_STF)
        ad&=~(BIT_20|BIT_19|BIT_10|BIT_9);
      else
        ad&=~(BIT_20|BIT_19|BIT_18|BIT_17);
    }
  }//2MB
  // MMU configured for 512K
  else if(Mmu.bank_length[bank]==MEM_512KB)
  {
    if(SSEConfig.bank_length[bank]==MEM_128KB)
    { //real memory
      if(IS_STF)
        ad&=~(BIT_18|BIT_9);
      else
        ad&=~(BIT_18|BIT_17);
    }
  }//512K
  if(bank==1&&ad<MEM_4MB)
    ad+=SSEConfig.bank_length[0];
#if defined(SSE_ENABLE_TRACE_LOG)
  if(ad1!=ad)
    TRACE_LOG("MMU confused ad %X -> %X\n",ad1,ad);
#endif
  return ad;
}


#if !defined(SSE_DEBUGGER)

BYTE mmu_confused_peek(MEM_ADDRESS const ad) {
  MEM_ADDRESS c_ad=mmu_confused_address(ad);
  if(c_ad==0xffffff)
    exception(EXCEPTION_BUS_ERROR,EA_READ,ad);
  else if(c_ad==0xfffffe)
    //gap in memory
    return 0xff;
  else if(c_ad<mem_len)
    return PEEK(c_ad);
  return 0xff;
}


WORD mmu_confused_dpeek(MEM_ADDRESS const ad) {
  MEM_ADDRESS c_ad=mmu_confused_address(ad);
  if(c_ad==0xffffff)
    exception(EXCEPTION_BUS_ERROR,EA_READ,ad);
  else if(c_ad==0xfffffe)
    //gap in memory
    return 0xffff;
  else if(c_ad<mem_len)
    return DPEEK(c_ad);
  return 0xffff;
}


#else

BYTE mmu_confused_peek(MEM_ADDRESS ad,bool bBErrOn) {
  MEM_ADDRESS c_ad=mmu_confused_address(ad);
  if(c_ad==0xffffff)
  {   //bus error
    if(bBErrOn) // no crash when it's for the debugger!
      exception(EXCEPTION_BUS_ERROR,EA_READ,ad);
    return 0;
  }
  else if(c_ad==0xfffffe)
    //gap in memory
    return 0xff;
  else if(c_ad<mem_len)
    return PEEK(c_ad);
  else
    return 0xff;
}


WORD mmu_confused_dpeek(MEM_ADDRESS ad,bool bBErrOn) {
  MEM_ADDRESS c_ad=mmu_confused_address(ad);
  if(c_ad==0xffffff)
  {   //bus error
    if(bBErrOn)
      exception(EXCEPTION_BUS_ERROR,EA_READ,ad);
    return 0;
  }
  else if(c_ad==0xfffffe)
    //gap in memory
    return 0xffff;
  else if(c_ad<mem_len)
    return DPEEK(c_ad);
  else
    return 0xffff;
}


LONG mmu_confused_lpeek(MEM_ADDRESS ad,bool bBErrOn) { // only called by d2_lpeek()
  WORD a=mmu_confused_dpeek(ad,bBErrOn);
  WORD b=mmu_confused_dpeek(ad+2,bBErrOn);
  return MAKELONG(b,a);
}

#endif//#if !defined(SSE_DEBUGGER)


void mmu_confused_poke_abus(BYTE const x) {
  MEM_ADDRESS c_ad=mmu_confused_address(iabus);
  if(c_ad==0xffffff)  //bus error
    exception(EXCEPTION_BUS_ERROR,EA_WRITE,iabus);
  else if(c_ad==0xfffffe)  //gap in memory
  {}
  else if((c_ad+1)<=mem_len)
    PEEK(c_ad)=x;
}


void mmu_confused_dpoke_abus(WORD const x) {
  MEM_ADDRESS c_ad=mmu_confused_address(iabus);
  if(c_ad==0xffffff)  //bus error
    exception(EXCEPTION_BUS_ERROR,EA_WRITE,iabus);
  else if(c_ad==0xfffffe)  //gap in memory
  {}
  else if((c_ad+2)<=mem_len)
    DPEEK(c_ad)=x;
}


EasyStr ReadStringFromMemory(MEM_ADDRESS ad,int const max_len) {
  // used by hd_gemdos and iow
  if(ad==0)
    return "";
  EasyStr a;
  a.SetLength(max_len);
  int n;
  char i;
  for(n=0;n<max_len;n++)
  {
#if defined(SSE_MMU_MONSTER_ALT_RAM)
    if(ad<mem_len)
#else
    if(ad<himem)
#endif
      i=PEEK(ad);
    else if(ad>=rom_addr && ad<rom_addr_end)
      i=ROM_PEEK(ad-rom_addr);
    else
      break;
    ad++;
    if(i=='\0')
      break;
    a[n]=i;
  }
  a[n]='\0';
  return a;
}


MEM_ADDRESS WriteStringToMemory(MEM_ADDRESS ad,char* c) {
  // used by hd_gemdos (and undefined "onegame")
  if(ad<MEM_FIRST_WRITEABLE)
    return 0;
  do
    SafePoke(ad++,*c);
  while(*(c++));
  return ad;
}


/////////////////
// SOUND (STE) //
/////////////////

#undef LOGSECTION
#define LOGSECTION LOGSECTION_SOUND


void TMmu::SetSoundControl(BYTE const io_src_b) { // only called by io_write
/*  $FFFF8900 ---- ---- ---- --cc RW Sound DMA Control
    cc:
    00  Sound DMA disabled (reset state)
    01  Sound DMA enabled, disable at end of frame
    11  Sound DMA enabled, repeat frame forever
*/
  if((SoundControl&SOUNDPLAY) && !(io_src_b&SOUNDPLAY)) // Stopping
  {
    TRACE_LOG("%d %d %d STE sound stop ",TIMING_INFO);
    uSoundFrameStart.d32=uNextSoundFrameStart.d32;
    uSoundFrameEnd.d32=uNextSoundFrameEnd.d32;
    uSoundFetchAd.d32=uSoundFrameStart.d32;
  }
  else if(io_src_b&SOUNDPLAY) // Playing
  {
    if(OPTION_HACKS && (SoundControl&SOUNDPLAY) && LINECYCLES>Glue.CurrentScanline.EndCycle) // can be 0
    { // We haven't finished previous frame (timing of SoundFetch() isn't precise)
      // (except if STVL is used)
      // If we're after DE, fetch some samples, it could finish it (Light.prg)
      if(Glue.bFetchingLine // if not, "impossible" to be late (E605 intro)
        && uSoundFrameEnd.d32-uSoundFetchAd.d32<10) // 4 max
      {
        TRACE_LOG("%d %d %d Fetch last samples at %X\n",TIMING_INFO,SteSndFetchAd);
        SoundFetch();
      }
    }
    if(SoundControl&SOUNDPLAY) // already playing
    {
      SoundControl=(io_src_b&(SOUNDLOOP|SOUNDPLAY)); // just in case?
      return;
    }
    uSoundFetchAd.d32=uSoundFrameStart.d32=uNextSoundFrameStart.d32;
    uSoundFrameEnd.d32=uNextSoundFrameEnd.d32;
    TRACE_LOG("%d %d %d STE sound start loop %d current %x frame %x->%x %d samples %fs ",
              TIMING_INFO,(io_src_b&BIT_1)>>1,uSoundFetchAd.d32,uSoundFrameStart.d32,
              uSoundFrameEnd.d32,(uSoundFrameEnd.d32-uSoundFrameStart.d32)/2,
              (float)((uSoundFrameEnd.d32-uSoundFrameStart.d32)/2)/(float)SteSoundFreq);
    if(uSoundFetchAd.d32==uSoundFrameEnd.d32  && (io_src_b&SOUNDLOOP)==0) // but if looped it will go (lazer insane)
    {
      TRACE_LOG("STOP\n");
      return; // Froggies bug PC $1723A
    }
#if defined(SSE_VID_STVL1)
    if(OPTION_C3 && SSEConfig.Stvl>=TStvl::VER_STESND) // check  version too
      Stvl.sreq=true; // will trigger fetches
    else
#endif
    if(!Glue.bFetchingLine)
      SoundFetch(); // fill FIFO
    if(!ste_sound_on_this_screen)
    {
      // Pad buffer with last byte from VBL to current position
      bool Mono=!!(shifter_sound_mode&TShifter::SOUNDMONO);
      //TRACE_LOG((Mono) ? (char*)"Mono":(char*)"Stereo");
      int freq_idx=0;
      if(VideoFreqAtStartOfVbl==NTSC_HZ)
        freq_idx=1;
      else if(VideoFreqAtStartOfVbl==MONO_HZ)
        freq_idx=2;
      WORD w1,w2;
      Shifter.SoundGetLastSample(&w1,&w2);
      for(int y=-TopScanlines[freq_idx];y<scan_y;y++)
      {
        int loop=(Mono) ? 2 : 1;
        SteSoundSamplesCountdown+=(int)SteSoundFreq*CyclesPerScanlineAtStartOfVbl/loop;
        while(SteSoundSamplesCountdown>=0)
        {
          for(int i=0;i<loop;i++)
          {
            SteSoundOutputCountdown+=sound_freq;
            while(SteSoundOutputCountdown>=0)
            {
              if(SteSoundChannelBufIdx>=SteSoundChannelBufLen)
                break;
              SteSoundChannelBuf[SteSoundChannelBufIdx++]=w1;
              SteSoundChannelBuf[SteSoundChannelBufIdx++]=w2;
              SteSoundOutputCountdown-=SteSoundFreq;
            }
          }
          SteSoundSamplesCountdown-=nSysCyclesPerSecond;
        }
      }
      ste_sound_on_this_screen=true;
    }//if(!ste_sound_on_this_screen)
  }
  TRACE_LOG("(Channels %d Freq %d)\n",2-((shifter_sound_mode&BIT_7)>>7),SteSoundFreq);
  SoundControl=io_src_b&(SOUNDLOOP|SOUNDPLAY);
  ASSERTB((COLOUR_MONITOR^(SoundControl&SOUNDPLAY))==((COLOUR_MONITOR==1)^((SoundControl&SOUNDPLAY)!=0)));
  //mfp_gpip_set_bit(MFP_GPIP_MONO_BIT,COLOUR_MONITOR^(SoundControl&SOUNDPLAY));
  mfp_gpip_set_bit(MFP_GPIP_MONO_BIT,(COLOUR_MONITOR==1)^((SoundControl&SOUNDPLAY)!=0));
  if((io_src_b&SOUNDPLAY) && Mfp.reg[MFPR_TACR]==MFP_TIMER_EVENT_COUNT
    && (Mfp.reg[MFPR_AER]&MFP_GPIP_ACIA_MASK))
    Mfp.TimerATick();
}               


bool TMmu::SoundStop() { // only called by SoundFetch()
  bool bStopped=true;
  uSoundFetchAd.d32=uSoundFrameStart.d32=uNextSoundFrameStart.d32;
  uSoundFrameEnd.d32=uNextSoundFrameEnd.d32;
  SoundControl&=~SOUNDPLAY;
  if(Mfp.reg[MFPR_TACR]==MFP_TIMER_EVENT_COUNT && !(Mfp.reg[MFPR_AER]&MFP_GPIP_ACIA_MASK))
    Mfp.TimerATick(); // I know of no case where it's used
  ASSERTB((COLOUR_MONITOR^(SoundControl&SOUNDPLAY))==((COLOUR_MONITOR!=0)^(SoundControl&SOUNDPLAY)));
  mfp_gpip_set_bit(MFP_GPIP_MONO_BIT,(COLOUR_MONITOR!=0)^(SoundControl&SOUNDPLAY)); // IRQ may trigger
  if(SoundControl&SOUNDLOOP)
  {
    TRACE_LOG("STE snd looping\n");
    SoundControl|=SOUNDPLAY; //Playing again immediately
    ASSERTB((COLOUR_MONITOR^1)==((COLOUR_MONITOR!=0)^(SoundControl&SOUNDPLAY)));
    //mfp_gpip_set_bit(MFP_GPIP_MONO_BIT,COLOUR_MONITOR^1);
    mfp_gpip_set_bit(MFP_GPIP_MONO_BIT,(COLOUR_MONITOR!=0)^(SoundControl&SOUNDPLAY));
    bStopped=false;
  }
  return bStopped;
}


// static for STVL (which calls it at the right timing)
void CALLBACK TMmu::SoundFetch() {
/*  "During horizontal blanking (transparent to the processor) the DMA sound
    chip fetches samples from memory and provides them to a digital-to-analog
    converter (DAC) at one of several constant rates, programmable as 
    (approximately) 50KHz (kilohertz), 25KHz, 12.5KHz, and 6.25KHz. 
    This rate is called the sample frequency. 
    Each sample is stored as a signed eight-bit quantity, where -128 (80 hex) 
    means full negative displacement of the speaker, and 127 (7F hex) means full 
    positive displacement. In stereo mode, each word represents two samples: the 
    upper byte is the sample for the left channel, and the lower byte is the 
    sample for the right channel. In mono mode each byte is one sample. However, 
    the samples are always fetched a word at a time, so only an even number of 
    mono samples can be played. "
    Approximation:
    Fetching should happen as soon as Video Enable stops, we do it at scanline.
*/
  for(int i=0;i<TShifter::DIGITAL_SOUND_BUFFER_LEN && SteSndFetchAd<himem
    && ((Mmu.SoundControl&SOUNDPLAY)!=0 && Shifter.SoundFifoIdx<TShifter::DIGITAL_SOUND_BUFFER_LEN);i++)
  {
    // not fetching memory is our way to deactivate digital sound
    Shifter.SoundFifo[Shifter.SoundFifoIdx++]=(SSEConfig.SteSoundOn)?SafeDPeek(SteSndFetchAd):0;
    SteSndFetchAd+=2;
    SteSndFetchAd&=0x3FFFFE;
    if(SteSndFetchAd==SteSndFrameEnd) // not >=: lazer insane
    {
      if(Mmu.SoundStop())
        break;
    }
    if(Shifter.SoundFifoIdx>=TShifter::DIGITAL_SOUND_BUFFER_LEN)
      break;
  }//nxt
#if defined(SSE_VID_STVL1)
  if(OPTION_C3 && SSEConfig.Stvl>=TStvl::VER_STESND)
    Stvl.sreq=(Shifter.SoundFifoIdx<TShifter::DIGITAL_SOUND_BUFFER_LEN && (Mmu.SoundControl&SOUNDPLAY));
#endif
}


#if defined(SSE_SOUND_CARTRIDGE)
/*  To play the digital sound of the MV16 cartridge, we hack Steem's STE
    sound emulation, trading stereo 8bit for mono 16bit.
    This reduces overhead (need no other sound buffer).
    The sound is played as is, without filter (just like STE sound!)
    https://www.youtube.com/watch?v=2cYJGTL0QrE actual hardware
    https://www.youtube.com/watch?v=nxAHqYP9hAg crack - PSG
    This also handles the Replay 16 cartridge and the Centronics cartridge
    used by Wings of Death and Lethal Xcess.
*/

void mv16_fetch(WORD data) {
#define last_write Microwire.StartTime // optimisation, recycle a COUNTER_VAR, starts at 0
  if(SSEConfig.mr16)
    data>>=1;
  a_s_t=A_S_T;
  int cycles=(a_s_t-last_write)&0xFFF; //fff for when last_write is 0!
  SteSoundSamplesCountdown+=SteSoundFreq*cycles; // this implies jitter
  while(SteSoundSamplesCountdown>/*=*/0)
  {
    SteSoundOutputCountdown+=sound_freq;
    while(SteSoundOutputCountdown>=0)
    {
      if(SteSoundChannelBufIdx>=SteSoundChannelBufLen)
        break;
      SteSoundChannelBuf[SteSoundChannelBufIdx++]=SteSoundLastWord.d16;
      SteSoundOutputCountdown-=SteSoundFreq;
    }
    SteSoundOutputCountdown+=sound_freq;
    SteSoundSamplesCountdown-=nSysCyclesPerSecond; 
  }
  last_write=a_s_t;
  ste_sound_on_this_screen=true;
  SteSoundLastWord.d16=data;
}

#undef last_write 

#endif


///////////
// Misc. //
///////////

void TMmu::Reset(bool const Cold) {
  // the MMU has no reset pin so it's rather virtual here despite
  // what the specification says...
  MonSTerHimem=0;
  if(Cold||!extended_monitor)
    vbase=VideoCounter=shifter_draw_pointer=0;
  ExtraBytesForHscroll=SoundControl=linewid=0;
  uSoundFetchAd.d32=uSoundFrameStart.d32=uNextSoundFrameStart.d32=0;
  uSoundFrameEnd.d32=uNextSoundFrameEnd.d32=0;
  if(Cold)
  {
    if(IS_STE)
      OPTION_WS=4; // STE is always in WS1 (=option 4) because there is only one chip for GLU and MMU
    else if(OPTION_RANDOM_WU)
      OPTION_WS=(rand()%4)+1; // choose a wakeup
    Restore();
    Glue.Update();
  }
}


void TMmu::Restore() {
  linewid0=ExtraBytesForHscroll=0;
  //     0  1  2  3  4  5
  // dl  0  3  4  5  6  3
  // ws  0  2  4  3  1  2
  //res  0  2  0  0 -2  2
  //sync 0  2  2  0  0  2
  ResMod[2]=ResMod[3]=FreqMod[3]=FreqMod[4]=0;
  WU[1]=WU[2]=WU[5]=WS[1]=WS[5]=ResMod[1]=ResMod[5]=FreqMod[1]=FreqMod[2]=FreqMod[5]=2;
  WU[3]=WU[4]=WS[4]=1;
  WS[2]=DL[2]=4;
  WS[3]=DL[1]=DL[5]=3;
  ResMod[4]=-2;
  DL[3]=5;
  DL[4]=6;
  if(!(OPTION_WS>=1 && OPTION_WS<=4)) // invalid WS?
    OPTION_WS=1; // OK for STF & STE
  uNextSoundFrameStart.d8[B3]=uNextSoundFrameEnd.d8[B3]=0; // 24bit
}


///////////
// VIDEO //
///////////


/*  Video Address Counter:

    STF - read only, writes ignored
    ff 8205   R               |xxxxxxxx|   Video Address Counter High
    ff 8207   R               |xxxxxxxx|   Video Address Counter Mid
    ff 8209   R               |xxxxxxxx|   Video Address Counter Low

    STE - read & write
    FF8204 ---- ---- --xx xxxx (High)
    FF8206 ---- ---- xxxx xxxx
    FF8208 ---- ---- xxxx xxx- (Low)

    The video counter can be anywhere in the ST RAM, which is limited to 4MB
    because MMU registers don't go higher
*/

/*  This function is vital if we want to emulate many demos because for overscan effects
*   it is necessary to synchronise with the video counter.
*/ 
void TMmu::UpdateVideoCounter(SHORT const CyclesIn) {
  //ASSERT(!OPTION_C3);
  MEM_ADDRESS vc;
  if(bad_drawing)
  {  // Fake SDP, eg extended monitor
    if(scan_y<0)
      vc=vbase;
    else if(scan_y<shifter_y)
    {
      int line_len=(160/res_vertical_scale);
      vc=vbase+scan_y*line_len+(MIN(CyclesIn/2*TICKS8,line_len) & ~1);
    }
    else
      vc=vbase+32000;
  }
  else if(Glue.bFetchingLine) // lines where the counter actually moves
  {
    if(OPTION_C2)
      Glue.CheckSideOverscan(); // this updates Bytes and StartCycle
    int bytes_to_count=Glue.CurrentScanline.Bytes;
    // 8 cycles latency before MMU starts prefetching
    int starts_counting=(Glue.CurrentScanline.StartCycle+MMU_PREFETCH_LATENCY)/(2*TICKS8);
    // can't be odd though (hires)
    starts_counting&=-2;
    // compute vc
    int c=CyclesIn/(2*TICKS8)-starts_counting;
    vc=VCountAtHSync;
    if(!bytes_to_count)
    {} // 0-byte lines
    else if(c>=bytes_to_count)
    {
      vc+=bytes_to_count;
      if(IS_STE && CyclesIn>=Glue.CurrentScanline.EndCycle && !no_LW)
        vc+=LINEWID*2; // LINEWID is in words
    }
    else if(c>=0)
    {
#if defined(SSE_420R5) // Nostalgic HW test broken v380!
      if(c&1)
        c++;
#else
      c&=-2;
#endif
      vc+=c;
    }
  }
  else if(Glue.vsync)
    vc=vbase; // during VSYNC, VCOUNT=VBASE
  else // lines witout fetching (before or after frame)
    vc=VCountAtHSync;
#if defined(SSE_MMU_MONSTER_ALT_RAM)
  if(mem_len<MEM_14MB) 
#else
  if(mem_len<=MEM_4MB) 
#endif
    vc&=0x3FFFFE; // enforce 21bit (4MB limit)
  VideoCounter=vc; // update member variable
}


MEM_ADDRESS TMmu::ReadVideoCounter(SHORT const CyclesIn) {
  //ASSERT(!OPTION_C3);
  UpdateVideoCounter(CyclesIn);
  return VideoCounter;
}


/*  This function is only called by io_write() in iow.cpp, if OPTION_C3 is not active.
    Now that video counter reckoning (Mmu.VideoCounter) is separated from 
    rendering (shifter_draw_pointer), a lot of cases that seemed complicated
    are simplified. Most hacks could be removed.

    Also, a simple test program allowed us to demystify writes to the video
    counter. It is actually straightforward, here's the rule:
    The byte in the MMU register is replaced with the byte on the bus, that's
    it, even if the counter is running at the time (Display Enable), and
    whatever words are in the Shifter, because
    1) The video counter increment happens (if DE) before the write.
    2) The video counter resides in the MMU and the Shifter never sees it.

*/

void TMmu::WriteVideoCounter(BYTE const reg,BYTE const io_src_b) {
  // reg: 4 for high byte of video address, 6 for medium, 8 for low
  // io_src_b: value to write (on existing bits)
  //ASSERT(!OPTION_C3);

  // some programs write to those addresses mostly by accident, it just must be ignored if
  // we don't emulate an STE (no crash)
  if(!IS_STE)
    return;

  SHORT CyclesIn=LINECYCLES;

  if(Glue.bFetchingLine)
    Shifter.Render(CyclesIn,TShifter::DISPATCHER_WRITE_VC);

  // change video counter
  UpdateVideoCounter(CyclesIn);
  const MEM_ADDRESS former_video_counter=VideoCounter;  
  if(!Glue.vsync) // it can be written but it would be reloaded at once during VSYNC
  {
    DWORD_B(&VideoCounter,(0x08-reg)/2)=io_src_b; // change appropriate byte
  }
  
  // enforce valid bits
#if defined(SSE_MMU_MONSTER_ALT_RAM)
  if(mem_len<MEM_14MB)
#else
  if(mem_len<=MEM_4MB)
#endif  
    VideoCounter&=0x003FFFFE;
  else
    VideoCounter&=0x00FFFFFE; // (hack)

  // update VCountAtHSync
  VCountAtHSync-=former_video_counter;
  VCountAtHSync+=VideoCounter;

#if defined(SSE_SHIFTER_421)
  // updating the video counter while video memory is being fetched
  // could cause display artefacts because of internal "shifter latency"
  // this is hacky
  if(!Glue.bFetchingLine || reg==8)
    shifter_draw_pointer=VideoCounter; // TalkTalk2 bees, Tekila, VibeCoding
#elif 1
  // OK TalkTalk2 bees, Tekila
  SHORT extracycles=SHIFTER_RASTER_PREFETCH_TIMING; // it's already 16 cycles, should be enough
  if((Glue.CurrentScanline.Tricks&TRICK_LINE_PLUS_20)&&!HSCROLL)
    extracycles+=12*TICKS8; // 12 cycles no fetch
  if(!Glue.bFetchingLine
    || CyclesIn<Glue.CurrentScanline.StartCycle+extracycles
    || CyclesIn>Glue.CurrentScanline.EndCycle+extracycles)
    shifter_draw_pointer=VideoCounter;
#else
  // OK TalkTalk2 bees, Tekila, 20 Years STE Megademo/ex reset screen
  if(!Glue.bFetchingLine
    || CyclesIn<Glue.CurrentScanline.StartCycle+SHIFTER_RASTER_PREFETCH_TIMING
    || CyclesIn>Glue.CurrentScanline.EndCycle+SHIFTER_RASTER_PREFETCH_TIMING)
    shifter_draw_pointer=VideoCounter;
#endif

  // writing the counter inhibits LINEWID addition (hypothesis based on a reading of schematics)
  if(CyclesIn==Glue.CurrentScanline.EndCycle+6*TICKS8)
    no_LW=true;
}

#undef LOGSECTION
