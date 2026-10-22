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
FILE: libretro-core.cpp
CONDITION: SSE_LIBRETRO must be defined
DESCRIPTION: This is the main source file of the libretro core, linked as
a DLL.
Libretro is a simple API that allows for the creation of games and emulators. 
The Steem core uses the API so that it can be integrated into RetroArch, a
program that endeavours to run all sorts of emulators.
TODO: Linux core
---------------------------------------------------------------------------*/


#include <pch.h>
#pragma hdrstop

#if defined(SSE_LIBRETRO)

#include "libretro.h"
#include "libretro-core.h"
#include <conditions.h>

#include <mymisc.h>
#include <debug.h>
#include <gui.h>
#include <options.h>
#include <computer.h>
#include <draw.h>
#include <run.h>
#include <osd.h>
#include <interface_stvl.h>
#include <stjoy.h>
#include <display.h>
#include <loadsave.h>
#include <key_table.h>



#if defined(SSE_LIBRETRO_DRIVESOUND) && !defined(SSE_LIBRETROSOUND2)
extern IDirectSound *DSObj;
#endif

// EmuTOS embedded in object code as BYTE array called tos_data
// EmuTOS can work with STE (vanilla) & STF (hacky)
#include "emutos.h"

// 3 possible resolutions in ST, and borders, we use one size for all
#define VIDEO_WIDTH ((320+32*2)*2) //border 40?
#define VIDEO_HEIGHT 540
#define VIDEO_PIXELS (VIDEO_WIDTH * VIDEO_HEIGHT)

static retro_video_refresh_t video_cb;
retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_environment_t environ_cb;

void handle_kbd_evt(bool down,unsigned keycode,uint32_t character,uint16_t key_modifiers);
retro_keyboard_event_t keyboard_cb;

static uint8_t *frame_buf;
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;
static bool use_audio_cb;
static float last_aspect;
static float last_sample_rate;
char retro_base_directory[4096];
char retro_game_path[4096];
static unsigned phase;

#if defined(SSE_LIBRETROSOUND1)
int snd_max_frames=0;
DWORD n_samples_this_vbl=0;
#endif

WORD CoreFreq=50; // Euro games

void check_options();

#if defined(SSE_LIBRETROMULTIDISK) //looks verbose

 /* Overview: To swap a disk image, eject the disk image with
 * set_eject_state(true).
 * Set the disk index with set_image_index(index). Insert the disk again
 * with set_eject_state(false).
 */
static bool retro_set_eject_state(bool ejected)
{
  TRACEB("retro_set_eject_state(%d)\n",ejected);
  if(ejected && !DumbDiskSwapper.ejected)
    FloppyDrive[DRIVE_A].RemoveDisk();
  if(!ejected && DumbDiskSwapper.ejected)
    FloppyDrive[DRIVE_A].SetDisk(DumbDiskSwapper.image_path);
  DumbDiskSwapper.ejected=ejected;
  return true;
}

/* Gets current eject state. The initial state is 'not ejected'. */
static bool retro_get_eject_state(void)
{
  TRACEB("retro_get_eject_state %d\n",DumbDiskSwapper.ejected);
  return DumbDiskSwapper.ejected;
}

static unsigned retro_get_image_index(void)
{
  TRACEB("retro_get_image_index %d\n",DumbDiskSwapper.image_index);
  return DumbDiskSwapper.image_index;
}


/* Sets image index. Can only be called when disk is ejected.
 * The implementation supports setting "no disk" by using an
 * index >= get_num_images().
 * 
 * When the game asks for it, you can change the current disk in the RetroArch "Disc Control" menu:

    Eject the current disk with "Eject Disc"
    Select the right disk index with "Current Disc Index"
    Insert the new disk with "Insert Disc"
 */
static bool retro_set_image_index(unsigned index)
{
  TRACEB("retro_set_image_index(%d)\n",index);
  DumbDiskSwapper.GetPath((WORD)index);
  return true;
}

static unsigned retro_get_num_images(void)
{
  TRACEB("retro_get_num_images %d\n",DumbDiskSwapper.num_images);
  return DumbDiskSwapper.num_images;
}

static bool retro_replace_image_index(unsigned index, const struct retro_game_info *info)
{
  TRACEB("retro_replace_image_index(%d)\n",index);
  return false;
}

/* Adds a new valid index (get_num_images()) to the internal disk list.
 * This will increment subsequent return values from get_num_images() by 1.
 * This image index cannot be used until a disk image has been set
 * with replace_image_index. */
static bool retro_add_image_index(void)
{
  TRACEB("retro_get_num_images\n");
  return false;
}

static bool retro_get_image_path(unsigned index, char *path, size_t len)
{
  TRACEB("retro_get_image_path(%d)\n",index);
  if(len>0 && index<DumbDiskSwapper.num_images)
  {
    strncpy(path,DumbDiskSwapper.GetPath((WORD)index).Text,len);
    TRACEB("%s\n",path);
    return true;
  }
  return false;
}

static bool retro_get_image_label(unsigned index, char *label, size_t len)
{
  TRACEB("retro_get_image_label(%d)\n",index);
  if(retro_get_image_path(index,label,len))
  {
    label=GetFileNameFromPath(label);
    TRACEB("%s\n",label);
    return true;
  }
  return false;
}


retro_disk_control_callback disk_interface = {
   retro_set_eject_state,
   retro_get_eject_state,
   retro_get_image_index,
   retro_set_image_index,
   retro_get_num_images,
   retro_replace_image_index,
   retro_add_image_index,
};

retro_disk_control_ext_callback diskControlExt  = {
   retro_set_eject_state,
   retro_get_eject_state,
   retro_get_image_index,
   retro_set_image_index,
   retro_get_num_images,
   retro_replace_image_index,
   retro_add_image_index,
   NULL, // set_initial_image
   retro_get_image_path,
   retro_get_image_label,
};

#endif

#ifdef WIN32

// called on loading the DLL then multiple times
extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance,DWORD dwReason,LPVOID /*lpReserved*/) {
  if(dwReason==DLL_PROCESS_ATTACH)
    ::hInstance=hInstance;
  return TRUE;
}

#endif//WIN32

bool get_default_tos() {
  TRACEB("get_default_tos()\n");
  MEM_ADDRESS new_rom_addr=(MEM_ADDRESS)tos_data[5]<<16;
  ASSERTB(new_rom_addr==0xFC0000||new_rom_addr==0xE00000);
  DWORD Len=(new_rom_addr==0xFC0000) ? (192*1024) : (256*1024); // should be 256K
  if(STRom)
    delete[] STRom;
  STRom=new BYTE[Len];
  tos_len=Len;
  rom_addr=new_rom_addr;
  rom_addr_end=rom_addr+tos_len;
#ifndef BIG_ENDIAN_PROCESSOR
  Rom_End=STRom+tos_len;
  Rom_End_minus_1=Rom_End-1;
  Rom_End_minus_2=Rom_End-2;
  Rom_End_minus_4=Rom_End-4;
#endif
  memset(STRom,0xff,Len);
  for(DWORD m=0;m<Len;m++) // little-endian: backwards
  {
    ROM_PEEK(m)=tos_data[m];
  }
  tos_version=ROM_DPEEK(2);
  SSEConfig.TosLanguage=ROM_PEEK(0x1D);
  ST_MODEL=STE;
  //SSEConfig.SwitchSTModel(ST_MODEL); // to adapt CPU clock
  Glue.Restore(); // for Decode before Power On so that IR is correct!
  return true;
}


static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   (void)level;
   va_list va;
   va_start(va, fmt);
   vfprintf(Debug.trace_file_pointer, fmt, va);
   va_end(va);
}


/*  This function is called once, and gives the implementation a chance
to initialize data structures.
*/
void retro_init(void) {

  // build stem_version_text eg "3.7.0" - quite complicated for what it does
  int d1=SSE_VERSION/100;
  int d2=(SSE_VERSION-d1*100)/10;
  int d3=SSE_VERSION-d1*100-d2*10;
  sprintf(stem_version_text,"%d.%d.%d",d1,d2,d3);
  sprintf(gAppBuildInfo,"%s R%d %dbit",stem_version_text,SSE_VERSION_R,SSE_BITNESS);
  
  SSEConfig.ShowNotify=0;

  keyboard_cb=handle_kbd_evt;

  frame_buf = (uint8_t*)malloc(VIDEO_PIXELS * sizeof(uint32_t));
  draw_mem=(draw_type*)frame_buf;
  Disp.VideoMemoryEnd=draw_mem+VIDEO_PIXELS;

  const char* dir = NULL;
  if(environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY,&dir) && dir)
  {
    snprintf(retro_base_directory,sizeof(retro_base_directory),"%s",dir); // /system

    // our RunDir will be /system/SteemSSE inside the RetroArch installation (32bit or 64bit)
    RunDir=dir;
    RunDir+=SLASH;
    RunDir+="SteemSSE";
    if(!Exists(RunDir))
    {
      CreateDirectory(RunDir.Text,NULL);
    }
    WriteDir=RunDir;

#if defined(SSE_WRITEDIR)
    UsersPath=RunDir;
    TempPath=RunDir;
#endif
    Debug.TraceStartStop();
    TRACE("RunDir %s\n",RunDir.Text);
    FloppyDrive[0].Id=0;
    FloppyDrive[0].Init();
    FloppyDrive[1].Id=1;
    FloppyDrive[1].Init();
#if defined(SSE_LIBRETRO_DRIVESOUND)
    DriveSoundDir[0]=RunDir;
    DriveSoundDir[1]=RunDir;
#endif
#if defined(SSE_LIBRETROSOUND2) || defined(SSE_LIBRETRO_DRIVESOUND)
    InitSound();
#endif
#if defined(SSE_LIBRETRO_DRIVESOUND) && defined(SSE_LIBRETROSOUND1)
    DSBUFFERDESC dsbd;
    WAVEFORMATEX wfx;
    wfx.wFormatTag=WAVE_FORMAT_PCM;
    wfx.nChannels=sound_num_channels;
    wfx.nSamplesPerSec=(DWORD)(sound_freq);
    wfx.wBitsPerSample=sound_num_bits;
    wfx.nBlockAlign=sound_bytes_per_sample;
    wfx.nAvgBytesPerSec=wfx.nSamplesPerSec*wfx.nBlockAlign;
    wfx.cbSize=0;
    ZeroMemory(&dsbd,sizeof(DSBUFFERDESC));
    dsbd.dwSize=sizeof(DSBUFFERDESC);
    dsbd.dwFlags=DSBCAPS_CTRLVOLUME|DSBCAPS_GLOBALFOCUS
      |DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_STICKYFOCUS;
    //dsbd.dwBufferBytes=psg_buf_length * sound_bytes_per_sample;
    dsbd.lpwfxFormat=&wfx;
    FloppyDrive[DRIVE_A].SoundLoadSamples(DSObj,&dsbd,&wfx);
    FloppyDrive[DRIVE_B].SoundLoadSamples(DSObj,&dsbd,&wfx);
#endif//#if defined(SSE_LIBRETRO_DRIVESOUND) && defined(SSE_LIBRETROSOUND1)

  }//if

