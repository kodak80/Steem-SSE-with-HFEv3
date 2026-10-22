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
FILE: psg.cpp
DESCRIPTION: Steem's original high-level Programmable Sound Generator
(Yamaha 2149) emulation, and a low-level emulation vaguely inspired by MESS.
The YM2149 also selects floppy disk drive unit and side (which the WD1772
can't do).
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#include <diskman.h>


#define PSG_ENV_SHAPE_HOLD BIT_0
#define PSG_ENV_SHAPE_CONT BIT_3
#define ENVELOPE_MASK 31

DWORD psg_adjust_envelope_start_time(DWORD t,DWORD new_envperiod);
void check_mamelike_ym_envelope(BYTE new_val);

int *psg_channels_buf=NULL;
DWORD psg_channels_buf_len=0; // variable
DWORD psg_buf_pointer[TYM2149::NUM_CHANNELS];
DWORD psg_tone_start_time[TYM2149::NUM_CHANNELS];
DWORD psg_envelope_start_time=0xfffff000;
int psg_voltage,psg_dv;
BYTE psg_reg[16]; // could also be in struct
BYTE psg_reg_select,psg_reg_data;
bool written_to_env_this_vbl=true; // for record YM
bool psg_noisetoggle=false;


void TYM2149::Reset() {
  ZeroMemory(psg_reg,16);
  psg_reg[PSGR_PORT_A]=0xff; // Set DTR RTS, no drive, side 0, strobe and GPO
#if defined(SSE_YM2149_LL)
  m_rng = 1; //it will take 2exp17=131072 values
  m_output[0] = m_output[1] = m_output[2] = 0;
  m_count[0] = m_count[1] = m_count[2] = 0;
  m_count_noise = m_count_env = 0;
  m_prescale_noise = 0;
  m_cycles=0; 
  // 32 steps for envelope in YM - we do it at reset for old snapshots
  m_env_step_mask=ENVELOPE_MASK; 
  m_holding=0;
  m_attack=15;
  m_hold=1; // Captain Blood
  time_at_vbl_start=0;
  time_of_last_sample=a_s_t;
#endif
}


///////////////
// PSG SOUND //
///////////////

#define LOGSECTION LOGSECTION_SOUND

// this function dispatches to the appropriate PSG sound emulation

void psg_set_reg(BYTE old_val,BYTE new_val)
  { // only called by io_write()
  BYTE &reg=psg_reg_select;
  if(old_val==new_val && reg!=PSGR_ENVELOPE_SHAPE) 
    return;
#if defined(SSE_STATS)
  if(reg!=PSGR_MIXER)
    Stats.nPsgSound++;
#endif
  if(!SoundActive()) // fast forward, mute...
  {
#if defined(SSE_YM2149_LL)
    if(reg==PSGR_ENVELOPE_SHAPE)
      check_mamelike_ym_envelope(new_val);
#endif
    return;
  }
#if defined(SSE_TIMINGS_US)
  int cpu_cycles_per_vbl=Glue.nFrameCycles;
#else
  ASSERTB(VideoFreqAtStartOfVbl);
#ifndef SSE_LEAN_AND_MEAN
  if(!VideoFreqAtStartOfVbl)
    return;
#endif
  int cpu_cycles_per_vbl=nSysCyclesPerSecond/VideoFreqAtStartOfVbl;
#endif
#if SCREENS_PER_SOUND_VBL != 1
  cpu_cycles_per_vbl*=SCREENS_PER_SOUND_VBL;
  DWORDLONG a64=(ABSOLUTE_SYS_TIME-sys_time_of_last_sound_vbl);
#else
  DWORDLONG a64=(a_s_t-sys_time_of_last_vbl);
#endif
  a64*=psg_n_samples_this_vbl; // eg *882  (44100/50)
  a64/=cpu_cycles_per_vbl; // eg 160420
  DWORD t=psg_time_of_last_vbl_for_writing+(DWORD)a64; // t's unit is #samples (total)
#if defined(SSE_YM2149_LL)
  if(OPTION_MAME_YM)
  {
    Psg.psg_write_buffer(t);
    if(reg==PSGR_ENVELOPE_SHAPE)
      check_mamelike_ym_envelope(new_val);
  }
  else 
#endif
  switch(reg) { // Steem legacy PSG sound emu
  case PSGR_TONE_PERIOD_A_LOW:
  case PSGR_TONE_PERIOD_A_HIGH:
  case PSGR_TONE_PERIOD_B_LOW:
  case PSGR_TONE_PERIOD_B_HIGH:
  case PSGR_TONE_PERIOD_C_LOW:
  case PSGR_TONE_PERIOD_C_HIGH:
  {
    BYTE abc=reg>>1; // compiler should do it anyway
    BYTE abcx2=reg&0xFE;
    // Freq is double buffered, it cannot change until the PSG reaches the end of the current square wave.
    // psg_tone_start_time[abc] is set to the last end of wave, so if it is in future don't do anything.
    // Overflow will be a problem, however at 50Khz that will take a day of non-stop output.
    if(t-psg_tone_start_time[abc]>0)
    {
      // before change
#if defined(BIG_ENDIAN_PROCESSOR)
      int toneperiod1=(((int)psg_reg[abcx2+1])<<8)+psg_reg[abcx2];
#else
      int toneperiod1=*(WORD*)&psg_reg[abcx2];
#endif
      if(toneperiod1>1)
      {
        double a=(double)toneperiod1*(double)sound_freq;
        a/=125000;// (15625*8);
        double b=t-psg_tone_start_time[abc];
        double a2=fmod(b,a);
        b-=a2;
        DWORD t2=psg_tone_start_time[abc]+(DWORD)b; // adjusted start
        // after change
        DWORD toneperiod2=(reg&1)
          ? (((DWORD)new_val)<<8)+psg_reg[abcx2]
          : (((DWORD)psg_reg[abcx2+1])<<8)+new_val;
        double a_bis=(double)toneperiod2*(double)sound_freq;
        a_bis/=125000;// (15625*8);
        // check progress of current wave
        if(!a2||a2>=a_bis)
          t2=t-(DWORD)a_bis; // start at once
        t=t2;
      }
      psg_write_buffer(abc,t);
      psg_tone_start_time[abc]=t;
    }
    break;
  }
  case PSGR_NOISE_PERIOD:
  case PSGR_MIXER:
    psg_write_buffer(0,t);
    psg_write_buffer(1,t);
    psg_write_buffer(2,t);
    break;
  case PSGR_AMPLITUDE_A:
  case PSGR_AMPLITUDE_B:
  case PSGR_AMPLITUDE_C:
    // YM doesn't quantize, it changes the level straight away.
    // Changing the volume fast is a method used to render samples
    psg_write_buffer(reg-PSGR_AMPLITUDE_A,t);
    break;
  case PSGR_ENVELOPE_PERIOD_LOW:
  case PSGR_ENVELOPE_PERIOD_HIGH:
  {
    psg_write_buffer(0,t);
    psg_write_buffer(1,t);
    psg_write_buffer(2,t);
    int new_envperiod=(reg&1)
          ? (((int)psg_reg[PSGR_ENVELOPE_PERIOD_HIGH])<<8)+new_val
          : (((int)new_val)<<8)+psg_reg[PSGR_ENVELOPE_PERIOD_LOW];
    if(!new_envperiod)
      new_envperiod=1; // min
    psg_envelope_start_time=psg_adjust_envelope_start_time(t,new_envperiod);
    break;
  }
  case PSGR_ENVELOPE_SHAPE:
    psg_write_buffer(0,t);
    psg_write_buffer(1,t);
    psg_write_buffer(2,t);
    psg_envelope_start_time=t;
    written_to_env_this_vbl=true;
    break;
  }//sw
}


////////////////////////
// SOUND STEEM LEGACY //
////////////////////////

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*                                PSG SOUND                                  */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*  tone frequency
     fT = fMaster
          -------
           16TP


ie. period = 16TP / 2 000 000
= TP/125000

 = TP / (8*15625)


 the envelope repetition frequency fE is obtained as follows from the
envelope setting period value EP (decimal):

       fE = fMaster        (fMaster if the frequency of the master clock)
            -------
             256EP

The period of the actual frequency fEA used for the envelope generated is
1/32 of the envelope repetition period (1/fE).

ie. period of whole envelope = 256EP/2 000 000 = 2*EP / 15625 (think this is one go through, eg. /)
Period of envelope change frequency is 1/32 of this, so EP/ 16*15625
Scale by 64 and multiply by frequency of buffer to get final period, 4*EP/15625

*/

/*      New scalings - 6/7/01

__TONE__  Period = TP/(8*15625) Hz = (TP*sound_freq)/(8*15625) samples
          Period in samples goes up to 1640.6.  Want this to correspond to 2000M, so
          scale up by 2^20 = 1M.  So period to initialise counter is
          (TP*sound_freq*2^17)/15625, which ranges up to 1.7 thousand million.  The
          countdown each time is 2^20, but we double this to 2^21 so that we can
          spot each half of the period.  To initialise the countdown, bit 0 of
          (time*2^21)/tonemodulo gives high or low.  The counter is initialised
          to tonemodulo-(time*2^21 mod tonemodulo).

__NOISE__ fudged similarly to tone.

__ENV__   Step period = 2EP/(15625*32) Hz.  Scale by 2^17 to scale 13124.5 to 1.7 thousand
          million.  Step period is (EP*sound_freq*2^13)/15625.

*/

#define VOLTAGE_FIXED_POINT 256
#define VFP VOLTAGE_FIXED_POINT
#define VZL VOLTAGE_ZERO_LEVEL
#define VA VFP*(PSG_CHANNEL_AMPLITUDE) // was 60 in old Steem

const int psg_flat_volume_level[16]={0*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,
  8*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,
  35*VA/1000+VZL*VFP,48*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,
  139*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,407*VA/1000
  +VZL*VFP,648*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP};

const int psg_envelope_level[8][64]={
    {1000*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,514*VA/1000
    +VZL*VFP,407*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,
    234*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,139*VA/1000+
    VZL*VFP,115*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,69*VA/1000
    +VZL*VFP,58*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,35*VA/1000
    +VZL*VFP,29*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,17*VA/1000
    +VZL*VFP,14*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,8*VA/1000
    +VZL*VFP,6*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,0*VA/1000+
    VZL*VFP,0*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,
    648*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,342*VA/1000+
    VZL*VFP,287*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,
    163*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,95*VA/1000+
    VZL*VFP,81*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,53*VA/1000+
    VZL*VFP,41*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,24*VA/1000+
    VZL*VFP,20*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,12*VA/1000+
    VZL*VFP,10*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,4*VA/1000+
    VZL*VFP,2*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP},
    {1000*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,514*VA/1000+
    VZL*VFP,407*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,
    234*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,139*VA/1000+
    VZL*VFP,115*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,69*VA/1000
    +VZL*VFP,58*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,35*VA/1000
    +VZL*VFP,29*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,17*VA/1000
    +VZL*VFP,14*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,8*VA/1000+
    VZL*VFP,6*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,0*VA/1000+
    VZL*VFP,0*VA/1000+VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,
    VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,
    VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,
    VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP},
    {1000*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,514*VA/1000+
    VZL*VFP,407*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,
    234*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,139*VA/1000+
    VZL*VFP,115*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,69*VA/1000
    +VZL*VFP,58*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,35*VA/1000
    +VZL*VFP,29*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,17*VA/1000
    +VZL*VFP,14*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,8*VA/1000+
    VZL*VFP,6*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,0*VA/1000+
    VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,2*VA/1000+
    VZL*VFP,4*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,10*VA/1000+
    VZL*VFP,12*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,20*VA/1000+
    VZL*VFP,24*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,41*VA/1000+
    VZL*VFP,53*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,81*VA/1000+
    VZL*VFP,95*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,
    163*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,287*VA/1000+
    VZL*VFP,342*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,
    648*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP},
    {1000*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,514*VA/1000+
    VZL*VFP,407*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,
    234*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,139*VA/1000+
    VZL*VFP,115*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,69*VA/1000
    +VZL*VFP,58*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,35*VA/1000
    +VZL*VFP,29*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,17*VA/1000
    +VZL*VFP,14*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,8*VA/1000+
    VZL*VFP,6*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,0*VA/1000+
    VZL*VFP,0*VA/1000+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+
    VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA
    +VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,
    VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+
    VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,
    VA+VZL*VFP},
    {0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,
    6*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,
    14*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,
    29*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,
    58*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,
    115*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,191*VA/1000+
    VZL*VFP,234*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,
    407*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,805*VA/1000+
    VZL*VFP,1000*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,2*VA/1000+
    VZL*VFP,4*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,10*VA/1000+
    VZL*VFP,12*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,20*VA/1000+
    VZL*VFP,24*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,41*VA/1000+
    VZL*VFP,53*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,81*VA/1000+
    VZL*VFP,95*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,
    163*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,287*VA/1000+
    VZL*VFP,342*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,
    648*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP},
    {0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,
    6*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,
    14*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,
    29*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,
    58*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,
    115*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,191*VA/1000+
    VZL*VFP,234*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,
    407*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,805*VA/1000+
    VZL*VFP,1000*VA/1000+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,
    VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+
    VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA
    +VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,
    VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA+VZL*VFP,VA
    +VZL*VFP,VA+VZL*VFP},
    {0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,
    6*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,
    14*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,
    29*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,
    58*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,
    115*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,191*VA/1000+
    VZL*VFP,234*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,
    407*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,805*VA/1000+
    VZL*VFP,1000*VA/1000+VZL*VFP,1000*VA/1000+VZL*VFP,805*VA/1000+VZL*VFP,
    648*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,407*VA/1000+VZL*VFP,342*VA/1000+
    VZL*VFP,287*VA/1000+VZL*VFP,234*VA/1000+VZL*VFP,191*VA/1000+VZL*VFP,
    163*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,115*VA/1000+VZL*VFP,95*VA/1000+
    VZL*VFP,81*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,58*VA/1000+VZL*VFP,53*VA/1000+
    VZL*VFP,41*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,29*VA/1000+VZL*VFP,24*VA/1000+
    VZL*VFP,20*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,14*VA/1000+VZL*VFP,12*VA/1000+
    VZL*VFP,10*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,6*VA/1000+VZL*VFP,4*VA/1000+
    VZL*VFP,2*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP},
    {0*VA/1000+VZL*VFP,0*VA/1000+VZL*VFP,2*VA/1000+VZL*VFP,4*VA/1000+VZL*VFP,
    6*VA/1000+VZL*VFP,8*VA/1000+VZL*VFP,10*VA/1000+VZL*VFP,12*VA/1000+VZL*VFP,
    14*VA/1000+VZL*VFP,17*VA/1000+VZL*VFP,20*VA/1000+VZL*VFP,24*VA/1000+VZL*VFP,
    29*VA/1000+VZL*VFP,35*VA/1000+VZL*VFP,41*VA/1000+VZL*VFP,53*VA/1000+VZL*VFP,
    58*VA/1000+VZL*VFP,69*VA/1000+VZL*VFP,81*VA/1000+VZL*VFP,95*VA/1000+VZL*VFP,
    115*VA/1000+VZL*VFP,139*VA/1000+VZL*VFP,163*VA/1000+VZL*VFP,191*VA/1000+
    VZL*VFP,234*VA/1000+VZL*VFP,287*VA/1000+VZL*VFP,342*VA/1000+VZL*VFP,
    407*VA/1000+VZL*VFP,514*VA/1000+VZL*VFP,648*VA/1000+VZL*VFP,805*VA/1000+
    VZL*VFP,1000*VA/1000+VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,
    VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,
    VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,
    VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP,VZL*VFP}};
#undef VFP
#undef VZL
#undef VA
#undef VOLTAGE_FIXED_POINT

#define PSG_ONE_MILLION 1048576
#define PSG_TWO_MILLION 2097152
#define PSG_TWO_TO_SEVENTEEN 131072

#define PSG_PREPARE_ENVELOPE          {                      \
      int envperiod=MAX( (((int)psg_reg[PSGR_ENVELOPE_PERIOD_HIGH]) <<8) \
        + psg_reg[PSGR_ENVELOPE_PERIOD_LOW],1);  \
      af=envperiod;                              \
      af*=sound_freq;                  \
      af*=((double)(1<<13))/15625;                               \
      psg_envmodulo=(int)af; \
      bf=(((DWORD)t)-psg_envelope_start_time); \
      bf*=(double)(1<<17); \
      psg_envstage=(int)floor(bf/af); \
      bf=fmod(bf,af); /*remainder*/ \
      psg_envcountdown=psg_envmodulo-(int)bf; \
      envdeath=-1;                                                                  \
      if ((psg_reg[PSGR_ENVELOPE_SHAPE]&PSG_ENV_SHAPE_CONT)==0 ||                  \
           (psg_reg[PSGR_ENVELOPE_SHAPE]&PSG_ENV_SHAPE_HOLD)){                      \
        if(psg_reg[PSGR_ENVELOPE_SHAPE]==11 || psg_reg[PSGR_ENVELOPE_SHAPE]==13)      \
          envdeath=psg_flat_volume_level[15];                                           \
        else                                                                          \
          envdeath=psg_flat_volume_level[0];                                              \
      }\
      envshape=(psg_reg[PSGR_ENVELOPE_SHAPE]&7);                    \
      if (psg_envstage>=32 && envdeath!=-1)                           \
        envvol=envdeath;                                             \
      else {                                                       \
        envvol=psg_envelope_level[envshape][psg_envstage & 63];            \
        /*if(OPTION_YM_12DB&&IS_STE&&OPTION_MICROWIRE)*/\
          /*envvol>>=1;*/\
        envvol>>=Microwire.PsgReduce; \
      }																															\
    }

