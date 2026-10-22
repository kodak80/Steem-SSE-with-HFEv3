/*---------------------------------------------------------------------------
PROJECT: Steem SSE
Atari ST emulator
Copyright (C) 2021 by Anthony Hayward and Russel Hayward + SSE

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

DOMAIN: Rendering
FILE: interface_pulse.cpp
DESCRIPTION: This file contains client code for PulseAudio.
TODO: refactor, clean up
---------------------------------------------------------------------------*/



#include "pch.h"

#if defined(SSE_UNIX_PULSEAUDIO)

#include <debug.h>
#include <computer.h>
#include <translate.h>


#include <gui.h>
#include <sound.h>

#include <signal.h>
#include <pulse/pulseaudio.h>
#include <pulse/rtclock.h>

#include <pulse/ext-device-manager.h>

#define LOGSECTION LOGSECTION_SOUND




//this whole part just to get device names!
//https://gist.githubusercontent.com/andrewrk/6470f3786d05999fcb48/raw/fc397bb4e3890ea86ab581fb419b87b956da85a4/pulseaudio-device-list.c

// Field list is here: http://0pointer.de/lennart/projects/pulseaudio/doxygen/structpa__sink__info.html
typedef struct pa_devicelist {
	uint8_t initialized;
	char name[512];
	uint32_t index;
	char description[256];
} pa_devicelist_t;

void pa_state_cb(pa_context *c, void *userdata);
void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata);
void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata);
int pa_get_devicelist(pa_devicelist_t *input, pa_devicelist_t *output);

// This callback gets called when our context changes state.  We really only
// care about when it's ready or if it has failed
void pa_state_cb(pa_context *c, void *userdata) {
	pa_context_state_t state;
	int *pa_ready = (int*)userdata;

	state = pa_context_get_state(c);
	switch  (state) {
		// There are just here for reference
		case PA_CONTEXT_UNCONNECTED:
		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
		default:
			break;
		case PA_CONTEXT_FAILED:
		case PA_CONTEXT_TERMINATED:
			*pa_ready = 2;
			break;
		case PA_CONTEXT_READY:
			*pa_ready = 1;
			break;
	}
}

// pa_mainloop will call this function when it's ready to tell us about a sink.
// Since we're not threading, there's no need for mutexes on the devicelist
// structure
void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata) {
    pa_devicelist_t *pa_devicelist = (pa_devicelist_t *)userdata;
    int ctr = 0;

    // If eol is set to a positive number, you're at the end of the list
    if (eol > 0) {
	return;
    }

    // We know we've allocated 16 slots to hold devices.  Loop through our
    // structure and find the first one that's "uninitialized."  Copy the
    // contents into it and we're done.  If we receive more than 16 devices,
    // they're going to get dropped.  You could make this dynamically allocate
    // space for the device list, but this is a simple example.
    for (ctr = 0; ctr < 16; ctr++) {
	if (! pa_devicelist[ctr].initialized) {
	    strncpy(pa_devicelist[ctr].name, l->name, 511);
	    strncpy(pa_devicelist[ctr].description, l->description, 255);
	    pa_devicelist[ctr].index = l->index;
	    pa_devicelist[ctr].initialized = 1;
	    break;
	}
    }
}

// See above.  This callback is pretty much identical to the previous
void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata) {
    pa_devicelist_t *pa_devicelist = (pa_devicelist_t *)userdata;
    int ctr = 0;

    if (eol > 0) {
	return;
    }

    for (ctr = 0; ctr < 16; ctr++) {
	if (! pa_devicelist[ctr].initialized) {
	    strncpy(pa_devicelist[ctr].name, l->name, 511);
	    strncpy(pa_devicelist[ctr].description, l->description, 255);
	    pa_devicelist[ctr].index = l->index;
	    pa_devicelist[ctr].initialized = 1;
	    break;
	}
    }
}