#if defined(SSE_LIBRETROMULTIDISK)
  // Disk control interface
   unsigned dci_version = 0;
   if (environ_cb(RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION, &dci_version) && (dci_version >= 1))
      environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, &diskControlExt);
   else
      environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &disk_interface);
#endif

  environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &keyboard_cb);

  check_options();

  srand(timeGetTime());
  make_crc32_table();
  fdc_make_crc16_table();

  cpu_routines_init();
#if defined(SSE_420R4)
  PCpal=Get_PCpal(); // defined in draw_c or asm_draw.asm
#ifndef SSE_NO_OSD
  osd_routines_init();
#endif
#else
  draw_routines_init();
#endif
  draw_init_resdependent();

  Ikbd.Init();

  // default EmuTOS
  get_default_tos();

  // default 1MB RAM
  BYTE ConfigBank1=MEMCONF_512;
  BYTE ConfigBank2=MEMCONF_512;
  SSEConfig.make_Mem(ConfigBank1,ConfigBank2);

  SetTimingFunctions();

  power_on();
  reset_peripherals(true);

  DISPLAY_SIZE=2;
  draw_line_length=VIDEO_WIDTH;
  draw_dest_ad=draw_mem;
  draw_dest_next_scanline=draw_dest_ad+draw_dest_increase_y;
  if(Draw.Buffered) //?
  {
    draw_store_dest_ad=draw_dest_ad;
    draw_dest_ad=draw_temp_line_buf;
    Draw.limit2=draw_temp_line_buf_lim;
  }
  else
    Draw.limit2=Disp.VideoMemoryEnd;
  Draw.limit1=draw_dest_ad;
  ChangeBorderSize(1);
  border_last_chosen=border=1;
  SSEConfig.IsInit=TRUE;

  ComputerRestore(); // wich calls Draw.MarshalParameters()
}


// called on close core or quit
void retro_deinit(void)
{
  TRACEB("retro_deinit()\n");
  OsdControl.no_draw=true;
  free(frame_buf);
  frame_buf = NULL;
#if defined(SSE_LIBRETRO_DRIVESOUND) || defined(SSE_LIBRETROSOUND2)
  SoundRelease();
#endif
}


unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}


void retro_set_controller_port_device(unsigned port, unsigned device)
{
   log_cb(RETRO_LOG_INFO, "Plugging device %u into port %u.\n", device, port);
}


/*  The frontend will typically request statically known information about the
core such as the name of the implementation, version number, etc. The information
returned must be allocated statically.*/
/* RetroArch uses .info files with this and more (licence...) information
*/
void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "SteemSSE";
   info->library_version  = stem_version_text; //"4.2.0"; //TODO
/*There are two modes of loading files with libretro. If the game engine requires to know the path
of where the ROM image was loaded from, the need_fullpath field in retro_system_info must
be set to true. If the path is required, the frontend will not load the file into the data/size fields,
and it is up to the implementation to load the file from disk. The path might be both relative and
absolute, and the implementation must check for both cases. This is useful if the ROM image is
too large to load into memory at once. It is also useful if the assests consist of many smaller
files, where it is necessary to know the path of a master file to infer the paths of the others.*/
   info->need_fullpath    = true;
   info->valid_extensions = "m3u|st|msa|dim|stx|scp|ipf|ctr|stw|hfe"; //duplicate in .info file, disable file cache in retroarch
   info->block_extract=false;
   //TODO m3u must be handled
}


/*This function lets the frontend know essential audio/video properties of the game. As this
information can depend on the game being loaded, this info will only be queried after a valid
ROM image has been loaded. It is important to accurately report FPS and audio sampling rates,
as FFmpeg recording relies on exact information to be able to run in sync for several hours.
this is called once after load_game() */
void retro_get_system_av_info(struct retro_system_av_info *info)
{
   TRACEB("retro_get_system_av_info\n");
   float aspect                = 0.0f; 
   float sampling_rate         = 44100.0f;//TODO

   info->geometry.base_width   = VIDEO_WIDTH;
   info->geometry.base_height  = VIDEO_HEIGHT;
   info->geometry.max_width    = VIDEO_WIDTH;
   info->geometry.max_height   = VIDEO_HEIGHT;
   info->geometry.aspect_ratio = aspect;

   info->timing.fps=CoreFreq;
   info->timing.sample_rate=sampling_rate;

   last_aspect                 = aspect;
   last_sample_rate            = sampling_rate;
}


/* Sets callbacks. retro_set_environment() is guaranteed to be called
 * before retro_init().
 *
 * The rest of the set_* functions are guaranteed to have been called
 * before the first call to retro_run() is made. */
/* This is called on Load Core, twice */
void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   bool allow_no_game = true;  // start possible with no disk -> GEM
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &allow_no_game);

   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
      log_cb = logging.log;
   else
      log_cb = fallback_log;

   static const struct retro_controller_description controllers[] = {
//      { "Mouse", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_MOUSE, 0) },
//      { "Nintendo DS", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1) },
     { "Keyboard", RETRO_DEVICE_ST_KEYBOARD },
     { "Joystick1", RETRO_DEVICE_ST_JOYSTICK0 }, // most common joystick port on ST is 1
     { "Joystick0", RETRO_DEVICE_ST_JOYSTICK1 },
     { "Mouse", RETRO_DEVICE_ST_MOUSE },
   };

   static const struct retro_controller_info ports[] = {
      { controllers, 1+1 },
      { NULL, 0 },
   };

   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

   struct retro_variable variables[] = {
     {"sse_screen_type","Monitor Type; Colour|Monochrome"},
     {"sse_screen_frequency","Video Frequency; 50|60|72"},
     {"sse_st_type","ST Model; STF|STE"},
     {"sse_st_ram","RAM; 512K|1MB|2MB|4MB"},
     {"sse_st_os","ROM (TOS); 100|102|104|162|205|206|Emu"},
     {"sse_drivesound","Drive Sound; On|Off"},
     {"sse_osd","On Screen Display; On|Off"},
     { NULL, NULL }
   };

   environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
}


void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}


/* Renders multiple audio frames in one go.
 *
 * One frame is defined as a sample of left and right channels, interleaved.
 * I.e. int16_t buf[4] = { l, r, l, r }; would be 2 frames.
 * Only one of the audio callbacks must ever be used.
 * Its prototype is size_t(*retro_audio_sample_batch_t)(const int16_t * samples, size_t num_frames)
 * The number of samples should be 2 * num_frames , with left and right channels 
 * interleaved every frame.
 * Using the batch callback, audio will not be copied in a temporary buffer, which can buy a slight
 * performance gain. Also, all data will be pushed to audio driver in one go, saving some slight
 * overhead.
 */
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}


void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}


void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}


void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}


void retro_reset(void)
{
  // called on command but not on load content
  TRACEB("retro_reset\n");
  check_options();
#if defined(SSE_DEBUG_TRACE)
  Debug.TraceGeneralInfos(TDebug::RESET);
#endif
  SetTimingFunctions();
  power_on();
  reset_st(RESET_COLD|RESET_STOP|RESET_NOCHANGESETTINGS|RESET_NOBACKUP);
}


static void update_input(void)
{
  
  //input_poll_cb(); //no need?
  
  // input_state_cb(unsigned port, unsigned device,unsigned index, unsigned id)
  int port = 0; // 
  int16_t mouse_x = input_state_cb(port,RETRO_DEVICE_MOUSE,0,RETRO_DEVICE_ID_MOUSE_X);
  int16_t mouse_y = input_state_cb(port,RETRO_DEVICE_MOUSE,0,RETRO_DEVICE_ID_MOUSE_Y);

  bool mouse_lb      = input_state_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
  bool mouse_rb      = input_state_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);

  if(mouse_lb)
    stick[N_JOY_PORT_0]|=BIT_7;
  else
    stick[N_JOY_PORT_0]&=~BIT_7;

  if(mouse_rb)
    stick[N_JOY_PORT_1]|=BIT_7;
  else
    stick[N_JOY_PORT_1]&=~BIT_7;
  
  ///////////////////if(mouse_x||mouse_y)
  {
    Ikbd.MouseVblDeltaX=mouse_x;//?
    Ikbd.MouseVblDeltaY=mouse_y;
    ///////mouse_vbl_delta=true;
  }
  
  // joysticks
        /*{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },*/
  //// up, down, left, right = bit 0, 1, 2, 3
  for(int playerID=0;playerID<PORTS_NUMBER;playerID++) // for 0, 1
  {
    int joyport=0+PORTS_NUMBER-1-playerID; // default port 1 -> joy 1, not 0
    stick[playerID]&=BIT_7; // clear positions, not button
    // no more global way?
    if(input_state_cb(joyport,RETRO_DEVICE_JOYPAD,0,RETRO_DEVICE_ID_JOYPAD_LEFT))
      stick[playerID]|=BIT_2;
    if(input_state_cb(joyport,RETRO_DEVICE_JOYPAD,0,RETRO_DEVICE_ID_JOYPAD_UP))
      stick[playerID]|=BIT_0;
    if(input_state_cb(joyport,RETRO_DEVICE_JOYPAD,0,RETRO_DEVICE_ID_JOYPAD_DOWN))
      stick[playerID]|=BIT_1;
    if(input_state_cb(joyport,RETRO_DEVICE_JOYPAD,0,RETRO_DEVICE_ID_JOYPAD_RIGHT))
      stick[playerID]|=BIT_3;
    if(input_state_cb(joyport,RETRO_DEVICE_JOYPAD,0,RETRO_DEVICE_ID_JOYPAD_A)
      || input_state_cb(joyport,RETRO_DEVICE_JOYPAD,0,RETRO_DEVICE_ID_JOYPAD_B))
      stick[playerID]|=BIT_7;
  }

}


