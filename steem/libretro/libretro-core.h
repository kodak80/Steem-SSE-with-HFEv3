#pragma once

#if defined(SSE_LIBRETROSOUND1)
extern DWORD n_samples_this_vbl;
extern int snd_max_frames;
#endif

#if defined(SSE_LIBRETROSOUND1B)
#include "libretro.h"
extern retro_audio_sample_t audio_cb;
#endif

//extern DWORD SnapshotSize;

// input devices
#define RETRO_DEVICE_ST_JOYSTICK0 RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define RETRO_DEVICE_ST_JOYSTICK1 RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
#define RETRO_DEVICE_ST_MOUSE RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_MOUSE, 0)
#define RETRO_DEVICE_ST_KEYBOARD RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_KEYBOARD, 0)

#define PORTS_NUMBER 2
#define ID_PLAYER1 0
#define ID_PLAYER2 1

#define MAX_BUTTONS 14 //?

typedef enum {
   BTN_B      = 0,
   BTN_Y      = 1,
   BTN_SELECT = 2,
   BTN_START  = 3,
   BTN_DUP    = 4,
   BTN_DDOWN  = 5,
   BTN_DLEFT  = 6,
   BTN_DRIGHT = 7,
   BTN_A      = 8,
   BTN_X      = 9,
   BTN_L      = 10,
   BTN_R      = 11,
   BTN_L2     = 12,
   BTN_R2     = 13,
} t_buttons;

typedef struct {
   unsigned char buttons[MAX_BUTTONS];
} t_button_cfg;