int pa_get_devicelist(pa_devicelist_t *input, pa_devicelist_t *output) {
    // Define our pulse audio loop and connection variables
    pa_mainloop *pa_ml;
    pa_mainloop_api *pa_mlapi;
    pa_operation *pa_op;
    pa_context *pa_ctx;


    // We'll need these state variables to keep track of our requests
    int state = 0;
    int pa_ready = 0;

    // Initialize our device lists
    memset(input, 0, sizeof(pa_devicelist_t) * 16);
    memset(output, 0, sizeof(pa_devicelist_t) * 16);

    // Create a mainloop API and connection to the default server
    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, "test");

    // This function connects to the pulse server
    pa_context_connect(pa_ctx, NULL, (pa_context_flags_t)0, NULL);


    // This function defines a callback so the server will tell us it's state.
    // Our callback will wait for the state to be ready.  The callback will
    // modify the variable to 1 so we know when we have a connection and it's
    // ready.
    // If there's an error, the callback will set pa_ready to 2
    pa_context_set_state_callback(pa_ctx, pa_state_cb, &pa_ready);

    // Now we'll enter into an infinite loop until we get the data we receive
    // or if there's an error
    for (;;) {
	// We can't do anything until PA is ready, so just iterate the mainloop
	// and continue
	if (pa_ready == 0) {
	    pa_mainloop_iterate(pa_ml, 1, NULL);
	    continue;
	}
	// We couldn't get a connection to the server, so exit out
	if (pa_ready == 2) {
	    pa_context_disconnect(pa_ctx);
	    pa_context_unref(pa_ctx);
	    pa_mainloop_free(pa_ml);
	    return -1;
	}
	// At this point, we're connected to the server and ready to make
	// requests
	switch (state) {
	    // State 0: we haven't done anything yet
	    case 0:
		// This sends an operation to the server.  pa_sinklist_info is
		// our callback function and a pointer to our devicelist will
		// be passed to the callback The operation ID is stored in the
		// pa_op variable
		pa_op = pa_context_get_sink_info_list(pa_ctx,
			pa_sinklist_cb,
			output
			);

		// Update state for next iteration through the loop
		state++;
		break;
	    case 1:
		// Now we wait for our operation to complete.  When it's
		// complete our pa_output_devicelist is filled out, and we move
		// along to the next state
		if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
		    pa_operation_unref(pa_op);

		    // Now we perform another operation to get the source
		    // (input device) list just like before.  This time we pass
		    // a pointer to our input structure
		    pa_op = pa_context_get_source_info_list(pa_ctx,
			    pa_sourcelist_cb,
			    input
			    );
		    // Update the state so we know what to do next
		    state++;
		}
		break;
	    case 2:
		if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
		    // Now we're done, clean up and disconnect and return
		    pa_operation_unref(pa_op);
		    pa_context_disconnect(pa_ctx);
		    pa_context_unref(pa_ctx);
		    pa_mainloop_free(pa_ml);
		    return 0;
		}
		break;
	    default:
		// We should never see this state
		fprintf(stderr, "in state %d\n", state);
		return -1;
	}
	// Iterate the main loop and go again.  The second argument is whether
	// or not the iteration should block until something is ready to be
	// done.  Set it to zero for non-blocking.
	pa_mainloop_iterate(pa_ml, 1, NULL);
    }
}


int Pulse_DeviceList(char *DevName) { //16x512
  int ctr;

  // This is where we'll store the input device list
  pa_devicelist_t pa_input_devicelist[16];

  // This is where we'll store the output device list
  pa_devicelist_t pa_output_devicelist[16];  
  
  if (pa_get_devicelist(pa_input_devicelist, pa_output_devicelist) < 0) {
    fprintf(stderr, "failed to get device list\n");
    return 0;
  }
  
  for (ctr = 0; ctr < 16; ctr++) {
    if (! pa_output_devicelist[ctr].initialized) {
      break;
    }
/* eg
=======[ Output Device #1 ]=======
Description: Built-in Audio Analog Stereo
Name: alsa_output.pci-0000_00_11.0.analog-stereo
Index: 0
*/    
    printf("=======[ Output Device #%d ]=======\n", ctr+1);
    printf("Description: %s\n", pa_output_devicelist[ctr].description);
    printf("Name: %s\n", pa_output_devicelist[ctr].name);
    printf("Index: %d\n", pa_output_devicelist[ctr].index);
    printf("\n");
    strncpy(&DevName[ctr*512],pa_output_devicelist[ctr].name,511);
  }  
  return ctr; // # output devices
  
}