void check_options() {
  TRACEB("check_options\n");
  struct retro_variable var;

  var.key = "sse_screen_type";
  var.value = NULL;
  if(environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var) && var.value)
  {
    bool colour=true;
    if(strcmp(var.value,"Colour") == 0) {
      CoreFreq=50;
    }
    else if(strcmp(var.value,"Monochrome") == 0) {
      colour=false;
      CoreFreq=72;
    }
   // if(colour!=SSEConfig.ColourMonitor)
    {
      SSEConfig.UpdateMonitor(colour);
      Draw.MarshalParameters();
    }
  }

  var.key = "sse_screen_frequency";
  var.value = NULL;
  if(environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var) && var.value)
  {
    CoreFreq=(WORD)atoi(var.value);
    ASSERTB(CoreFreq==50||CoreFreq==60||CoreFreq==72);
  }

  var.key = "sse_st_type";
  var.value = NULL;
  if(environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var) && var.value)
  {
    BYTE new_st_model=ST_MODEL;
    if(strcmp(var.value,"STF") == 0) {
      new_st_model=STF;
    }
    else if(strcmp(var.value,"STE") == 0) {
      new_st_model=STE;
    }
    SSEConfig.SwitchSTModel(new_st_model);
  }

  var.key = "sse_st_ram";
  var.value = NULL;
  if(environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var) && var.value)
  {
    BYTE ConfigBank1=MEMCONF_512;
    BYTE ConfigBank2=MEMCONF_512;
    if(strcmp(var.value,"512K") == 0) {
      ConfigBank2=0;
    }
    else if(strcmp(var.value,"1MB") == 0) {
    }
    else if(strcmp(var.value,"2MB") == 0) {
      ConfigBank1=MEMCONF_2MB;
      ConfigBank2=0;
    }
    else if(strcmp(var.value,"4MB") == 0) {
      ConfigBank1=MEMCONF_2MB;
      ConfigBank2=MEMCONF_2MB;
    }
    SSEConfig.make_Mem(ConfigBank1,ConfigBank2);
  }
  var.key = "sse_st_os";
  var.value = NULL;
  if(environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var) && var.value)
  {
    // file is tosxxx.img where XXX is 100|102|104|162|205|206
    // we try to load it from RunDir
    char tos_file[16]="tosxxx.img";
    strncpy(tos_file+3,var.value,3);
    EasyStr path=RunDir;
    path+=SLASH;
    path+=tos_file;
    bool success=load_TOS(path);
    if(!success)
      get_default_tos(); // embedded EmuTOS
  }
 
  var.key = "sse_drivesound";
  var.value = NULL;
  if(environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var) && var.value)
  {
    if(strcmp(var.value,"On") == 0) {
      OPTION_DRIVE_SOUND=1;
    }
    else if(strcmp(var.value,"Off") == 0) {
      OPTION_DRIVE_SOUND=0;
    }
  }

  var.key = "sse_osd";
  var.value = NULL;
  if(environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var) && var.value)
  {
    if(strcmp(var.value,"On") == 0) {
      OsdControl.no_draw=0;
      OsdControl.disable=0;
    }
    else if(strcmp(var.value,"Off") == 0) {
      OsdControl.no_draw=1;
      OsdControl.disable=1;
    }
  }

}


static void audio_callback(void)
{
/*
   for (unsigned i = 0; i < 30000 / 60; i++, phase++)
   {
      int16_t val = 0x800 * sinf(2.0f * M_PI * phase * 300.0f / 30000.0f);
      audio_cb(val, val);
   }

   phase %= 100;
*/
}


static void audio_set_state(bool enable)
{
   (void)enable;
}


void retro_run(void)
{
  // run emulation for one frame
  // render video and audio
  update_input();

  // change in core options?
  bool updated = false;
  if(environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE,&updated) && updated)
    check_options();

  runstate=RUNSTATE_RUNNING;

  Draw.Pitch=draw_line_length;
  draw_dest_increase_y=Draw.EffectivePitch=Draw.VerPix*Draw.Pitch;
  draw_dest_ad=draw_mem;
  draw_dest_next_scanline=draw_dest_ad+draw_dest_increase_y;
  
  if(Draw.Buffered)
  {
    draw_store_dest_ad=draw_dest_ad;
    draw_dest_ad=draw_temp_line_buf;
    Draw.limit2=draw_temp_line_buf_lim;
  }
  else
    Draw.limit2=Disp.VideoMemoryEnd;
  Draw.limit1=draw_dest_ad;


  draw_lock=true;
  timer=timeGetTime();
#if defined(SSE_LIBRETROSOUND1)
  n_samples_this_vbl=psg_n_samples_this_vbl;
#endif
  run();

#if defined(SSE_LIBRETROSOUND1) && !defined(SSE_LIBRETROSOUND1B)
  audio_batch_cb(SoundBuf,psg_n_samples_this_vbl);
#endif
#if defined(SSE_LIBRETROSOUND2)
  SoundVBL();
#endif
  video_cb(frame_buf,VIDEO_WIDTH,VIDEO_HEIGHT,VIDEO_WIDTH*4);
}


bool retro_load_game(const struct retro_game_info *info)
{
  // this function is called on start even if no content selected
  
  TRACEB("retro_load_game\n");

  struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },

      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },//?
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },//?
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },//?
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },//?

      { 0 },
   };


   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
      return false;
   }

   /* If need_fullpath is true and retro_load_game() is called:
    *    - retro_game_info::path is guaranteed to have a valid path
    *    - retro_game_info::data and retro_game_info::size are invalid
    *
    * If need_fullpath is false and retro_load_game() is called:
    *    - retro_game_info::path may be NULL
    *    - retro_game_info::data and retro_game_info::size are guaranteed
    *      to be valid
    */
   if(info!=NULL)
   {
     TRACEB("retro_game_path %s\n",info->path);
     snprintf(retro_game_path,sizeof(retro_game_path),"%s",info->path);
     FloppyDrive[DRIVE_A].SetDisk(info->path);
   }

   // not recommended - don't use
   struct retro_audio_callback audio_cb2 = { audio_callback, audio_set_state };
   use_audio_cb = environ_cb(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &audio_cb2);

   //check_options();
   retro_reset();

   InitKeyTable();
#if defined(SSE_420R2B)
   ikbd_run_start(LITTLE_PC==SafeLPeek(4));
#else
   ikbd_run_start(LITTLE_PC==rom_addr);
#endif
   timer=timeGetTime();
   srand(timer); // each thread must seed
   //osd_start_time=timer;
#if (defined(SSE_LIBRETROSOUND1) && defined(SSE_LIBRETRO_DRIVESOUND)) || defined(SSE_LIBRETROSOUND2)
   UseSound=1;
   SoundStart();
#endif
   init_screen();
   //PortsRunStart();
   osd_init_run(true);
   ////Debug.TraceGeneralInfos(TDebug::RESET);
   return true;
}


void retro_unload_game(void)
{
  TRACEB("retro_unload_game()\n");
  FloppyDrive[DRIVE_A].RemoveDisk(true);

#if (defined(SSE_LIBRETROSOUND1) && defined(SSE_LIBRETRO_DRIVESOUND)) || defined(SSE_LIBRETROSOUND2)
  SoundStop();
  UseSound=0;
#endif

}


unsigned retro_get_region(void)
{
  // that would depend on game -> could be a game setting
   //return RETRO_REGION_NTSC;
  //return RETRO_REGION_PAL; //?
  return (CoreFreq==60) ? RETRO_REGION_NTSC : RETRO_REGION_PAL;
}


bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   return false;
}


size_t retro_serialize_size(void)
{
  size_t size=mem_len+16000; // larger than necessary 12068;
  TRACEB("retro_serialize_size() %d\n",size);
  return size;
}


bool retro_serialize(void *data_, size_t size)
{
  // save state to memory, not a file (RetroArch will handle it)
  // size should be the one we gave
  TRACEB("retro_serialize\n");
  int ramoffset=0;
  int *pRamoffset=&ramoffset;
  if(LoadSaveAllStuff((BYTE*)data_,LS_SAVE,SNAPSHOT_VERSION,false,pRamoffset)==ERR_OK)
  {
    BYTE *dest=(BYTE*)data_+*pRamoffset;
    memcpy(dest,STMem+MEM_EXTRA_BYTES,mem_len);
    return true;
  }
  return false;
}