#define PSG_PREPARE_NOISE  {                              \
      int noiseperiod=(1+(psg_reg[PSGR_NOISE_PERIOD]&0x1f));      \
      af=((double)noiseperiod*(double)sound_freq);                              \
      af*=((double)(1<<17))/15625; \
      psg_noisemodulo=(int)af; \
      bf=t; \
      bf*=(double)(1<<20); \
      bf=fmod(bf,af); \
      psg_noisecountdown=psg_noisemodulo-(int)bf; \
}

#define PSG_PREPARE_TONE       {                          \
      af=((double)toneperiod*(double)sound_freq);                              \
      af*=((double)(1<<17))/15625;                               \
      psg_tonemodulo_2=(int)af; \
      bf=(((DWORD)t)-psg_tone_start_time[abc]); \
      bf*=(double)(1<<21); \
      bf=fmod(bf,af*2); \
      af=bf-af;               \
      if(af>=0){                  \
        psg_tonetoggle=false;       \
        bf=af;                      \
      }                           \
      psg_tonecountdown=psg_tonemodulo_2-(int)bf; \
}

#define PSG_ENVELOPE_ADVANCE           {                        \
          psg_envcountdown-=PSG_TWO_TO_SEVENTEEN;  \
          while (psg_envcountdown<0){           \
            psg_envcountdown+=psg_envmodulo;             \
            psg_envstage++;                   \
            if (psg_envstage>=32 && envdeath!=-1){                           \
              envvol=envdeath;                                             \
            }else{                                                       \
              envvol=psg_envelope_level[envshape][psg_envstage & 63];            \
            }																															\
          }\
}


