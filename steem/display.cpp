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

DOMAIN: Rendering
FILE: display.cpp
DESCRIPTION: A class to encapsulate the process of outputting to the display.
This contains the GDI, DirectDraw, Direct3D and X code used by Steem for
output.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <draw.h>
#include <gui.h>
#include <debug.h>
#include <computer.h>
#include <display.h>
#include <osd.h>
#include <translate.h>
#include <dderr_meaning.h>
#include <diskman.h>
#include <infobox.h>
#include <notifyinit.h>
#include <stdarg.h>
#ifdef DEBUG_BUILD
#include <debugger.h>
#endif
#if defined(SSE_VID_RECORD_AVI)
#include <AVI/AviFile.h> // AVI (DD-only)
#endif


#if defined(BCC_BUILD) && defined(SSE_VID_DD)
#pragma message DD DIRECTDRAW_VERSION
#endif

#if !defined(SSE_NO_FREEIMAGE)
// those are the same names as in the library
FI_INITPROC FreeImage_Initialise;
FI_DEINITPROC FreeImage_DeInitialise;
FI_CONVFROMRAWPROC FreeImage_ConvertFromRawBits;
FI_SAVEPROC FreeImage_Save;
FI_FREEPROC FreeImage_Free;
FI_SUPPORTBPPPROC FreeImage_FIFSupportsExportBPP;
#endif

DWORD monitor_width,monitor_height;

/*  Steem SSE outputs at 32bit only (True Color). Format is X8R8G8B8.
    The former Steem 8bit, 16bit and 24bit routines have been removed in
    v4.1.2 to simplify the source. */
const BYTE BytesPerPixel=4;

/*  This is in case the format is R8G8B8X8 but I don't think it happens
    on current systems. It is used by DD and GDI, ignored by DD3. */
BYTE rgb32_bluestart_bit=0;

BYTE FullScreen=0; // can't be bool

#ifdef UNIX
#ifdef NO_SHM
bool TrySHM=false;
#else
bool TrySHM=true;
#endif
#endif

#if defined(SSE_VID_DD)
//Notice there's 64bit support for DirectDraw, strange for a deprecated library.
#if !defined(MINGW_BUILD)
SET_GUID(CLSID_DirectDraw,0xD7B70EE0,0x4340,0x11CF,0xB0,0x63,0x00,0x20,0xAF,0xC2,0xCD,0x35);
SET_GUID(IID_IDirectDraw,0x6C14DB80,0xA733,0x11CE,0xA5,0x21,0x00,0x20,0xAF,0x0B,0xE5,0x60);
SET_GUID(IID_IDirectDraw2,0xB3A6F3E0,0x2B43,0x11CF,0xA2,0xDE,0x00,0xAA,0x00,0xB9,0x33,0x56);
#endif
#if defined(SSE_VID_DD7) 
#if _MSC_VER == 1200 //VC6
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        EXTERN_C const GUID name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
DEFINE_GUID( IID_IDirectDraw7,0x15e65ec0,0x3b9c,0x11d2,0xb9,0x2f,0x00,0x60,0x97,0x97,0xea,0x5b );
#endif
#endif

const BYTE HzIdxToHz[NUM_HZ]={0,50,60,MONO_HZ,100,120};

#if defined(SSE_VID_2SCREENS)

void get_fullscreen_totalrect(RECT *rc) {
  if(draw_fs_blit_mode==DFSM_STRETCHBLIT)
  {
    rc->left=rc->top=0;
    rc->right=Disp.fs_res[Disp.fs_res_choice].x;
    rc->bottom=Disp.fs_res[Disp.fs_res_choice].y;
  }
  else if(draw_fs_blit_mode==DFSM_FAKEFULLSCREEN)
  {
    if(OPTION_FAKE_FULLSCREEN)
      *rc=Disp.rcMonitor; // monitor 1 or 2
    else
    { // monitor 1
      rc->left=rc->top=0;
      rc->right=monitor_width;
      rc->bottom=monitor_height;
    }
  }
  else
  {
    rc->left=rc->top=0;
    rc->right=Disp.SurfaceWidth;
    rc->bottom=Disp.SurfaceHeight;
  } 
}

#endif


void get_fullscreen_rect(RECT *rc) {
  if(draw_fs_blit_mode==DFSM_STRETCHBLIT)
  {
    rc->left=rc->top=0;
    rc->right=Disp.fs_res[Disp.fs_res_choice].x;
    rc->bottom=Disp.fs_res[Disp.fs_res_choice].y;
  }
  else if(draw_fs_blit_mode==DFSM_FAKEFULLSCREEN) 
  {
#if defined(SSE_VID_DD) && defined(SSE_VID_2SCREENS)
    if(OPTION_FAKE_FULLSCREEN)
      *rc=Disp.rcMonitor;
    else
#endif
    {
      rc->left=rc->top=0;
      rc->right=monitor_width;
      rc->bottom=monitor_height;
    }
  }
#ifndef NO_CRAZY_MONITOR
  else if(extended_monitor) 
  {
    rc->left=rc->top=0;
    rc->right=MIN(em_width,Disp.SurfaceWidth);
    rc->bottom=MIN(em_height,Disp.SurfaceHeight);
    if(FullScreen && runstate!=RUNSTATE_RUNNING)
    {
      int x_gap=(Disp.SurfaceWidth-em_width)/2;
      int y_gap=(Disp.SurfaceHeight-em_height)/2;
      rc->left+=x_gap;
      rc->right+=x_gap;
      rc->top+=y_gap;
      rc->bottom+=y_gap;
    }
  }
#endif
  else if(border)
  {
    rc->left=(800-(SideBorderSizeWin+320+SideBorderSizeWin)*2)/2;
    rc->top=(600-(TopBorderSize+200+BottomBorderSize)*2)/2;
    rc->right=rc->left+(SideBorderSizeWin+320+SideBorderSizeWin)*2;
    rc->bottom=rc->top+(TopBorderSize+200+BottomBorderSize)*2;
  }
  else
  {
    rc->left=0;
    rc->top=int(using_res_640_400?0:40);
    rc->right=640;
    rc->bottom=int(using_res_640_400?400:440);
  }
}


unsigned short UNMAKE_DDHRESULT(long code) {
  long FACDD=0x876;
  return (unsigned short)((FACDD>>16)^code);
}


//If the function succeeds, the return value is the number of bytes
//copied into the buffer, not including the null-terminating character,
//or zero if the error does not exist.
int DDGetErrorDescription(HRESULT Error,char *buf,int size) {
  //hInstance is a Steem global, this didn't belong to 'include'
  return LoadString(hInstance,UNMAKE_DDHRESULT(Error),buf,size);
}

#if defined(SSE_ENABLE_TRACE_LOG)

char *GetTextFromDDError(HRESULT hr) {  
  static char text[100];
  DDGetErrorDescription(hr,text,99);
  return text;
}

#define REPORT_DD_ERR(function,dderr) if(dderr) TRACE_LOG("DD ERR "function" %s\n",GetTextFromDDError(dderr))

#else

#define REPORT_DD_ERR(x,y)

#endif

#endif//dd

#define LOGSECTION LOGSECTION_VIDEO_RENDERING

#if defined(SSE_VID_D3D) && defined(SSE_ENABLE_TRACE_LOG)
char *GetTextFromD3DError(HRESULT hr){  //stolen somewhere
  char *text="Undefined error";     
  switch (hr)  {    
  case D3D_OK:      text="D3D_OK";break;    
  case D3DOK_NOAUTOGEN:      text="D3DOK_NOAUTOGEN";break;    
  case D3DERR_CONFLICTINGRENDERSTATE:      text="D3DERR_CONFLICTINGRENDERSTATE";break;    
  case D3DERR_CONFLICTINGTEXTUREFILTER:      text="D3DERR_CONFLICTINGTEXTUREFILTER";break;    
  case D3DERR_CONFLICTINGTEXTUREPALETTE:      text="D3DERR_CONFLICTINGTEXTUREPALETTE";break;    
  case D3DERR_DEVICELOST:      text="D3DERR_DEVICELOST";break;    
  case D3DERR_DEVICENOTRESET:      text="D3DERR_DEVICENOTRESET";break;    
  case D3DERR_DRIVERINTERNALERROR:      text="D3DERR_DRIVERINTERNALERROR";break;    
  case D3DERR_INVALIDCALL:      text="D3DERR_INVALIDCALL";break;    
  case D3DERR_INVALIDDEVICE:      text="D3DERR_INVALIDDEVICE";break;    
  case D3DERR_MOREDATA:      text="D3DERR_MOREDATA";break;    
  case D3DERR_NOTAVAILABLE:      text="D3DERR_NOTAVAILABLE";break;    
  case D3DERR_NOTFOUND:      text="D3DERR_NOTFOUND";break;    
  case D3DERR_OUTOFVIDEOMEMORY:      text="D3DERR_OUTOFVIDEOMEMORY";break;    
  case D3DERR_TOOMANYOPERATIONS:      text="D3DERR_TOOMANYOPERATIONS";break;    
  case D3DERR_UNSUPPORTEDALPHAARG:      text="D3DERR_UNSUPPORTEDALPHAARG";break;    
  case D3DERR_UNSUPPORTEDALPHAOPERATION:      text="D3DERR_UNSUPPORTEDALPHAOPERATION";break;    
  case D3DERR_UNSUPPORTEDCOLORARG:      text="D3DERR_UNSUPPORTEDCOLORARG";break;    
  case D3DERR_UNSUPPORTEDCOLOROPERATION:      text="D3DERR_UNSUPPORTEDCOLOROPERATION";break;    
  case D3DERR_UNSUPPORTEDFACTORVALUE:      text="D3DERR_UNSUPPORTEDFACTORVALUE";break;    
  case D3DERR_UNSUPPORTEDTEXTUREFILTER:      text="D3DERR_UNSUPPORTEDTEXTUREFILTER";break;    
  case D3DERR_WRONGTEXTUREFORMAT:      text="D3DERR_WRONGTEXTUREFORMAT";break;    
  case E_FAIL:      text="E_FAIL";break;    
  case E_INVALIDARG:      text="E_INVALIDARG";break;    
  case E_OUTOFMEMORY:      text="E_OUTOFMEMORY";break;  
  }   
  return text;
}
#define REPORT_D3D_ERR(function,d3derr) if(d3derr) TRACE_LOG(function" %s\n",GetTextFromD3DError(d3derr))

#else
#define REPORT_D3D_ERR(x,y)
#endif//d3d


TSteemDisplay::TSteemDisplay() {
#ifdef WIN32
#if defined(SSE_VID_DD)
  DDObj=NULL;
  DDPrimarySur=NULL;
  DDBackSur=NULL;
#if defined(SSE_VID_DD_3BUFFER_WIN)
  OurBackSur=DDBackSur2=NULL;
#endif
  DDClipper=NULL;
  DDBackSurIsAttached=DDExclusive=false;
#endif
  GDIBmp=NULL;
  GDIBmpMem=NULL;
#if !defined(SSE_NO_FREEIMAGE)
  hFreeImage=NULL;
  ScreenShotFormatOpts=0;
#endif
  DrawToVidMem=false;
  BlitHideMouse=DrawLetterboxWithGDI=false;
#if defined(SSE_VID_D3D)
  pD3D=NULL;	// Used to create the D3DDevice
  pD3DDevice=NULL;	// Our rendering device
  pD3DTexture=NULL;
  pD3DSprite=NULL;
  m_Adapter=D3DADAPTER_DEFAULT;
  ScreenShotExt="png";
  ScreenShotFormat=FIF_PNG;
#else
  ScreenShotExt="bmp";
  ScreenShotFormat=0;
#endif
#endif//WIN32
#ifdef UNIX
  DrawToVidMem=false;
  DoAsyncBlit=0;
  X_Img=NULL;
  AlreadyWarnedOfBadMode=0;
  GoToFullscreenOnRun=0;
#ifndef NO_SHM
  XSHM_Attached=0;
  XSHM_Info.shmaddr=(char*)-1;
  XSHM_Info.shmid=-1;
  SHMCompletion=LASTEvent;
  asynchronous_blit_in_progress=false;
#endif
#ifndef NO_XVIDMODE
  XVM_Modes=NULL;
#endif
#endif
#if defined(SSE_420R4)
  ScreenShotMinSize=ScreenShotAlwaysAddNum=false;
  ScreenShotUseFullName=true;
#else
  ScreenShotMinSize=ScreenShotUseFullName=ScreenShotAlwaysAddNum=false;
#endif
#if defined(SSE_VID_NEOPIC)
  pNeoFile=NULL;
#endif
  RunOnChangeToWindow=false;
  Method=UseMethod[nUseMethod=0]=DISPMETHOD_NONE;
}


TSteemDisplay::~TSteemDisplay() {
  Release();
};


void TSteemDisplay::SetMethods(int Method1,...) {
  va_list vl;
  int arg=Method1;
  va_start(vl,Method1);
  for(int n=0;n<MAX_DISPMETHODS;n++) 
  {
    UseMethod[n]=arg;
    if(arg==0) 
      break;
    arg=va_arg(vl,int);
  }
  va_end(vl);
  nUseMethod=0;
}


void TSteemDisplay::Init() {
  Release();
  if(FullScreen==0)
  {
    monitor_width=GetScreenWidth();
    monitor_height=GetScreenHeight();
  }
#if !defined(SSE_VID_2SCREENS)
  rcMonitor.left=rcMonitor.top=0;
  rcMonitor.right=monitor_width;
  rcMonitor.bottom=monitor_height;
#endif
#if !defined(SSE_NO_FREEIMAGE)
  FreeImageLoad();
#endif
  for(;nUseMethod<MAX_DISPMETHODS;nUseMethod++)
  {
    switch(UseMethod[nUseMethod]) {
#if defined(SSE_VID_D3D)
    case DISPMETHOD_D3D:
      if(D3DInit()==D3D_OK)
      {
        D3D9_OK=TRUE;
        Method=UseMethod[nUseMethod++];
        return;
      }
      break;
#endif
#if defined(SSE_VID_DD)
    case DISPMETHOD_DD:
      if(InitDD()==DD_OK)
      {
        Method=UseMethod[nUseMethod++];
        return;
      }
#endif
#ifdef WIN32
    case DISPMETHOD_GDI:
      TRACE("GDI\n");
      if(InitGDI())
      {
        Method=UseMethod[nUseMethod++];
        return;
      }
      break;
#endif
#ifdef UNIX
    case DISPMETHOD_X:
      if(InitX())
      {
        Method=UseMethod[nUseMethod++];
        return;
      }
      break;
    case DISPMETHOD_XSHM:
      if(InitXSHM())
      {
        Method=UseMethod[nUseMethod++];
        return;
      }
      break;
#endif
    case DISPMETHOD_NONE:
      return;
    }//sw
  }//nxt
}


// for each frame, sequence is lock-write frame (emulate)-unlock-blit


HRESULT TSteemDisplay::Lock() { // called by draw_begin(), and in DD builds FullscreenRunStart() and DDCreateSurfaces()
  HRESULT derr=DDERR_GENERIC;

#if defined(SSE_EMU_THREAD)
  if(OPTION_EMUTHREAD && runstate!=RUNSTATE_STOPPED && (SuspendRendering||VideoLock.blocked))
    return derr;
#endif

#if defined(SSE_STATS)
#if defined(SSE_STATS_QP)
  LARGE_INTEGER tLock0;
  QueryPerformanceCounter(&tLock0);
#else
  DWORD tLock0=timeGetTime();
#endif
#endif

  switch(Method) {
#if defined(SSE_VID_DD)
  case DISPMETHOD_DD:
  {
    if(!OurBackSur)
      return DDERR_SURFACELOST;
    derr=OurBackSur->IsLost();
    REPORT_DD_ERR("IsLost",derr);
    // Restore surfaces after event such as screen saver
    if(derr==DDERR_SURFACELOST) 
    {
      BYTE former_msg=StatusInfo.MessageIndex;
      derr=RestoreSurfaces();
      if(derr!=DD_OK) // wait next frame
      {
         REPORT_DD_ERR("RestoreSurfaces",derr);
         StatusInfo.MessageIndex=TStatusInfo::BLIT_ERROR;
         //runstate=RUNSTATE_STOPPED; 
      }
      else if(StatusInfo.MessageIndex==TStatusInfo::BLIT_ERROR)
        StatusInfo.MessageIndex=TStatusInfo::MESSAGE_NONE;
#ifdef SSE_GUI_STATUS_BAR
      if(former_msg!=StatusInfo.MessageIndex)
        UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
#endif
    }
    DDBackSurDesc.dwSize=sizeof(DDBackSurDesc);
#if defined(SSE_VID_DD_3BUFFER_WIN)
    if(OPTION_3BUFFER_WIN && DDBackSur2)
    {
      SurfaceToggle=!SurfaceToggle; // toggle at lock
      OurBackSur=(SurfaceToggle)?DDBackSur2:DDBackSur;
    }
    else
      OurBackSur=DDBackSur;
    if((derr=OurBackSur->Lock(NULL,&DDBackSurDesc,DDLOCK_WAIT|DDLockFlags,NULL))!=DD_OK) 
    {
      REPORT_DD_ERR("Lock",derr);
//      if(DErr!=DDERR_SURFACELOST && DErr!=DDERR_SURFACEBUSY) 
      if(StatusInfo.MessageIndex!=TStatusInfo::BLIT_ERROR)
      {
        StatusInfo.MessageIndex=TStatusInfo::BLIT_ERROR;
        UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
      }
      return derr;
    }
#else
    if((derr=DDBackSur->Lock(NULL,&DDBackSurDesc,DDLOCK_WAIT|DDLockFlags,NULL))!=DD_OK) {
      if(derr!=DDERR_SURFACELOST && derr!=DDERR_SURFACEBUSY) {
        DDError(T("DirectDraw Lock Error"),derr);
        Init();
      }
      return derr;
    }
#endif
    draw_line_length=DDBackSurDesc.lPitch;
    draw_mem=(draw_type*)DDBackSurDesc.lpSurface;
    break;
  }//case
#endif
#if defined(SSE_VID_D3D)
  case DISPMETHOD_D3D:
    derr=D3DLock();
    break;
#endif
#ifdef WIN32
  case DISPMETHOD_GDI:
    draw_mem=(draw_type*)GDIBmpMem;
    draw_line_length=GDIBmpLineLength;
    derr=DD_OK;
    break;
#endif
#ifdef UNIX
  case DISPMETHOD_X:
  case DISPMETHOD_XSHM:
    if(XD==NULL)
      break;
    WaitForAsyncBlitToFinish();
    draw_mem=(draw_type*)(X_Img->data);
    draw_line_length=X_Img->bytes_per_line;
    derr=DD_OK;
    break;
#endif
  }

#if defined(SSE_STATS)
#if defined(SSE_STATS_QP)
  LARGE_INTEGER tLock1;
  QueryPerformanceCounter(&tLock1);
  Stats.tLock=tLock1.QuadPart-tLock0.QuadPart;
#else
  DWORD tLock1=timeGetTime();
  Stats.tLock=tLock1-tLock0;
#endif
#endif

  // compute locked video memory as pitch * #lines
  DWORD n_lines=
#if defined(SSE_VID_D3D)
    (Disp.Method==DISPMETHOD_D3D)?TextureHeight:
#endif
    SurfaceHeight;
  VideoMemorySize=draw_line_length*n_lines;
  VideoMemoryEnd=(draw_type*)((BYTE*)draw_mem+VideoMemorySize);
#if defined(SSE_VID_TRACE_SIZE)
  TRACE("Lock frame %d %dx%d VRAM %p-%p (%dK) pitch %d\n",FRAME,draw_line_length,
    n_lines,draw_mem,VideoMemoryEnd,VideoMemorySize>>10,draw_line_length);
#endif
#if defined(SSE_DRAW_C)
  draw_line_length>>=2; // in DWORDS
#else
#if defined(SSE_VID_STVL1)
  draw_line_pitch_dw=draw_line_length/4;
#endif
#endif
  Draw.Pitch=draw_line_length;
  draw_dest_increase_y=Draw.EffectivePitch=Draw.VerPix*Draw.Pitch;
  return derr;
}