bool retro_unserialize(const void *data_, size_t size)
{
  // load state
  TRACEB("retro_unserialize\n");
  int ramoffset=0;
  int *pRamoffset=&ramoffset;
  reset_st(RESET_COLD|RESET_STOP|RESET_NOCHANGESETTINGS|RESET_NOBACKUP); //no hang
  if(LoadSaveAllStuff((BYTE*)data_,LS_LOAD,SNAPSHOT_VERSION,false,pRamoffset)==ERR_OK)
  {
    ASSERTB(ramoffset);
    TRACEB("ramoffset %d\n",ramoffset);
    BYTE *p=(BYTE*)data_;
    p+=*pRamoffset;
    BYTE *dest=STMem+MEM_EXTRA_BYTES;
    memcpy(dest,p,mem_len);
    LoadSnapShotUpdateVars(SNAPSHOT_VERSION);
    Disp.VideoMemoryEnd=draw_mem+VIDEO_PIXELS; //why?
    //temp
#if 1


    DISPLAY_SIZE=2; // why is it 1 by default?
    draw_line_length=VIDEO_WIDTH;

    //Draw.Pitch=draw_line_length;
    //draw_dest_increase_y=Draw.EffectivePitch=Draw.VerPix*Draw.Pitch;

    draw_dest_ad=draw_mem;
    draw_dest_next_scanline=draw_dest_ad+draw_dest_increase_y;

    if(Draw.Buffered)
    {
      draw_store_dest_ad=draw_dest_ad;
      draw_dest_ad=draw_temp_line_buf;
      Draw.limit2=draw_temp_line_buf_lim;
    }
    else
      Draw.limit2=Disp.VideoMemoryEnd;
    Draw.limit1=draw_dest_ad;
#endif    
    ComputerRestore();

    return true;
  }
  return false;
}


// don't think we need to bother
void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

// don't think we need to bother
size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}


// don't think we need to bother
void retro_cheat_reset(void)
{}

// don't think we need to bother
void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}


/* Callback type passed in RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK.
 * Called by the frontend in response to keyboard events.
 * down is set if the key is being pressed, or false if it is being released.
 * keycode is the RETROK value of the char.
 * character is the text character of the pressed key. (UTF-32).
 * key_modifiers is a set of RETROKMOD values or'ed together.
 *
 * The pressed/keycode state can be indepedent of the character.
 * It is also possible that multiple characters are generated from a
 * single keypress.
 * Keycode events should be treated separately from character events.
 * However, when possible, the frontend should try to synchronize these.
 * If only a character is posted, keycode should be RETROK_UNKNOWN.
 *
 * Similarily if only a keycode event is generated with no corresponding
 * character, character should be 0.
 */ 
void handle_kbd_evt(bool down,unsigned keycode,uint32_t character,uint16_t key_modifiers) {
  TRACEB("handle_kbd_evt down%d keycode%d (%c) character%d mod%d\n",down,keycode,keycode,character,key_modifiers);
  UINT VKCode=keycode;

  if(VKCode>=0x61 && VKCode<=0x7a) // a-z
    VKCode-=0x20; // A-Z

  DWORD Up=!down;
  int Extended=IGNORE_EXTEND;//key_modifiers; 
  ///int Extended=1;
  
  // quick fix translate RETROK to VK
  switch(VKCode) {
  case RETROK_SEMICOLON:
    VKCode=0xBA;
    break;
  case RETROK_TILDE:
    VKCode=0xC0;
    break;
  case 0x2B: // +
  case 0x2C: // ,
  case 0x2D: // -
  case 0x2E: // .
  case 0x2F: // /
    VKCode+=0x90; 
    break;
    // etc. TODO
  }//sw

  if(keycode!=302) //scrllock
    HandleKeyPress(VKCode,Up,Extended);
}


/********************************************************************************************
    Rest of this file:
    The following definitions and functions were copied from run.cpp and adapted to libretro
**********************************************************************************************/ 


void event_scanline_sub();

//EVENTPROC const event_mfp_timer_timeout[4]={event_timer_a_timeout,
  //event_timer_b_timeout,event_timer_c_timeout,event_timer_d_timeout};
COUNTER_VAR time_of_next_timer_b=0;
// fast_forward_max_speed=(1000 / (max %/100)); 0 for unlimited
int fast_forward=0,fast_forward_max_speed=0;
int slow_motion=0,slow_motion_speed=100;
int run_speed_ticks_per_second=1000;
int avg_frame_time_counter=0;
int frameskip=1,frameskip_count=1;
DWORD avg_frame_time=0;
bool disable_speed_limiting=false;
bool fast_forward_stuck_down=false;
bool flashlight_flag=false;

#ifdef UNIX
bool RunWhenStop=false; // for fullscreen
#endif

EVENTPROC event_vector;
COUNTER_VAR time_of_next_event;
COUNTER_VAR time_of_last_hbl_interrupt, time_of_last_vbl_interrupt;
COUNTER_VAR sys_timer_at_res_change;
int runstate;
int run_start_time;
BYTE stem_runmode;
DWORD avg_frame_time_timer,frame_delay_timeout,timer;
DWORD speed_limit_wait_till;
DWORD auto_frameskip_target_time;
DWORD start_of_this_second;
DWORD nframes_this_second;

#ifdef WIN32
TMicroTime MicroTime;
#endif


void run() {
  // Called by retro_run(). We run the emulation for just one frame
#ifdef SSE_HD6301_LL
  //Ikbd.Crashed=0;
  //mousek=0;
#endif
  bool ExcepHappened;
  runstate=RUNSTATE_RUNNING;
  Glue.m_Status.stop_emu=0;
  //SoundStart();
  //Glue.AddFreqChange(Glue.VideoFreq);
  draw_begin();
  //PortsRunStart();
  stem_runmode=STEM_MODE_CPU;
  //timer=timeGetTime();
  //run_start_time=timer; // For log speed limiting
  //osd_init_run(true);
  ioaccess=0;
#if defined(SSE_MAIN_LOOP)
#if defined(SSE_EMU_THREAD) && !defined(_DEBUG) // want to see exception
  try  // apart thread or not
#endif
#endif
  {
#if defined(SSE_EMU_THREAD) && defined(SSE_MAIN_LOOP2) 
    //_se_translator_function old_se_f=
    _set_se_translator(trans_func);
#endif
    //for(;;); // TEST stuck in infinite loop
    //int a=0; int b=5/a;  printf("yoho %d",b);// TEST SEH exception
    do {
      ExcepHappened=false;
      TRY_M68K_EXCEPTION
        while(runstate==RUNSTATE_RUNNING) 
        {
          // sys_cycles is the amount of cycles before next event.
          // So it is *decremented* by instruction timings, not incremented.
          while(sys_cycles>0&&runstate==RUNSTATE_RUNNING)
          {
#ifdef DEBUG_BUILD
            pc_history_y[pc_history_idx]=scan_y;
            pc_history_c[pc_history_idx]=LINECYCLES;
            pc_history[pc_history_idx++]=(pc&0x00FFFFFF);
            if(pc_history_idx>=HISTORY_SIZE) 
              pc_history_idx=0;
#endif
            m68kProcess();
#ifdef DEBUG_BUILD
            debug_first_instruction=false;
            CHECK_BREAKPOINT
#endif
          }//wend
#ifdef DEBUG_BUILD
          if(runstate!=RUNSTATE_RUNNING) 
            break;
          //stem_runmode=STEM_MODE_INSPECT; // why? we're still running
#endif
          for(int i=0;sys_cycles<=0 && (runstate==RUNSTATE_RUNNING);i++) // get out of buggy loop
          {
#if defined(SSE_DEBUGGER_FAKE_IO)
            if(TRACE_MASK2&TRACE_CONTROL_EVENT)
              TRACE_EVENT(event_vector);
#endif
            event_vector();
            prepare_next_event();
            /*
            if(//!OPTION_EMUTHREAD && // also for emuthread: avoids killing the thread
              i==10) // get out of buggy loop
            {
              PeekEvent();
              i=0; // i business to avoid load
            }
            */
          }
          CHECK_BREAKPOINT
          DEBUG_ONLY(stem_runmode=STEM_MODE_CPU; )
        }//while (runstate==RUNSTATE_RUNNING)
      CATCH_M68K_EXCEPTION
        TMC68kException e=ExceptionObject;
        ExcepHappened=true;
#ifndef DEBUG_BUILD
        e.crash();
#else
        stem_runmode=STEM_MODE_INSPECT;
        bool alertflag=false;
        if(crash_notification!=CRASH_NOTIFICATION_NEVER)
        {
          alertflag=true;
          //TRY_M68K_EXCEPTION
            if(e.bombs>8)
              alertflag=false;
            else if(crash_notification==CRASH_NOTIFICATION_NOT_TOS
              && e.u_pc.d32>=rom_addr && e.u_pc.d32<rom_addr_end)
              alertflag=false;
            else if(crash_notification==CRASH_NOTIFICATION_BOMBS_DISPLAYED
              && SafeLPeek(e.bombs*4)<rom_addr) //not bombs routine
              alertflag=false;
          //CATCH_M68K_EXCEPTION
            //alertflag=true;
          //END_M68K_EXCEPTION
        }
        DEBUG_ONLY(stem_runmode=STEM_MODE_CPU;) // avoid wrong timing in bus_jam
        if(!alertflag)
          e.crash();
        else
        {
          bool was_locked=draw_lock;
          draw_end();
          if(runstate==RUNSTATE_STOPPED)
            draw(false);
#if defined(SSE_DEBUGGER_STATUS_BAR)
          char crash_msg[60];
          sprintf(crash_msg,"Exception %d bombs",e.bombs); // can become HALT
          DbgStatusBarMsg(crash_msg);
          runstate=RUNSTATE_STOPPING;
          e.crash(); //crash
          debug_trace_crash(e);
          ExcepHappened=false;
          if(Debug.DialogOnStopEvent)
#endif
          {
            if(IDOK==Alert(
              "Exception - do you want to crash (OK)\nor trace? (CANCEL)",
              EasyStr("Exception ")+e.bombs,MB_OKCANCEL|MB_ICONEXCLAMATION)) 
            {
                e.crash();
                if(was_locked)
                  draw_begin();
            }
            else
            {
              runstate=RUNSTATE_STOPPING;
              e.crash(); //crash
              debug_trace_crash(e);
              ExcepHappened=false;
            }
          }
        }
        if(debug_num_bk)
          breakpoint_check();
        if(runstate!=RUNSTATE_RUNNING)
          ExcepHappened=false;
#endif
      END_M68K_EXCEPTION
    } while(ExcepHappened);
    //_set_se_translator(old_se_f);
  }
#if defined(SSE_MAIN_LOOP)
#if defined(SSE_EMU_THREAD) // apart thread or not
#if defined(SSE_MAIN_LOOP2)
  catch(SE_Exception e) {
    e.handle_exception();
  }
#elif !defined(_DEBUG)
  catch(...) {
    Alert(T("Unknown exception"),T(STEEM_CRASH_TXT),MB_ICONEXCLAMATION|MB_OK);
    TRACE("Unknown exception\n");
  }
#endif
#endif
#endif
  runstate=RUNSTATE_STOPPED;
  Glue.m_Status.stop_emu=0;
}