#define PSG_TONE_ADVANCE       {                            \
          psg_tonecountdown-=PSG_TWO_MILLION;  \
          while (psg_tonecountdown<0){           \
            psg_tonecountdown+=psg_tonemodulo_2;             \
            psg_tonetoggle=!psg_tonetoggle;                   \
          }\
}

#define PSG_NOISE_ADVANCE  {                         \
          psg_noisecountdown-=PSG_ONE_MILLION;   \
          while (psg_noisecountdown<0){   \
            psg_noisecountdown+=psg_noisemodulo;      \
            Psg.m_rng ^= (((Psg.m_rng & 1) ^ ((Psg.m_rng >> 3) & 1)) << 17); \
            Psg.m_rng >>= 1; \
            psg_noisetoggle=(Psg.m_rng&1);   \
          }\
}


void psg_write_buffer(BYTE abc,DWORD to_t) {
  if(!SSEConfig.YmSoundOn)
    return;
  //buffer starts at time time_of_last_vbl
  //we've written up to psg_buf_pointer[abc]
  //so start at pointer and write to to_t,
  int psg_tonemodulo_2,psg_noisemodulo;
  int psg_tonecountdown,psg_noisecountdown;
  double af,bf;
  bool psg_tonetoggle=true;
  int *p=psg_channels_buf+psg_buf_pointer[abc];
  DWORD t=psg_time_of_last_vbl_for_writing+psg_buf_pointer[abc];
  to_t=MAX(to_t,t);
  to_t=MIN(to_t,psg_time_of_last_vbl_for_writing+psg_channels_buf_len);
  int count=MAX(MIN((DWORD)(to_t-t),psg_channels_buf_len-psg_buf_pointer[abc]),0ul);
  if(!count)
    return;

  BYTE abcx2=abc<<1;
  BYTE abc8=abc+8;
  BYTE tonebit=1<<abc;
  BYTE noisebit=8<<abc;
#if defined(BIG_ENDIAN_PROCESSOR)
  int toneperiod=(((int)psg_reg[abcx2+1])<<8)+psg_reg[abcx2];
#else
  int toneperiod=*(WORD*)&psg_reg[abcx2];
#endif
  if((psg_reg[abc8]&BIT_4)==0)
  { // Not Enveloped
    int vol=psg_flat_volume_level[psg_reg[abc8]];
    //if(OPTION_YM_12DB&&IS_STE&&OPTION_MICROWIRE)
      //vol>>=1;
    vol>>=Microwire.PsgReduce;
    if((psg_reg[PSGR_MIXER]&tonebit)==0 /*&&(toneperiod>9)*/)
    { //tone enabled
      PSG_PREPARE_TONE
      if((psg_reg[PSGR_MIXER]&noisebit)==0)
      { //noise enabled
        PSG_PREPARE_NOISE
        for(;count>0;count--)
        {
          if(psg_tonetoggle
#if defined(SSE_DEBUGGER_MUTE_SOUNDCHANNELS)
            &&(!((4>>abc)&(d2_dpeek(FAKE_IO_START+20)>>12))) // chan
#endif
            ||psg_noisetoggle
#if defined(SSE_DEBUGGER_MUTE_SOUNDCHANNELS)
            &&(!((1<<11)&d2_dpeek(FAKE_IO_START+20))) // noise
#endif            
            )
            p++; // implicit =0
          else
            *(p++)+=vol;
          PSG_TONE_ADVANCE
          PSG_NOISE_ADVANCE
        }
      }
      else
      { //tone only
        for(;count>0;count--)
        {
          if(psg_tonetoggle
#if defined(SSE_DEBUGGER_MUTE_SOUNDCHANNELS)
            &&(!((4>>abc)&(d2_dpeek(FAKE_IO_START+20)>>12))) // chan
#endif
            )
            p++;
          else
            *(p++)+=vol;
          PSG_TONE_ADVANCE
        }
      }
    }
    else if((psg_reg[PSGR_MIXER]&noisebit)==0)
    { //noise enabled, not tone
      PSG_PREPARE_NOISE
      for(;count>0;count--)
      {
        if(psg_noisetoggle
#if defined(SSE_DEBUGGER_MUTE_SOUNDCHANNELS)
          &&(!((1<<11)&d2_dpeek(FAKE_IO_START+20))) // noise
#endif          
          )
          p++;
        else
          *(p++)+=vol;
        PSG_NOISE_ADVANCE
      }
    }
    else
    { //nothing enabled
      for(;count>0;count--)
        *(p++)+=vol;
    }
    psg_buf_pointer[abc]=to_t-psg_time_of_last_vbl_for_writing;
    return;
  }
  else
  {// Enveloped
    int envdeath,psg_envstage,envshape;
    int psg_envmodulo,envvol,psg_envcountdown;
    PSG_PREPARE_ENVELOPE;
    if((psg_reg[PSGR_MIXER]&tonebit)==0/*&&(toneperiod>9)*/)
    { //tone enabled
      PSG_PREPARE_TONE
      if((psg_reg[PSGR_MIXER]&noisebit)==0)
      { //noise enabled
        PSG_PREPARE_NOISE
        for(;count>0;count--)
        {
          if(psg_tonetoggle
#if defined(SSE_DEBUGGER_MUTE_SOUNDCHANNELS)
            &&(!((4>>abc)&(d2_dpeek(FAKE_IO_START+20)>>12))) // chan
            &&(!((1<<10)&d2_dpeek(FAKE_IO_START+20))) // env
#endif
            ||psg_noisetoggle
#if defined(SSE_DEBUGGER_MUTE_SOUNDCHANNELS)
            &&(!((1<<11)&d2_dpeek(FAKE_IO_START+20))) // noise
#endif            
            )
            p++;
          else
            *(p++)+=envvol;
          PSG_TONE_ADVANCE
          PSG_NOISE_ADVANCE
          PSG_ENVELOPE_ADVANCE
        }
      }
      else
      { //tone only
        for(;count>0;count--)
        {
          if(psg_tonetoggle
#if defined(SSE_DEBUGGER_MUTE_SOUNDCHANNELS)
            &&(!((4>>abc)&(d2_dpeek(FAKE_IO_START+20)>>12))) // chan
            &&(!((1<<10)&d2_dpeek(FAKE_IO_START+20))) // env
#endif            
            )
            p++;
          else
            *(p++)+=envvol;
          PSG_TONE_ADVANCE
          PSG_ENVELOPE_ADVANCE
        }
      }
    }
    else if((psg_reg[PSGR_MIXER]&noisebit)==0)
    { //noise enabled
      PSG_PREPARE_NOISE
      for(;count>0;count--)
      {
        if(psg_noisetoggle
#if defined(SSE_DEBUGGER_MUTE_SOUNDCHANNELS)
          &&(!((1<<11)&d2_dpeek(FAKE_IO_START+20))) // noise
#endif          
          )
          p++;
        else
          *(p++)+=envvol;
        PSG_NOISE_ADVANCE
        PSG_ENVELOPE_ADVANCE
      }
    }
    else
    { //nothing enabled
      for(;count>0;count--)
      {
        *(p++)+=envvol;
        PSG_ENVELOPE_ADVANCE
      }
    }
    psg_buf_pointer[abc]=to_t-psg_time_of_last_vbl_for_writing;
  }
}