void TSteemDisplay::Unlock() {
#if defined(SSE_VID_TRACE_SIZE)
  TRACE("Unlock frame %d\n",FRAME);
#endif
#if defined(SSE_STATS)
#if defined(SSE_STATS_QP)
  LARGE_INTEGER tUnlock0;
  QueryPerformanceCounter(&tUnlock0);
#else
  DWORD tUnlock0=timeGetTime();
#endif
#endif
  switch(Method) {
#if defined(SSE_VID_D3D)
  case DISPMETHOD_D3D:
    D3DUnlock();
    break;
#endif
#if defined(SSE_VID_DD)
  case DISPMETHOD_DD:
  {
#if defined(SSE_VID_DD_3BUFFER_WIN)
    OurBackSur=(OPTION_3BUFFER_WIN && DDBackSur2 && SurfaceToggle) 
      ? DDBackSur2 : DDBackSur;
    HRESULT DErr;
    DErr=OurBackSur->Unlock(NULL);
#else
    HRESULT DErr=DDBackSur->Unlock(NULL);
#endif
    REPORT_DD_ERR("Unlock",DErr);
    if(DErr==DDERR_SURFACELOST)
    {
      TRACE_LOG("Unlock Surface lost\n");
      DErr=RestoreSurfaces(); //v4
      if(DErr!=DD_OK) 
      {
        REPORT_DD_ERR("RestoreSurfaces",DErr);
        StatusInfo.MessageIndex=TStatusInfo::BLIT_ERROR;
        UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
      }
    }
#if defined(SSE_VID_RECORD_AVI) // it is extremely slow
    if(video_recording && runstate==RUNSTATE_RUNNING)
    {
      static HDC OffscrDC;
      static myDDSURFACEDESC ddsd2;
      static HBITMAP OffscrBmp;
      HDC SurfDC;
      OurBackSur->GetDC(&SurfDC);
      if(!pAviFile)
      {
        TRACE_LOG("Start AVI recording, frameskip %d\n",frameskip);
        pAviFile=new CAviFile(SSE_VID_RECORD_AVI_FILENAME,
          mmioFOURCC(video_recording_codec[0],video_recording_codec[1],
          video_recording_codec[2],video_recording_codec[3]),
          VideoFreqAtStartOfVbl/frameskip);
        ZeroMemory(&ddsd2, sizeof(ddsd2));
        ddsd2.dwSize = sizeof(ddsd2);
        OurBackSur->GetSurfaceDesc(&ddsd2);
        OffscrBmp = CreateCompatibleBitmap(SurfDC,ddsd2.dwWidth,ddsd2.
        dwHeight);
        OffscrDC = CreateCompatibleDC(SurfDC);
        SelectObject(OffscrDC, OffscrBmp);
      }
      
      BitBlt(OffscrDC, 0, 0,ddsd2.dwWidth,ddsd2.dwHeight, SurfDC, 0, 0, SRCCOPY);
      
      if(video_recording==2 || pAviFile->AppendNewFrame(OffscrBmp))
      {
        delete pAviFile;
        pAviFile=NULL;
        video_recording=0;
        DeleteDC(OffscrDC); // important, release Windows resources!
        //DeleteObject(OldBmp);
        DeleteObject(OffscrBmp);
        
      }
      OurBackSur->ReleaseDC(SurfDC);
    }
#endif
    break;
  }
#endif//SSE_VID_DD
#ifdef WIN32
  case DISPMETHOD_GDI:
    SetBitmapBits(GDIBmp,GDIBmpSize,GDIBmpMem);
    break;
#endif
#ifdef UNIX
  case DISPMETHOD_X:
  case DISPMETHOD_XSHM:
    break;
#endif
  }//sw
#if defined(SSE_STATS)
#if defined(SSE_STATS_QP)
  LARGE_INTEGER tUnlock1;
  QueryPerformanceCounter(&tUnlock1);
  Stats.tUnlock=tUnlock1.QuadPart-tUnlock0.QuadPart;
#else
  DWORD tUnlock1=timeGetTime();
  Stats.tUnlock=tUnlock1-tUnlock0;
#endif
#endif
}

#if defined(SSE_VID_BFI)
bool TSteemDisplay::Blit(BOOL erase/*=FALSE*/)
#else
bool TSteemDisplay::Blit()
#endif
{
  bool success=false;
#if defined(SSE_EMU_THREAD)
  if(SuspendRendering || VideoLock.blocked)
    return success;
#endif
#if defined(SSE_STATS)
#if defined(SSE_STATS_QP)
  LARGE_INTEGER tBlit0;
  QueryPerformanceCounter(&tBlit0);
#else
  DWORD tBlit0=timeGetTime();
#endif
#endif
  switch(Method) {
#if defined(SSE_VID_D3D)
  case DISPMETHOD_D3D:
#if defined(SSE_VID_BFI)
    success=D3DBlit(erase);
#else
    success=D3DBlit();
#endif
    break;
#endif
#if defined(SSE_VID_DD)
  case DISPMETHOD_DD:
  {
    HRESULT DErr=NULL;
    // if we're in BLIT ERROR condition, wait until Lock can recreate the
    // surfaces
    if(StatusInfo.MessageIndex==TStatusInfo::BLIT_ERROR)
      return false;
    if(FullScreen) 
    {
      if(runstate==RUNSTATE_RUNNING) 
      {
        switch(draw_fs_blit_mode) {
        case DFSM_FLIP:
          DErr=DDPrimarySur->Flip(NULL,0); //DDFLIP_WAIT);
#if defined(SSE_VID_TRACE_SIZE)
          TRACE("BLIT %d %d %d %d TO fullscreen ERR %d\n",draw_blit_source_rect.left,draw_blit_source_rect.top,draw_blit_source_rect.right,
            draw_blit_source_rect.bottom,DErr);
#endif
          break;
        case DFSM_STRAIGHTBLIT:
          DErr=DDPrimarySur->BltFast(Draw.BltDst.left,Draw.BltDst.top,DDBackSur,
            &draw_blit_source_rect,DDBLTFAST_WAIT);
#if defined(SSE_VID_TRACE_SIZE)
          TRACE("BLIT %d %d %d %d TO %d,%d ERR %d\n",draw_blit_source_rect.left,draw_blit_source_rect.top,draw_blit_source_rect.right,draw_blit_source_rect.bottom,
            draw_blit_source_rect.left,draw_blit_source_rect.top,DErr);
#endif
          break;
        case DFSM_STRETCHBLIT:
        case DFSM_FAKEFULLSCREEN:
        {
          RECT Dest;
          Dest=LetterBoxRectangle;
#if defined(SSE_VID_TRACE_SIZE)
          TRACE("BLIT %d %d %d %d TO %d %d %d %d\n",draw_blit_source_rect.left,draw_blit_source_rect.top,draw_blit_source_rect.right,draw_blit_source_rect.bottom,Dest.left,Dest.top,Dest.right,Dest.bottom);
#endif
#if defined(SSE_VID_DD_3BUFFER_WIN)
          if(OPTION_3BUFFER_WIN && DDBackSur2)
          {
            OurBackSur=(!SurfaceToggle)?DDBackSur2:DDBackSur;
            if(OurBackSur->GetBltStatus(DDGBS_CANBLT)==DD_OK)
              DErr=DDPrimarySur->Blt(&Dest,OurBackSur,&draw_blit_source_rect,
                DDBLT_WAIT,NULL);
          }
          else
#endif
            DErr=DDPrimarySur->Blt(&Dest,DDBackSur,&draw_blit_source_rect,DDBLT_WAIT,NULL);
          break;
        }//case
        }//sw
        if(DErr==DDERR_SURFACELOST) 
        {
          DErr=RestoreSurfaces();
          if(DErr!=DD_OK)
          { // can happen if idle for long
            REPORT_DD_ERR("RestoreSurfaces",DErr);
            StatusInfo.MessageIndex=TStatusInfo::BLIT_ERROR;
            runstate=RUNSTATE_STOPPED; // fullscreen, stop on BLIT ERROR
#ifdef SSE_GUI_STATUS_BAR
            if(FullScreen)
              UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
#endif
#ifdef SSE_STATS
            StatsStatic.nBlitError++;
#endif
          }
        }
        else if(DErr) 
        {
          REPORT_DD_ERR("Fullscreen blit error",DErr);
          StatusInfo.MessageIndex=TStatusInfo::BLIT_ERROR;
          runstate=RUNSTATE_STOPPING;
          //REFRESH_STATUS_BAR;
        }
      }
      else
      { //not running right now
        HCURSOR OldCur=(BlitHideMouse) ? SetCursor(NULL) : NULL;
        RECT Dest;
        get_fullscreen_rect(&Dest);
        for(int i=0;i<2;i++) 
        {
#if defined(SSE_VID_DD_3BUFFER_WIN)
          OurBackSur= (OPTION_3BUFFER_WIN && !SurfaceToggle && DDBackSur2)
            ? DDBackSur2: DDBackSur;
          DErr=DDPrimarySur->Blt(&Dest,OurBackSur,&draw_blit_source_rect,
            DDBLT_WAIT,NULL);
#else
          DErr=DDPrimarySur->Blt(&Dest,DDBackSur,&draw_blit_source_rect,DDBLT_WAIT,NULL);
#endif
          REPORT_DD_ERR("Blit",DErr);
          if(DErr==DDERR_SURFACELOST) 
          {
            if(i==0) 
              DErr=RestoreSurfaces();
            if(DErr!=DD_OK) 
            {
              REPORT_DD_ERR("RestoreSurfaces",DErr);
              StatusInfo.MessageIndex=TStatusInfo::BLIT_ERROR;
              UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
              break;
            }
          }
          else
            break;
        }
        if(BlitHideMouse) 
          SetCursor(OldCur);
      }
    }
    else // not Fullscreen:
    {  
      HCURSOR OldCur=(stem_mousemode==STEM_MOUSEMODE_DISABLED&&BlitHideMouse) 
        ? SetCursor(NULL) : NULL;
      RECT &dest=Draw.BltDst;
      for(int i=0;i<2;i++) 
      {
#if defined(SSE_VID_DD_3BUFFER_WIN)
        OurBackSur=(OPTION_3BUFFER_WIN && !SurfaceToggle && DDBackSur2) 
          ? DDBackSur2:DDBackSur;
        DErr=DDPrimarySur->Blt(&dest,OurBackSur,&draw_blit_source_rect,
          DDBLT_WAIT,NULL);
#else
        DErr=DDPrimarySur->Blt(&dest,DDBackSur,&draw_blit_source_rect,DDBLT_WAIT,NULL);
#endif
#if defined(SSE_VID_TRACE_SIZE)
        TRACE("BLIT %d %d %d %d TO %d %d %d %d (%dx%d) ERR %d\n",
          draw_blit_source_rect.left,draw_blit_source_rect.top,
          draw_blit_source_rect.right,draw_blit_source_rect.bottom,
          dest.left,dest.top,dest.right,dest.bottom,
          dest.right-dest.left,dest.bottom-dest.top,DErr);
#endif
        REPORT_DD_ERR("Blit",DErr);
        if(DErr==DDERR_SURFACELOST)
        {
          if(i==0) 
            DErr=RestoreSurfaces();
        }
        else
          break;
      }
      if(DErr!=DD_OK) 
      { // the surface couldn't be recreated or we get another error such as
        // "DirectDraw does not have enough memory to perform the operation"
        // when the screen saver triggers or for any other reason
        // we just enter BLIT ERROR condition but go on, Steem will restore
        // the surfaces as soon as possible
        REPORT_DD_ERR("Blit",DErr);
        StatusInfo.MessageIndex=TStatusInfo::BLIT_ERROR;
        UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
#ifdef SSE_STATS
        StatsStatic.nBlitError++;
#endif
      }
      if(stem_mousemode==STEM_MOUSEMODE_DISABLED && BlitHideMouse) 
        SetCursor(OldCur);
    }
    success=(DErr==DD_OK);
    break;
  }
#endif
#ifdef WIN32
  case DISPMETHOD_GDI:
  {
    RECT dest;
    GetClientRect(StemWin,&dest);
    HDC dc=GetDC(StemWin);
    SetStretchBltMode(dc,COLORONCOLOR);
#if defined(SSE_GUI_STATUS_BAR)
    int sbh=GuiSM.m_statusbar_height;
#else
    int sbh=0;
#endif
    success=(StretchBlt(dc,0,MENUHEIGHT,dest.right,dest.bottom-(MENUHEIGHT+sbh),
      GDIBmpDC,draw_blit_source_rect.left,draw_blit_source_rect.top,
      draw_blit_source_rect.right-draw_blit_source_rect.left,
      draw_blit_source_rect.bottom-draw_blit_source_rect.top,SRCCOPY)!=0);
    ReleaseDC(StemWin,dc);
    break;
  }
#endif
#ifdef UNIX
  case DISPMETHOD_X:
  case DISPMETHOD_XSHM:
  {
    if(XD==NULL)
      break;
    int sx,sy,sw,sh,dx,dy;
    Window ToWin;
    if(FullScreen)
    {
      ToWin=XVM_FullWin;
      sx=draw_blit_source_rect.left;
      sy=draw_blit_source_rect.top;
      sw=draw_blit_source_rect.right;
      sh=draw_blit_source_rect.bottom;
      dx=MAX((XVM_FullW-sw)/2,0);
      dy=MAX((XVM_FullH-sh)/2,0);
      if(sh>XVM_FullH) 
        sh=XVM_FullH;
    }
    else
    {
      ToWin=StemWin;
      XWindowAttributes wa;
#ifdef DOC
      typedef struct {
        int x, y; /* location of window */
        int width, height; /* width and height of window */
        int border_width; /* border width of window */
        int depth; /* depth of window */
        Visual *visual; /* the associated visual structure */
        Window root; /*root of screen containing window */
        int class; /* InputOutput, InputOnly*/
        int bit_gravity; /*one of the bit gravity values */
        int win_gravity; /*one of the window gravity values */
        int backing_store; /* NotUseful, WhenMapped, Always */
        unsigned long backing_planes; /* planes to be preserved if possible */
        unsigned long backing_pixel; /*value to be used when restoring planes */
        Bool save_under; /*boolean, should bits under be saved? */
        Colormap colormap; /* color map to be associated with window */
        Bool map_installed; /* boolean, is color map currently installed*/
        int map_state; /* IsUnmapped, IsUnviewable, IsViewable */
        long all_event_masks; /*set of events all people have interest in*/
        long your_event_mask; /*my event mask */
        long do_not_propagate_mask; /* set of events that should not propagate */
        Bool override_redirect; /* boolean value for override-redirect */
        Screen *screen; /* back pointer to correct screen */
      } XWindowAttributes;
#endif
      XGetWindowAttributes(XD,StemWin,&wa);
      int w=wa.width,h=wa.height-(MENUHEIGHT);
      if(w<=0 || h<=0) 
        return true;
      dx=(w-draw_blit_source_rect.right)/2;
      dy=(h-draw_blit_source_rect.bottom)/2;
      sx=draw_blit_source_rect.left;
      sy=draw_blit_source_rect.top;
      sw=draw_blit_source_rect.right;
      sh=draw_blit_source_rect.bottom;
      if(dx<0)
      {
        sx-=dx;
        sw=w;
        dx=0;
      }
      if(dy<0)
      {
        sy-=dy;
        sh=h;
        dy=0;
      }
      dy+=MENUHEIGHT;
    }
    bool DoneIt=false;
    //	printf("XPutImage(... ,%i,%i,%i,%i,%i,%i)\n",draw_blit_source_rect.left,draw_blit_source_rect.top,
    //              dx,dy,sw,sh);
#ifdef DOC
    XPutImage (display, d, gc, image, src_x, src_y, dest_x, dest_y, width, height)
    Display *display;
    Drawable d;
    GC gc;
    XImage *image;
    int src_x, src_y;
    int dest_x, dest_y;
    unsigned int width, height;
    display Specifies the connection to the X server.
    d Specifies the drawable.
    gc Specifies the GC.
    image Specifies the image you want combined with the rectangle.
    src_x Specifies the offset in X from the left edge of the image defined by the XImage
    structure.
    src_y Specifies the offset in Y from the top edge of the image defined by the XImage
    structure.
    dest_x
    dest_y Specify the x and y coordinates, which are relative to the origin of the drawable
    and are the coordinates of the subimage.
    width
    height Specify the width and height of the subimage, which define the dimensions of the
    rectangle.
#endif
    //TRACE_LOG("Blit %d,%d,%d,%d to %d,%d\n",sx,sy,sw,sh,dx,dy);
    int DErr=-1;
    switch(Method) {
    case DISPMETHOD_X:
      DErr=XPutImage(XD,ToWin,DispGC,X_Img,sx,sy,dx,dy,sw,sh);
      DoneIt=true;
      break;
    case DISPMETHOD_XSHM:
#ifndef NO_SHM
      Disp.WaitForAsyncBlitToFinish();
      DErr=XShmPutImage(XD,ToWin,DispGC,X_Img,sx,sy,dx,dy,sw,sh,True);
      asynchronous_blit_in_progress=true;
      if(Disp.DoAsyncBlit==0) 
        Disp.WaitForAsyncBlitToFinish();
      DoneIt=true;
#endif
      break;
    }
#if defined(SSE_VID_TRACE_SIZE)
    TRACE("BLIT %dx%d TO %d,%d (%dx%d) ERR %d\n",sx,sy,dx,dy,sw,sh,DErr);
#endif
    
#ifdef DRAW_ALL_ICONS_TO_SCREEN
    XSetForeground(XD,DispGC,BlackCol);
    for (int n=Ico16.NumIcons-1;n>=0;n--){
      Ico16.DrawIcon(n,ToWin,DispGC,n*16,30);
      XDrawString(XD,ToWin,DispGC,n*16,65,EasyStr(n),strlen(EasyStr(n)));
    }
    for (int n=Ico32.NumIcons-1;n>=0;n--){
      Ico32.DrawIcon(n,ToWin,DispGC,n*32,80);
      XDrawString(XD,ToWin,DispGC,n*32,135,EasyStr(n),strlen(EasyStr(n)));
    }
#endif
#ifdef DRAW_TIMER_TO_SCREEN
    EasyStr tt=timer;
    XDrawString(XD,ToWin,DispGC,10,40,tt,tt.Length());
#endif
#if defined(SSE_420R2B)
    if(runstate!=RUNSTATE_RUNNING && LITTLE_PC==SafeLPeek(4))
#else
    if(runstate!=RUNSTATE_RUNNING && LITTLE_PC==rom_addr)
#endif
    {
      // If all initialisation failed might be 0x0
      if(sw>=320 && sh>=200) osd_draw_reset_info(dx,dy,sw,sh);
    }
    success=DoneIt;
    break;
  }//case
#endif//ux
  }//sw
#if defined(SSE_STATS)
#if defined(SSE_STATS_QP)
  QueryPerformanceCounter(&Stats.tBlit1);
  Stats.tBlit=Stats.tBlit1.QuadPart-tBlit0.QuadPart;
#else
  DWORD tBlit1=timeGetTime();
  Stats.tBlit=tBlit1-tBlit0;
#endif
#endif
  return success;
}


#ifdef UNIX

void TSteemDisplay::WaitForAsyncBlitToFinish() {
#ifndef NO_SHM
  if(asynchronous_blit_in_progress==0) 
    return;
  XEvent ev;
  clock_t wait_till=clock()+(CLOCKS_PER_SEC/50);
  for (int wait=50000;wait>=0;wait--){
    if (XCheckTypedEvent(XD,SHMCompletion,&ev)) break;
    if (clock()>wait_till) break;
  }
  asynchronous_blit_in_progress=false;
#endif
}

#endif//UNIX


#if defined(SSE_VID_DD)
void TSteemDisplay::VSync() {
  if(!DDObj)
    return;
  // we can't do real vsync, so we target a line in the middle of the screen,
  // knowing we'll be off by much at times
  DWORD middle=monitor_height/2;
  DWORD line;
  HRESULT DErr;
  do {
    DErr=DDObj->GetScanLine(&line);
  } while(line<middle&&!DErr);
}
#endif//DD