void event_vbl_interrupt() {
  // this is based on the same function in run.cpp, but without timing computing
#if defined(SSE_DONGLE)
/*  When pressing some button of his cartridge, the player triggered an MFP
    interrupt.
    By releasing the button, the concerned bit should change state. We use
    no counter but do it at first VBL for simplicity.
*/
  if(cart)
  {
    switch(DONGLE_ID) {
#if defined(SSE_DONGLE_URC) 
    case TDongle::URC:
      if(!(Mfp.reg[MFPR_GPIP]&0x40))
        mfp_gpip_set_bit(MFP_GPIP_RING_BIT,true); // Ultimate Ripper
      break;
#endif
#if defined(SSE_DONGLE_MULTIFACE) // cart + monochrome
    case TDongle::MULTIFACE:
      if(!(Mfp.reg[MFPR_GPIP]&0x80))
        mfp_gpip_set_bit(MFP_GPIP_MONO_BIT,true);
      break;
#endif
    }
  }//if
#endif
#if defined(SSE_VID_STVL1)
  if(OPTION_C3)
    time_of_next_event=A_S_T+Stvl.tick8*TICKS8;
  else
#endif
/*  With GLU/video event refactoring, we call event_scanline() one time fewer,
    if we did now it would mess up some timings, so we call the sub
    with some HBL-dependent tasks: DMA sound, HD6301 & CAPS emu.
    Important for Relapse STE sound
*/
  {
    event_scanline_sub();
    Glue.VCount=0;
  }
  if(!OPTION_C3&&!extended_monitor)
  {
    if(Draw.Buffered)
      draw_dest_ad=draw_store_dest_ad;
    scanline_drawn_so_far=0;
    VCountAtHSync=shifter_draw_pointer;
  }
  DiskEmu.Update(); // for sound, OSD
  
  if(draw_lock) 
  {

    //draw_end();
    //draw_blit();
    osd_draw_begin();
    osd_draw(); //wrks +-
  }
  if(floppy_mediach[DRIVE_A]) 
    floppy_mediach[DRIVE_A]--;  //counter for media change

  //if(floppy_mediach[DRIVE_B]) 
    //floppy_mediach[DRIVE_B]--;  //counter for media change
#if !defined(SSE_LIBRETRONUKE)
  if((--shortcut_vbl_count)<0) 
  {
    ShortcutsCheck();
    shortcut_vbl_count=SHORTCUT_VBLS_BETWEEN_CHECKS;
  }
#endif
  if(new_n_cpu_cycles_per_second) 
  {
    if(new_n_cpu_cycles_per_second!=nSysCyclesPerSecond) 
    {
      nSysCyclesPerSecond=new_n_cpu_cycles_per_second;
      AdaptCpuBoost();
    }
    new_n_cpu_cycles_per_second=0;
  }

  // cycles for one full frame
  Glue.nFrameCycles=Glue.nLines*CyclesPerScanline[VideoFreqIdx];
#if !defined(SSE_NO_OLD_IKBD)
  if(!OPTION_C1 && NumJoysticks)
    JoyGetPoses(); // Get the positions of all the PC joysticks
#endif
  IKBD_VBL();    // Handle ST joysticks and mouse
#if defined(SSE_HD6301_LL)
  if(OPTION_C1)
    Ikbd.Vbl();
#endif
  RS232_VBL();   // Update all flags, check for the phone ringing

  SoundVBL();   // Write a VBLs worth + a bit of samples to the sound card
#if defined(SSE_DRIVE_SOUND)
/*  We don't check the option here because we may have to suddenly stop
    motor sound loop.
*/
  FloppyDrive[DRIVE_A].SoundVBL();
  FloppyDrive[DRIVE_B].SoundVBL();
#endif
  SteSoundChannelBufIdx=0;  //need to maintain this even if sound off
  ste_sound_on_this_screen=(Mmu.SoundControl&TMmu::SOUNDPLAY)||Shifter.SoundFifoIdx;
  // The MFP clock aligns with the CPU clock every 8000 CPU cycles
  while(abs_quick(ABSOLUTE_SYS_TIME-sys_time_of_first_mfp_tick)>160000*TICKS8)
    sys_time_of_first_mfp_tick+=160000*TICKS8;
  while(abs_quick(ABSOLUTE_SYS_TIME-shifter_cycle_base)>160000*TICKS8)
    shifter_cycle_base+=160000; //SS 60000?
  shifter_tick8=(HSCROLL>>Shifter.ShiftMode);
  left_border=LeftBorderSize;
  right_border=RightBorderSize;
  scanline_drawn_so_far=video_first_draw_line=0;
  video_last_draw_line=shifter_y;
  Glue.Vbl();
  VideoFreqAtStartOfVbl=Glue.VideoFreq;
  //ASSERT(VideoFreqAtStartOfVbl!=GLUE_HZ71);
  CyclesPerScanlineAtStartOfVbl=CyclesPerScanline[VideoFreqIdx];
  sys_time_of_last_vbl=time_of_next_event;
  PasteVBL();
  Debug.Vbl();
  Cpu.UpdateCyclesForEClock(); // refresh
#if defined(SSE_VID_STVL1)
  if(OPTION_C3)
    prepare_next_event();
#endif
  runstate=RUNSTATE_STOPPING;
}


inline void prepare_event_check_for_timer_timeout(int tn) {
  if(mfp_timer_check[tn]) // only if IRQ enabled!
  {
    if((time_of_next_event-mfp_timer_timeout[tn])>=0)
    {
      time_of_next_event=mfp_timer_timeout[tn];
      event_vector=event_mfp_timer_timeout[tn];
    }
  }
}


void prepare_next_event() {
  if(OPTION_C3)
  {
    event_vector=event_dummy;
    time_of_next_event=A_S_T+EIGHT_MILLION;
  }
  else
  {
    Glue.GetNextVideoEvent();
  }
  if(Mfp.reg[MFPR_TBCR]==MFP_TIMER_EVENT_COUNT)
  {
    if((time_of_next_event-time_of_next_timer_b)>=0)
    {
      time_of_next_event=time_of_next_timer_b;
      event_vector=event_timer_b;
    }
  }
  // check timers for timeouts
  prepare_event_check_for_timer_timeout(MFP_TIMER_A);
  prepare_event_check_for_timer_timeout(MFP_TIMER_B);
  prepare_event_check_for_timer_timeout(MFP_TIMER_C);
  prepare_event_check_for_timer_timeout(MFP_TIMER_D);
#ifdef DEBUG_BUILD
  if(debug_run_until==DRU_CYCLE)
  {
    if((time_of_next_event-debug_run_until_val)>=0)
    {
      time_of_next_event=debug_run_until_val;
      event_vector=event_debug_stop;
    }
  }
#endif
#if USE_PASTI
  if((time_of_next_event-pasti_update_time)>=0)
  {
    time_of_next_event=pasti_update_time;
    event_vector=event_pasti_update;
  }
#endif
  if((time_of_next_event-Fdc.update_time)>=0)
  {
    time_of_next_event=Fdc.update_time;
    event_vector=event_wd1772;
  }
  if((time_of_next_event-FloppyDrive[DRIVE_A].time_of_next_ip)>=0
    && FloppyDrive[DRIVE_A].ImageType.Manager==MNGR_WD1772) //402R6
  {
    time_of_next_event=FloppyDrive[DRIVE_A].time_of_next_ip;
    event_vector=event_driveA_ip;
  }
  if((time_of_next_event-FloppyDrive[DRIVE_B].time_of_next_ip)>=0
    && FloppyDrive[DRIVE_B].ImageType.Manager==MNGR_WD1772) //402R6
  {
    time_of_next_event=FloppyDrive[DRIVE_B].time_of_next_ip;
    event_vector=event_driveB_ip;
  }
  if(time_of_next_event-time_of_event_acia>=0)
  {
    time_of_next_event=time_of_event_acia;
    event_vector=event_acia;
  }
  // It is safe for events to be in past, whatever happens events
  // cannot get into a constant loop.
  // If a timer is set to shorter than the time for an MFP interrupt then it will
  // happen a few times, but eventually will go into the future (as the interrupt can
  // only fire once, when it raises the IPL).
  int oo=(int)(time_of_next_event-sys_timer);
  // sys_timer must always be set to the next 4 cycle boundary after time_of_next_event
  //SS: this is still true after rounding refactoring
  //guess (!) it's because it enforces CPU R/W cycle, which is still 4 cycles
  //(clocks) in our reckoning, but still don't see how exactly
  oo=(oo+(4*TICKS8-1)) & -(4*TICKS8);
  sys_cycles+=oo;sys_timer+=oo;
}


#define LOGSECTION LOGSECTION_MFP_TIMERS