DWORD psg_adjust_envelope_start_time(DWORD t,DWORD new_envperiod) {
  double b,c;
  int envperiod=MAX((((int)psg_reg[PSGR_ENVELOPE_PERIOD_HIGH])<<8)
    +psg_reg[PSGR_ENVELOPE_PERIOD_LOW],1);
  b=t-psg_envelope_start_time;
  c=b*(double)new_envperiod;
  c/=(double)envperiod;
  c=t-c;         //new env start time
  return (DWORD)c;
}

#undef PSG_ONE_MILLION
#undef PSG_TWO_MILLION
#undef PSG_TWO_TO_SEVENTEEN


//////////////////////
// SOUND LJBK TABLE //
//////////////////////

/*  It's a table for the 3 channels at once, because there's some interaction
*   between channels on the ST.
*   The sound is very saturated.
*/

void TYM2149::FreeFixedVolTable() {
  if(p_fixed_vol_3voices)
  {
    TRACE_LOG("free memory of PSG table %p\n",p_fixed_vol_3voices);
    delete [] p_fixed_vol_3voices;
    p_fixed_vol_3voices=NULL;
  }
}


bool TYM2149::LoadFixedVolTable(bool check_dmamix) {
  bool ok=false;
  FreeFixedVolTable();
  p_fixed_vol_3voices=new WORD[16*16*16];
  ZeroMemory(p_fixed_vol_3voices,sizeof(WORD)*16*16*16);//warning
  // first look for the file, if it isn't there, use internal resource
  EasyStr filename=RunDir+SLASH+SSE_PLUGIN_DIR1+SLASH+YM2149_FIXED_VOL_FILENAME;
  FILE *fp=fopen(filename.Text,"r+b");
  if(!fp)
  {
    filename=RunDir+SLASH+SSE_PLUGIN_DIR2+SLASH+YM2149_FIXED_VOL_FILENAME;
    fp=fopen(filename.Text,"r+b");
  }
  if(!fp)
  {
    filename=RunDir+SLASH+YM2149_FIXED_VOL_FILENAME;
    fp=fopen(filename.Text,"r+b");
  }
  if(fp)
  {
    int nwords=(int)FREAD(p_fixed_vol_3voices,sizeof(WORD),16*16*16,fp);
    if(nwords==16*16*16)
      ok=true;
    TRACE_LOG("PSG %s loaded %d\n",CHECKPATH(filename.Text),ok);
    fclose(fp);
  }
#if 1 && defined(SSE_FILES_IN_RC)  
  else
  {
#ifdef WIN32
    HRSRC rc=FindResource(hInstance,MAKEINTRESOURCE(IDR_YM2149),RT_RCDATA);
    //ASSERT(rc);
    if(rc)
#endif
    {
#ifdef WIN32
      HGLOBAL hglob=LoadResource(hInstance,rc);
      if(hglob)
#endif
      {
#ifdef WIN32        
        size_t size=SizeofResource(hInstance,rc);
        BYTE *pdata=(BYTE*)LockResource(hglob);
#endif
#ifdef UNIX
        BYTE *pdata=(BYTE*)Get_ym2149_fixed_vol_bin();
        BYTE *pdata_end=(BYTE*)Get_ym2149_fixed_vol_bin_end();
        size_t size=pdata_end-pdata;
#endif
        if(pdata && size==sizeof(WORD)*16*16*16) //8Kb
        {
          memcpy(p_fixed_vol_3voices,pdata,size);
          ok=true;
          TRACE_LOG("PSG table loaded in %p\n",p_fixed_vol_3voices);
        }
      }
    }
  }
#endif  

/*  In previous versions, the zero (silence) was a very negative value,
    for sampled YM as well as for 'Steem native'.
    If we want the zero to be set at the zero of a signed 16bit wave, we
    must reduce the range in both tables.
    So we simply lose the last bit to half the range.
    I doubt the ST has a 16bit dynamic range anyway :), we lose nothing.
    All values are positive, the signed sound wave will be totally lopsided.*/
#if defined(SSE_GUI_421_REORDER)
  int shift=Microwire.PsgReduce;//MAX(Microwire.PsgReduce,(BYTE)1);
  if(IS_STE&&OPTION_MICROWIRE)
  {
    if(check_dmamix&&Microwire.mixer==2)
      shift=16; // zeroes
  }
#else
  int shift=1;
/*  STE sound mixer
    According to Atari doc
    00 -12 dB
    01 Mix GI sound chip output
    10 Do not mix GI sound chip output
    11 reserved
    But the -12 dB setting doesn't work (hardware bug), that's why there's a 
    convenience option (hack) in Steem
    Pacemaker writes 0.*/

  if(IS_STE&&OPTION_MICROWIRE)
  {
    //if(OPTION_YM_12DB)
      //shift=2;
    if(Microwire.PsgReduce)
      shift=Microwire.PsgReduce;
    else if(check_dmamix&&Microwire.mixer==2)
      shift=16; // zeroes
  }
#endif
  for(int i=0;i<16*16*16;i++)
    p_fixed_vol_3voices[i]>>=shift; 
  SSEConfig.ym2149_fixed_vol=ok;
  return ok;
}