void TSteemDisplay::FullscreenRunStart(bool Temp) {
#ifdef WIN32
#ifndef SSE_LEAN_AND_MEAN
  if(FullScreen==0)
    return;
#endif
  if(!Temp) 
  {
    bool ChangeSize=false;
    int w=640,h=400;
#ifndef NO_CRAZY_MONITOR
    if(extended_monitor&&((int)em_width<GetScreenWidth()||(int)em_height<GetScreenHeight())) 
    {
      ChangeSize=true;
      w=em_width;
      h=em_height;
    }
#endif
#if defined(SSE_VID_DD)
    int hz=0;
    if(extended_monitor==0&&draw_fs_blit_mode!=DFSM_FAKEFULLSCREEN)
    {
      if(prefer_res_640_400 && border==0 && DDDisplayModePossible[2]) 
      {
        ChangeSize=true;
        hz=prefer_pc_hz[0];
      }
    }
#endif
    if(ChangeSize) 
    {
#if defined(SSE_VID_D3D)
      if(SetDisplayMode()==DD_OK) 
      {}
      else
#endif
#if defined(SSE_VID_DD)
      int hz_ok=0;
      if(SetDisplayMode(w,h,BytesPerPixel*8,hz,&hz_ok)==DD_OK)
      {
        if(hz)
          tested_pc_hz[0]=MAKEWORD(hz,hz_ok);
        using_res_640_400=true; // also for extended monitor, was so in Steem 3.2 ?
      }
      else
#endif
        change_fullscreen_display_mode(0);
     }
  }
#if defined(SSE_VID_DD) && !defined(SSE_VID_DD_MISC)
  if(DDPrimarySur)
    DDPrimarySur->SetClipper(NULL);
#endif
  ShowAllDialogs(false);
  SetStemMouseMode(STEM_MOUSEMODE_WINDOW);
#ifdef WIN32
#if defined(SSE_VID_DD) && !defined(SSE_VID_DD_MISC)
/*  This could interfere with user rights somehow, and fullscreen
    wouldn't display anything if Steem isn't run as administrator.
    Very strange: build in VS2008 when SSE_VID_DD_MISC isn't defined,
    it doesn't work as simple user, rename the file, it does. Win 10 Pro.
*/
  if(DrawLetterboxWithGDI==0) 
    LockWindowUpdate(StemWin);
#endif
  while(ShowCursor(FALSE)>=0);
  SetCursor(NULL);
#endif
#if defined(SSE_VID_DD)
  // delete screen
  Lock(); // as in DDCreateSurfaces()...
  if(draw_mem)
    ZeroMemory(draw_mem,VideoMemorySize);
  Unlock();
  if(OPTION_FAKE_FULLSCREEN)
  {
#if defined(SSE_VID_DD_3BUFFER_WIN)
    OurBackSur=(OPTION_3BUFFER_WIN && !SurfaceToggle && DDBackSur2) 
      ? DDBackSur2:DDBackSur;
    DDPrimarySur->Blt(&rcMonitor,OurBackSur,&draw_blit_source_rect,
      DDBLT_WAIT,NULL);
#else
    DErr=DDPrimarySur->Blt(&rcMonitor,DDBackSur,&draw_blit_source_rect,DDBLT_WAIT,NULL);
#endif
  }
  //  Compute LetterBoxRectangle first
  if(draw_fs_blit_mode==DFSM_STRETCHBLIT||draw_fs_blit_mode==DFSM_FAKEFULLSCREEN)
  { // correct AR like in D3D build
    float stx=(float)STXPixels();
    float sty=(float)STYPixels();
    if(OPTION_ST_ASPECT_RATIO && screen_res<HIRES)
      sty*=ST_ASPECT_RATIO_DISTORTION; // "reserve" more pixels
    get_fullscreen_rect(&LetterBoxRectangle);
    int horiz_pixels=LetterBoxRectangle.right-LetterBoxRectangle.left;
    int vert_pixels=LetterBoxRectangle.bottom-LetterBoxRectangle.top;
    if(!OPTION_FULLSCREEN_AR || !stx || !sty)
      ; // we dont' correct the AR in this mode, take full screen
    else
    {
      float multx=horiz_pixels/stx;
      float multy=vert_pixels/sty;
      if(OPTION_FULLSCREEN_AR==2) //crisp
      { // remove rest
        multx=(float)(int)multx;
        multy=(float)(int)multy;
      }
      float mult= (multx<multy) ? multx : multy;
      int sw=(int)(mult*stx);
      int sh=(int)(mult*sty);
      int diffw=(horiz_pixels-sw);
      int diffh=(vert_pixels-sh);
      LetterBoxRectangle.left+=diffw/2;
      LetterBoxRectangle.right-=diffw/2;
      LetterBoxRectangle.top+=diffh/2;
      LetterBoxRectangle.bottom-=diffh/2;
      TRACE("Ratio (%d) %f Zone %dx%d ",OPTION_FULLSCREEN_AR,mult,
        LetterBoxRectangle.right-LetterBoxRectangle.left,
        LetterBoxRectangle.bottom-LetterBoxRectangle.top);
    }

  }
  else // 640x400 and 800x600
  {
    if(border)
    {
      LetterBoxRectangle.top=(600-400-2*(TopBorderSize+BottomBorderSize))/2;
      LetterBoxRectangle.bottom=600-LetterBoxRectangle.top;
      int SideGap=(800 - (SideBorderSizeWin+320+SideBorderSizeWin)*2) / 2;
      LetterBoxRectangle.left=SideGap;
      LetterBoxRectangle.right=800-SideGap;
    }
    else
    {
      LetterBoxRectangle.top=draw_fs_topgap;
      LetterBoxRectangle.bottom=440;
      LetterBoxRectangle.right=640;
    }
  }
  TRACE_LOG("Fullscreen letterbox %d %d %d %d\n",LetterBoxRectangle.left,
    LetterBoxRectangle.top,LetterBoxRectangle.right,LetterBoxRectangle.bottom);
  DrawFullScreenLetterbox();
#endif//SSE_VID_DD
#endif//WIN32
#ifdef UNIX
  if(FullScreenBut.checked) 
    ChangeToFullScreen();
#endif
}

#if !defined(SSE_LIBRETRONUKE)
void TSteemDisplay::FullscreenRunEnd() {
#ifdef WIN32
  while(ShowCursor(TRUE)<0);
  if(!OPTION_FULLSCREEN_GUI)
  {
    ChangeToWindowedMode(); // this should work on all systems
    return;
  }
  ShowAllDialogs(true);
  InvalidateRect(StemWin,NULL,TRUE);
#endif//WIN32
#ifdef UNIX
  if(FullScreen)
  {
    ChangeToWindowedMode();
    draw(true);
  }
#endif
}
#endif//#if !defined(SSE_LIBRETRONUKE)

void TSteemDisplay::ScreenChange() {
#if defined(SSE_EMU_THREAD)
  if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
    VideoLock.Lock();
#endif
  TRACE_LOG("ScreenChange()\n");
  draw_end();
  switch(Method) {
#if defined(SSE_VID_D3D)
  case DISPMETHOD_D3D:
    if(D3DCreateSurfaces()!=DD_OK) 
      Init();
    break;
#endif
#if defined(SSE_VID_DD)
  case DISPMETHOD_DD:
    if(DDCreateSurfaces()!=DD_OK) 
      Init();
    break;
#endif
#ifdef WIN32
  case DISPMETHOD_GDI:
    if(!InitGDI())
      Init(); // it's hopeless!
    else
      Method=DISPMETHOD_GDI;
    break;
#endif
#ifdef UNIX
  case DISPMETHOD_X:
    if(InitX())
      Method=DISPMETHOD_X;
    else
      Init();
    break;
  case DISPMETHOD_XSHM:
    if(InitXSHM())
      Method=DISPMETHOD_XSHM;
    else
      Init();
    break;
#endif
  }//sw
#if defined(SSE_EMU_THREAD)
  VideoLock.Unlock();
#endif
}


#if defined(SSE_VID_DD) || defined(UNIX)

void TSteemDisplay::DrawFullScreenLetterbox() {
  if(FullScreen==0||extended_monitor||using_res_640_400)
    return;
#ifdef SSE_VID_DD
  if(!OurBackSur||!DDPrimarySur)
    return;
#if defined(SSE_VID_2SCREENS)
  RECT rc;
  get_fullscreen_totalrect(&rc);
#endif
  if((draw_fs_topgap||border) && Method==DISPMETHOD_DD) 
  {
    DDBLTFX bfx;
    ZeroMemory(&bfx,sizeof(DDBLTFX));
    bfx.dwSize=sizeof(DDBLTFX);
    bfx.dwFillColor=RGB(0,0,0);
    HDC dc=NULL;
    if(DrawLetterboxWithGDI) 
      dc=GetDC(StemWin);
#if defined(SSE_VID_2SCREENS)
    RECT Dest={rc.left,rc.top,rc.right,LetterBoxRectangle.top};
#else
    RECT Dest={0,0,GetScreenWidth(),LetterBoxRectangle.top};
#endif
    OurBackSur->Blt(&Dest,NULL,NULL,DDBLT_COLORFILL | DDBLT_WAIT,&bfx);
    if(dc) 
      FillRect(dc,&Dest,(HBRUSH)GetStockObject(BLACK_BRUSH));
    else
      DDPrimarySur->Blt(&Dest,NULL,NULL,DDBLT_COLORFILL | DDBLT_WAIT,&bfx);
    Dest.top=LetterBoxRectangle.bottom;
#if defined(SSE_VID_2SCREENS)
    Dest.bottom=rc.bottom;
#else
    Dest.bottom=SurfaceHeight;
#endif
    OurBackSur->Blt(&Dest,NULL,NULL,DDBLT_COLORFILL | DDBLT_WAIT,&bfx);
    if(dc)
      FillRect(dc,&Dest,(HBRUSH)GetStockObject(BLACK_BRUSH));
    else
      DDPrimarySur->Blt(&Dest,NULL,NULL,DDBLT_COLORFILL | DDBLT_WAIT,&bfx);
    if(border||(draw_fs_blit_mode==DFSM_STRETCHBLIT)
      ||(draw_fs_blit_mode==DFSM_FAKEFULLSCREEN))
    {
      Dest.right=LetterBoxRectangle.left;
#if defined(SSE_VID_2SCREENS)
      Dest.top=rc.top;
#else
      Dest.bottom=SurfaceHeight;
#endif
      OurBackSur->Blt(&Dest,NULL,NULL,DDBLT_COLORFILL | DDBLT_WAIT,&bfx);
      if(dc)
        FillRect(dc,&Dest,(HBRUSH)GetStockObject(BLACK_BRUSH));
      else
        DDPrimarySur->Blt(&Dest,NULL,NULL,DDBLT_COLORFILL | DDBLT_WAIT,&bfx);
      Dest.left=LetterBoxRectangle.right;
#if defined(SSE_VID_2SCREENS)
      Dest.right=rc.right;
#else
      Dest.right=SurfaceWidth;
#endif
      OurBackSur->Blt(&Dest,NULL,NULL,DDBLT_COLORFILL | DDBLT_WAIT,&bfx);
      if(dc)
        FillRect(dc,&Dest,(HBRUSH)GetStockObject(BLACK_BRUSH));
      else
        DDPrimarySur->Blt(&Dest,NULL,NULL,DDBLT_COLORFILL | DDBLT_WAIT,&bfx);
    }
    if(dc) 
      ReleaseDC(StemWin,dc);
  }
#endif//dd
#ifdef UNIX
  XSetForeground(XD,DispGC,BlackCol);
  int w_gap=XVM_FullW-draw_blit_source_rect.right;
  int h_gap=XVM_FullH-draw_blit_source_rect.bottom;
  if(w_gap)
  {
    XFillRectangle(XD,XVM_FullWin,DispGC,0,0,w_gap/2,XVM_FullH);
    XFillRectangle(XD,XVM_FullWin,DispGC,w_gap/2+draw_blit_source_rect.right,0,
      w_gap/2+1,600);
  }
  if(h_gap)
  {
    XFillRectangle(XD,XVM_FullWin,DispGC,0,0,XVM_FullW,h_gap/2);
    XFillRectangle(XD,XVM_FullWin,DispGC,0,h_gap/2+draw_blit_source_rect.bottom,
      XVM_FullW,h_gap/2+1);
  }
#endif
}


HRESULT TSteemDisplay::RestoreSurfaces() {
  HRESULT DErr;
  switch(Method) {
#ifdef UNIX
  case DISPMETHOD_X:
  case DISPMETHOD_XSHM:
    ScreenChange();
    DErr=DD_OK;
    break;
#endif
#if defined(SSE_VID_DD)
  case DISPMETHOD_DD:
    DErr=DDERR_GENERIC;
    if(DDPrimarySur && DDBackSur) 
    {
      draw_end();
      DErr=DDPrimarySur->Restore();
      if(DErr==DD_OK) 
      {
        DErr=DDBackSur->Restore();
#if defined(SSE_VID_DD_3BUFFER_WIN)
        if(OPTION_3BUFFER_WIN && DErr==DD_OK && DDBackSur2) 
          DErr=DDBackSur2->Restore();
        SurfaceToggle=true;
        VSyncTiming=0;
#endif
        TRACE_LOG("Restore surfaces %d\n",DErr);
      }
    }
    break;
#endif
  default:
    DErr=DD_OK;
  }//sw
  return DErr;
}

#endif//#if defined(SSE_VID_DD) || defined(UNIX)


bool TSteemDisplay::CanGoToFullScreen() {
  bool YesWeCan;
  switch(Method) {
#if defined(SSE_VID_DD) // only normal borders for real DD fullscreen
  case DISPMETHOD_DD:
    YesWeCan=(!(border>1 && draw_fs_blit_mode!=DFSM_STRETCHBLIT 
      && draw_fs_blit_mode!=DFSM_FAKEFULLSCREEN));
    break;
#endif
#if defined(SSE_VID_D3D)
  case DISPMETHOD_D3D:
    YesWeCan=true; // ?
    break;
#endif
#if defined(UNIX) && !defined(NO_XVIDMODE)
  case DISPMETHOD_X:
  case DISPMETHOD_XSHM:
  {
    int evbase,errbase;
    YesWeCan=XF86VidModeQueryExtension(XD,&evbase,&errbase);
    break;
  }
#endif
  default:
    YesWeCan=false;
  }//sw
  TRACE_LOG("Can go fullscreen 1:%d, Method #%d\n",YesWeCan,Method);
  return YesWeCan;
}


/* Disp.ChangeToFullScreen()
    |
    L-> change_fullscreen_display_mode()
          |
          L-> Disp.SetDisplayMode()
*/

#if !defined(SSE_LIBRETRONUKE)