#if 0
int main(int argc, char *argv[]) {
//int get_list()  {
//int Pulse_DeviceList() {
    int ctr;

    // This is where we'll store the input device list
    pa_devicelist_t pa_input_devicelist[16];

    // This is where we'll store the output device list
    pa_devicelist_t pa_output_devicelist[16];

    if (pa_get_devicelist(pa_input_devicelist, pa_output_devicelist) < 0) {
	fprintf(stderr, "failed to get device list\n");
	return 1;
    }

    for (ctr = 0; ctr < 16; ctr++) {
	if (! pa_output_devicelist[ctr].initialized) {
	    break;
	}
	printf("=======[ Output Device #%d ]=======\n", ctr+1);
	printf("Description: %s\n", pa_output_devicelist[ctr].description);
	printf("Name: %s\n", pa_output_devicelist[ctr].name);
	printf("Index: %d\n", pa_output_devicelist[ctr].index);
	printf("\n");
    }

    for (ctr = 0; ctr < 16; ctr++) {
	if (! pa_input_devicelist[ctr].initialized) {
	    break;
	}
	printf("=======[ Input Device #%d ]=======\n", ctr+1);
	printf("Description: %s\n", pa_input_devicelist[ctr].description);
	printf("Name: %s\n", pa_input_devicelist[ctr].name);
	printf("Index: %d\n", pa_input_devicelist[ctr].index);
	printf("\n");
    }
    return 0;
}
#endif







// partially based on pascat.c which has the following notice
/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering
  Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/






#define TIME_EVENT_USEC 50000

#define CLEAR_LINE "\x1B[K"

static enum { RECORD, PLAYBACK } mode = PLAYBACK;

pa_context *context = NULL;
pa_stream *stream = NULL;
pa_mainloop_api *mainloop_api = NULL;

void *pulse_buffer = NULL;

size_t buffer_length = 0, buffer_index = 0;

pa_io_event* stdio_event = NULL;

char *stream_name = NULL, *client_name = NULL, *pulse_device = NULL;

#ifdef SSE_DEBUG
bool const verbose = true;
#else
bool const verbose = false;
#endif

//static pa_volume_t volume = PA_VOLUME_NORM;
//static int volume_is_set = 0;

static pa_sample_spec sample_spec = {
    .format = PA_SAMPLE_S16LE,
    .rate = 44100,
    .channels = 2
};

pa_channel_map channel_map;
int channel_map_set = 0;

//static pa_stream_flags_t flags = PA_STREAM_NOFLAGS;
static int flags = PA_STREAM_NOFLAGS;

static size_t latency = 0, process_time=0;


//pa_mainloop* main_loop = NULL;
pa_threaded_mainloop *main_loop=NULL;


char *bn="steem", *server = NULL;
pa_time_event *time_event = NULL;



bool pulse_init=false;


/* A shortcut for terminating the application */
static void quit(int ret) {
    assert(mainloop_api);
    mainloop_api->quit(mainloop_api, ret);
}

/* Write some data to the stream */
static void do_stream_write(size_t length) {
  //TRACE("do_stream_write %d\n",length);
  if(!length||!psg_buf_length)
    return;
  pulse_buffer=(BYTE*)pa_xmalloc(length);
  BYTE* b=(BYTE*)pulse_buffer;
  int pointer_byte=sound_buf_pointer;
  pointer_byte%=psg_buf_length; // Get sample count within buffer
  pointer_byte*=sound_bytes_per_sample; // Convert to bytes
  DWORD bufferSize=length/sound_bytes_per_sample;
  for(DWORD i=0;i<bufferSize;i++)
  {
    for(int a=0;a<sound_bytes_per_sample;a++)
    {
      if(pointer_byte>=X_SOUND_BUF_LEN_BYTES)
        *b=*(b-2);
      else
        *b=x_sound_buf[pointer_byte];
      if(sound_num_bits==8) // CPU optimises this
        *b^=128;
      pointer_byte++;
      if(pointer_byte>=X_SOUND_BUF_LEN_BYTES) //TODO
      {}//pointer_byte-=X_SOUND_BUF_LEN_BYTES;
      b++;
    }
    sound_buf_pointer++;
  }  

  buffer_length=psg_buf_length;//?
  size_t l;
  //assert(length);

    //if (!buffer || !buffer_length)
      //  return;

  l = length;
 // if (l > buffer_length)
   // l = buffer_length;
  buffer_index=0;

  if (pa_stream_write(stream, (uint8_t*) pulse_buffer + buffer_index, l, NULL, 0,
    PA_SEEK_RELATIVE) < 0) {
      TRACE("pa_stream_write() failed: %s\n", pa_strerror(pa_context_errno(context)));
      quit(1);
      return;
  }
  //else TRACE("write stream %p + %x %d ok\n",buffer,buffer_index, l);
  pa_xfree(pulse_buffer);
  pulse_buffer=NULL;
    
    
}