/////////////////////
// SOUND LOW-LEVEL //
/////////////////////

#if defined(SSE_YM2149_LL)

/*  The following was inspired by MAME project, especially ay8910.cpp.
    thx Couriersud.
    Notice the emulation is both simple and short. But tests in VS2015 profiler
    show that it uses more CPU than Steem's way (PREPARE, ADVANCE...), a good
    deal of that is used by the antialiasing filter.
    We take advantage of more powerful computers... */


#if defined(SSE_DEBUGGER_MUTE_SOUNDCHANNELS)
#define NOISE_OUTPUT() (((1<<11)&d2_dpeek(FAKE_IO_START+20))?0:(m_rng & 1))
#else
#define NOISE_OUTPUT()          (m_rng & 1)
#endif

#if 1 // opt: computed at write

#define NOISE_ENABLEQ(_chan)  noise_enabled[_chan]
#define TONE_ENABLEQ(_chan)   tone_enabled[_chan]
#define TONE_PERIOD(_chan) tone_period[_chan]
#define ENVELOPE_PERIOD() env_period

#else

#define NOISE_ENABLEQ(_chan)  ((psg_reg[PSGR_MIXER] >> (3 + _chan)) & 1)
#define TONE_ENABLEQ(_chan)   ((psg_reg[PSGR_MIXER] >> (_chan)) & 1)