void TSteemDisplay::ChangeToFullScreen() {
  if(!CanGoToFullScreen() || FullScreen 
#if defined(SSE_VID_DD)
    ||DDExclusive
#endif
    )
    return;
  TRACE_LOG("Going fullscreen...\n");
  draw_end();
#ifdef WIN32
#if defined(SSE_GUI_STATUS_BAR)
  ShowWindow(hStatusBar,SW_HIDE);
#endif
  if(runstate==RUNSTATE_RUNNING) 
  {
    Glue.m_Status.stop_emu=1;
    PostMessage(StemWin,WM_SYSCOMMAND,SC_MAXIMIZE,2);
  }
  else if(runstate!=RUNSTATE_STOPPED)
  { //Keep trying until succeed!
    PostMessage(StemWin,WM_SYSCOMMAND,SC_MAXIMIZE,2);
  }
  else if(bAppMinimized)
  {
    ShowWindow(StemWin,SW_RESTORE);
    PostMessage(StemWin,WM_SYSCOMMAND,SC_MAXIMIZE,2);
  }
  else
  {
    bool MaximizeDiskMan=false;
    if(OPTION_FULLSCREEN_GUI && DiskMan.IsVisible())
    {
      if(IsIconic(DiskMan.Handle)) 
        ShowWindow(DiskMan.Handle,SW_RESTORE);
      MaximizeDiskMan=DiskMan.FSMaximized;
      SetWindowLong(DiskMan.Handle,GWL_STYLE,
        (GetWindowLong(DiskMan.Handle,GWL_STYLE)&~WS_MAXIMIZE)&~WS_MINIMIZEBOX);
    }
    FullScreen=true;
    DirectoryTree::PopupParent=StemWin;
    GetWindowRect(StemWin,&rcPreFS); // "before fullscreen"
    if(OPTION_FULLSCREEN_GUI)
    {
      ShowWindow(GetDlgItem(StemWin,IDC_BACKTOWIN),SW_SHOWNA); // icon "back to windowed"
      ShowWindow(GetDlgItem(StemWin,IDC_QUIT),SW_SHOWNA); //Quit Steem
      if(OptionBox.IsVisible())
      {
        OptionBox.DestroyCurrentPage();
        OptionBox.CreatePage(OptionBox.Page);
      }
    }
    SetWindowLong(StemWin,GWL_STYLE,WS_VISIBLE);
#if defined(SSE_VID_D3D) && !defined(SSE_VID_2SCREENS) // done in D3DCreateSurfaces()
    SetWindowPos(StemWin,HWND_TOPMOST,0,0,D3DFsW,D3DFsH,0);
#endif

#if defined(SSE_VID_DD)
#if defined(SSE_VID_2SCREENS)
    RECT rc;
    get_fullscreen_totalrect(&rc);
    // Compute size
    int cw=rc.right-rc.left;
    int ch=rc.bottom-rc.top;
    SetWindowPos(StemWin,0,rc.left,rc.top,cw,ch,0);
#else
    int w=640,h=480;
    if(border)
      w=800,h=600;
#ifndef NO_CRAZY_MONITOR
    if(extended_monitor) 
      w=em_width,h=em_height;
#endif
    if(draw_fs_blit_mode==DFSM_STRETCHBLIT) 
    {
      w=fs_res[fs_res_choice].x;
      h=fs_res[fs_res_choice].y;
    }
    else if(draw_fs_blit_mode==DFSM_FAKEFULLSCREEN) 
    {
      w=rcMonitor.right-rcMonitor.left;
      h=rcMonitor.bottom-rcMonitor.top;
    }
    SetWindowPos(StemWin,HWND_TOPMOST,0,0,w,h,0);
#endif
#endif//DD

    CheckResetDisplay(true);
#if defined(SSE_VID_DD) && !defined(SSE_VID_DD_MISC)
    ClipWin=CreateWindow("Steem Fullscreen Clip Window","",WS_CHILD | WS_VISIBLE |
                          WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                          0,MENUHEIGHT,w,h-MENUHEIGHT,StemWin,(HMENU)1111,hInstance,NULL);
    DDClipper->SetHWnd(0,ClipWin);
#endif
    bool ShowInfoBox=InfoBox.IsVisible();
    for(int n=0;n<nStemDialogs;n++) 
    {
      if(DialogList[n]!=&InfoBox) 
      {
#ifdef DEBUG_BUILD
        if(DialogList[n]!=&HistList)
#endif
          DialogList[n]->MakeParent(StemWin);
        if(OPTION_FULLSCREEN_GUI && DialogList[n]->IsVisible())
          InvalidateRect(DialogList[n]->Handle,NULL,FALSE);
      }
    }
    InfoBox.Hide();
    SetParent(ToolTip,StemWin);
#if defined(SSE_VID_DD)
    if(!OPTION_FAKE_FULLSCREEN)
    {
      HRESULT DErr=DDObj->SetCooperativeLevel(StemWin,DDSCL_EXCLUSIVE
        |DDSCL_FULLSCREEN|DDSCL_ALLOWREBOOT);
      if(DErr!=DD_OK) 
      {
        REPORT_DD_ERR("SetCooperativeLevel",DErr);
        DDError(T("Can't SetCooperativeLevel to exclusive"),DErr);
        Init();
        return;
      }
      DDExclusive=true;
    }
#endif
    if(change_fullscreen_display_mode(0)==DD_OK) 
    {
#if defined(SSE_VID_DD)
      if(OPTION_FAKE_FULLSCREEN)
      {        
        if(DDCreateSurfaces()!=DD_OK)
        {
          ChangeToWindowedMode(true);
          return;
        }
      }
#endif
      if(OPTION_FULLSCREEN_GUI) // don't show dialogs if we run at once...
      {
        if(ShowInfoBox) 
          InfoBox.Show();
        if(MaximizeDiskMan) 
        {
          SendMessage(DiskMan.Handle,WM_SETREDRAW,FALSE,0);
          ShowWindow(DiskMan.Handle,SW_MAXIMIZE);
          PostMessage(DiskMan.Handle,WM_SETREDRAW,TRUE,0);
        }
        OptionBox.EnableBorderOptions(true);
      }
      SetForegroundWindow(StemWin);
      SetFocus(StemWin);
      palette_convert_all();
      ONEGAME_ONLY( DestroyNotifyInitWin(); )
      if(OPTION_FULLSCREEN_GUI) 
      {
        InvalidateRect(StemWin,NULL,FALSE);
        if(OptionBox.IsVisible())
          InvalidateRect(OptionBox.Handle,NULL,FALSE);
        if(DiskMan.IsVisible())
          InvalidateRect(DiskMan.Handle,NULL,FALSE);
        REFRESH_STATUS_BAR_GX;
      }
      else
      {
        CLICK_PLAY_BUTTON();
      }
    }
    else
    { //back to windowed mode
      TRACE_LOG("Can't go fullscreen 2\n");
      ChangeToWindowedMode(true);
    }
  }
#endif//WIN32

#if defined(UNIX) && !defined(NO_XVIDMODE)
  int Screen=XDefaultScreen(XD);
  //  int XVM_nModes,XVM_ViewX,XVM_ViewY; SS declared
  //  XF86VidModeModeInfo **XVM_Modes;   in display.h
  // memory for XVM_Modes requested by X, it's our task to free it
  if(XF86VidModeGetAllModeLines(XD,Screen,&XVM_nModes,&XVM_Modes)==0)
    return;
  TRACE_LOG("XVM_nModes=%d ",XVM_nModes);
  int mult=2;
#if defined(SSE_VID_SIZE4)
  if(!extended_monitor && DISPLAY_SIZE>=3 && !(prefer_res_640_400 && border==0))
    mult=DISPLAY_SIZE;
#endif  
  int w=320*mult,h=240*mult;
  if(border)
    w=400*mult,h=300*mult;
  else if(prefer_res_640_400)
    w=640,h=400;
#ifndef NO_CRAZY_MONITOR
  if(extended_monitor)
    w=em_width,h=em_height;
#endif
  TRACE_LOG("Fullscreen try w=%d,h=%d\n",w,h);
  XF86VidModeModeInfo *Mode=NULL;
  int diff=0xffff;
  for(int a=0;a<2;a++)
  {
    if(prefer_res_640_400 && border==0) 
      h=400;
    for(int n=0;n<2;n++)
    {
      for(int i=0;i<XVM_nModes;i++)
      {
        if(a==0)
        {
          // get exact
          if(XVM_Modes[i]->hdisplay==w && XVM_Modes[i]->vdisplay==h)
          {
            TRACE_LOG("Exact match mode %d w %d h%d\n",i,w,h);
            Mode=XVM_Modes[i];
            break;
          }
        }
        else
        {
          // get closest
          if(XVM_Modes[i]->hdisplay>=w && XVM_Modes[i]->vdisplay>=h)
          {
            int new_diff=(XVM_Modes[i]->hdisplay-w)+(XVM_Modes[i]->vdisplay-h);
            if(new_diff<diff)
            {
              TRACE_LOG("Close match mode %d w %d h%d\n",i,XVM_Modes[i]->hdisplay,XVM_Modes[i]->vdisplay);
              Mode=XVM_Modes[i];
              diff=new_diff;
            }
          }
        }//if
      }//nxt i
      if(Mode || h!=400)
        break;
      h=480*mult;
    }
    if(Mode)
      break;
  }//nxt a
  if(Mode==NULL)
  {
    Alert(T("Can't change to fullscreen. Your video card doesn't support the required screen mode")+
            " ("+w+"x"+h+")",T("ERROR"),MB_ICONERROR);
    XFree(XVM_Modes);
    return;
  }
  // SS: only the specified options are set, by using both mask & struct
  // XVM_FullWin is a Window declared in display.h
  w=Mode->hdisplay, h=Mode->vdisplay; // why were
  int x=(GetScreenWidth()-w)/2, y=(GetScreenHeight()-h)/2; // those 2 lines missing?
  XSetWindowAttributes swa; 
  swa.backing_store=NotUseful;
  swa.override_redirect=True;
#if defined(SSE_UNIX__)  // was wrong?
  XVM_FullWin=XCreateWindow(XD,XDefaultRootWindow(XD),0,0,w,h,Screen,
#else
  XVM_FullWin=XCreateWindow(XD,XDefaultRootWindow(XD),x,y,w,h,Screen,			    
#endif
			    CopyFromParent,InputOutput,CopyFromParent,
          CWBackingStore | CWOverrideRedirect,&swa);
  SetProp(XD,XVM_FullWin,cWinProc,(DWORD_PTR)XVM_WinProc);
  SetProp(XD,XVM_FullWin,cWinThis,(DWORD_PTR)this);
  SetProp(XD,XVM_FullWin,hxc::cModal,-1);
  XSelectInput(XD,XVM_FullWin,KeyPressMask | KeyReleaseMask |
                            ButtonPressMask | ButtonReleaseMask |
                            ExposureMask | FocusChangeMask);
  XMapWindow(XD,XVM_FullWin);
  XF86VidModeGetViewPort(XD,Screen,&XVM_ViewX,&XVM_ViewY);
#if defined(SSE_UNIX)
  TRACE_LOG("Viewport %d,%d\n",XVM_ViewX,XVM_ViewY);
  XWarpPointer(XD, None, XDefaultRootWindow(XD),0, 0, 0, 0, 0, 0);
  //XWarpPointer(XD, None, XVM_FullWin,0, 0, 0, 0, 0, 0);
  //XWarpPointer(XD,Screen,XVM_FullWin,0,0,0,0,x,y); //
  //XWarpPointer(XD,None,XVM_FullWin,0,0,0,0,window_mouse_centre_x,window_mouse_centre_y);
//  XWarpPointer(XD,None,XDefaultRootWindow(XD),0,0,0,0,window_mouse_centre_x,window_mouse_centre_y);
#endif
  FullScreen=XF86VidModeSwitchToMode(XD,Screen,Mode);
  if(FullScreen)
  {
    // SS location of the upper left corner of the viewport into the virtual screen
    TRACE_LOG("set viewport %d,%d\n",x,y);
    XF86VidModeSetViewPort(XD,Screen,x,y);
    //XF86VidModeSetViewPort(XD,Screen,0,0);
    draw_grille_black=MAX((int)draw_grille_black,50);
    XGrabPointer(XD,XVM_FullWin,False,ButtonPressMask | ButtonReleaseMask,
                  GrabModeAsync,GrabModeAsync,XVM_FullWin,EmptyCursor,CurrentTime);
    window_mouse_centre_x=w/2;
    window_mouse_centre_y=h/2;
    XWarpPointer(XD,None,XVM_FullWin,0,0,0,0,window_mouse_centre_x,
      window_mouse_centre_y);
    mouse_vbl_delta_x=0;
    mouse_vbl_delta_y=0;
    mouse_vbl_delta=false;
    XVM_FullW=w;
    XVM_FullH=h;
    if(XVM_FullH<480) 
      using_res_640_400=true;
    TRACE_LOG("in fullscreen %dx%d\n",w,h);
  }
  else
  {
    XDestroyWindow(XD,XVM_FullWin);
    Alert(T("Can't change to fullscreen. There was an error switching to the required screen mode")+
            " ("+w+"x"+h+")",T("ERROR"),MB_ICONERROR);
    XFree(XVM_Modes);
  }
#endif
}

#endif//#if !defined(SSE_LIBRETRONUKE)

HRESULT TSteemDisplay::SetDisplayMode(
#if defined(SSE_VID_DD)
                                      int w,int h,int bpp,int hz,int *hz_ok
#else
#endif
                                      ) {
  //TRACE_LOG("SetDisplayMode\n");
  HRESULT DErr=DDERR_GENERIC;
  switch(Method) {
#if defined(SSE_VID_D3D)
  case DISPMETHOD_D3D:
    DErr=D3DCreateSurfaces();
    break;
#endif
#if defined(SSE_VID_DD)
  case DISPMETHOD_DD:
    if(DDExclusive && DDObj) 
    {
      int idx=-1;
      if(w==640&&h==480) 
        idx=0;
      if(w==800&&h==600) 
        idx=1;
      if(w==640&&h==400) 
        idx=2;
      if(draw_fs_blit_mode==DFSM_STRETCHBLIT) 
        idx=3;
      if(idx>=0) 
      {
        for(int n=1;n<NUM_HZ;n++)
        {
          if(hz==HzIdxToHz[n]) 
          {
            hz=DDClosestHz[idx][n];
            break;
          }
        }
      }
      TRACE_LOG("SetDisplayMode %dx%d %dbit %dHz\n",w,h,bpp,hz);
      DErr=DDObj->SetDisplayMode(w,h,bpp,hz,0);
      if(DErr!=DD_OK)
      {
        if(hz_ok)
          *hz_ok=0;
        DErr=DDObj->SetDisplayMode(w,h,bpp,0,0);
      }
      else
      {
        if(hz_ok) 
          *hz_ok=(hz<<16)+1;
      }
      if(DErr!=DD_OK) 
      {
        //      DDError(T("Can't SetDisplayMode"),DErr);
        //      Init();
        REPORT_DD_ERR("SetDisplayMode",DErr);
      }
      if((DErr=DDCreateSurfaces())!=DD_OK) 
        Init();
    }//if
    break;
#endif
#ifdef UNIX
  case DISPMETHOD_X:
  case DISPMETHOD_XSHM:
    break;
#endif
  }//sw
  return DErr;
}


#if !defined(SSE_LIBRETRONUKE)

void TSteemDisplay::FlipToDialogsScreen() { // only called by Alert()
#if defined(SSE_VID_DD)
  if(Method==DISPMETHOD_DD && DDObj)
    DDObj->FlipToGDISurface();
#endif
}


void TSteemDisplay::ChangeToWindowedMode(bool Emergency) {
  if( 
#if defined(SSE_VID_DD)
    !DDExclusive &&
#endif
    FullScreen==0) 
    return;
#ifdef WIN32
#ifndef SSE_NO_SCREENSAVER
  if(FullScreen) 
    TScreenSaver::killTimer();
#endif
  bool CanChangeNow=true;
#if defined(SSE_EMU_THREAD) && !defined(SSE421R2)
  bool oldSuspendRendering=SuspendRendering;
  SuspendRendering=true;
#endif
  if(runstate==RUNSTATE_RUNNING) 
  {
    Glue.m_Status.stop_emu=1;
    PostMessage(StemWin,WM_COMMAND,MAKEWPARAM(106,BN_CLICKED),
      (LPARAM)GetDlgItem(StemWin,IDC_BACKTOWIN));
    CanChangeNow=false;
  }
  else if(runstate!=RUNSTATE_STOPPED) 
  { //Keep trying until succeed!
    PostMessage(StemWin,WM_COMMAND,MAKEWPARAM(IDC_BACKTOWIN,BN_CLICKED),
      (LPARAM)GetDlgItem(StemWin,IDC_BACKTOWIN));
    CanChangeNow=false;
  }
  if(CanChangeNow||Emergency) 
  {
    TRACE_LOG("Going windowed mode...\n");
#if defined(SSE_VID_DD)
    if(DDExclusive && DDObj)
#endif
    {
      if(draw_lock)
        Unlock();
#if defined(SSE_VID_D3D)
#if defined(SSE_EMU_THREAD)
      // immediately reset
      d3dpp.Windowed=TRUE;
      d3dpp.FullScreen_RefreshRateInHz=0;
      pD3DDevice->Reset(&d3dpp); // back from true fullscreen
#else
      D3DDestroySurfaces();
#endif
#endif//#if defined(SSE_VID_D3D)
#if defined(SSE_VID_DD)
      DDDestroySurfaces();
      DDObj->RestoreDisplayMode();
      DDObj->SetCooperativeLevel(StemWin,DDSCL_NORMAL);
      DDExclusive=false;
#endif
    }
    FullScreen=0;
#if defined(SSE_VID_D3D)
    if(D3DCreateSurfaces()!=DD_OK)
      Init();
#endif
#if defined(SSE_VID_DD)
    if(DDCreateSurfaces()!=DD_OK)
      Init();
#if !defined(SSE_VID_DD_MISC)
    else
      DDClipper->SetHWnd(0,StemWin);
#endif
#endif
    CheckResetDisplay(true); // Hide fullscreen reset display
#if defined(SSE_VID_DD) && !defined(SSE_VID_DD_MISC)
    ToolsDeleteAllChildren(ToolTip,ClipWin);
    DestroyWindow(ClipWin);
#else
    ToolsDeleteAllChildren(ToolTip,StemWin);
#endif
    DirectoryTree::PopupParent=NULL;
#if defined(SSE_VID_DD) && !defined(SSE_VID_DD_MISC)
    LockWindowUpdate(NULL);
#endif
    // Sometimes things won't work if you do them immediately after switching to
    // windowed mode, so post a message and resize all the windows back when we can
    PostMessage(StemWin,WM_USER,12,0);
    ChangeToWinTimeOut=timeGetTime()+2000;
    //InvalidateRect(StemWin,NULL,true);
    if(OptionBox.IsVisible())
    {
      OptionBox.DestroyCurrentPage();
      OptionBox.CreatePage(OptionBox.Page);
    }
#if defined(SSE_GUI_STATUS_BAR)
    if(OPTION_STATUS_BAR)
    {
      REFRESH_STATUS_BAR;
      ShowWindow(hStatusBar,SW_SHOW);
    }
#endif
  }
#if defined(SSE_EMU_THREAD) && !defined(SSE421R2)
  SuspendRendering=oldSuspendRendering;
#endif
#endif//WIN32

#ifdef UNIX
#if !defined(NO_XVIDMODE)
  int Screen=XDefaultScreen(XD);
  XF86VidModeSwitchToMode(XD,Screen,XVM_Modes[0]);
  XF86VidModeSetViewPort(XD,Screen,XVM_ViewX,XVM_ViewY);
  XFree(XVM_Modes);
  XDestroyWindow(XD,XVM_FullWin);
  FullScreen=0;
  using_res_640_400=0;
#endif//!NO_XVIDMODE
#endif
}

#endif

void TSteemDisplay::Release() {
  draw_end();
#ifdef WIN32
  if(GDIBmp!=NULL) 
  {
    DeleteDC(GDIBmpDC);   GDIBmpDC=NULL;
    DeleteObject(GDIBmp); GDIBmp=NULL;
    delete[] GDIBmpMem;
  }
#endif
#if defined(SSE_VID_DD)
  if(DDObj!=NULL) 
  {
    if(DDExclusive||FullScreen) 
    {
      ChangeToWindowedMode(true);
    }
    DDDestroySurfaces();
    if(DDClipper!=NULL) 
    {
      DDClipper->Release();
      DDClipper=NULL;
    }
    DDObj->Release();
    DDObj=NULL;
  }
#endif
#if defined(SSE_VID_D3D)
  D3DRelease();
#if defined(SSE_VID_D3D_SWEETFX)
  if(hD3Dhack)
    SteemFreeLibrary(hD3Dhack);
#endif
#endif
#ifdef UNIX
#ifndef NO_SHM
  if(XSHM_Attached && XD)
  {
    XSync(XD,False);
    if(XD)
    {
      XShmDetach(XD,&XSHM_Info);
      XSHM_Attached=0;
    }
    if(XD)
      XSync(XD,False);
  }
#endif
  if(X_Img)
  {
    XDestroyImage(X_Img);
    X_Img=NULL;
  }
#ifndef NO_SHM
  if(XSHM_Info.shmaddr!=(char*)-1)
  {
    shmdt(XSHM_Info.shmaddr);
    XSHM_Info.shmaddr=(char*)-1;
  }
  if(XSHM_Info.shmid!=-1)
  {
    shmctl(XSHM_Info.shmid,IPC_RMID,0);
    XSHM_Info.shmid=-1;
  }
#endif
#endif//ux
  palette_remove();
  Method=DISPMETHOD_NONE;
#if defined(SSE_EMU_THREAD)
  SuspendRendering=false;
#endif
}


#ifdef SHOW_WAVEFORM

void TSteemDisplay::DrawWaveform()
{
#ifdef WIN32
  HDC dc;
  if (Method==DISPMETHOD_DD){
    if (DDBackSur->GetDC(&dc)!=DD_OK) return;
  }else if (Method==DISPMETHOD_GDI){
    dc=GDIBmpDC;
  }else{
    return;
  }
  int base=shifter_y-10;
  SelectObject(dc,GetStockObject((STpal[0]<0x777) ? WHITE_PEN:BLACK_PEN));
  MoveToEx(dc,0,base-129,0);
  LineTo(dc,shifter_x,base-129);
  MoveToEx(dc,0,base+1,0);
  LineTo(dc,shifter_x,base+1);
  MoveToEx(dc,0,base - temp_waveform_display[0]/2,0);
  for (int x=0;x<draw_blit_source_rect.right;x++){
    LineTo(dc,x,base - temp_waveform_display[x*SHOW_WAVEFORM]/2);
  }
  MoveToEx(dc,temp_waveform_play_counter/SHOW_WAVEFORM,0,0);
  LineTo(dc,temp_waveform_play_counter/SHOW_WAVEFORM,shifter_y);
  if (Method==DISPMETHOD_DD) DDBackSur->ReleaseDC(dc);
#endif//WIN32
}
#endif


// TODO
int TSteemDisplay::STXPixels() {
  int st_x_pixels=HOR_PIXELS_LO+(border!=0)*SideBorderSizeWin*2; //displayed
  if(screen_res>LORES || video_mixed_output
#if defined(SSE_VID_STVL1)
    || OPTION_C3 && COLOUR_MONITOR 
#endif
    )
    st_x_pixels*=2;
  return st_x_pixels;
}


int TSteemDisplay::STYPixels() {
  int st_y_pixels=VER_PIXELS_LO+(border!=0)*(TopBorderSize+BottomBorderSize);
  if(screen_res>LORES || video_mixed_output
#if defined(SSE_VID_STVL1)
    || OPTION_C3 && COLOUR_MONITOR //&& (draw_win_mode[screen_res]==DWM_NOSTRETCH)
#endif
    )
    st_y_pixels*=2;
  return st_y_pixels;
}


#ifdef WIN32

#if defined(SSE_VID_DD)

HRESULT TSteemDisplay::InitDD() {
  SetNotifyInitText("DirectDraw");
  HRESULT DErr;
  try{
    IDirectDraw *DDObj1=NULL;
    DErr=CoCreateInstance(CLSID_DirectDraw,NULL,CLSCTX_ALL,IID_IDirectDraw,(void**)&DDObj1);
    if(DErr!=S_OK||DDObj1==NULL) 
    {
      EasyStr Err="Unknown error";
      switch(DErr) {
      case REGDB_E_CLASSNOTREG:
        Err="The specified class is not registered in the registration database.";
        break;
      case E_OUTOFMEMORY:
        Err="Out of memory.";
        break;
      case E_INVALIDARG:
        Err="One or more arguments are invalid.";
        break;
      case E_UNEXPECTED:
        Err="An unexpected error occurred.";
        break;
      case CLASS_E_NOAGGREGATION:
        Err="This class cannot be created as part of an aggregate.";
        break;
      }
      Err=EasyStr("CoCreateInstance error\n\n")+Err;
      TRACE_LOG("%s\n",Err.Text); //bug (no .Text) found by MinGW
#ifndef ONEGAME
      MessageBox(NULL,Err,T("Steem Engine DirectDraw Error"),
                 MB_ICONEXCLAMATION|MB_SETFOREGROUND|MB_TASKMODAL|MB_TOPMOST);
#endif
      return ~DD_OK;
    }
    if((DErr=DDObj1->Initialize(NULL))!=DD_OK) 
    {
      DDObj1->Release();
      REPORT_DD_ERR("Initialize",DErr);
      return DDError("Initialise FAILED",DErr);
    }
#if defined(SSE_VID_DD7)
/*  Apparently using DirectDraw 7 instead of 2 doesn't change anything.
    This was cheap to do anyway.
*/
    if((DErr=DDObj1->QueryInterface(IID_IDirectDraw7,(LPVOID*)&DDObj))!=DD_OK)
#else
    if((DErr=DDObj1->QueryInterface(IID_IDirectDraw2,(LPVOID*)&DDObj))!=DD_OK)
#endif
    {
      REPORT_DD_ERR("QueryInterface",DErr);
      return DDError("QueryInterface FAILED",DErr);
    }
    if((DErr=DDObj->SetCooperativeLevel(StemWin,DDSCL_NORMAL))!=DD_OK)
    {
      REPORT_DD_ERR("SetCooperativeLevel",DErr);
      return DDError("SetCooperativeLevel FAILED",DErr);
    }
    if((DErr=DDObj->CreateClipper(0,&DDClipper,NULL))!=DD_OK)
    {
      REPORT_DD_ERR("CreateClipper",DErr);
      return DDError("CreateClipper FAILED",DErr);
    }
    if((DErr=DDClipper->SetHWnd(0,StemWin))!=DD_OK)
    {
      REPORT_DD_ERR("SetHWnd",DErr);
      return DDError("SetHWnd FAILED",DErr);
    }
    Method=DISPMETHOD_DD;
    Draw.MarshalParameters();
    if((DErr=DDCreateSurfaces())!=DD_OK) 
    {
      Method=0;
      return DErr;
    }
    DDLockFlags=DDLOCK_NOSYSLOCK;
    ddsd.dwSize=sizeof(ddsd);
    if(DDBackSur->Lock(NULL,&ddsd,DDLOCK_WAIT|DDLockFlags,NULL)!=DD_OK) 
    {
      DDLockFlags=0;
      if((DErr=DDBackSur->Lock(NULL,&ddsd,DDLOCK_WAIT|DDLockFlags,NULL))!=DD_OK) 
      {
        REPORT_DD_ERR("Lock",DErr);
        return DDError("Lock test FAILED",DErr);
      }
    }
    DDBackSur->Unlock(NULL);
    ZeroMemory(DDDisplayModePossible,sizeof(DDDisplayModePossible));
    ZeroMemory(DDClosestHz,sizeof(DDClosestHz));
    ZeroMemory(&fs_res,NFSRES*sizeof(POINT));
    DDObj->EnumDisplayModes(DDEDM_REFRESHRATES,NULL,this,DDEnumModesCallback);
    for(int idx=0;idx<NPC_HZ_CHOICES;idx++) 
    {
      for(int n=1;n<NUM_HZ;n++) 
        if(DDClosestHz[idx][n]==0) 
          DDClosestHz[idx][n]=HzIdxToHz[n];
    }
    TRACE_LOG("Formats 8bit %d 16bit %d 32bit %d\n",
      SSEConfig.VideoCard8bit,SSEConfig.VideoCard16bit,SSEConfig.VideoCard32bit);
#if defined(SSE_ENABLE_TRACE_LOG)
    DDCAPS caps_driver;
    DDObj->GetCaps(&caps_driver,NULL);
#if defined(SSE_VID_DD7)
    TRACE_LOG("DD7 Init OK, caps %X %X\n",caps_driver.dwCaps,caps_driver.dwCaps2);
#else
    TRACE_LOG("DD2 Init OK, caps %X %X %X\n",caps_driver.dwCaps,caps_driver.dwCaps2,caps_driver.ddsCaps.dwCaps);
#endif
#endif
    return DD_OK;
  }catch(...){
    TRACE_LOG("DirectDraw caused DISASTER!\n");
    return DDError("DirectDraw caused DISASTER!",DDERR_EXCEPTION);
  }
}



HRESULT WINAPI TSteemDisplay::DDEnumModesCallback(myLPDDSURFACEDESC pddsd,
                                                  LPVOID t) {  
/*  Finally understood why DirectDraw fullscreen wouldn't work on some
    systems. It's not the resolution, it's bpp. All video card drivers
    won't support 16bit display. 
*/
  if(pddsd->ddpfPixelFormat.dwRGBBitCount==8)
    SSEConfig.VideoCard8bit=true;
  else if(pddsd->ddpfPixelFormat.dwRGBBitCount==16)
    SSEConfig.VideoCard16bit=true;
  else if(pddsd->ddpfPixelFormat.dwRGBBitCount==32)
    SSEConfig.VideoCard32bit=true;
  // this is a static function, hence the need for This 
  TSteemDisplay *This=(TSteemDisplay*)t;
  int idx=-1;
  if(pddsd->dwWidth==640&&pddsd->dwHeight==480) 
    idx=0;
  if(pddsd->dwWidth==800&&pddsd->dwHeight==600) 
    idx=1;
  if(pddsd->dwWidth==640&&pddsd->dwHeight==400) 
    idx=2;
  if(pddsd->dwWidth==monitor_width && pddsd->dwHeight==monitor_height)
    idx=3;
  // record res for custom res
  for(int i=0;i<NFSRES;i++)
  {
    if(Disp.fs_res[i].x==(LONG)pddsd->dwWidth 
      && Disp.fs_res[i].y==(LONG)pddsd->dwHeight)
      break; // already recorded
    if(Disp.fs_res[i].x==0) // free
    {
      Disp.fs_res[i].x=pddsd->dwWidth;
      Disp.fs_res[i].y=pddsd->dwHeight;
      break;
    }
  }
  if(idx>=0) 
  {
    This->DDDisplayModePossible[idx]=true;
    TRACE_LOG("Adding idx %d w %d h%d %dHz\n",idx,pddsd->dwWidth,pddsd->dwHeight,pddsd->dwRefreshRate);
    for(int n=1;n<NUM_HZ;n++) 
    {
      int diff=abs(HzIdxToHz[n]-int(pddsd->dwRefreshRate));
      int curdiff=abs(HzIdxToHz[n]-int(This->DDClosestHz[idx][n]));
      if(diff<curdiff && diff<=DISP_MAX_FREQ_LEEWAY) 
        This->DDClosestHz[idx][n]=pddsd->dwRefreshRate;
    }
#ifndef NO_CRAZY_MONITOR //4.1.0
    if(idx==3) for(int i=0;i<EXTMON_RESOLUTIONS;i++) 
    {
      if(extmon_res[i][0]==0)
        extmon_res[i][0]=pddsd->dwWidth;
      if(extmon_res[i][1]==0)
        extmon_res[i][1]=pddsd->dwHeight;
    }
#endif
  }
  return DDENUMRET_OK;
}


HRESULT TSteemDisplay::DDCreateSurfaces() {
  HRESULT DErr=1234;
  if(!DDObj)
    return DErr;
  DDDestroySurfaces();
  int ExtraFlags=0;
  for(int n=0;n<2;n++) 
  {
    ZeroMemory(&ddsd,sizeof(ddsd));
    ddsd.dwSize=sizeof(ddsd);
    ddsd.dwFlags=DDSD_CAPS;
    ddsd.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE | ExtraFlags;
    if(FullScreen&&!OPTION_FAKE_FULLSCREEN) 
    {
      ddsd.ddsCaps.dwCaps|=DDSCAPS_FLIP | DDSCAPS_COMPLEX;
      ddsd.dwFlags|=DDSD_BACKBUFFERCOUNT;
      ddsd.dwBackBufferCount=1;
      // In fullscreen mode, this is as simple as that, like in the D3D build.
      if(OPTION_3BUFFER_FS)
        ddsd.dwBackBufferCount++;
    }
    if(FullScreen&&OPTION_FAKE_FULLSCREEN) 
    {
#if defined(SSE_VID_2SCREENS)
      CheckCurrentMonitorConfig(); // Update monitor rectangle
#endif
      ddsd.dwWidth=rcMonitor.right-rcMonitor.left;
      ddsd.dwHeight=rcMonitor.bottom-rcMonitor.top;
    }
    if((DErr=DDObj->CreateSurface(&ddsd,&DDPrimarySur,NULL))!=DD_OK) 
    {
      if(n==0) 
      {
        if(ExtraFlags)
          ExtraFlags = 0;
        else
          ExtraFlags=DDSCAPS_SYSTEMMEMORY;
      }
      else
      {
        REPORT_DD_ERR("DDPrimarySur",DErr);
        // Another DirectX app is fullscreen so fail silently
        if(DErr==DDERR_NOEXCLUSIVEMODE) return DErr;
        // Otherwise make a big song and dance!
        return DDError("CreateSurface for PrimarySur FAILED",DErr);
      }
    }
    else
      break;
  }
  ddsd.dwSize=sizeof(ddsd);
  DDPrimarySur->GetSurfaceDesc(&ddsd);
  if(FullScreen&&!OPTION_FAKE_FULLSCREEN)
    DDBackSurIsAttached=true;
#if defined(SSE_VID_DD_MISC)
  else // Windows 10, clipper ruins fullscreen?
#endif
  if((DErr=DDPrimarySur->SetClipper(DDClipper))!=DD_OK) 
  {
    REPORT_DD_ERR("SetClipper",DErr);
    return DDError("SetClipper FAILED",DErr);
  }
  if(FullScreen==0||OPTION_FAKE_FULLSCREEN)
  {
    if(!DrawToVidMem) // only for buffer in window mode
      ExtraFlags=DDSCAPS_SYSTEMMEMORY; // Like malloc
    for(int n=0;n<2;n++) 
    {
      ZeroMemory(&DDBackSurDesc,sizeof(DDBackSurDesc));
      DDBackSurDesc.dwSize=sizeof(DDBackSurDesc);
      DDBackSurDesc.dwFlags=DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
      DDBackSurDesc.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN | ExtraFlags;
      Draw.MarshalParameters();
      DDBackSurDesc.dwWidth=Draw.SrcWidth;
      DDBackSurDesc.dwHeight=Draw.SrcHeight + 1;
      if((DErr=DDObj->CreateSurface(&DDBackSurDesc,&DDBackSur,NULL))!=DD_OK) 
      {
        if(n==0)
          ExtraFlags=0;
        else
        {
          REPORT_DD_ERR("DDBackSur",DErr);
          return DDError("CreateSurface for BackSur FAILED",DErr);
        }
      }
      else
      {
#if defined(SSE_VID_DD_3BUFFER_WIN)
        if(OPTION_3BUFFER_WIN)
        {// Let's create a second back surface for our "triple buffer"
          DErr=DDObj->CreateSurface(&DDBackSurDesc,&DDBackSur2,NULL);
          if(DErr!=DD_OK)
          {
            REPORT_DD_ERR("DDBackSur2",DErr);
            //ASSERT(DDBackSur2==NULL);
            DDBackSur2=NULL; // doc doesn't state it is null
          }
          VSyncTiming=0;
          SurfaceToggle=true; // will be toggled false at first lock
        }
#endif
        break;
      }
    }
  }
  else
  {// Fullscreen
    myDDSCAPS caps;
    ZeroMemory(&caps,sizeof(caps));
    caps.dwCaps=DDSCAPS_BACKBUFFER;
    if((DErr=DDPrimarySur->GetAttachedSurface(&caps,&DDBackSur))!=DD_OK) 
    {
      REPORT_DD_ERR("DDBackSur",DErr);
      return DDError("CreateSurface for BackSur FAILED",DErr);
    }
  }
  DDBackSurDesc.dwSize=sizeof(DDBackSurDesc);
  if((DErr=DDBackSur->GetSurfaceDesc(&DDBackSurDesc))!=DD_OK) 
  {
    REPORT_DD_ERR("DDBackSurDesc",DErr);
    return DDError("GetSurfaceDesc for BackSur FAILED",DErr);
  }
  SurfaceWidth=DDBackSurDesc.dwWidth;
  SurfaceHeight=DDBackSurDesc.dwHeight;
  ASSERTB((DDBackSurDesc.ddpfPixelFormat.dwRGBBitCount/8)==4);
  rgb32_bluestart_bit=((DDBackSurDesc.ddpfPixelFormat.dwBBitMask==0x0000ff00) ? 8:0);
  draw_mem=NULL;
  OurBackSur=DDBackSur;
  if(runstate==RUNSTATE_STOPPED)
  {
    DErr=Lock();
    if(draw_mem)
    {
      ZeroMemory(draw_mem,VideoMemorySize);
      Unlock();
    }
  }
  draw_init_resdependent();
  palette_prepare(true);
  TRACE_LOG("Primary %dx%d %dbit caps %X flags %X FS %d buffers %d pitch %d\n",ddsd.dwWidth, ddsd.dwHeight,ddsd.ddpfPixelFormat.dwRGBBitCount,ddsd.ddsCaps.dwCaps, ddsd.dwFlags,(FullScreen?(OPTION_FAKE_FULLSCREEN?2:1):0),ddsd.dwBackBufferCount,ddsd.lPitch);
  TRACE_LOG("Back %dx%d %dbit caps %X flags %X buffers %d pitch %d\n",DDBackSurDesc.dwWidth, DDBackSurDesc.dwHeight,DDBackSurDesc.ddpfPixelFormat.dwRGBBitCount,DDBackSurDesc.ddsCaps.dwCaps,DDBackSurDesc.dwFlags,DDBackSurDesc.dwBackBufferCount,DDBackSurDesc.lPitch);
  return DD_OK;
}


void TSteemDisplay::DDDestroySurfaces() {
#if defined(SSE_VID_D3D)
  D3DDestroySurfaces();
#endif
  if(DDPrimarySur)
  {
    DDPrimarySur->Release(); 
    DDPrimarySur=NULL;
    if(DDBackSurIsAttached) 
      DDBackSur=NULL;
  }
  if(DDBackSur)
  {
    if(draw_lock)
      draw_end();
    DDBackSur->Release(); 
    DDBackSur=NULL;
  }
#if defined(SSE_VID_DD_3BUFFER_WIN)
  if(DDBackSur2) 
  {
    DDBackSur2->Release(); 
    DDBackSur2=OurBackSur=NULL;
  }
#endif
  DDBackSurIsAttached=false;
}


HRESULT TSteemDisplay::DDError(char *ErrorText,HRESULT DErr) {
  Release();
  StatusInfo.MessageIndex = TStatusInfo::BLIT_ERROR;
  char Text[1000];
  strcpy(Text,ErrorText);
  strcat(Text,"\n\n");
  DDGetErrorDescription(DErr,Text+(int)strlen(Text),499-(int)strlen(Text));
  TRACE_LOG("!!!!!!!\n%s\n!!!!!!!!!!!!!!",Text);
  strcat(Text,EasyStr("\n\n")+T("Would you like to disable the use of DirectDraw?"));
#ifndef ONEGAME
  int Ret=MessageBox(NULL,Text,T("Steem Engine DirectDraw Error"),
    MB_YESNO|MB_ICONEXCLAMATION|MB_SETFOREGROUND|MB_TASKMODAL|MB_TOPMOST);
  if(Ret==IDYES) 
    WriteCSFStr("Options","NoDirectDraw","1",globalINIFile);
#endif
  return DErr;
}

#endif


bool TSteemDisplay::InitGDI() { // generally Direct X is used instead
  Release(); // note this will kill D3D, and reset Method
  Method=DISPMETHOD_GDI;
  WORD w,h;
  WORD wd=(WORD)(Draw.BltDst.right-Draw.BltDst.left);
  WORD hd=(WORD)(Draw.BltDst.bottom-Draw.BltDst.top);
  WORD ws=(WORD)(Draw.BltSrc.right-Draw.BltSrc.left);
  WORD hs=(WORD)(Draw.BltSrc.bottom-Draw.BltSrc.top);
  if(FullScreen)
  {
#if defined(SSE_VID_2SCREENS)
    CheckCurrentMonitorConfig(); // Update monitor rectangle
#endif
    w=(WORD)monitor_width;
    h=(WORD)monitor_height;
  }
  else // based on handling of WM_SIZE
  {
    w=MAX(wd,ws);
    h=MAX(hd,hs);
  }
  TRACE_LOG("InitGDI src %dx%d dst %dx%d srf %dx%d\n",ws,hs,wd,hd,w,h);
  HDC dc=GetDC(NULL);
  GDIBmp=CreateCompatibleBitmap(dc,w,h);
  ReleaseDC(NULL,dc);
  if(GDIBmp==NULL)
    return false;
  BITMAP BmpInf;
  GetObject(GDIBmp,sizeof(BITMAP),&BmpInf);
  ASSERTB(((BmpInf.bmBitsPixel+7)/8)==4);
  GDIBmpLineLength=BmpInf.bmWidthBytes;
  GDIBmpSize=GDIBmpLineLength*BmpInf.bmHeight;
  GDIBmpDC=CreateCompatibleDC(NULL);
  SelectObject(GDIBmpDC,GDIBmp);
  SelectObject(GDIBmpDC,fnt);
#if defined(SSE_BADALLOC)
  GDIBmpMem=new BYTE[GDIBmpSize+1];
#else
  try{
    GDIBmpMem=new BYTE[GDIBmpSize+1];
  }catch (...){
    TRACE_LOG("GDI ERROR\n");
    GDIBmpMem=NULL;
    Release();
    return false;
  }
#endif
  {
    SetPixel(GDIBmpDC,0,0,RGB(255,0,0));
    GetBitmapBits(GDIBmp,GDIBmpSize,GDIBmpMem);
    DWORD RedBitMask=0;
    for(int i=BytesPerPixel-1;i>=0;i--)
    {
      RedBitMask<<=8;
      RedBitMask|=GDIBmpMem[i];
    }
    rgb32_bluestart_bit=int((RedBitMask==0xff000000) ? 8:0);
  }
  SurfaceWidth=w;
  SurfaceHeight=h;
  palette_prepare(true);
  draw_init_resdependent();
  return true;
}


#if !defined(SSE_NO_FREEIMAGE)

void TSteemDisplay::FreeImageLoad() {
  if(hFreeImage) 
    return;
  hFreeImage=SteemLoadLibrary(FREE_IMAGE_DLL);
  if(hFreeImage==NULL)
    return;
  FreeImage_Initialise=(FI_INITPROC)GetProcAddress(hFreeImage,"_FreeImage_Initialise@4");
  FreeImage_DeInitialise=(FI_DEINITPROC)GetProcAddress(hFreeImage,"_FreeImage_DeInitialise@0");
  FreeImage_ConvertFromRawBits=
    (FI_CONVFROMRAWPROC)GetProcAddress(hFreeImage,"_FreeImage_ConvertFromRawBits@36");
  FreeImage_FIFSupportsExportBPP=
    (FI_SUPPORTBPPPROC)GetProcAddress(hFreeImage,"_FreeImage_FIFSupportsExportBPP@8");
  FreeImage_Save=(FI_SAVEPROC)GetProcAddress(hFreeImage,"_FreeImage_Save@16");
  FreeImage_Free=(FI_FREEPROC)GetProcAddress(hFreeImage,"_FreeImage_Free@4");
  if(!FreeImage_Free) // breaking change!
    FreeImage_Free=(FI_FREEPROC)GetProcAddress(hFreeImage,"_FreeImage_Unload@4");
  if(FreeImage_Initialise==NULL||FreeImage_DeInitialise==NULL||
    FreeImage_ConvertFromRawBits==NULL||FreeImage_Save==NULL||
    FreeImage_FIFSupportsExportBPP==NULL||FreeImage_Free==NULL) 
  {
    SteemFreeLibrary(hFreeImage);//hFreeImage=NULL;
    return;
  }
  FreeImage_Initialise(TRUE);
  SSEConfig.FreeImageDll=TRUE;
}

#endif

#endif //WIN32

#pragma warning (disable: 4701) //SurLineLen

HRESULT TSteemDisplay::SaveScreenShot() {
  Str ShotFile=ScreenShotNextFile;
  ScreenShotNextFile="";
  bool ToClipboard=(ScreenShotFormat==IF_TOCLIPBOARD);
  if(!ToClipboard && ShotFile.Empty()) // create file name
  {
    DWORD Attrib=GetFileAttributes(ScreenShotFol.Text);
    if(Attrib==INVALID_FILE_ATTRIBUTES||(Attrib & FILE_ATTRIBUTE_DIRECTORY)==0) 
      return DDERR_GENERIC;

#ifdef WIN32 // get ext right!
  EasyStringList format_sl;
  ScreenShotGetFormats(&format_sl);
  for(int i=0;i<format_sl.NumStrings;i++)
    if(format_sl[i].Data[0]==ScreenShotFormat)
      OptionBox.ChangeScreenShotFormat(ScreenShotFormat,format_sl[i].String);
#endif

#if defined(SSE_VID_D3D) && !defined(SSE_NO_FREEIMAGE)
    Str Exts=ScreenShotExt; // can be JPG or PNG too
#else
    Str Exts="bmp";
#ifdef WIN32
    if(hFreeImage)
      Exts=ScreenShotExt;
#endif
#endif

#if defined(SSE_VID_NEOPIC)
    if(ScreenShotFormat==IF_NEO)
      Exts="NEO";
#endif
    EasyStr FirstWord="Steem_";
    if(FloppyDisk[DRIVE_A].DiskName.NotEmpty())
    {
      FirstWord=FloppyDisk[DRIVE_A].DiskName;
      if(!ScreenShotUseFullName)
      {
        char *spc=strchr(FirstWord,' ');
        if(spc) 
          *spc='\0';
      }
    }
    bool AddNumExt=true;
    if(ScreenShotUseFullName)
    {
      ShotFile=ScreenShotFol+SLASH+FirstWord+"."+Exts;
      if(!Exists(ShotFile)) 
        AddNumExt=ScreenShotAlwaysAddNum;
    }
    if(AddNumExt) 
    {
      int Num=0;
      do {
        if(++Num>=100000) 
          return DDERR_GENERIC;
        ShotFile=ScreenShotFol+SLASH+FirstWord+"_"
          +(EasyStr("00000")+Num).Rights(5)+"."+Exts;
      } while(Exists(ShotFile));
    }
  }
#if defined(SSE_VID_NEOPIC)
  if(ScreenShotFormat==IF_NEO && pNeoFile!=NULL)
  {
    //ASSERT(!ToClipboard);
    pNeoFile->resolution=screen_res;
    SWAP_BIG_ENDIAN_WORD(pNeoFile->resolution);
    // palette was already copied (sooner=better)
    for(int i=0;i<16000;i++)
    {
      pNeoFile->data[i]=SafeDPeek(vbase+i*2);
      SWAP_BIG_ENDIAN_WORD(pNeoFile->data[i]);
    }
    FILE *fp=fopen(ShotFile,"wb");
    if(fp)
    {
      FWRITE(pNeoFile,sizeof(neochrome_file),1,fp);
      TRACE_LOG("Save screenshot %s res %d\n",CHECKPATH(ShotFile.Text),screen_res);
      fclose(fp);
    }
    delete pNeoFile;
    pNeoFile=NULL;
    return DD_OK;
  }
#endif
  BYTE *SurMem=NULL;
  LONG SurLineLen;
  int w=Draw.BltDst.right-Draw.BltDst.left,h;
#ifdef WIN32
#if defined(SSE_VID_D3D)
  IDirect3DSurface9 *BackBuff=NULL,*SaveSur=NULL;
#endif
#if defined(SSE_VID_DD)
  myIDirectDrawSurface *SaveSur=NULL;
#endif
  HBITMAP SaveBmp=NULL;
#endif//WIN32
#if defined(SSE_GUI_STATUS_BAR)
  int sbh=GuiSM.m_statusbar_height; // part of client zone
#else
  int sbh=0;
#endif
  // Need to create new surfaces so we can blit in the same way we do to the
  // window, just in case image must be stretched. We can't do this ourselves
  // (even if we wanted to) because some video cards will blur.
  switch(Method) {
#if defined(SSE_VID_D3D)
  case DISPMETHOD_D3D:
  {
    if(!pD3D||!pD3DDevice)
      return DDERR_GENERIC;
    HRESULT DErr;
    D3DDISPLAYMODE d3ddm;
    if((DErr=pD3D->GetAdapterDisplayMode(m_Adapter,&d3ddm))!=D3D_OK)
    {
      REPORT_D3D_ERR("GetAdapterDisplayMode",DErr);
      return DErr;
    }
    h=Draw.BltDst.bottom-Draw.BltDst.top;
/*  Source = BackBuff, Destination = SaveSur
    We just get a pointer to the back buffer.
*/
    if((DErr=pD3DDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&BackBuff))!=0)
    {
      REPORT_D3D_ERR("GetBackBuffer",DErr);
      return DErr;
    }
/*  If the target surface is a plain surface, we can't use StretchRect on it,
    so we use CreateRenderTarget() instead.
    TRUE for lockable, necessary for FreeImage.
*/
    if((DErr=pD3DDevice->CreateRenderTarget(w,h,d3ddm.Format,D3DMULTISAMPLE_NONE,
      0,TRUE,&SaveSur,NULL))!=D3D_OK)
    {
      REPORT_D3D_ERR("CreateRenderTarget",DErr);
      return DErr;
    }
    RECT rcDest={0,0,0,0};
    rcDest.right=w;
    rcDest.bottom=h;
    RECT rcSrc=rcDest;
    if(ScreenShotMinSize) // option
    {
      if(border)
      {
        rcDest.right=WinSizeBorder[screen_res][0].x;
        rcDest.bottom=WinSizeBorder[screen_res][0].y;
      }
      else
      {
        rcDest.right=WinSize[screen_res][0].x;
        rcDest.bottom=WinSize[screen_res][0].y;
      }
    }
    // copy source->destination
    if((DErr=pD3DDevice->StretchRect(BackBuff,&rcSrc,SaveSur,&rcDest,D3DTEXF_NONE))!=DS_OK)
    {
      REPORT_D3D_ERR("StretchRect",DErr);
      // fall back on making SaveSur the back buffer
      if((DErr=pD3DDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&SaveSur))!=D3D_OK)
      {
        REPORT_D3D_ERR("StretchRect",DErr);
      }
    }
    if(BackBuff)
      BackBuff->Release();
    w=rcDest.right;h=rcDest.bottom;
    if(!ToClipboard)
    {
#if !defined(SSE_NO_FREEIMAGE)
      if(hFreeImage)
      {
        D3DLOCKED_RECT LockedRect;
        if((DErr=SaveSur->LockRect(&LockedRect,NULL,0))!=0)//390
        {
          REPORT_D3D_ERR("LockRect",DErr);
          SaveSur->Release();
          return DErr;
        }
        SurLineLen=LockedRect.Pitch;
        SurMem=(BYTE*)LockedRect.pBits;
      }
      else
      {
        D3DXIMAGE_FILEFORMAT fileformat=D3DXIFF_BMP;
        switch(ScreenShotFormat) { //note the function can't save in tga or ppm format
        case FIF_JPEG:
          fileformat=D3DXIFF_JPG;
          break;
        case FIF_PNG:
          fileformat=D3DXIFF_PNG;
          break;
        }
        DErr=D3DXSaveSurfaceToFile(ShotFile,fileformat,SaveSur,NULL,&rcDest);
        TRACE_LOG("Save screenshot %s %dx%d native ERR%d\n",CHECKPATH(ShotFile.Text),w,h,DErr);
        SaveSur->Release();
        return DErr;
      }
#else
      DErr=D3DXSaveSurfaceToFile(ShotFile,(D3DXIMAGE_FILEFORMAT)ScreenShotFormat,
        SaveSur,NULL,&rcDest);
      TRACE_LOG("Save screenshot %s %dx%d native ERR%d\n",CHECKPATH(ShotFile.Text),w,h,DErr);
      SaveSur->Release();
      return DErr;
#endif
    }
    break;
  }
#endif
#ifdef SSE_VID_DD
  case DISPMETHOD_DD:
  {
    if(OurBackSur==NULL)
      return DDERR_GENERIC;
    RECT rcDest={0,0,0,0};
    if(ScreenShotMinSize) 
    {
      if(border) 
      {
        rcDest.right=WinSizeBorder[screen_res][0].x;
        rcDest.bottom=WinSizeBorder[screen_res][0].y;
      }
      else
      {
        rcDest.right=WinSize[screen_res][0].x;
        rcDest.bottom=WinSize[screen_res][0].y;
      }
    }
    else
    {
      if(FullScreen)
      {
        get_fullscreen_rect(&rcDest);
        OffsetRect(&rcDest,-rcDest.left,-rcDest.top);
      }
      else
      {
        GetClientRect(StemWin,&rcDest);
        rcDest.bottom-=MENUHEIGHT+sbh;
      }
    }
    w=rcDest.right;h=rcDest.bottom;
    HRESULT DErr;
    myDDSURFACEDESC SaveSurDesc;
    ZeroMemory(&SaveSurDesc,sizeof(SaveSurDesc));
    SaveSurDesc.dwSize=sizeof(SaveSurDesc);
    SaveSurDesc.dwFlags=DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    SaveSurDesc.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    SaveSurDesc.dwWidth=w;
    SaveSurDesc.dwHeight=h;
    DErr=DDObj->CreateSurface(&SaveSurDesc,&SaveSur,NULL);
    if(DErr!=DD_OK) 
      return DErr;
    DErr=SaveSur->Blt(&rcDest,OurBackSur,&draw_blit_source_rect,DDBLT_WAIT,NULL);
    if(DErr!=DD_OK) 
    {
      SaveSur->Release();
      return DErr;
    }
    if(SaveSur->IsLost()==DDERR_SURFACELOST) 
    {
      SaveSur->Release();
      return DErr;
    }
    if(!ToClipboard) 
    {
      SaveSurDesc.dwSize=sizeof(DDSURFACEDESC);
      DErr=SaveSur->Lock(NULL,&SaveSurDesc,DDLOCK_WAIT|DDLockFlags,NULL);
      if(DErr!=DD_OK) 
      {
        SaveSur->Release();
        return DErr;
      }
      SurMem=(BYTE*)SaveSurDesc.lpSurface;
      SurLineLen=SaveSurDesc.lPitch;
    }
    break;
  }
#endif//#ifdef SSE_VID_DD
#ifdef WIN32
  case DISPMETHOD_GDI:
  {
    if(GDIBmp==NULL) 
      return DDERR_GENERIC;
    BITMAP BmpInf;
    RECT rcDest;
    GetClientRect(StemWin,&rcDest);
    h=rcDest.bottom-(MENUHEIGHT+sbh);
    HDC dc=GetDC(NULL);
    SaveBmp=CreateCompatibleBitmap(dc,w,h);
    ReleaseDC(NULL,dc);
    HDC SaveBmpDC=CreateCompatibleDC(NULL);
    SelectObject(SaveBmpDC,SaveBmp);
    SetStretchBltMode(SaveBmpDC,COLORONCOLOR);
    StretchBlt(SaveBmpDC,0,0,w,h,GDIBmpDC,draw_blit_source_rect.left,
      draw_blit_source_rect.top,draw_blit_source_rect.right
      -draw_blit_source_rect.left,draw_blit_source_rect.bottom
      -draw_blit_source_rect.top,SRCCOPY);
    DeleteDC(SaveBmpDC);
    if(!ToClipboard) 
    {
      GetObject(SaveBmp,sizeof(BITMAP),&BmpInf);
      SurLineLen=BmpInf.bmWidthBytes;
#if defined(SSE_BADALLOC)
      DWORD BmpBytes=SurLineLen*BmpInf.bmHeight;
      SurMem=new BYTE[BmpBytes];
      GetBitmapBits(SaveBmp,BmpBytes,SurMem);
#else
      try {
        DWORD BmpBytes=SurLineLen*BmpInf.bmHeight;
        SurMem=new BYTE[BmpBytes];
        GetBitmapBits(SaveBmp,BmpBytes,SurMem);
      } catch(...) {
        DeleteObject(SaveBmp);
        return DDERR_GENERIC;
      }
#endif
    }
    break;
  }
#endif//WIN32
#ifdef UNIX
  // No need to create a new surface here, X can't stretch
  case DISPMETHOD_XSHM:
  case DISPMETHOD_X:
    if(X_Img==NULL) 
      return DDERR_GENERIC;
    w=draw_blit_source_rect.right;
    h=draw_blit_source_rect.bottom;
    SurMem=(LPBYTE)X_Img->data;
    SurLineLen=X_Img->bytes_per_line;
    break;
#endif//UNIX
  default:
    return DDERR_GENERIC;
  }//sw
  BYTE *Pixels=SurMem;
  bool ConvertPixels=true;