inline void handle_timeout(int tn) {
  //TRACE_LOG2("handle_timeout(%d)\n",tn);
  if(mfp_timer_period_change[tn])
  {
    Mfp.CalcTimerPeriod(tn);
    mfp_timer_period_change[tn]=false;
    mfp_timer_check[tn]=mfp_timer_enabled[tn];
  }
  COUNTER_VAR new_timeout=mfp_timer_timeout[tn];
  a_s_t=A_S_T;
  COUNTER_VAR cmp;
#ifndef SSE_LEAN_AND_MEAN
  if(mfp_timer_period[tn]>0)
#endif
    do 
    {
      new_timeout+=mfp_timer_period[tn];
      cmp=new_timeout-a_s_t;
    } while(cmp<0 /*|| cmp==0&&cpu_cycles_multiplier<32.0*/);

  mfp_timer_period_current_fraction[tn]+=mfp_timer_period_fraction[tn]; 
  // this guarantees that we're always at the right cycle, despite
  // the inconvenience of a ratio
  //if(tn==MFP_TIMER_A) { TRACE_LOG2("current %d, %d/%d\n",mfp_timer_period_current_fraction[tn],mfp_timer_period_fraction[tn],MFP_TIMER_PRECISION); }
  if(mfp_timer_period_current_fraction[tn]>=MFP_TIMER_PRECISION)
  {
    mfp_timer_period_current_fraction[tn]-=MFP_TIMER_PRECISION;
//    if(tn==MFP_TIMER_A) { TRACE_LOG2("current %d\n",mfp_timer_period_current_fraction[tn]); }
    new_timeout++;
#if defined(SSE_DEBUGGER_FAKE_IO)
    if(!(tn==MFP_TIMER_A&&!(TRACE_MASK2&TRACE_CONTROL_IRQ_TA)
      || tn==MFP_TIMER_B&&!(TRACE_MASK2&TRACE_CONTROL_IRQ_TB)
      || tn==MFP_TIMER_C&&!(TRACE_MASK2&TRACE_CONTROL_IRQ_TC)
      || tn==MFP_TIMER_D&&!(TRACE_MASK2&TRACE_CONTROL_IRQ_TD)))
    {
      TRACE_LOG2("+1 ");
    }
#endif
  }
  Mfp.Counter[tn]=Mfp.reg[MFPR_TADR+tn]; // load counter
  BYTE prescale_index=(Mfp.GetTimerControlRegister(tn)&MFP_TIMER_DELAY_MASK);
  Mfp.Prescale[tn]=(BYTE)mfp_timer_prescale[prescale_index]; // load prescale (bad if 0)
  Mfp.GetInterrupt(mfp_timer_irq[tn],mfp_timer_timeout[tn]);
  //ASSERT(new_timeout-A_S_T>0);
#if defined(SSE_DEBUGGER_FAKE_IO)
  if(!( tn==MFP_TIMER_A&&!(TRACE_MASK2&TRACE_CONTROL_IRQ_TA)
      || tn==MFP_TIMER_B&&!(TRACE_MASK2&TRACE_CONTROL_IRQ_TB)
      || tn==MFP_TIMER_C&&!(TRACE_MASK2&TRACE_CONTROL_IRQ_TC)
      || tn==MFP_TIMER_D&&!(TRACE_MASK2&TRACE_CONTROL_IRQ_TD)))
  {
    TRACE_LOG2(PRICV " Timer %c timeout, next " PRICV "\n",mfp_timer_timeout[tn],
      'A'+tn,new_timeout);
  }
#endif
  mfp_timer_timeout[tn]=new_timeout;
#if defined(SSE_STATS)
  Stats.nMfpTimeout[tn]++;
  DWORD divisor=Mfp.Prescale[tn]*BYTE_00_TO_256(Mfp.Counter[tn]);
#ifndef SSE_LEAN_AND_MEAN
  if(divisor) // should be
#endif
  Stats.fTimer[tn]=Mfp.xtal/divisor; //Hz
#endif
}


void event_timer_a_timeout() {
  handle_timeout(MFP_TIMER_A);
}


void event_timer_b_timeout() {
  handle_timeout(MFP_TIMER_B);
}


void event_timer_c_timeout() {
  handle_timeout(MFP_TIMER_C);
}


void event_timer_d_timeout() {
  handle_timeout(MFP_TIMER_D);
}


#undef LOGSECTION
#define LOGSECTION LOGSECTION_INTERRUPTS

void event_timer_b() {
  Mfp.time_of_last_tb_tick=time_of_next_timer_b;
  if(OPTION_C3||scan_y<video_last_draw_line) 
  {
    if(Mfp.reg[MFPR_TBCR]==MFP_TIMER_EVENT_COUNT) 
    { // timer B tick
#if defined(SSE_STATS)
      Stats.nTimerbtick++;
#endif
#if defined(SSE_DEBUGGER_FRAME_REPORT)
      if(FRAME_REPORT_MASK2 & FRAME_REPORT_MASK_TIMER_B)
        FrameEvents.Add(scan_y,(short)(time_of_next_timer_b-LINECYCLE0), "TB",
        mfp_timer_counter[MFP_TIMER_B]/64);
#endif
      mfp_timer_counter[MFP_TIMER_B]-=64;
      Mfp.tbctr_old=Mfp.Counter[MFP_TIMER_B];
      Mfp.Counter[MFP_TIMER_B]--;
      if(mfp_timer_counter[MFP_TIMER_B]<64) 
      {
#if defined(SSE_STATS)
        Stats.nTimerb++;
#endif
#if defined(SSE_ENABLE_TRACE_LOG)
#if defined(SSE_DEBUGGER_FAKE_IO) //timers only when checked in mask
        if(TRACE_MASK2&TRACE_CONTROL_IRQ_TB)
#endif
          if(mfp_interrupt_enabled[8]) 
            TRACE_LOG("F%d y%d c%d Timer B pending\n",TIMING_INFO); //?
#endif
        mfp_timer_counter[MFP_TIMER_B]=BYTE_00_TO_256(Mfp.reg[MFPR_TBDR])*64;
        Mfp.Counter[MFP_TIMER_B]=Mfp.reg[MFPR_TBDR];
        Mfp.GetInterrupt(MFP_INT_TIMER_B,time_of_next_timer_b);
      }
/*  Besides generating a count pulse, the active transition of the auxiliary
    input signal will also produce an interrupt on the I3 or I4 interrupt
    channel, if the interrupt channel is enabled.
*/
      Mfp.GetInterrupt(MFP_INT_BLITTER,time_of_next_timer_b);
    }
  }
  time_of_next_timer_b+=EIGHT_MILLION; // put into future
  if(Mfp.reg[MFPR_AER]&MFP_GPIP_BLITTER_MASK)
    Glue.m_Status.timerb_start=1;
  else
    Glue.m_Status.timerb_end=1;
}

#undef LOGSECTION


void event_scanline_sub() {
/*  We take some tasks out of event_scanline(), so we can execute them from
    event_vbl_interrupt().
*/
#define LOGSECTION LOGSECTION_AGENDA
  // CHECK_AGENDA (formerly a macro)
  if(++hbl_count==agenda_next_time)
  {
    if(agenda_length)
    {
      if(agenda_length)
      {
        while((signed int)(hbl_count-agenda[agenda_length-1].time)>=0)
        {
          agenda_length--;
          TRACE_LOG("agenda execute #%d %p(%d)\n",agenda_length,
            agenda[agenda_length].perform,agenda[agenda_length].param);
          if(agenda[agenda_length].perform!=NULL) 
            agenda[agenda_length].perform(agenda[agenda_length].param);
          if(agenda_length)
            agenda_next_time=agenda[agenda_length-1].time;
          else
          {
            agenda_next_time=hbl_count-1; // wait 42 hours
            break;
          }
        }//wend
      }
    }
  }
#undef LOGSECTION
#if defined(SSE_HD6301_LL)
  if(OPTION_C1)
  {
    //ASSERT(HD6301_OK);
    ASSERTB(stem_runmode==STEM_MODE_CPU); // this assert found an issue
    hd6301_run_cycles(A_S_T);
    if(Ikbd.Crashed)
    {
      TRACE("6301 CRASH\n");
      UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
      runstate=RUNSTATE_STOPPING;
    }
  }
#endif
#if defined(SSE_DISK_CAPS)
  if(Caps.Active==1)
    Caps.Hbl();
#endif
  if(IS_STE && ste_sound_on_this_screen)
  {
#if defined(SSE_VID_STVL1)
    if(!OPTION_C3 || SSEConfig.Stvl<0x101)
#endif
      Mmu.SoundFetch();
    if(!(Glue.CurrentScanline.Tricks&TRICK_0BYTE_LINE)) // Fckcancr (on flicker!)
      Shifter.SoundPlay();
#if defined(SSE_VID_STVL1)
    if(!OPTION_C3 || SSEConfig.Stvl<0x101)
#endif
    if(!Glue.bFetchingLine) // no DE this line
      Mmu.SoundFetch(); // make sure FIFO is filled
  }
}