#define TONE_PERIOD(_chan)\
  ( psg_reg[(_chan) << 1] | ((psg_reg[((_chan) << 1) | 1] & 0x0f) << 8) )
#define ENVELOPE_PERIOD()       ((psg_reg[PSGR_ENVELOPE_PERIOD_LOW] \
  | (psg_reg[PSGR_ENVELOPE_PERIOD_HIGH]<<8)))

#endif

void check_mamelike_ym_envelope(BYTE new_val) {
  // an apart function because we want to update low-level YM's envelope ('buzzer') 
  // parameters also in fast forward or muted modes - like the rest of lle-ym,
  // code was lifted from MAME
  Psg.m_attack=(new_val&0x04) ? Psg.m_env_step_mask : 0x00;
  if((new_val&0x08)==0)
  {
    /* if Continue = 0, map the shape to the equivalent one which has Continue = 1 */
    Psg.m_hold=1;
    Psg.m_alternate=Psg.m_attack;
  }
  else
  {
    Psg.m_hold=new_val&0x01;
    Psg.m_alternate=new_val&0x02;
  }
  Psg.m_env_step=Psg.m_env_step_mask;
  Psg.m_holding=0;
  //ASSERT(Psg.m_env_step>=0);
  Psg.m_env_volume=(Psg.m_env_step ^ Psg.m_attack); //no need?
  Psg.m_count_env=0; // (from Hatari)
  written_to_env_this_vbl=true;
}