#ifdef WIN32
#if 0 && !defined(SSE_NO_FREEIMAGE) // we must convert anyway :(
  if(hFreeImage && !ToClipboard)
  {
    if(FreeImage_FIFSupportsExportBPP((FREE_IMAGE_FORMAT)ScreenShotFormat,
      BytesPerPixel*8))
      ConvertPixels=0;
  }
#endif
#endif
  if(ToClipboard) 
  {
    ConvertPixels=false;
#ifdef WIN32
#if defined(SSE_VID_D3D) || defined(SSE_VID_DD)
    if(Method==DISPMETHOD_DD || Method==DISPMETHOD_D3D) 
    {
      HDC DDSaveSurDC=NULL;
      HRESULT DErr=SaveSur->GetDC(&DDSaveSurDC);
      if(DErr!=DD_OK) 
      {
        REPORT_D3D_ERR("GetDC",DErr);
        SaveSur->Release();
        return DErr;
      }
      HDC dc=GetDC(NULL);
      SaveBmp=CreateCompatibleBitmap(dc,w,h);
      ReleaseDC(NULL,dc);
      HDC SaveBmpDC=CreateCompatibleDC(NULL);
      SelectObject(SaveBmpDC,SaveBmp);
      BitBlt(SaveBmpDC,0,0,w,h,DDSaveSurDC,0,0,SRCCOPY);
      DeleteDC(SaveBmpDC);
      SaveSur->ReleaseDC(DDSaveSurDC);
    }
    if(OpenClipboard(StemWin)) 
    {
      EmptyClipboard();
      SetClipboardData(CF_BITMAP,SaveBmp);
      TRACE_LOG("Copy screenshot %dx%d to clipboard\n",w,h);
      CloseClipboard(); // don't return here, must clean up
    }
#endif
#endif//WIN32
  }
  else if(ConvertPixels) // convert to 24bit (R8G8B8)
  {
    Pixels=new BYTE[w*h*3 + 16];
    BYTE *pPix=Pixels;
    DWORD *pSur=(LPDWORD)(SurMem+((h-1)*SurLineLen)),*pSurLineEnd; // ignore warning
    if(rgb32_bluestart_bit) // unlikely
    {
      while((LPBYTE)pSur>=SurMem) {
        pSurLineEnd=pSur+w;
        for(;pSur<pSurLineEnd;pSur++) 
        {
          *(LPDWORD)pPix=(*pSur)>>rgb32_bluestart_bit;
          pPix+=3;
        }
        pSur=(LPDWORD)((LPBYTE)pSur-SurLineLen)-w;
      }
    }
    else 
    {
      while((LPBYTE)pSur>=SurMem) {
        pSurLineEnd=pSur+w;
        for(;pSur<pSurLineEnd;pSur++) 
        {
          *(LPDWORD)pPix=*pSur;
          pPix+=3;
        }
        pSur=(LPDWORD)((LPBYTE)pSur-SurLineLen)-w;
      }
    }
  }
  if(!ToClipboard) // save
  {
#ifdef WIN32
#if !defined(SSE_NO_FREEIMAGE)
    if(hFreeImage) 
    {
      FIBITMAP *FIBmp;
#if 0
      if(ConvertPixels)
#endif
      {
        FIBmp=FreeImage_ConvertFromRawBits(Pixels,w,h,w*3,24,0xff0000,
          0x00ff00,0x0000ff,false); //flip pic
      }
#if 0 // seems to fail (black picture)
      else
      {
        DWORD r_mask=0xff0000 << rgb32_bluestart_bit;
        DWORD g_mask=0x00ff00 << rgb32_bluestart_bit;
        DWORD b_mask=0x0000ff << rgb32_bluestart_bit;
        FIBmp=FreeImage_ConvertFromRawBits(SurMem,w,h,SurLineLen,BytesPerPixel*8,
                                          r_mask,g_mask,b_mask,false);
      }
#endif
      TRACE_LOG("Save screenshot %s %dx%d opts %d FreeImage\n",CHECKPATH(ShotFile.Text),
       w,h,ScreenShotFormatOpts);
      FreeImage_Save((FREE_IMAGE_FORMAT)ScreenShotFormat,FIBmp,ShotFile,ScreenShotFormatOpts);
      FreeImage_Free(FIBmp);
    }
    else // GDI
#endif//#if !defined(SSE_NO_FREEIMAGE)
#endif
    {
      BITMAPINFOHEADER bih;
      ZeroMemory(&bih,sizeof(BITMAPINFOHEADER));
      bih.biSize=sizeof(BITMAPINFOHEADER);
      bih.biWidth=w;
      bih.biHeight=h;
#ifdef WIN32
      bih.biPlanes=1;
      bih.biBitCount=24;
#endif
#ifdef UNIX
      bih.biPlanes_biBitCount=MAKELONG(1,24);
#endif
      FILE *fp=fopen(ShotFile,"wb");
      if(fp)
      {
        // File header
        WORD bfType=19778; //'BM';
        DWORD bfSize=14 /*sizeof(BITMAPFILEHEADER)*/ + sizeof(BITMAPINFOHEADER)+(w*h*3);
        WORD bfReserved1=0;
        WORD bfReserved2=0;
        DWORD bfOffBits=14 /*sizeof(BITMAPFILEHEADER)*/ + sizeof(BITMAPINFOHEADER);
        FWRITE(&bfType,sizeof(bfType),1,fp);
        FWRITE(&bfSize,sizeof(bfSize),1,fp);
        FWRITE(&bfReserved1,sizeof(bfReserved1),1,fp);
        FWRITE(&bfReserved2,sizeof(bfReserved2),1,fp);
        FWRITE(&bfOffBits,sizeof(bfOffBits),1,fp);
        FWRITE(&bih,sizeof(bih),1,fp);
        FWRITE(Pixels,w*h*3,1,fp);
        fclose(fp);
      }
    }
  }