/* This is called whenever new data may be written to the stream */
static void stream_write_callback(pa_stream *s, size_t length, void *userdata) {
  //TRACE("stream_write_callback stream %x %d bytes\n",s,length);
  assert(s);
  assert(length > 0);
  ASSERT(s==stream);

  //if (stdio_event)
    //  mainloop_api->io_enable(stdio_event, PA_IO_EVENT_INPUT);

  //if (!buffer)
    //  return;

  do_stream_write(length);
}

/* This is called whenever new data may is available */
static void stream_read_callback(pa_stream *s, size_t length, void *userdata) {
    TRACE("stream_read_callback\n");
#if 0
    const void *data;
    assert(s);
    assert(length > 0);

    if (stdio_event)
        mainloop_api->io_enable(stdio_event, PA_IO_EVENT_OUTPUT);

    if (pa_stream_peek(s, &data, &length) < 0) {
        fprintf(stderr, "pa_stream_peek() failed: %s\n", pa_strerror(pa_context_errno(context)));
        quit(1);
        return;
    }

    assert(data);
    assert(length > 0);

    if (buffer) {
        buffer = pa_xrealloc(buffer, buffer_length + length);
        memcpy((uint8_t*) buffer + buffer_length, data, length);
        buffer_length += length;
    } else {
        buffer = pa_xmalloc(length);
        memcpy(buffer, data, length);
        buffer_length = length;
        buffer_index = 0;
    }

    pa_stream_drop(s);
#endif    
}