void TYM2149::psg_write_buffer(DWORD to_t, bool vbl) {
  //ASSERT(OPTION_MAME_YM);

  if(!psg_n_samples_this_vbl||!SSEConfig.YmSoundOn)
    return;

  // compute #samples at our current sample rate
  DWORD t=(psg_time_of_last_vbl_for_writing+psg_buf_pointer[0]);
  to_t=MAX(to_t,t);
  to_t=MIN(to_t,psg_time_of_last_vbl_for_writing+psg_channels_buf_len);
  int count=MAX(MIN((DWORD)(to_t-t),psg_channels_buf_len-psg_buf_pointer[0]),0ul);
  if(!count && !AntiAlias)
    return;

  int *p=psg_channels_buf+psg_buf_pointer[0];

  if(!AntiAlias)
    *p=0;

  // YM2149 @2mhz = 1/4 * 8mhz clock  - On STF, same as CPU
  // when is next sample due?
  COUNTER_VAR time_to_send_next_sample=m_cycles+(int)ym2149_cycles_per_sample;

  COUNTER_VAR ym2149_cycles_at_start_of_loop=m_cycles;
  int samples_sent=0;

  COUNTER_VAR cycles_to_run=0;
  if(AntiAlias)
  {
    DWORDLONG m8cycles=FRAMECYCLES;
    if(IS_STE) // on STE, independent quartz ~8MHz
    {
      m8cycles*=STE_CLOCK8*TICKS8; // int32 would overflow
      m8cycles/=CpuNormalHz;
    }
    COUNTER_VAR psg_frame_cycles=(COUNTER_VAR)m8cycles/(4*TICKS8);
    COUNTER_VAR psg_already_run=m_cycles-time_at_vbl_start;
    if(psg_already_run<0)
      time_at_vbl_start=m_cycles,psg_already_run=0;
    cycles_to_run=psg_frame_cycles-psg_already_run;
    if(cycles_to_run<=0)
      return;
  }

  /* The 8910 has three outputs, each output is the mix of one of the three */
  /* tone generators and of the (single) noise generator. The two are mixed */
  /* BEFORE going into the DAC. The formula to mix each channel is: */
  /* (ToneOn | ToneDisable) & (NoiseOn | NoiseDisable). */
  /* Note that this means that if both tone and noise are disabled, the output */
  /* is 1, not 0, and can be modulated changing the volume. */

  /* buffering loop */
  
  for(int i=0; (AntiAlias&&!vbl) ? (i<cycles_to_run) : count;i+=8) // override vbl: Steem may want to run longer TODO
  {
    m_cycles+=8;  //the driver is clocked with clock / 8  (250Khz)

    //SS We compute noise then envelope, then we compute each tone and
    // mix each channel

    m_count_noise++;
#ifdef SSE_LEAN_AND_MEAN
    if (m_count_noise >= psg_reg[PSGR_NOISE_PERIOD])
#else
    if (m_count_noise >= (psg_reg[PSGR_NOISE_PERIOD] & 0x1f))
#endif
    {
      /* toggle the prescaler output. Noise is no different to
       * channels.
       */
      m_count_noise = 0;
      m_prescale_noise ^= 1;

      if (m_prescale_noise)
      {
        /* The Random Number Generator of the 8910 is a 17-bit shift */
        /* register. The input to the shift register is bit0 XOR bit3 */
        /* (bit0 is the output). This was verified on AY-3-8910 and YM2149 chips. */
        m_rng ^= (((m_rng & 1) ^ ((m_rng >> 3) & 1)) << 17);
        m_rng >>= 1;
      }
    }

    /* update envelope */
    if (m_holding == 0)
    {
      m_count_env++;
      if (m_count_env >= ENVELOPE_PERIOD()) // "m_step"=1 for YM2149
      {
        m_count_env = 0;
        m_env_step--;

        /* check envelope current position */
        if (m_env_step < 0)
        {
          if (m_hold)
          {
            if (m_alternate)
              m_attack ^= m_env_step_mask;
            m_holding = 1;
            m_env_step = 0;
          }
          else
          {
            /* if CountEnv has looped an odd number of times (usually 1), */
            /* invert the output. */
            if (m_alternate && (m_env_step & (m_env_step_mask + 1)))
              m_attack ^= m_env_step_mask;

            m_env_step &= m_env_step_mask;
          }
        }
      }
    }
    m_env_volume = (m_env_step ^ m_attack);

    //as in psg's AlterV
    BYTE index[NUM_CHANNELS],interpolate[4];
    *(int*)interpolate=0;
    int vol=0;
    //TRACE_OSD("%d %d %d",TONE_PERIOD(0),TONE_PERIOD(1),TONE_PERIOD(2));
    for(int abc=0;abc<NUM_CHANNELS;abc++)
    {
      // Tone
      BYTE enveloped=((psg_reg[abc+8] & BIT_4)!=0);
      m_count[abc]++;
#if defined(SSE_DEBUGGER_MUTE_SOUNDCHANNELS)
      if(!((4>>abc)&(d2_dpeek(FAKE_IO_START+20)>>12))) // chan      
#endif
      if(m_count[abc]>=TONE_PERIOD(abc)) 
      {
        //ASSERT(!(m_output[abc]&0xfe));
        m_output[abc] ^= 1;
        m_count[abc]=0;
      }

      // mixing
      m_vol_enabled[abc] = (m_output[abc] | TONE_ENABLEQ(abc)) 
        & (NOISE_OUTPUT() | NOISE_ENABLEQ(abc));

      // from here on, specific to Steem rendering
      // pick correct volume or 0
      if(!m_vol_enabled[abc])
        index[abc]=0;
      else if(enveloped)
      {
#if defined(SSE_DEBUGGER_MUTE_SOUNDCHANNELS)
        if(!((1<<10)&d2_dpeek(FAKE_IO_START+20))) // env
#endif
        index[abc]=(BYTE)(m_env_volume>>1); // vol 5bit -> 4bit (our table)
      }
      else
#ifdef SSE_LEAN_AND_MEAN
        index[abc]=(psg_reg[abc+8]); // vol 4bit shifted
#else
        index[abc]=(psg_reg[abc+8]&0xF); // vol 4bit shifted
#endif
      ASSERTB((index[abc]&0xF0)==0);
      interpolate[abc]=(enveloped && index[abc]>0 && !(m_env_volume&1)) ? 1 : 0;
    }//nxt abc
    vol=p_fixed_vol_3voices[(16*16)*index[2]+16*index[1]+index[0]]; // is it the correct order?
    if(*(int*)(&interpolate[0])) // should interpolate at least one channel
    {
      int vol2=p_fixed_vol_3voices[(16*16)*(index[2]-interpolate[2])
        +16*(index[1]-interpolate[1])+(index[0]-interpolate[0])];
      vol=(int)sqrt((double)vol*(double)vol2); // never clear to me which is better
      //vol=(int)sqrt((float)vol * (float)vol2); // glitches? or is it random
    }

    // Thanks to this kick-ass filter, we can avoid horrible aliasing in all
    // sample rates.
    *p=(AntiAlias) ? (int)AntiAlias->do_sample((double)vol) : vol;

    if(m_cycles-time_to_send_next_sample>=0
      && (unsigned)(p-psg_channels_buf)<=psg_channels_buf_len)
    {
      int copy=*p;
      *(++p)=copy; //same value, not zero
      count--;
      samples_sent++;
      frame_samples++;
      time_of_last_sample=m_cycles;
      if(AntiAlias!=NULL)
      {
        time_to_send_next_sample=(COUNTER_VAR)((double)time_at_vbl_start
          +((double)frame_samples+1.0)*ym2149_cycles_per_sample);
      }
      else
        time_to_send_next_sample=ym2149_cycles_at_start_of_loop
          +(int)(((double)samples_sent+1)*ym2149_cycles_per_sample);
    }
  }
  psg_buf_pointer[0]=to_t-psg_time_of_last_vbl_for_writing;
  psg_buf_pointer[2]=psg_buf_pointer[1]=psg_buf_pointer[0];
}