void CALLBACK event_scanline() {
  event_scanline_sub();
#if defined(SSE_VID_STVL1)
  if(OPTION_C3)
  { // copy scanline from  STVL buffer to video memory, twice if line doubled
    time_of_next_event=A_S_T+Stvl.tick8*TICKS8;
    // timer B before scanline
    if(sys_cycles<=0 && event_vector==event_timer_b)
    {
#if defined(SSE_DEBUGGER_FAKE_IO)
      if(TRACE_MASK2&TRACE_CONTROL_EVENT)
        TRACE_EVENT(event_vector);
#endif
      event_vector();
    }
    for(int n=0;n<Stvl.hsync;n++) // in case of 'No Buddies Land' type 0byte lines
    if(draw_lock && Stvl.render_y>render_vstart && Stvl.render_y<=render_vend)
    {
      DWORD *source_start=Stvl.draw_mem_ptr_min+render_hstart;
      DWORD *source_end=source_start+render_scanline_length;
      // the checks are necessary, anything may happen
      if(draw_mem_line_ptr>=(DWORD*)draw_mem&&source_end<Stvl.draw_mem_ptr_max)
      {
        if(draw_mem_line_ptr+render_scanline_length<(DWORD*)Disp.VideoMemoryEnd)
          for(int i=0;i<render_scanline_length;i++)
            draw_mem_line_ptr[i]=source_start[i];
        draw_mem_line_ptr+=draw_line_pitch_dw;
        if(Draw.VerPix>1)
        {
          if(!SCANLINES_OK && draw_mem_line_ptr+render_scanline_length
            <(DWORD*)Disp.VideoMemoryEnd)
          {
            for(int i=0;i<render_scanline_length;i++)
              draw_mem_line_ptr[i]=source_start[i];
          }
          draw_mem_line_ptr+=draw_line_pitch_dw;
        }
#if defined(SSE_VID_STVL_DBG)
        Stvl.dbg_npixels=0;
#endif
      }
    }
#if defined(SSE_DEBUGGER_TOPOFF) // many false alerts just like with C2
    if((DEBUGGER_CONTROL_MASK2&DEBUGGER_CONTROL_TOPOFF)
      && scan_y==-29 && !Stvl.vde && freq_change_this_scanline)
    {
      runstate=RUNSTATE_STOPPING;
      SET_WHY_STOP("Top off missed");
    }
    if((DEBUGGER_CONTROL_MASK2&DEBUGGER_CONTROL_BOTTOMOFF)
      && scan_y==200 && !Stvl.vde && freq_change_this_scanline)
    {
      runstate=RUNSTATE_STOPPING;
      SET_WHY_STOP("Bottom off missed");
    }
#endif
  }
  else
#endif
  if(scan_y>=draw_first_scanline_for_border && scan_y<draw_last_scanline_for_border)
  {
    ASSERTB(bad_drawing==0);
    if(bad_drawing==0) // !0 rarer than out of limits
      Shifter.DrawScanlineToEnd();
  }
  if(OPTION_C2)
  {
    if(Glue.bFetchingLine)
      Glue.EndHBL(); // check for +2 -2 errors + unstable Shifter
    // we check vertical overscan only on interesting scanlines
    if(Glue.CurrentScanline.Cycles>GLU_SCANLINE_CYCLES_72HZ
      && (scan_y==-30||scan_y==-1||scan_y==video_last_draw_line-1&&scan_y<245||scan_y==170)
      ||scan_y==video_last_draw_line-1&&screen_res==HIRES)
      Glue.CheckVerticalOverscan(); // check top & bottom borders
  }
  if(freq_change_this_scanline) 
  {
    if(OPTION_C3 || GlueFreqChangeTime[GlueFreqChangeIdx]<time_of_next_event-16*TICKS8
      && ShifterModeChangeTime[ShifterModeChangeIdx]<time_of_next_event-16*TICKS8)
      freq_change_this_scanline=false;
    if(draw_line_off)
    {
      palette_convert_all();
      draw_line_off=false;
    }
  }
  scanline_drawn_so_far=0;
#if 0 && defined(DEBUG_BUILD)
/*  Enforce register limitations, so that "report SDP" isn't messed up
    in the debug build.
*/
#if defined(SSE_MMU_MONSTER_ALT_RAM)
  if(mem_len<14*0x100000) 
#else
  if(mem_len<=MEM_4MB) 
#endif
    shifter_draw_pointer&=0x3FFFFE;
#endif
#if defined(SSE_VID_STVL1) 
  if(OPTION_C3)
    shifter_draw_pointer=VCountAtHSync=Stvl.vcount.d32;
  else
#endif
  if(Glue.bFetchingLine)
  {
#if 0
    //looks nice but takes more CPU power (another CheckSideOverscan round)
    Mmu.UpdateVideoCounter(LINECYCLES);
    shifter_draw_pointer=VCountAtHSync=Mmu.VideoCounter;
#else
    short added_bytes=Glue.CurrentScanline.Bytes;
    // extra words for HSCROLL are included in Bytes
    if(IS_STE && added_bytes && !Mmu.no_LW)
      added_bytes+=((WORD)LINEWID)<<1; 
    VCountAtHSync+=added_bytes;
#if defined(SSE_MMU_MONSTER_ALT_RAM)
    if(mem_len<MEM_14MB) 
#else
    if(mem_len<=MEM_4MB) 
#endif
      VCountAtHSync&=0x3FFFFE; // Leavin' Teramis
    shifter_draw_pointer=VCountAtHSync;
#endif
  }
  else 
    VCountAtHSync=shifter_draw_pointer;
  Mmu.VideoCounter=VCountAtHSync;
  TimeOfHSyncOff=time_of_next_event; 
#if defined(SSE_VID_STVL1)
  if(OPTION_C3)
  {
#if defined(SSE_DEBUGGER_FRAME_REPORT)
    if((FRAME_REPORT_MASK2 & FRAME_REPORT_MASK_SHIFTER_TRICKS_BYTES))
      FrameEvents.Add(scan_y,Stvl.video_linecycles,'#',Stvl.dbg_fetched_words*2);
#endif
    Glue.CurrentScanline.Cycles=Stvl.video_linecycles*TICKS8; //?
    scan_y++;
    Glue.bFetchingLine=Glue.FetchingLine();
  }
  else
#endif
    Glue.IncScanline(); // will call Shifter.IncScanline()
#if defined(SSE_DEBUGGER_FRAME_REPORT)
  if(Glue.bFetchingLine && (FRAME_REPORT_MASK1&FRAME_REPORT_MASK_VC_LINES)) 
    FrameEvents.Add(scan_y,0,"VC",shifter_draw_pointer); 
#endif
#ifdef DEBUG_BUILD
  if(debug_run_until==DRU_SCANLINE)
  {
    if(debug_run_until_val==scan_y)
    {
      if(runstate==RUNSTATE_RUNNING) 
      {
        runstate=RUNSTATE_STOPPING;
        runstate_why_stop="Run until";
      }
#if defined(SSE_DEBUGGER_FRAME_REPORT)
      FrameEvents.Report();
#endif
    }
  }
#endif
  Glue.hbl_pending=true;
  Glue.hbl_pending_time=TimeOfHSyncOff;
  update_ipl(Glue.hbl_pending_time);
#if defined(SSE_VID_DD_3BUFFER_WIN)
  // DirectDraw Check for Window VSync at each ST scanline!
  if(OPTION_3BUFFER_WIN&&!FullScreen)
    Disp.BlitIfVBlank();
#endif
  Glue.m_Status.hbi_done=false;
  Glue.m_Status.scanline_done=true;
  Glue.m_Status.vc_reload_done=false;
#if defined(SSE_VID_STVL1)
  if(OPTION_C3)
    prepare_next_event();
#endif
}

#undef LOGSECTION


// This happens about 60 cycles into scanline 247 (50Hz)
void event_start_vbl() {
  Glue.m_Status.vc_reload_done=true; // checked this line
#if defined(SSE_DEBUGGER_FRAME_REPORT)
  if(FRAME_REPORT_MASK1 & FRAME_REPORT_MASK_VIDEOBASE)
    FrameEvents.Add(scan_y,LINECYCLES,'r',VideoFreqIdx); // "reload"
#endif
  Glue.vsync=true;
#if defined(SSE_OSD_FPS_INFO)
  if(OPTION_OSD_FPSINFO)
    Debug.vcount_at_vsync=shifter_draw_pointer; // can be >vbase+32KB if overscan
#endif
  // As soon as VSYNC is asserted, the MMU keeps on copying VBASE to VCOUNT
  // We don't emulate the continuous copy but we copy at start and stop
  // (event_start_vbl(),event_trigger_vbi())
  Mmu.VideoCounter=shifter_draw_pointer=vbase;
  VCountAtHSync=shifter_draw_pointer;
  left_border=LeftBorderSize;
  right_border=RightBorderSize;
  Glue.m_Status.vbl_done=false;
}




void AdaptCpuBoost() {
  n_millions_cycles_per_sec=(nSysCyclesPerSecond/TICKS8)/1000000;
  int factor=n_millions_cycles_per_sec;
  cpu_cycles_multiplier=(double)factor/(8);
  SSEConfig.CpuBoost=(int)cpu_cycles_multiplier;
  SSEConfig.CpuBoosted=(SSEConfig.CpuBoost > 1);
  for(int idx=0;idx<3;idx++)
  { //3 frequencies
    CyclesPerScanline[idx]=(int)(CyclesPerScanline8MHz[idx]*cpu_cycles_multiplier);
  }
  Mfp.InitTimers();
  SetTimingFunctions();
  if(runstate==RUNSTATE_RUNNING) 
    prepare_next_event();
  ////CheckResetDisplay();
}


#if USE_PASTI

void event_pasti_update() {
  if(!(hPasti && (pasti_active || FloppyDrive[DRIVE].ImageType.Extension==EXT_STX)))
  {
    pasti_update_time=time_of_next_event+EIGHT_MILLION;
    return;
  }
  struct pastiIOINFO pioi;
  pioi.stPC=pc;
  pioi.cycles=time_of_next_event/TICKS8;
  pasti->Io(PASTI_IOUPD,&pioi);
  pasti_handle_return(&pioi);
}

#endif


#ifdef DEBUG_BUILD

void event_debug_stop() {
  if(runstate==RUNSTATE_RUNNING)
  {
    runstate=RUNSTATE_STOPPING;
    runstate_why_stop="Run until";
  }
  debug_run_until=DRU_OFF; // Must be here to prevent freeze up as this event never goes into the future!
}

#endif


// SSE added events