#ifdef WIN32
  switch(Method) {
#if defined(SSE_VID_D3D)
  case DISPMETHOD_D3D:
    if(!ToClipboard)
      SaveSur->UnlockRect();
    SaveSur->Release();
    break;
#endif
#if defined(SSE_VID_DD)
  case DISPMETHOD_DD:
    if(!ToClipboard)
      SaveSur->Unlock(NULL);
    SaveSur->Release();
    break;
#endif
  case DISPMETHOD_GDI:
    delete[] SurMem;
  }//sw
  if(SaveBmp) 
    DeleteObject(SaveBmp);
#endif//WIN32
  if(ConvertPixels) 
    delete[] Pixels;
  return DD_OK;
}

#pragma warning (default: 4701)


void draw_init_resdependent() {
  if(draw_grille_black<4) 
    draw_grille_black=4;
  make_palette_table(col_brightness,col_contrast);
  palette_convert_all();
#ifndef SSE_NO_OSD
  if(osd_plasma_pal) 
  {
    delete[] osd_plasma_pal; osd_plasma_pal=NULL;
    delete[] osd_plasma;     osd_plasma=NULL;
  }
#endif
}


#ifdef WIN32

#if 0 && !defined(SSE_NO_FREEIMAGE)

bool TSteemDisplay::ScreenShotIsFreeImageAvailable() {
  if(hFreeImage)
    return true;
  return (SteemLoadLibrary(FREE_IMAGE_DLL)!=NULL); //so we load it anyway now
  /*
  Str Path;
  Path.SetLength(MAX_PATH);
  char *FilNam;
  if(SearchPath(NULL,FREE_IMAGE_DLL,NULL,MAX_PATH,Path.Text,&FilNam)>0) 
    return true;
  if(Exists(RunDir+"\\FreeImage\\" FREE_IMAGE_DLL)) 
    return true;
  if(Exists(RunDir+"\\FreeImage\\FreeImage\\" FREE_IMAGE_DLL)) 
    return true;
  return 0;
  */
}

#endif//#if !defined(SSE_NO_FREEIMAGE)


void TSteemDisplay::ScreenShotGetFormats(EasyStringList *pSL) {
#if !defined(SSE_NO_FREEIMAGE)
  //bool FIAvailable=ScreenShotIsFreeImageAvailable();
#endif
  pSL->Sort=eslNoSort;
  pSL->Add(T("To Clipboard"),IF_TOCLIPBOARD);
  pSL->Add("BMP",FIF_BMP);
#if !defined(SSE_NO_FREEIMAGE)
  //if(FIAvailable) 
  if(SSEConfig.FreeImageDll) // TODO shouldn't the list comne from the plugin?
  {
    pSL->Add("JPEG (.jpg)",FIF_JPEG);
    pSL->Add("PNG",FIF_PNG);
    pSL->Add("TARGA (.tga)",FIF_TARGA);
    pSL->Add("TIFF",FIF_TIFF);
    pSL->Add("PBM",FIF_PBM);
    pSL->Add("PGM",FIF_PGM);
    pSL->Add("PPM",FIF_PPM);
  }
#endif//#if !defined(SSE_NO_FREEIMAGE)
#if defined(SSE_VID_D3D)
#if !defined(SSE_NO_FREEIMAGE)
  else
#endif
  {
    pSL->Add("JPEG (.jpg)",FIF_JPEG);
    pSL->Add("PNG",FIF_PNG);
  }
#endif
#if defined(SSE_VID_NEOPIC)
  pSL->Add("NEO",IF_NEO);
#endif
}


#if !defined(SSE_NO_FREEIMAGE)

void TSteemDisplay::ScreenShotGetFormatOpts(EasyStringList *pSL) {
  pSL->Sort=eslNoSort;
  switch(ScreenShotFormat) {
  case FIF_BMP:
    if(SSEConfig.FreeImageDll)
    {
      pSL->Add(T("Normal"),BMP_DEFAULT);
      pSL->Add("RLE",BMP_SAVE_RLE);
    }
    break;
  case FIF_JPEG:
    pSL->Add(T("Superb Quality"),JPEG_QUALITYSUPERB);
    pSL->Add(T("Good Quality"),JPEG_QUALITYGOOD);
    pSL->Add(T("Normal"),JPEG_QUALITYNORMAL);
    pSL->Add(T("Average Quality"),JPEG_QUALITYAVERAGE);
    pSL->Add(T("Bad Quality"),JPEG_QUALITYBAD);
    break;
  case FIF_PBM:case FIF_PGM:case FIF_PPM:
    pSL->Add(T("Binary"),PNM_SAVE_RAW);
    pSL->Add("ASCII",PNM_SAVE_ASCII);
    break;
  }
}

#endif//#if !defined(SSE_NO_FREEIMAGE)


#if defined(SSE_VID_DD_3BUFFER_WIN)
/*  When the option is on, this function is called a lot
    during emulation (each scanline) and during VBL idle
    times too, so the processor is always busy. TODO
    Scrolling is also sketchy, triple buffering only removes
    tearing.
*/

BOOL TSteemDisplay::BlitIfVBlank() {
  BOOL Blanking=FALSE;  
  if(Disp.DDObj && A_S_T-Disp.VSyncTiming>80000-60000) // avoid bursts
  {
    Disp.DDObj->GetVerticalBlankStatus(&Blanking);
    if(Blanking)
    {
      Disp.VSyncTiming=A_S_T;
      draw_blit();
    }
  }
  return Blanking;
}

#endif


#if defined(SSE_VID_D3D)
/*  Direct3D9 support

    D3D support was introduced in v3.7.0, only for fullscreen.

    As of v3.8.2 there are two separate builds for DirectDraw support
    and for Direct3D support, both windowed and fullscreen modes.
    This makes options (a little) simpler, and that way people don't need to
    update their computer (D3D9 install not always complete). 

    We use DirectX9 and the ID3DXSprite interface.
 */

#ifdef BCC_BUILD
// yes sir, the old BCC5.5 will build the D3D9 version too
#pragma comment(lib, "../../3rdparty/d3d/bcc/d3d9.lib")
#pragma comment(lib, "../../3rdparty/d3d/bcc/d3dx9_43.lib")
#pragma message D3D DIRECT3D_VERSION
#endif

#if _MSC_VER == 1200 // VC6 -  also for this dinosaur
#define D3D_DISABLE_9EX
#pragma comment(lib, "../../3rdparty/d3d/d3d9.lib")
#pragma comment(lib, "../../3rdparty/d3d/d3dx9.lib")
#endif

#if _MSC_VER >= 1500
// d3d9.lib d3dx9d.lib
#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "D3dx9.lib")
#endif

#if defined(SSE_VID_BFI)
bool TSteemDisplay::D3DBlit(BOOL erase/*=FALSE*/)
#else
bool TSteemDisplay::D3DBlit()
#endif
{
  // erase: draw a black frame instead
  HRESULT d3derr=E_FAIL;
  if(pD3DDevice && pD3DSprite)
  {
    RECT &dest=Draw.BltDst;
    HCURSOR OldCur=NULL;
    if(!FullScreen)
    {
      if(BlitHideMouse && stem_mousemode==STEM_MOUSEMODE_DISABLED)
        OldCur=SetCursor(NULL);
      GetClientRect(StemWin,&dest);
      dest.top+=MENUHEIGHT;
#if defined(SSE_GUI_STATUS_BAR)
      dest.bottom-=GuiSM.m_statusbar_height;
#endif
    }
    d3derr=pD3DDevice->BeginScene();
    if(d3derr==D3D_OK)
      d3derr=pD3DSprite->Begin(0); // the picture is one big sprite
    // at player's discretion, fullsceen or not
    if(TextureFilter)
    {
      pD3DDevice->SetSamplerState(0,D3DSAMP_MAGFILTER,TextureFilter);
      pD3DDevice->SetSamplerState(0,D3DSAMP_MINFILTER,TextureFilter);
    }
    if(d3derr==D3D_OK)
#if defined(SSE_VID_BFI)
      d3derr=pD3DSprite->Draw(pD3DTexture,&draw_blit_source_rect,NULL,NULL,
                              ((erase)?0:0xFFFFFFFF));
#else
      d3derr=pD3DSprite->Draw(pD3DTexture,&draw_blit_source_rect,NULL,NULL,0xFFFFFFFF);
#endif
#if defined(SSE_VID_TRACE_SIZE)
    TRACE_LOG("Sprite %d %d %d %d to %d %d %d %d ERR %d\n",draw_blit_source_rect.left,
      draw_blit_source_rect.top,draw_blit_source_rect.right,draw_blit_source_rect.bottom,
      rcSprite.left,rcSprite.top,rcSprite.right,rcSprite.bottom,d3derr);
#endif
    if(d3derr==D3D_OK)
      d3derr=pD3DSprite->End();
    if(d3derr==D3D_OK)
      d3derr=pD3DDevice->EndScene();
    if(d3derr!=D3D_OK)
    {}
    else if(!FullScreen)
    {
      if(!OPTION_TOOLBAR&&!OPTION_STATUS_BAR) // full client area is ours
        d3derr=pD3DDevice->Present(NULL,NULL,NULL,NULL);
      else
        d3derr=pD3DDevice->Present(NULL,&dest,NULL,NULL);
#if defined(SSE_VID_TRACE_SIZE)
      TRACE("F%d BLIT TO %d %d %d %d ERR %d\n",FRAME,dest.left,dest.top,dest.right,dest.bottom,d3derr);
#endif
      if(BlitHideMouse && stem_mousemode==STEM_MOUSEMODE_DISABLED)
        SetCursor(OldCur);
    }
    else // FullScreen
    {
      d3derr=pD3DDevice->Present(NULL,NULL,NULL,NULL);
#if defined(SSE_VID_TRACE_SIZE)
      TRACE("F%d BLIT TO FullScreen ERR %d\n",FRAME,d3derr);
#endif
    }
  }
  //ASSERT(d3derr != D3DERR_WASSTILLDRAWING);
  if(d3derr!=D3D_OK && d3derr!=D3DERR_WASSTILLDRAWING) 
  {
    REPORT_D3D_ERR("Blit",d3derr);
    if(StatusInfo.MessageIndex!=TStatusInfo::BLIT_ERROR)
    {
      TRACE("%s %d\n","BLIT ERROR",d3derr);
      StatusInfo.MessageIndex=TStatusInfo::BLIT_ERROR;
      UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
#ifdef SSE_STATS
      StatsStatic.nBlitError++;
#endif
    }
  }
  else if(d3derr==D3D_OK && StatusInfo.MessageIndex==TStatusInfo::BLIT_ERROR)
  {
    StatusInfo.MessageIndex=TStatusInfo::MESSAGE_NONE;
    UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
  }
  return (d3derr==D3D_OK);
}