/* This routine is called whenever the stream state changes */
void stream_state_callback(pa_stream *s, void *userdata) {
    TRACE_LOG("stream_state_callback %x %d\n",s,pa_stream_get_state(s));
    assert(s);

    switch (pa_stream_get_state(s)) {
        case PA_STREAM_CREATING:
            TRACE_LOG("PA_STREAM_CREATING\n");
            break;
        case PA_STREAM_TERMINATED:
            TRACE_LOG("PA_STREAM_TERMINATED\n");
            break;

        case PA_STREAM_READY:
            if (1||verbose) {
                TRACE_LOG("PA_STREAM_READY\n");
                const pa_buffer_attr *a;
                char cmt[PA_CHANNEL_MAP_SNPRINT_MAX], sst[PA_SAMPLE_SPEC_SNPRINT_MAX];

                TRACE_LOG("Stream successfully created.\n");

                if (!(a = pa_stream_get_buffer_attr(s)))
                    TRACE("pa_stream_get_buffer_attr() failed: %s\n", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
                else {

                    if (mode == PLAYBACK)
                    {
                        TRACE_LOG("Buffer metrics: maxlength=%u, tlength=%u, prebuf=%u, minreq=%u\n", a->maxlength, a->tlength, a->prebuf, a->minreq);
                    }
                    /*else {
                        assert(mode == RECORD);
                        TRACE("Buffer metrics: maxlength=%u, fragsize=%u\n", a->maxlength, a->fragsize);
                    }*/
                }

                TRACE_LOG("Using sample spec '%s', channel map '%s'.\n",
                        pa_sample_spec_snprint(sst, sizeof(sst), pa_stream_get_sample_spec(s)),
                        pa_channel_map_snprint(cmt, sizeof(cmt), pa_stream_get_channel_map(s)));

                TRACE_LOG("Connected to device %s (%u, %ssuspended).\n",
                        pa_stream_get_device_name(s),
                        pa_stream_get_device_index(s),
                        pa_stream_is_suspended(s) ? "" : "not ");
            }

            break;

        case PA_STREAM_FAILED:
        default:
            TRACE("PA_STREAM_FAILED: %s\n", pa_strerror(pa_context_errno(context)));
            if(s==stream)
              quit(1);
    }
}

static void stream_suspended_callback(pa_stream *s, void *userdata) {
#ifdef SSE_DEBUG
  TRACE("stream_suspended_callback\n");
  assert(s);

  if (verbose) {
      if (pa_stream_is_suspended(s))
          fprintf(stderr, "Stream device suspended.%s \n", CLEAR_LINE);
      else
          fprintf(stderr, "Stream device resumed.%s \n", CLEAR_LINE);
  }
#endif
}

static void stream_underflow_callback(pa_stream *s, void *userdata) {
#ifdef SSE_DEBUG
  TRACE("stream_underflow_callback\n");
  assert(s);

  if (verbose)
      fprintf(stderr, "Stream underrun.%s \n",  CLEAR_LINE);
#endif
}

static void stream_overflow_callback(pa_stream *s, void *userdata) {
#ifdef SSE_DEBUG
  TRACE("stream_overflow_callback\n");
  assert(s);

  if (verbose)
      fprintf(stderr, "Stream overrun.%s \n", CLEAR_LINE);
#endif
}

void stream_started_callback(pa_stream *s, void *userdata) {
#ifdef SSE_DEBUG
  TRACE_LOG("stream_started_callback %x %x\n",s,userdata);
  assert(s);

  if (verbose)
      fprintf(stderr, "Stream started.%s \n", CLEAR_LINE);
#endif
}

static void stream_moved_callback(pa_stream *s, void *userdata) {
#ifdef SSE_DEBUG
  TRACE("stream_moved_callback\n");
  assert(s);

  if (verbose)
      fprintf(stderr, "Stream moved to device %s (%u, %ssuspended).%s \n", pa_stream_get_device_name(s), pa_stream_get_device_index(s), pa_stream_is_suspended(s) ? "" : "not ",  CLEAR_LINE);
#endif
}

static void stream_buffer_attr_callback(pa_stream *s, void *userdata) {
#ifdef SSE_DEBUG
  TRACE("stream_buffer_attr_callback\n");
  assert(s);

  if (verbose)
      fprintf(stderr, "Stream buffer attributes changed.%s \n",  CLEAR_LINE);
#endif
}

static void stream_event_callback(pa_stream *s, const char *name, pa_proplist *pl, void *userdata) {
  TRACE("stream_event_callback\n");
  char *t;
  assert(s);
  assert(name);
  assert(pl);
  t = pa_proplist_to_string_sep(pl, ", ");
  fprintf(stderr, "Got event '%s', properties '%s'\n", name, t);
  pa_xfree(t);
}

/* This is called whenever the context status changes */
static void context_state_callback(pa_context *c, void *userdata) {
    TRACE_LOG("context_state_callback %d\n",pa_context_get_state(c));
    assert(c);

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY: {
            TRACE_LOG("PA_CONTEXT_READY\n");
            int r;
            pa_buffer_attr buffer_attr;

            assert(c);
            assert(!stream);

            /*if (verbose)
                fprintf(stderr, "Connection established.%s \n", CLEAR_LINE);*/

            TRACE_LOG("%d %d %d %s\n",sample_spec.channels,sample_spec.format,sample_spec.rate,stream_name);
            if (!(stream = pa_stream_new(c, stream_name, &sample_spec, channel_map_set 
              ? &channel_map : NULL))) {
                TRACE("pa_stream_new() failed: %s\n", pa_strerror(pa_context_errno(c)));
                goto fail;
            }

            pa_stream_set_state_callback(stream, stream_state_callback, NULL);
            pa_stream_set_write_callback(stream, stream_write_callback, NULL);
            pa_stream_set_read_callback(stream, stream_read_callback, NULL);
            pa_stream_set_suspended_callback(stream, stream_suspended_callback, NULL);
            pa_stream_set_moved_callback(stream, stream_moved_callback, NULL);
            pa_stream_set_underflow_callback(stream, stream_underflow_callback, NULL);
            pa_stream_set_overflow_callback(stream, stream_overflow_callback, NULL);
            pa_stream_set_started_callback(stream, stream_started_callback, NULL);
            pa_stream_set_event_callback(stream, stream_event_callback, NULL);
            pa_stream_set_buffer_attr_callback(stream, stream_buffer_attr_callback, NULL);

            if (latency > 0) {
                memset(&buffer_attr, 0, sizeof(buffer_attr));
                buffer_attr.tlength = (uint32_t) latency;
                buffer_attr.minreq = (uint32_t) process_time;
                buffer_attr.maxlength = (uint32_t) -1;
                buffer_attr.prebuf = (uint32_t) -1;
                buffer_attr.fragsize = (uint32_t) latency;
                flags |= PA_STREAM_ADJUST_LATENCY;
            }
            
            
            
            
          //   pa_cvolume vol;
         // vol.channels=WavFileFormat.nChannels;
         // vol.values[0]=vol.values[1]=Sound_Volume+10000)*5; // liimted to < max
            
           pa_volume_t vol=(SoundVolume+10000)*6;
            TRACE_LOG("device %s mode %d latency %d\n",pulse_device,mode,latency);
            if (mode == PLAYBACK) {
                pa_cvolume cv;
                if ((r = pa_stream_connect_playback(stream, pulse_device, latency > 0 
                ? &buffer_attr : NULL, (pa_stream_flags_t)flags, /*volume_is_set */ SoundVolume
                ? pa_cvolume_set(&cv, sample_spec.channels, vol) : NULL, NULL)) < 0) {
                    TRACE("pa_stream_connect_playback() failed: %s\n", pa_strerror(pa_context_errno(c)));
                    goto fail;
                }
            /*
            } else {
                if ((r = pa_stream_connect_record(stream, pulse_device, latency > 0 
                    ? &buffer_attr : NULL, (pa_stream_flags_t)flags)) < 0) {
                    TRACE2("pa_stream_connect_record() failed: %s\n", pa_strerror(pa_context_errno(c)));
                    goto fail;
                }*/
            }
            
#if defined(SSE_DRIVE_SOUND)
    //TRACE("OPTION_DRIVE_SOUND %d\n",OPTION_DRIVE_SOUND);
    if(OPTION_DRIVE_SOUND)
    {
      FloppyDrive[DRIVE_A].SoundLoadSamples();
      FloppyDrive[DRIVE_B].SoundLoadSamples();
      
    }
#endif               
            
            break;
        }

        case PA_CONTEXT_TERMINATED:
            quit(0);
            break;

        case PA_CONTEXT_FAILED:
        default:
            TRACE("Connection failure: %s\n", pa_strerror(pa_context_errno(c)));
            goto fail;
    }

    return;

fail:
    TRACE("fail\n");
    quit(1);

}