void CALLBACK event_trigger_vbi() { //6X cycles into frame (colour)
  ASSERTB(Glue.VCount==0||OPTION_C3);
#if defined(SSE_OSD_FPS_INFO)
  if(OPTION_OSD_FPSINFO)
    Debug.vbase_at_vbi=vbase; // program can change vbase at end of frame
#endif
  // VideoFreqIdx and video_freq are incorrect if mode=2 on colour screen
  // at least start the frame with correct video freq
  BYTE const idx=(BYTE)((Glue.ShiftMode&2) ? Glue.FREQ_IDX_71 : 
    ( (Glue.SyncMode&Glue.SYNCPAL) ? Glue.FREQ_IDX_50 : Glue.FREQ_IDX_60 ));
  Glue.VideoFreq=Glue.Freq[idx];
  ASSERTB(Glue.VideoFreq);
  //scan_y=-TopScanlines[idx];
  VideoFreqIdx=idx;
  // note: now we know that it's not a down counter on real HW but anyways, it works
  // Glue.nLines is used for microseconds timings, option C3 or not
  if(Glue.ShiftMode&HIRES) // 71hz (monochrome)
  {
    Glue.nLines=GLU_MONO_SCANLINES; //501
    Glue.de_start_line=TopScanlines[Glue.FREQ_IDX_71];
    Glue.de_end_line=Glue.de_start_line+400-1;
  }
  else 
  {
    if(Glue.SyncMode&Glue.SYNCPAL) // 50hz
      Glue.nLines=GLU_PAL_SCANLINES; //313
    else // 60hz
      Glue.nLines=GLU_NTSC_SCANLINES; //263
    Glue.de_start_line=TopScanlines[VideoFreqIdx];
    Glue.de_end_line=Glue.de_start_line+200-1;
  }
#if defined(SSE_VID_STVL1) 
  if(OPTION_C3)
  {
    // blit the frame, set vbi pending
    // delete rest of 60hz or 71hz screen, because our rendering
    // surface has just too many lines for those frequencies
    if(Stvl.framefreq!=50 && border
      && draw_mem_line_ptr>(DWORD*)draw_mem
      && draw_mem_line_ptr<(DWORD*)Disp.VideoMemoryEnd)
    {      
      for(DWORD* i=draw_mem_line_ptr;i<(DWORD*)Disp.VideoMemoryEnd;i++)
        *i=0;
    }
    if(Stvl.framefreq && Glue.PreviousVideoFreq!=Stvl.framefreq)
    {
      StvlUpdate();
      Glue.PreviousVideoFreq=Stvl.framefreq;
#if defined(SSE_VID_D3D_VSYNC)
      if(OPTION_AUTOVSYNC&&!FullScreen||FullScreen&&OPTION_AUTOVSYNC_FS&&OPTION_FAKE_FULLSCREEN)
        Disp.D3DCreateSurfaces(); // probably switch on/off autovsync, too bad if there's a glitch
#endif
      UPDATE_STATUS_BAR_PART(SB_PART_FREQ);
#if !defined(SSE_GUI_421_REORDER)
      OptionBox.UpdateSTVideoPage();
#endif
      draw_grille_black=4;
    }
    Stvl.render_y=0;
    draw_mem_line_ptr=(DWORD*)draw_mem; // Disp.DrawToVidMem is ignored
    event_vbl_interrupt();
    Glue.vbl_pending=true;
    Glue.vbl_pending_time=A_S_T+Stvl.tick8*TICKS8;
    update_ipl(Glue.vbl_pending_time);
    scan_y=-TopScanlines[idx];
    return;
  }
  else
#endif
  {
    if(Glue.PreviousVideoFreq!=Glue.Freq[idx])
    {
      //TRACE_LOG("(%d->%d) ",Glue.PreviousVideoFreq,Glue.Freq[idx]);
      Glue.PreviousVideoFreq=Glue.Freq[idx];
#if defined(SSE_VID_D3D_VSYNC)
      if(OPTION_AUTOVSYNC&&!FullScreen||FullScreen&&OPTION_AUTOVSYNC_FS&&OPTION_FAKE_FULLSCREEN)
        Disp.D3DCreateSurfaces(); // probably switch on/off autovsync, too bad if there's a glitch
#endif
      init_screen();
      UPDATE_STATUS_BAR_PART(SB_PART_FREQ); // new frequency in status bar
#if !defined(SSE_GUI_421_REORDER)
      OptionBox.UpdateSTVideoPage(); // new frequency in option box
#endif
      draw_grille_black=4;
    }
  }
  //ASSERT(!Glue.m_Status.vbi_done);
  // As soon as VSYNC is asserted, the MMU keeps on copying VBASE to VCOUNT
  // We don't emulate the continuous copy but we copy at start and stop
  // (event_start_vbl(),event_trigger_vbi())
  Glue.vsync=false;
  Mmu.VideoCounter=VCountAtHSync=shifter_draw_pointer=vbase;
#if defined(SSE_HARDWARE_OVERSCAN)
  // hack to get correct display
  if(OPTION_HWOVERSCAN && SSEConfig.OverscanOn)
  {
    int off=0;
    if(COLOUR_MONITOR)
    {
      if(VideoFreqAtStartOfVbl==PAL_HZ)
        off=(OPTION_HWOVERSCAN==LACESCAN) ? (27*236-8*3) : (23*224+2*8); 
      // and 236*24-80+22+8+8+8+8+8 for other "generic" overscan circuit
      else //TODO
        off=(OPTION_HWOVERSCAN==LACESCAN) ? (20*234-8*3) : (16*224+2*8);
      if(border>=2)
        off+=8;
    }
    SHIFT_SDP(off);
    Mmu.VideoCounter=shifter_draw_pointer;
  }
#endif
  Glue.vbl_pending=true;
  Glue.vbl_pending_time=time_of_next_event;
  update_ipl(Glue.vbl_pending_time);
  Glue.m_Status.vbi_done=true;
  scan_y=-Glue.de_start_line;
}


/*  There's an event for floppy (STW etc.) because we want to handle DRQ for each
    byte, and the resolution of HBL is too gross for that:

    6256 bytes/ track , 5 revs /s= 31280 bytes
    1 second= 8021248 CPU cycles in our emu
    8021248/31280 = 256,433 cycles / byte
    8000000/31280 = 255,754 cycles / byte
    One HBL= 512 cycles at 50hz.

    Caps works with HBL because it hold its own cycle count.
    
    Here we should transfer control, or dispatch to handlers
*/

void event_wd1772() {
  Fdc.current_time=time_of_next_event;
  Fdc.OnUpdate();
}


void event_driveA_ip() {
  FloppyDrive[DRIVE_A].IndexPulse();
}


void event_driveB_ip() {
  FloppyDrive[DRIVE_B].IndexPulse();
}


//  ACIA event to handle IO with both 6301 and MIDI
// TODO risk of simultanate events?

COUNTER_VAR time_of_event_acia;

void event_acia() {
  if(OPTION_C1)
  {
    // find ACIA event to run
    // start transmission
//    ASSERT(time_of_event_acia==time_of_next_event);
    if(acia[ACIA_IKBD].LineTxBusy==2 && time_of_event_acia==acia[ACIA_IKBD].TimeTx)
      acia[ACIA_IKBD].TransmitTDR();
    else if(acia[ACIA_MIDI].LineTxBusy==2 && time_of_event_acia==acia[ACIA_MIDI].TimeTx)
      acia[ACIA_MIDI].TransmitTDR();
    // IKBD
    else if(acia[ACIA_IKBD].LineRxBusy==1 && time_of_event_acia==acia[ACIA_IKBD].TimeRx)
      agenda_keyboard_replace(0); // from IKBD
    else if(acia[ACIA_IKBD].LineTxBusy && time_of_event_acia==acia[ACIA_IKBD].TimeTx)
      agenda_ikbd_process(acia[ACIA_IKBD].tdrs); // to IKBD
    // MIDI
    else if(acia[ACIA_MIDI].LineRxBusy && time_of_event_acia==acia[ACIA_MIDI].TimeRx)
      agenda_midi_replace(0); // from MIDI
    else if(acia[ACIA_MIDI].LineTxBusy && time_of_event_acia==acia[ACIA_MIDI].TimeTx)
    { // to MIDI, do the job here
      acia[ACIA_MIDI].LineTxBusy=0; 
      MIDIPort.OutputByte(acia[ACIA_MIDI].tdrs);
      // send next MIDI note if any
      if(!(acia[ACIA_MIDI].sr&BIT_1))
        acia[ACIA_MIDI].TransmitTDR();
    }
    time_of_event_acia=time_of_next_event+nSysCyclesPerSecond; // put into future
    // schedule next ACIA event if any (if not, it's still in the future)
    // it's not very smart but I see no better way for now
    if(acia[ACIA_IKBD].LineRxBusy==1 && acia[ACIA_IKBD].TimeRx-time_of_event_acia<0)
      time_of_event_acia=acia[ACIA_IKBD].TimeRx;
    if(acia[ACIA_IKBD].LineTxBusy && acia[ACIA_IKBD].TimeTx-time_of_event_acia<0)
      time_of_event_acia=acia[ACIA_IKBD].TimeTx;
    if(acia[ACIA_MIDI].LineRxBusy && acia[ACIA_MIDI].TimeRx-time_of_event_acia<0)
      time_of_event_acia=acia[ACIA_MIDI].TimeRx;
    if(acia[ACIA_MIDI].LineTxBusy && acia[ACIA_MIDI].TimeTx-time_of_event_acia<0)
      time_of_event_acia=acia[ACIA_MIDI].TimeTx;
  }
  else
    time_of_event_acia=time_of_next_event+nSysCyclesPerSecond; // put into future
}


void event_dummy() { 
  // used when STVL is active
  // if it's actually reached, it's probably a bug
  TimeOfHSyncOff=time_of_next_event;
}

#undef LOGSECTION


#endif//#if defined(SSE_LIBRETRO)