HRESULT check_device_type(D3DDEVTYPE DeviceType,D3DFORMAT DisplayFormat) {
// local helper for D3DInit()
  HRESULT d3derr=Disp.pD3D->CheckDeviceType(Disp.m_Adapter,DeviceType,
    DisplayFormat,DisplayFormat,false);
  return d3derr;
}


HRESULT TSteemDisplay::D3DCreateSurfaces() {

#if defined(SSE_VID_D3D_SWEETFX)
  // If we enter/leave CRT emulation, must reinit D3D to get correct function pointers
  TNotify myNotify(OPTION_CRT_EMU ? "D3D" : NULL); // creating CRT surface is slow
  BOOL SameSurface=TRUE;
  if(OPTION_CRT_EMU!=SSEConfig.CrtEmu)
  {
    TRACE_LOG("%s %d ","CRT emu",OPTION_CRT_EMU);
    Disp.D3DInit(); // switch Direct3DCreate9() pointer
    SameSurface=FALSE;
  }
#endif

  HRESULT d3derr=pD3D ? D3D_OK : (~D3D_OK);
  // Present Parameters
  ZeroMemory(&d3dpp, sizeof(d3dpp));
  d3dpp.hDeviceWindow=StemWin;
  //d3dpp.SwapEffect=D3DSWAPEFFECT_COPY; // we use a dest RECT
  d3dpp.SwapEffect=D3DSWAPEFFECT_DISCARD; //  recommended by Microsoft, let D3D9 figure it out
  d3dpp.Flags=0; // we lock the texture, not the backbuffer

  Draw.MarshalParameters(); // update VSync
  if(Draw.VSync)
  {
    d3dpp.PresentationInterval=D3DPRESENT_INTERVAL_ONE;
#if 0
    // should work but it doesn't for me... even if the card claims it supports that
    DWORD fp=Freq;
    while(fp>=100)
    {
      d3dpp.PresentationInterval<<=1; // assume it's 1, 2, 3, 4...
      fp>>=1;
    }
#endif
  }
  else
    d3dpp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
  d3dpp.BackBufferCount=1;
  d3dpp.BackBufferFormat=m_DisplayFormat; 
  d3dpp.Windowed=TRUE;
  d3dpp.FullScreen_RefreshRateInHz=0; // as desktop
  if(FullScreen)
  {
    if(OPTION_FAKE_FULLSCREEN)
    {
#if defined(SSE_VID_2SCREENS)
      CheckCurrentMonitorConfig(); // Update monitor rectangle
#endif
      SurfaceWidth=monitor_width;
      SurfaceHeight=monitor_height;
      //d3dpp.hDeviceWindow=NULL;
    }
    else
    {
      D3DDISPLAYMODE Mode;
      d3derr=pD3D->EnumAdapterModes(m_Adapter,m_DisplayFormat,D3DMode,&Mode);
      SurfaceWidth=Mode.Width;
      SurfaceHeight=Mode.Height;
      d3dpp.Windowed=FALSE;
      d3dpp.BackBufferFormat=Mode.Format;
      if(OPTION_3BUFFER_FS)
        d3dpp.BackBufferCount++; // as simple as this
      if(!OPTION_FULLSCREEN_DEFAULT_HZ)
        Freq=d3dpp.FullScreen_RefreshRateInHz=Mode.RefreshRate;
      TRACE_LOG("D3D adapter %d mode %d %dx%d %dHz format %d ERR %d\n",m_Adapter,
        D3DMode,Mode.Width,Mode.Height,d3dpp.FullScreen_RefreshRateInHz,Mode.Format,d3derr);
    }
  }
  else // based on handling of WM_SIZE
  {
    SurfaceWidth=Draw.BltDst.right-Draw.BltDst.left;
    SurfaceHeight=Draw.BltDst.bottom-Draw.BltDst.top;
  }
  d3dpp.BackBufferWidth=SurfaceWidth;
  d3dpp.BackBufferHeight=SurfaceHeight;
  //TRACE_LOG("d3derr %d pD3DDevice %p StatusInfo.MessageIndex %d\n",d3derr,pD3DDevice,StatusInfo.MessageIndex);
  // Create or reset device
  if(SameSurface) // compare present parameters
    SameSurface=(memcmp(&d3dpp_save,&d3dpp,sizeof(D3DPRESENT_PARAMETERS))==0);
  // Surfaces that depend on the device must be released first
  D3DDestroySurfaces();
  if(d3derr!=D3D_OK)
    pD3DDevice=NULL;
  else
  {
    BOOL ReuseDevice=(pD3DDevice!=NULL &&  StatusInfo.MessageIndex!=TStatusInfo::BLIT_ERROR);
    BOOL LostDevice=FALSE;
    if(ReuseDevice)
    {
      D3DDEVICE_CREATION_PARAMETERS dcp;
      pD3DDevice->GetCreationParameters(&dcp);
      if(m_Adapter!=dcp.AdapterOrdinal)
      {
        TRACE_LOG("Screen was changed from %d to %d\n",dcp.AdapterOrdinal,m_Adapter);
        ReuseDevice=FALSE;
      }
    }
    if(ReuseDevice)
    {
      d3derr=pD3DDevice->TestCooperativeLevel();
      if(d3derr!=D3D_OK)
      {
        REPORT_D3D_ERR("TestCooperativeLevel",d3derr);
      }
      if(d3derr!=D3D_OK && d3derr!=D3DERR_DEVICENOTRESET)
      {
        LostDevice=TRUE;
        ReuseDevice=FALSE;
      }
      else
      {
        if(!SameSurface || d3derr)
          d3derr=pD3DDevice->Reset(&d3dpp);
        else // this works on border change
        {
          TRACE_LOG("Skipping reset identical surfaces\n");
          d3derr=D3D_OK;
        }
        if(d3derr!=D3D_OK)
        {
          ReuseDevice=FALSE;
          REPORT_D3D_ERR("Reset",d3derr);
        }
      }
    }
    if(!ReuseDevice || d3derr)
    {
      if(pD3DDevice && !LostDevice)
        pD3DDevice->Release(); // no leak
      d3derr=pD3D->CreateDevice(m_Adapter,m_DeviceType,StemWin,m_vtx_proc,
        &d3dpp,&pD3DDevice);
    }
    TRACE_LOG("%s %s %d:%dx%d b%d F%d i%X S%d VS%d ","D3D",(ReuseDevice?"Reset":"Create"),
              m_Adapter,d3dpp.BackBufferWidth,d3dpp.BackBufferHeight,border,d3dpp.BackBufferFormat,
              d3dpp.PresentationInterval,d3dpp.SwapEffect,Draw.VSync);
    if(FullScreen)
    {
      TRACE_LOG("%s W%d B%d %dHz %s %d\n","FullScreen",d3dpp.Windowed,d3dpp.BackBufferCount,
                d3dpp.FullScreen_RefreshRateInHz,"ERROR",d3derr);
    }
    else
    {
      TRACE_LOG("%dHz %s %d\n",Freq,"ERROR",d3derr);
    }
    if(d3derr==D3D_OK)
    {
      // compute SizeHash like in UpdateSurfaces, which isn't the only way
      // to create new surfaces
      SizeHash=Draw.cw*12+Draw.ch*13+Draw.Idx*3+(Draw.Stretch*16)+Draw.VSync*17
        +Draw.res*18;
      memcpy(&d3dpp_save,&d3dpp,sizeof(D3DPRESENT_PARAMETERS)); // record present parameters
    }
    else
    {
      REPORT_D3D_ERR("CreateSurfaces",d3derr);
      memset(&d3dpp_save,0,sizeof(D3DPRESENT_PARAMETERS)); // reset
    }
  }

  // Create texture, this is where we draw
  // It must be big enough but not too big or it could get slow, so
  // we must handle cases...
  if(FullScreen)
  {
    TextureWidth=SurfaceWidth;
    TextureHeight=SurfaceHeight;
  }
  else
  {
    TextureWidth=Draw.BltSrc.right-Draw.BltSrc.left;
    TextureHeight=Draw.BltSrc.bottom-Draw.BltSrc.top + 1;
    //TRACE_LOG("Texture start %dx%d\n",TextureWidth,TextureHeight);
    if(!Draw.Stretch&&Draw.Size==3)
    {
      TextureWidth*=4; TextureWidth/=3;
      TextureHeight*=4; TextureHeight/=3;
    }
    else if(Draw.Size<3)
    {
      TextureHeight*=2; // scanlines?
      if(Draw.Size==1)
        TextureWidth*=2; // LO/MED
    }
    TextureWidth=MAX(SurfaceWidth,(unsigned long)TextureWidth);
    TextureHeight=MAX(SurfaceHeight,(unsigned long)TextureHeight);
  }
  if(d3derr==D3D_OK)
  {
    d3derr=pD3DDevice->CreateTexture(TextureWidth,TextureHeight,1,D3DUSAGE_DYNAMIC,
      d3dpp.BackBufferFormat,D3DPOOL_DEFAULT,&pD3DTexture,NULL);
    if(d3derr!=D3D_OK) // could be too big
    {
      REPORT_D3D_ERR("CreateTexture",d3derr);
      d3derr=pD3DDevice->CreateTexture(SurfaceWidth,SurfaceHeight,1,D3DUSAGE_DYNAMIC,
        d3dpp.BackBufferFormat,D3DPOOL_DEFAULT,&pD3DTexture,NULL);
    }
  }
  TRACE_LOG("Texture %dx%d ERR %d\n",TextureWidth,TextureHeight,d3derr);

  // fullscreen GUI, this may work with fake fullscreen, or true fullscreen
  // on older OS
  if(FullScreen && OPTION_FULLSCREEN_GUI && d3derr==D3D_OK)
  {
    d3derr=pD3DDevice->SetDialogBoxMode(TRUE);
    if(d3derr!=D3D_OK)
    {
      REPORT_D3D_ERR("SetDialogBoxMode",d3derr);
    }
  }

  // Create sprite
  if(d3derr==D3D_OK)
    d3derr=D3DSpriteInit();

  // Misc.
  if(d3derr==D3D_OK && StatusInfo.MessageIndex==TStatusInfo::BLIT_ERROR)
    StatusInfo.MessageIndex=TStatusInfo::MESSAGE_NONE;
  if(d3derr==D3D_OK)
  {
    draw_init_resdependent();
    palette_prepare(true);
  }
  if(d3derr!=D3D_OK)
  {
    TRACE("%s %d\n","BLIT ERROR",d3derr);
    if(StatusInfo.MessageIndex!=TStatusInfo::BLIT_ERROR)
    {
      StatusInfo.MessageIndex=TStatusInfo::BLIT_ERROR;
      UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
    }
#ifdef SSE_STATS
    StatsStatic.nBlitError++;
#endif
    REPORT_D3D_ERR("CreateSurfaces",d3derr);
    FullScreen=0;
  }

#if defined(SSE_VID_2SCREENS)
  if(d3derr==D3D_OK && FullScreen)
  {
    // Update window in absolute coordinates (depend on which screen we're on)
    SetWindowPos(StemWin,HWND_TOPMOST,rcMonitor.left,rcMonitor.top,
      SurfaceWidth,SurfaceHeight,SWP_FRAMECHANGED);
  }
#endif

  return d3derr;

}


void TSteemDisplay::D3DDestroySurfaces() {
  TRACE_LOG("D3DDestroySurfaces S %p T %p\n",pD3DSprite,pD3DTexture);
  if(pD3D && pD3DDevice)
  {
    if(pD3DSprite)
    {
      //TRACE_LOG("destroy sprite\n");
      pD3DSprite->Release();
      pD3DSprite=NULL;
    }
    if(pD3DTexture)
    {
      if(draw_lock)
        draw_end();
      pD3DTexture->Release();
      pD3DTexture=NULL;
    }
  }
}

#undef LOGSECTION
#define LOGSECTION LOGSECTION_INIT


HRESULT TSteemDisplay::D3DInit() {
  if(pD3D)
    pD3D->Release();
  else
    SetNotifyInitText("D3D");
  pD3DDevice=NULL;
  // Create the D3D object - computer needs DirectX9
#if defined(SSE_VID_D3D_SWEETFX)
/*  Look for d3d9sweetfx.dll (d3d9.dll renamed).
    Get the pointer to the Direct3DCreate9 method.
    NOTE: no Direct3DCreate9Ex
    We do it that way so that the plugin can be put into a folder like the rest
    and so that the effects are not enabled by default, and the Scroll Lock key
    is not intercepted (SweetFX_settings.txt shouldn't exist!)
    Thereby we feature much requested CRT emulation with very little effort!
    Thx to CeeJay.dk and cgwg, Themaister, DOLLS, Boulotaur2024
*/
  if(!hD3Dhack)
    hD3Dhack=SteemLoadLibrary(SSE_SWEETFX_D3D_HACK);
  IDirect3D9 *(WINAPI *pDirect3DCreate9)(UINT SDKVersion)=NULL;
  if(hD3Dhack && OPTION_CRT_EMU)
  {
    pDirect3DCreate9=(IDirect3D9*(WINAPI*)(UINT SDKVersion)) // fake
      GetProcAddress(hD3Dhack,"Direct3DCreate9");
  }
  SSEConfig.CrtEmu=(pDirect3DCreate9!=NULL);
  if(!pDirect3DCreate9)
    pDirect3DCreate9=Direct3DCreate9; // true
  if((pD3D=pDirect3DCreate9(D3D_SDK_VERSION))==NULL)
#else
  if((pD3D=Direct3DCreate9(D3D_SDK_VERSION))==NULL)
#endif
  {
    TRACE_LOG("D3D9 Init Fail!\n");
    return E_FAIL; 
  }
  D3DDISPLAYMODE d3ddm;
  pD3D->GetAdapterDisplayMode(m_Adapter, &d3ddm);
  m_DisplayFormat=d3ddm.Format;
  // do it once, keeping result
#if defined(SSE_VID_2SCREENS)
  // Probe capacities of video card, starting with desktop mode, HW
  // http://en.wikibooks.org/wiki/DirectX/9.0/Direct3D/Initialization
  m_DeviceType=D3DDEVTYPE_HAL; // first suppose good hardware
  HRESULT d3derr=check_device_type(m_DeviceType,m_DisplayFormat);
  if(d3derr!=D3D_OK) // could be "alpha" in desktop format?
  {
    REPORT_D3D_ERR("check_device_type1",d3derr);
    d3derr=check_device_type(m_DeviceType,D3DFMT_X8R8G8B8); // try X8R8G8B8 format
    if(d3derr!=D3D_OK) // no HW abilities?
    {
      REPORT_D3D_ERR("check_device_type2",d3derr);
      m_DeviceType=D3DDEVTYPE_REF; // try software processing (slow)
      d3derr=check_device_type(m_DeviceType,m_DisplayFormat);
      TRACE_LOG("D3D: poor hardware detected, software rendering ERR %d\n",d3derr);
    }
  }
  //ASSERT(!d3derr);
  D3DCAPS9 caps;
  d3derr=pD3D->GetDeviceCaps(m_Adapter,m_DeviceType,&caps);
  TRACE_LOG("D3D %p DevCaps $%X HW quality %X intervals %X texture %dx%d err %d\n",
    pD3D,
    caps.DevCaps,caps.DevCaps&(D3DDEVCAPS_HWTRANSFORMANDLIGHT|D3DDEVCAPS_PUREDEVICE),
    caps.PresentationIntervals,caps.MaxTextureWidth,caps.MaxTextureHeight,d3derr);
  if(caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) 
  {
    TRACE_LOG("T&L ");
    m_vtx_proc=D3DCREATE_HARDWARE_VERTEXPROCESSING;
    if(caps.DevCaps & D3DDEVCAPS_PUREDEVICE) 
    {
      TRACE_LOG("Pure device ");
      m_vtx_proc|=D3DCREATE_PUREDEVICE;
    }
  }
  else 
  {
    TRACE_LOG("Software vertex ");
    m_vtx_proc=D3DCREATE_SOFTWARE_VERTEXPROCESSING;
  }
  TRACE_LOG("vtx_proc = $%X\n",m_vtx_proc);
#else // BCC D3D

  UINT Adapter=D3DADAPTER_DEFAULT;
  HDC hdc = GetDC(StemWin);
  WORD bitsperpixel= GetDeviceCaps(hdc, BITSPIXEL); // another D3D shortcoming
  ReleaseDC(StemWin, hdc);
  TRACE_LOG("Screen %dx%d %dHz format %d %dbit\n",d3ddm.Width,d3ddm.Height,d3ddm.RefreshRate,d3ddm.Format,bitsperpixel);
  // Probe capacities of video card, starting with desktop mode, HW
  // http://en.wikibooks.org/wiki/DirectX/9.0/Direct3D/Initialization
  D3DFORMAT checkDisplayFormat=m_DisplayFormat;
  m_DeviceType=D3DDEVTYPE_HAL;
  HRESULT d3derr=check_device_type(m_DeviceType,checkDisplayFormat);
  if(d3derr) // could be "alpha" in desktop format?
  {
    REPORT_D3D_ERR("check_device_type1",d3derr);
    checkDisplayFormat=D3DFMT_X8R8G8B8; // try X8R8G8B8 format
    d3derr=check_device_type(m_DeviceType,checkDisplayFormat);
    if(d3derr) // no HW abilities?
    {
      REPORT_D3D_ERR("check_device_type2",d3derr);
      D3DDEVTYPE DeviceType=D3DDEVTYPE_REF; // try software processing (slow)
      d3derr=check_device_type(DeviceType,checkDisplayFormat);
      TRACE_LOG("D3D: poor hardware detected, software rendering ERR %d\n",d3derr);
    }
  }
  //ASSERT(!d3derr);
  D3DCAPS9 caps;
  d3derr=pD3D->GetDeviceCaps(Adapter,m_DeviceType,&caps);
//  TRACE_LOG("DevCaps $%X HW quality %X err %d\n",caps.DevCaps,caps.DevCaps&(D3DDEVCAPS_HWTRANSFORMANDLIGHT|D3DDEVCAPS_PUREDEVICE),d3derr);
  TRACE_LOG("DevCaps $%X HW quality %X intervals %X err %d\n",caps.DevCaps,caps.DevCaps&(D3DDEVCAPS_HWTRANSFORMANDLIGHT|D3DDEVCAPS_PUREDEVICE),caps.PresentationIntervals,d3derr);
  if( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) {
    TRACE_LOG("T&L\n");
    m_vtx_proc = D3DCREATE_HARDWARE_VERTEXPROCESSING;
    if( caps.DevCaps & D3DDEVCAPS_PUREDEVICE ) {
      TRACE_LOG("Pure device\n");
      m_vtx_proc |= D3DCREATE_PUREDEVICE;
    }
  } else {
    TRACE_LOG("Software vertex\n");
    m_vtx_proc = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
  }
  TRACE_LOG("vtx_proc = $%X\n",m_vtx_proc);
  ASSERTB((bitsperpixel/8)==4);
#endif
  TRACE_LOG("D3D9 Init OK type %d flags $%X i%X\n",m_DeviceType,m_vtx_proc,caps.PresentationIntervals);
  return S_OK;
}

#undef LOGSECTION
#define LOGSECTION LOGSECTION_VIDEO_RENDERING


HRESULT TSteemDisplay::D3DLock() {
  HRESULT d3derr=E_FAIL;
#if defined(SSE_EMU_THREAD)
  if(SuspendRendering)
    return d3derr;
#endif
  BYTE former_msg=StatusInfo.MessageIndex;
  draw_mem=NULL;
  // Restore surfaces after event such as screen saver
  if(StatusInfo.MessageIndex==TStatusInfo::BLIT_ERROR)
  {
    TRACE_LOG("F%d Recreate surfaces in Lock\n",FRAME);
    d3derr=D3DCreateSurfaces(); 
  }
  if(pD3DDevice&&pD3DTexture)
  {
    D3DLOCKED_RECT LockedRect;
    d3derr=pD3DTexture->LockRect(0,&LockedRect,NULL,0);
    if(d3derr!=D3D_OK)
    {
      REPORT_D3D_ERR("LockRect",d3derr); // it happens at reset...
      if(runstate==RUNSTATE_RUNNING)
        StatusInfo.MessageIndex=TStatusInfo::BLIT_ERROR;
    }
    else
    {
      draw_line_length=LockedRect.Pitch;
      draw_mem=(draw_type*)LockedRect.pBits;
    }
  }
#ifdef SSE_GUI_STATUS_BAR
  if(former_msg!=StatusInfo.MessageIndex)
    UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
#endif
  return d3derr;
}


void TSteemDisplay::D3DRelease() {
  // called by void TSteemDisplay::Release()
  TRACE_LOG("D3DRelease D %p %s %p\n",pD3DDevice,"D3D",pD3D);
  D3DDestroySurfaces(); // destroys texture, sprite
  if(pD3D)
  {
    if(pD3DDevice)
    {
      pD3DDevice->Release();
      pD3DDevice=NULL;
    }
    pD3D->Release();
    pD3D=NULL;
    D3D9_OK=false;
  }
}


void TSteemDisplay::D3DUnlock() {
  HRESULT d3derr=E_FAIL;
#if defined(SSE_EMU_THREAD)
  if(!(SuspendRendering||VideoLock.blocked))
#endif
  {
    if(pD3DDevice&&pD3DTexture)
       d3derr=pD3DTexture->UnlockRect(0);
    if(d3derr!=D3D_OK)
    {
      REPORT_D3D_ERR("D3DUnlock",d3derr);
    }
  }
}