#ifdef DEADC0DE
/* Connection draining complete */
static void context_drain_complete(pa_context*c, void *userdata) {
  TRACE("context_drain_complete\n");
  pa_context_disconnect(c);
}

/* Stream draining complete */
static void stream_drain_complete(pa_stream*s, int success, void *userdata) {
    TRACE("stream_drain_complete\n");

    if (!success) {
        fprintf(stderr, "Failed to drain stream: %s\n", pa_strerror(pa_context_errno(context)));
        quit(1);
    }

    if (verbose)
        fprintf(stderr, "Playback stream drained.\n");

    pa_stream_disconnect(stream);
    pa_stream_unref(stream); 
    stream = NULL;

    if (!pa_context_drain(context, context_drain_complete, NULL)) // linker error
        pa_context_disconnect(context);
    else {
        if (verbose)
            fprintf(stderr, "Draining connection to server.\n");
    }
}


/* New data on STDIN **/
static void stdin_callback(pa_mainloop_api*a, pa_io_event *e, int fd, pa_io_event_flags_t f, void *userdata) {
  TRACE("stdin_callback\n");
#if 0
    size_t l, w = 0;
    ssize_t r;

    assert(a == mainloop_api);
    assert(e);
    assert(stdio_event == e);

    if (buffer) {
        mainloop_api->io_enable(stdio_event, PA_IO_EVENT_NULL);
        return;
    }

    if (!stream || pa_stream_get_state(stream) != PA_STREAM_READY || !(l = w = pa_stream_writable_size(stream)))
        l = 4096;

    buffer = pa_xmalloc(l);

    if ((r = read(fd, buffer, l)) <= 0) {
        if (r == 0) {
            if (verbose)
                fprintf(stderr, "Got EOF.\n");

            if (stream) {
                pa_operation *o;

                if (!(o = pa_stream_drain(stream, stream_drain_complete, NULL))) {
                    fprintf(stderr, "pa_stream_drain(): %s\n", pa_strerror(pa_context_errno(context)));
                    quit(1);
                    return;
                }

                pa_operation_unref(o);
            } else
                quit(0);

        } else {
            fprintf(stderr, "read() failed: %s\n", strerror(errno));
            quit(1);
        }

        mainloop_api->io_free(stdio_event);
        stdio_event = NULL;
        return;
    }

    buffer_length = (uint32_t) r;
    buffer_index = 0;

    if (w)
        do_stream_write(w);
#endif
}

/* Some data may be written to STDOUT */
static void stdout_callback(pa_mainloop_api*a, pa_io_event *e, int fd, pa_io_event_flags_t f, void *userdata) {
  TRACE("stdout_callback\n");
#if 0
    ssize_t r;

    assert(a == mainloop_api);
    assert(e);
    assert(stdio_event == e);

    if (!buffer) {
        mainloop_api->io_enable(stdio_event, PA_IO_EVENT_NULL);
        return;
    }

    assert(buffer_length);

    if ((r = write(fd, (uint8_t*) buffer+buffer_index, buffer_length)) <= 0) {
        fprintf(stderr, "write() failed: %s\n", strerror(errno));
        quit(1);

        mainloop_api->io_free(stdio_event);
        stdio_event = NULL;
        return;
    }

    buffer_length -= (uint32_t) r;
    buffer_index += (uint32_t) r;

    if (!buffer_length) {
        pa_xfree(buffer);
        buffer = NULL;
        buffer_length = buffer_index = 0;
    }
#endif
}
#endif