void TYM2149::Restore() {
  m_env_step_mask=ENVELOPE_MASK;
  ASSERTB(sound_freq);
#ifndef SSE_LEAN_AND_MEAN
  if(sound_freq)
#endif
  {
    ym2149_cycles_per_sample= (IS_STE) 
      ? (((double)STE_CLOCK8/4)/(double)sound_freq)
      : (((double)CpuNormalHz/(4*TICKS8))/(double)sound_freq);
  }
}

#endif//mame-like


#undef ENVELOPE_MASK
#undef PSG_ENV_SHAPE_HOLD
#undef PSG_ENV_SHAPE_CONT


///////////////////
// DRIVE CONTROL //
///////////////////

BYTE TYM2149::CurrentDrive() {
  BYTE drive=NO_VALID_DRIVE; // different from floppy_current_drive(); internal code
  switch(psg_reg[PSGR_PORT_A]&(BIT_2|BIT_1)) {
  case BIT_2:
    drive=DRIVE_A;
    break;
  case BIT_1:
    drive=DRIVE_B;
    break;
  case 0:
    // selecting both drives (both bits cleared) still selects drive A:
    // if drive B: is not connected - Captain Dynamo
    if(DiskMan.nFloppyDrives==1)
      drive=DRIVE_A;
    break;
  }
#if defined(SSE_DRIVE_FREEBOOT)
  if(DiskMan.nFloppyDrives>1 && FloppyDrive[DRIVE_B].bFreeboot && drive!=NO_VALID_DRIVE)
    drive=!drive;
#endif
  return drive;
}


#if defined(SSE_DRIVE_FREEBOOT)

void TYM2149::CheckFreeboot() {
  // this is used by CURRENT_SIDE hence the MFM manager
  if(SelectedDrive==DRIVE_A && FloppyDrive[SelectedDrive].bFreeboot)
    SelectedSide=1;
}

#endif

#undef LOGSECTION