HRESULT TSteemDisplay::D3DSpriteInit() {
  HRESULT d3derr=E_FAIL;
  if(!pD3D||!pD3DDevice)
  {
    REPORT_D3D_ERR("SpriteInit",d3derr);
    return d3derr;
  }
  // TODO we could also return if no need to init same sprite
  if(pD3DSprite)
    pD3DSprite->Release(); //so we can init sprite anytime
  d3derr=D3DXCreateSprite(pD3DDevice,&pD3DSprite);
  if(d3derr!=D3D_OK)
  {
    REPORT_D3D_ERR("SpriteInit",d3derr);
    return d3derr;
  }
  Draw.MarshalParameters();
#if defined(SSE_ENABLE_TRACE_LOG)
  BYTE &size=Draw.Size;
  BYTE &stretch=Draw.Stretch;
#endif
  // we need DST rectangle
  RECT &dst=rcSprite;
  float sw,sh,tx,ty;
  LONG srcw=Draw.BltSrc.right-Draw.BltSrc.left;
  LONG srch=Draw.BltSrc.bottom-Draw.BltSrc.top;
  if(FullScreen)
  {
    dst=Disp.rcMonitor;
    OffsetRect(&dst,-dst.left,-dst.top); // 0,0
    if(OPTION_FULLSCREEN_AR) // compute correct aspect ratio
    {
      DWORD stx=STXPixels(); // argh!
      DWORD sty=STYPixels();
#ifndef NO_CRAZY_MONITOR
      if(extended_monitor && em_width && em_height)
      {
        stx=em_width;
        sty=em_height;
      }
#endif
      if(OPTION_ST_ASPECT_RATIO && screen_res<HIRES)
        sty=(int)((float)sty*ST_ASPECT_RATIO_DISTORTION); // "reserve" more pixels
      float staspect=(float)stx/sty; //1.6 without borders
      BOOL WideSurface=((float)SurfaceWidth>(float)SurfaceHeight*staspect);
      float w,wd,h,hd;
      if(WideSurface)
      {
        w=dst.bottom*staspect;
        wd=(dst.right-w)/2;
        dst.left+=(int)wd;
        dst.right-=(int)wd;
      }
      else // "4:3"
      {
        h=dst.right/staspect;
        hd=(dst.bottom-h)/2;
        dst.top+=(int)hd;
        dst.bottom-=(int)hd;
      }
    }
    // create sprite that allows to transform SRC rectangle into DST rectangle
    tx=(float)dst.left;
    ty=(float)dst.top;
    sw=(float)(dst.right-dst.left)/(float)(srcw);
    sh=(float)(dst.bottom-dst.top)/(float)(srch);

    //  Trade-off, we sacrifice some screen space to have better proportions.
    if(OPTION_FULLSCREEN_AR==2) //crisp
    {
      sw=(float)(int)sw;
      sh=(float)(int)sh;
      tx=(Disp.SurfaceWidth-srcw*sw)/2;
      ty=(Disp.SurfaceHeight-srch*sh)/2;
    }
  }
  else //!FullScreen
  {
    dst=Draw.BltDst; // BltDst computed in StemWin
    tx=0.0;
    ty=0.0;
    sw=(float)(dst.right-dst.left)/(float)(srcw);
    sh=(float)(dst.bottom-dst.top)/(float)(srch);
    OffsetRect(&dst,-dst.left,-dst.top);
  }
  TRACE_LOG("Sprite %dx%d -> %dx%d sw %f tx %f sh %f ty %f\n",
      srcw,srch,dst.right-dst.left,dst.bottom-dst.top,sw,tx,sh,ty);
  D3DMATRIX matrix= {
    sw,              0.0f,            0.0f,            0.0f,
    0.0f,            sh,              0.0f,            0.0f,
    0.0f,            0.0f,            1.0f,            0.0f,
    tx,              ty,              0.0f,            1.0f
  };

  static D3DMATRIX oldMatrix;
  if(sw==oldMatrix._11&&sh==oldMatrix._22&&tx==oldMatrix._41&&ty==oldMatrix._42)
  {
    TRACE_LOG("Skip sprite SetTransform\n");
  }
  else
    memcpy(&oldMatrix,&matrix,sizeof(D3DMATRIX));

  d3derr=pD3DSprite->SetTransform((D3DXMATRIX*)&matrix);
  if(d3derr!=D3D_OK)
  {
    REPORT_D3D_ERR("SetTransform",d3derr);
  }
#ifndef SSE_LEAN_AND_MEAN
  if(pD3DDevice)
#endif
  pD3DDevice->Clear(0,0,D3DCLEAR_TARGET,0,0,0);
#if defined(SSE_ENABLE_TRACE_LOG)
  TRACE_LOG("%s R%d size %d aspect %d resize %d stretch %d scanlines %d ST aspect %d\n","D3D",
    screen_res,(FullScreen?SSEConfig.FullScreenSize:size),OPTION_FULLSCREEN_AR,
    ResChangeResize,stretch,OPTION_SCANLINES,OPTION_ST_ASPECT_RATIO);
#endif
  return d3derr;
}


void TSteemDisplay::D3DUpdateWH(UINT display_mode) {
  if(!pD3D)
    return;
  D3DDISPLAYMODE d3ddm;
  pD3D->GetAdapterDisplayMode(m_Adapter, &d3ddm);
  D3DDISPLAYMODE Mode; 
  pD3D->EnumAdapterModes(m_Adapter,d3ddm.Format,display_mode,&Mode);
  D3DFsW=Mode.Width;
  D3DFsH=Mode.Height;
  TRACE_LOG("D3DUpdateWH mode %d w %d h %d\n",display_mode,D3DFsW,D3DFsH);
}

#endif//d3d


#if defined(SSE_VID_2SCREENS)

BOOL TSteemDisplay::CheckCurrentMonitorConfig(HWND win) {
  BOOL bChanged=FALSE;
  if(win==NULL) //default
    win=StemWin;
  // Get Windows handle to monitor. This function requires Windows 2000.
  HMONITOR hCurrentMonitor=MonitorFromWindow(win,MONITOR_DEFAULTTOPRIMARY);
  // Get and memorize monitor's Windows rectangle
  MONITORINFO myMonitorInfo;
  myMonitorInfo.cbSize = sizeof(myMonitorInfo);
  GetMonitorInfo(hCurrentMonitor, &myMonitorInfo);
  rcMonitor = myMonitorInfo.rcMonitor;
#if defined(SSE_VID_D3D)
  if(pD3D)
  {
    // Get the current desktop display info
    if(win!=StemWin)
      return bChanged;
    // Determine current display
    UINT n_monitors=pD3D->GetAdapterCount();
    for(UINT i=0;i<n_monitors;i++)
    {
      HMONITOR that_monitor_handle=pD3D->GetAdapterMonitor(i);
      if(that_monitor_handle==hCurrentMonitor)
      {
        if(i!=m_Adapter)
        {
          TRACE_LOG("Change D3D adapter to %d\n",i);
          m_Adapter=i; // D3DCreateSurfaces will call us back...
          bChanged=TRUE;
          // Classy interface, change mode (2 max) and update fullscreen page
          UINT buf=oldD3DMode;
          oldD3DMode=D3DMode;
          D3DMode=buf;
          if(OptionBox.Handle && OptionBox.Page==3) 
          {
            OptionBox.DestroyCurrentPage();
            OptionBox.CreatePage(OptionBox.Page);
          }
        }
      }
    }

    D3DDISPLAYMODE d3ddm;
    pD3D->GetAdapterDisplayMode(m_Adapter, &d3ddm);
    m_DisplayFormat=d3ddm.Format;
    Freq=d3ddm.RefreshRate; // record display frequency (eg 60)
    monitor_width=d3ddm.Width;
    monitor_height=d3ddm.Height;
#if defined(SSE_VID_2SCREENS)
    if(bChanged)
    {
      TRACE_LOG("Monitor %d %dx%d %dHz\n",m_Adapter,monitor_width,monitor_height,Freq);
      GuiSM.Update(true); // true: don't recurse here (unimportant?)
      REFRESH_STATUS_BAR;
    }
#endif
  }//pD3D
#endif
#if defined(SSE_VID_DD)
  if(Disp.DDObj)
    Disp.DDObj->GetMonitorFrequency(&Freq);
#endif
  return bChanged;
}

#endif

#endif//WIN32


bool TSteemDisplay::BorderPossible() { // should we restrict?
  return (rcMonitor.right-rcMonitor.left>640); // is it up-to-date?
}

#ifdef UNIX

bool TSteemDisplay::CheckDisplayMode(DWORD red_mask,DWORD green_mask,DWORD blue_mask)
{
  bool Valid=0;
  {
    rgb32_bluestart_bit=0;
    Valid=(blue_mask==0x0000ff && green_mask==0x00ff00 && red_mask==0xff0000);
    if (!Valid){
      if (blue_mask==0x0000ff00 && green_mask==0x00ff0000 && red_mask==0xff000000){
        Valid=true;
        rgb32_bluestart_bit=8;
      }
    }
  }
  if (!Valid){
    if (AlreadyWarnedOfBadMode==0){
    	EasyStr Text=T("Sorry, your current screen mode is not supported by Steem.");
      {
      	Text+="\n\n";
        Text+=T("If you want you can e-mail us with the below text and we'll consider adding support for it:")+"\n\n";

        EasyStr Bin;
        for (int n=0;n<BytesPerPixel*8;n++){
          char c='0';
          if ((red_mask >> n) & 1) c='R';
          if ((green_mask >> n) & 1) c='G';
          if ((blue_mask >> n) & 1) c='B';
          Bin.Insert(c,0);
        }
        Text+=Bin;
      }
      MessageBox(0,Text,T("Display Error"),MB_ICONINFORMATION);
    	AlreadyWarnedOfBadMode=true;
    }
    return 0;
  }
  return true;
}


bool TSteemDisplay::InitX() 
{
  if (XD==NULL) return 0;
  TRACE_LOG("SteemDisplay::InitX()\n");
  Release();

  int Scr=XDefaultScreen(XD);
  int w=640,h=480;
  if (Disp.BorderPossible()){
    w=640+4* (SideBorderSize); // 768 or 800 or 832
    h=400+2*(TopBorderSize+BottomBorderSize)+MENUHEIGHT;
  }
#if defined(SSE_VID_SIZE4)
  //if(SSEConfig.Size4)
  if(DISPLAY_SIZE>2)
  {
    TRACE_LOG("X display x2\n");
    w*=2; h*=2;
  }
#endif    
  if(extended_monitor){
    w=GetScreenWidth();
    h=GetScreenHeight();
  }
  int Depth=XDefaultDepth(XD,Scr);
  char *ImgMem=(char*)malloc(w*h*BytesPerPixel);
  X_Img=XCreateImage(XD,XDefaultVisual(XD,Scr),
                      Depth,ZPixmap,0,ImgMem,
                      w,h,BytesPerPixel*8,0);
  if (X_Img){
    if (CheckDisplayMode(X_Img->red_mask,X_Img->green_mask,X_Img->blue_mask)==0){
      Release();
      return 0;
    }
  }else{
    free(ImgMem);
    MessageBox(0,T("Couldn't create XImage."),T("Display Error"),MB_ICONINFORMATION);
    return 0;
  }

  SurfaceWidth=w;
  SurfaceHeight=h;
  draw_init_resdependent();
  palette_prepare(true);

  return true;
}
//---------------------------------------------------------------------------
#ifndef NO_SHM
_XFUNCPROTOBEGIN
int XShmGetEventBase(
#if NeedFunctionPrototypes
Display *
#endif
);
_XFUNCPROTOEND
#endif

bool TSteemDisplay::InitXSHM()
{
// SS SHM = shared memory; this is the general case, more performant, can be disabled 
#ifdef NO_SHM
  return 0;
#else
  TRACE_LOG("SteemDisplay::InitXSHM()\n");
  if (XD==NULL) return 0;

  Release();

  if (XShmQueryExtension(XD)==0){
    MessageBox(0,T("MIT shared memory extension not available."),T("SHM Error"),MB_ICONINFORMATION | MB_OK);
    return 0;
  }

  int Scr=XDefaultScreen(XD);
  int w=640,h=480;
  if (BorderPossible()){
    w=640+4* (SideBorderSize);
    h=400+2*(TopBorderSize+BottomBorderSize)+MENUHEIGHT;
  }
#if defined(SSE_VID_SIZE4)
  if(DISPLAY_SIZE>2)
  {
    TRACE_LOG("XSHM display x2\n");
    w*=2; h*=2;
  }
#endif  
  if(extended_monitor){
    w=GetScreenWidth();
    h=GetScreenHeight();
  }
  X_Img=XShmCreateImage(XD,XDefaultVisual(XD,Scr),
                 XDefaultDepth(XD,Scr),ZPixmap,NULL,&XSHM_Info,w,h);
  if (X_Img==NULL){
    MessageBox(0,T("Couldn't create shared memory XImage."),T("SHM Error"),MB_ICONINFORMATION);
    Release();return 0;
  }
  TRACE_LOG("XSHM %dx%d\n",w,h);
  update_CanUse_400(w,h);
  XSHM_Info.shmid=shmget(IPC_PRIVATE,X_Img->bytes_per_line*X_Img->height,IPC_CREAT | 0777);
  if (XSHM_Info.shmid==-1){
    MessageBox(0,T("Couldn't allocate shared memory."),T("SHM Error"),MB_ICONINFORMATION);
    Release();return 0;
  }

  XSHM_Info.shmaddr=(char*)shmat(XSHM_Info.shmid,0,0);
  if (XSHM_Info.shmaddr==(char*)-1){
    MessageBox(0,T("Couldn't attach shared memory."),T("SHM Error"),MB_ICONINFORMATION);
    Release();return 0;
  }
  X_Img->data=XSHM_Info.shmaddr;

  XSHM_Info.readOnly=0;
  if (XShmAttach(XD,&XSHM_Info)==0){
    MessageBox(0,T("The X server couldn't attach the shared memory."),T("SHM Error"),MB_ICONINFORMATION);
    Release();return 0;
  }
  XSHM_Attached=true;

  SHMCompletion=XShmGetEventBase(XD)+ShmCompletion;
//	SHMCompletion=65; //it is for us!
  if (CheckDisplayMode(X_Img->red_mask,X_Img->green_mask,X_Img->blue_mask)==0){
    Release();return 0;
  }

//  printf(EasyStr("Bytes per pixel=")+BytesPerPixel+"  Depth="+XDefaultDepth(XD,Scr)+"\n");
  TRACE("Bytes per pixel=%d Depth=%d\n",BytesPerPixel,XDefaultDepth(XD,Scr));
  SurfaceWidth=w;
  SurfaceHeight=h;
  draw_init_resdependent();
  palette_prepare(true);

  return true;
#endif
}

//---------------------------------------------------------------------------
void TSteemDisplay::Surround()
{
  if (FullScreen) return;

  XWindowAttributes wa;
  XGetWindowAttributes(XD,StemWin,&wa);

  int w=wa.width,h=wa.height-(MENUHEIGHT);

  int sw=draw_blit_source_rect.right;
  int sh=draw_blit_source_rect.bottom;
  int dx=(w-(sw+4))/2;
  int dy=(h-(sh+4))/2;
  int fx1=dx,fy1=dy,fx2=dx+sw+4,fy2=dy+sh+4;
  XSetForeground(XD,DispGC,BkCol);

  int bh=dy;
  if (h & 1) bh++;
  if (dy>0){ //draw grey border top and bottom
    XFillRectangle(XD,StemWin,DispGC,0,MENUHEIGHT,w,dy);
  }else{
    dy=0;
    fy1=0;
    fy2=h;
  }
  if (bh>0) XFillRectangle(XD,StemWin,DispGC,0,dy+sh+(MENUHEIGHT+4),w,bh);

  int rw=dx;
  if (w & 1) rw++;
  if (dx>0){ //draw grey border left and right
    XFillRectangle(XD,StemWin,DispGC,0,dy+(MENUHEIGHT),dx,sh+4);
  }else{
    fx1=0;
    fx2=w;
  }
  if (rw>0) XFillRectangle(XD,StemWin,DispGC,dx+sw+4,dy+(MENUHEIGHT),rw,sh+4);

  fy1+=MENUHEIGHT;fy2+=MENUHEIGHT;
  XSetForeground(XD,DispGC,BlackCol);
  XDrawLine(XD,StemWin,DispGC,fx1+1,fy1+1,fx2-1,fy1+1);
  XDrawLine(XD,StemWin,DispGC,fx1+1,fy1+2,fx1+1,fy2-1);
  XSetForeground(XD,DispGC,BorderDarkCol);
  XDrawLine(XD,StemWin,DispGC,fx1,fy1,fx2-1,fy1);
  XDrawLine(XD,StemWin,DispGC,fx1,fy1,fx1,fy2-1);
  XSetForeground(XD,DispGC,WhiteCol);
  XDrawLine(XD,StemWin,DispGC,fx1+1,fy2-1,fx2-1,fy2-1);
  XDrawLine(XD,StemWin,DispGC,fx2-1,fy1+1,fx2-1,fy2-1);
  XSetForeground(XD,DispGC,BorderLightCol);
  XDrawLine(XD,StemWin,DispGC,fx1+2,fy2-2,fx2-2,fy2-2);
  XDrawLine(XD,StemWin,DispGC,fx2-2,fy1+2,fx2-2,fy2-2);
}


//---------------------------------------------------------------------------
#ifndef NO_XVIDMODE
int TSteemDisplay::XVM_WinProc(void*,Window Win,XEvent *Ev)
{
#ifndef NO_SHM
  if (Ev->type==Disp.SHMCompletion){
    Disp.asynchronous_blit_in_progress=false;
    return PEEKED_MESSAGE;
  }
#endif
  switch (Ev->type){
    case Expose:
#if SSE_VERSION>=370
      draw_grille_black=MAX((int)draw_grille_black,50);
#else
      draw_grille_black=MAX(draw_grille_black,50);
#endif
      break;
    case ButtonPress: // For MMB
    case ButtonRelease:
    case KeyPress:
    case KeyRelease:
      return StemWinProc(NULL,StemWin,Ev);
    case FocusOut:
      runstate=RUNSTATE_STOPPING;
      break;
  }
  return PEEKED_MESSAGE;
}
#endif//!NO_XVIDMODE
//---------------------------------------------------------------------------

#endif//UNIX


void TSteemDisplay::UpdateSurfaces(int const cw,int const ch) {
  // cw,ch: window client size
  TRACE_LOG("UpdateSurfaces(%d,%d)\n",cw,ch);
  Draw.cw=cw,Draw.ch=ch;
  Draw.MarshalParameters();
  DWORD newSize=cw*12+ch*13+Draw.Idx*3+(Draw.Stretch*16)+Draw.VSync*17+Draw.res*18;
  if(newSize!=SizeHash && cw>=HOR_PIXELS_LO && ch>=VER_PIXELS_LO)
  { 
    TRACE_LOG("Recreate surfaces\n");
    switch(Method) {
#ifdef SSE_VID_DD
    case DISPMETHOD_DD:
      DDCreateSurfaces();
      break;
#endif
#ifdef SSE_VID_D3D
    case DISPMETHOD_D3D:
      if(FullScreen&&!OPTION_FAKE_FULLSCREEN)// apparently when in fullscreen, can't reset/create surfaces?
        D3DSpriteInit();
      else
        D3DCreateSurfaces();
      break;
#endif
#ifdef WIN32
    case DISPMETHOD_GDI:
      InitGDI();
      break;
#endif
#ifdef UNIX
    case DISPMETHOD_X:
    case DISPMETHOD_XSHM:
      ScreenChange();
      break;
#endif
    default:
      BREAKPOINT(UpdateSurfaces);
    }//sw
    SizeHash=newSize;
  }
}


#if defined(SSE_VID_LS)

// Load a picture and display it on Steem's main window
HRESULT TSteemDisplay::LoadScreenShot(char* path) {
  HRESULT hr=(HRESULT)-1;
#if defined(SSE421R2)
  static EasyStr sPersist;
  if(path==NULL)
    path=sPersist.Text;
  else
    sPersist=path;
#endif
  switch(Method) {
#if defined(SSE_VID_D3D)
  case DISPMETHOD_D3D:
  {
    // .bmp, .dds, .dib, .hdr, .jpg, .pfm, .png, .ppm, and .tga. NOT .gif
    IDirect3DSurface9* BackBuff=NULL;
    hr=pD3DTexture->GetSurfaceLevel(0,&BackBuff);
    if(hr==D3D_OK)
    {
#if defined(SSE_VID_BORDERS) && defined(SSE421R1)
      RECT rc=Draw.BltSrc;
      OffsetRect(&rc,-rc.left,0); // adjust for Large border hack
      hr=D3DXLoadSurfaceFromFile(BackBuff,NULL,&rc,path,NULL,D3DX_DEFAULT,0,NULL);
#else
      hr=D3DXLoadSurfaceFromFile(BackBuff,NULL,&Draw.BltSrc,path,NULL,D3DX_DEFAULT,0,NULL);
#endif
      BackBuff->Release();
      draw_blit(); // as soon as it's blit, backbuffer can be lost
    }
    break;
  }
#endif
  // other methods: TODO
  }//sw
  return hr;
}

#endif//#if defined(SSE_VID_LS)

#undef LOGSECTION