/* UNIX signal to quit recieved */
static void exit_signal_callback(pa_mainloop_api*m, pa_signal_event *e, int sig, void *userdata) {
  if (verbose)
  {
      TRACE("Got signal, exiting.\n");
  }
  quit(0);
}


#ifdef DEADC0DE
/* Show the current latency */
static void stream_update_timing_callback(pa_stream *s, int success, void *userdata) {
  TRACE("stream_update_timing_callback\n");
/*
  pa_usec_t l, usec;
  int negative = 0;

  assert(s);

  if (!success ||
      pa_stream_get_time(s, &usec) < 0 ||
      pa_stream_get_latency(s, &l, &negative) < 0) {
      fprintf(stderr, "Failed to get latency: %s\n", pa_strerror(pa_context_errno(context)));
      quit(1);
      return;
  }

  fprintf(stderr, "Time: %0.3f sec; Latency: %0.0f usec.  \r",
          (float) usec / 1000000,
          (float) l * (negative?-1.0f:1.0f));*/
}


/* Someone requested that the latency is shown */
static void sigusr1_signal_callback(pa_mainloop_api*m, pa_signal_event *e, int sig, void *userdata) {
  TRACE("sigusr1_signal_callback\n");

  if (!stream)
      return;

  pa_operation_unref(pa_stream_update_timing_info(stream, stream_update_timing_callback, NULL));
}


static void time_event_callback(pa_mainloop_api *m, pa_time_event *e, const struct timeval *tv, void *userdata) {
  TRACE("time_event_callback\n");
/*
  if (stream && pa_stream_get_state(stream) == PA_STREAM_READY) {
      pa_operation *o;
      if (!(o = pa_stream_update_timing_info(stream, stream_update_timing_callback, NULL)))
          fprintf(stderr, "pa_stream_update_timing_info() failed: %s\n", pa_strerror(pa_context_errno(context)));
      else
          pa_operation_unref(o);
  }

  struct timeval now;
  gettimeofday(&now, NULL);
  pa_timeval_add(&now, TIME_EVENT_USEC);
  m->time_restart(e, &now);*/
}
#endif

HRESULT Pulse_Init() {
  HRESULT ok=DS_OK;
  if (!client_name)
      client_name = pa_xstrdup(bn);

  if (!stream_name)
      stream_name = pa_xstrdup(client_name);  
  TRACE_LOG("client_name %s stream_name %s\n",client_name,stream_name);



//  if(sound_device_name.IsEmpty())
  {

  }


  if(ok==DS_OK)
  {
    pulse_init=true;
    UseSound=XS_PULSE;
  }
  /*else
    Pulse_Release();*/
  TRACE_LOG("PulseAudio init %d\n",pulse_init);
  return ok;
}


void Pulse_Release() {
    Pulse_FreeBuffer(true);
    //pa_xfree(server);
    //pa_xfree(device);
    //pa_xfree(client_name);//TODO
    //pa_xfree(stream_name);
    pulse_init=false;
}


void Pulse_ChangeVolume() {
  if(!stream)
    return;
  pa_volume_t vol=SoundVolume ? (SoundVolume+10000)*6 :  PA_VOLUME_NORM;
  pa_cvolume cv;
  pa_cvolume_set(&cv, sample_spec.channels, vol);
  DWORD idx=pa_stream_get_index(stream);
  pa_context_set_sink_input_volume(context,idx,&cv,NULL,NULL);

}

/*
void pa_ext_device_manager_read_cb(pa_context *c, const pa_ext_device_manager_info *info, int eol, void *userdata) {
  TRACE("YO\n");
}*/

HRESULT Pulse_StartBuffer(int flatlevel1,int flatlevel2) {
  HRESULT ok=DSERR_GENERIC;
  // called by SoundStartBuffer(int flatlevel1,int flatlevel2)
  if(!pulse_init || !sound_bytes_per_sample)
    return ok;
  if(stream)
    Pulse_FreeBuffer(true);
    
  if(!(main_loop = pa_threaded_mainloop_new()))
  {
    TRACE("pa_mainloop_new() failed.\n");
  }
  else
    ok=DS_OK;

  if(ok==DS_OK)    
  {
    //mainloop_api = pa_mainloop_get_api(main_loop);
    TRACE_LOG("main_loop %p\n",main_loop);
    mainloop_api = pa_threaded_mainloop_get_api(main_loop);
    
    int r = pa_signal_init(mainloop_api);
    ASSERT(r==0);
    pa_signal_new(SIGINT, exit_signal_callback, NULL);//?
    pa_signal_new(SIGTERM, exit_signal_callback, NULL);//?
    /*if (!(stdio_event = mainloop_api->io_new(mainloop_api,
                                             mode == PLAYBACK ? STDIN_FILENO : STDOUT_FILENO,
                                             mode == PLAYBACK ? PA_IO_EVENT_INPUT : PA_IO_EVENT_OUTPUT,
                                             mode == PLAYBACK ? stdin_callback : stdout_callback, NULL))) {
        fprintf(stderr, "io_new() failed.\n");
        ok=DSERR_GENERIC;
    }*/

    /* Create a new connection context */
    if (!(context = pa_context_new(mainloop_api, client_name))) {
        TRACE("pa_context_new() failed.\n");
        ok=DSERR_GENERIC;
    }

    pa_context_set_state_callback(context, context_state_callback, NULL);

    /* Connect the context */
    if (pa_context_connect(context, server, (pa_context_flags_t)0, NULL) < 0) {
        TRACE("pa_context_connect() failed: %s\n", pa_strerror(pa_context_errno(context)));
        ok=DSERR_GENERIC;
    }
    
    
    // pa_ext_device_manager_read(context,&pa_ext_device_manager_read_cb,NULL);


/*    if (verbose) {
        struct timeval now;
        gettimeofday(&now, NULL);
        pa_timeval_add(&now, TIME_EVENT_USEC);
        if (!(time_event = mainloop_api->time_new(mainloop_api, &now, time_event_callback, NULL))) {
            fprintf(stderr, "time_new() failed.\n");
            ok=DSERR_GENERIC;
        }
    }*/
  }    
    
  psg_buf_length=X_SOUND_BUF_LEN_BYTES/sound_bytes_per_sample;
    
  sample_spec.channels=sound_num_channels;
  sample_spec.format=(sound_num_bits==16) ? PA_SAMPLE_S16LE : PA_SAMPLE_U8;
  sample_spec.rate=sound_freq;
  
  pulse_device=sound_device_name.Text;
  TRACE_LOG("device %s\n",pulse_device);
 
  sound_buf_pointer=0;  
  XSoundInitBuffer(flatlevel1,flatlevel2);
    
  /* Run the main loop */
  
  //if (pa_mainloop_run(main_loop, (int*)&ok) < 0) {
  if (pa_threaded_mainloop_start(main_loop) < 0) {
    TRACE("pa_mainloop_run() failed.\n");
    ok=DSERR_GENERIC;
  }
  else
  {
    TRACE_LOG("pa_mainloop_run() called\n");
    ok=DS_OK;
  }    
      
  // then wait for callback
  return ok;
}


void Pulse_FreeBuffer(bool Immediate) {
  
    TRACE_LOG("Pulse_FreeBuffer %d stream %p\n",Immediate,stream);
  
    if(pulse_buffer)
      pa_xfree(pulse_buffer);
    pulse_buffer=NULL;
      
    if(main_loop)
      pa_threaded_mainloop_stop (main_loop);  

    //Pulse_FreeBuffer(true);
    if(stream)
      pa_stream_unref(stream);
    stream=NULL;
    
    if(context)
      pa_context_unref(context);
    context=NULL;

    if (stdio_event) {
        assert(mainloop_api);
        mainloop_api->io_free(stdio_event);
    }

    if (time_event) {
        assert(mainloop_api);
        mainloop_api->time_free(time_event);
    }

    if (main_loop) {
        pa_signal_done();
        //pa_mainloop_free(main_loop);
        
        pa_threaded_mainloop_free(main_loop);
        main_loop=NULL;
    }
 
}


DWORD Pulse_GetTime() {
  if(stream==NULL) 
    return 0;
  return sound_buf_pointer;
}


HRESULT Pulse_Stop(bool Immediate) {
  Pulse_FreeBuffer(Immediate);
#if defined(SSE_DRIVE_SOUND)
  //if(OPTION_DRIVE_SOUND)
  {
    FloppyDrive[DRIVE_A].SoundReleaseBuffers();
    FloppyDrive[DRIVE_B].SoundReleaseBuffers();
  }
#endif  
  return DS_OK;
}


bool Pulse_IsPlaying() { 
  return (stream!=NULL);
}

#undef LOGSECTION

#endif//#ifdef SSE_UNIX_PULSEAUDIO

