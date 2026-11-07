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

DOMAIN: Disk image
FILE: disk_hfe.cpp
CONDITION: SSE_DISK_HFE must be defined
DESCRIPTION: The HFE interface is based on the STW interface, so that
integration in Steem (disk manager, FDC commands...) is straightforward.
HFE (Rev.1.1) is a lot like STW, except the bits of each MFM word are reversed
and data of each side is intertwined, which is the way HxC hardware works.

HFEv3 (signature "HXCHFEV3") is also supported. It has the same header and
track layout as v1, but the track stream may contain byte-aligned opcodes,
recognised by the low nibble of the raw byte being 0xF (bits are stored in
reverse order, so opcode $F1 is stored as $8F, etc.):
  $0F NOP       no bitcells
  $8F INDEX     index pulse position
  $4F BITRATE   +1 byte: new cell length in 1/36 us units (72 = 2us = DD)
  $CF SKIPBITS  +1 byte: skip the first 0-7 bits of the following byte
  $2F RAND      a byte of random (weak) bits
This is the format written by the HxC software and read by FlashFloppy and
HxC hardware; FlashFloppy's src/image/hfe.c was used as reference.
Because Steem's MFM managers expect a fixed array of 16-bit MFM words, each v3
side/track is decoded once into such an array (resampled to the header bit
rate where BITRATE opcodes change it, and rotated so that the index is at
word 0), with a parallel mask of weak bits that are re-randomised on every
read. Tracks written by the emulator are re-encoded on save.

Writes never change the original image file. Like the GTi Gotek interface,
the image is saved as Game.sav.hfe beside Game.hfe, and when Game.hfe is
inserted again the save copy is loaded instead. Delete Game.sav.hfe to go back
to the original. A .sav.hfe inserted directly is updated in place.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#if defined(SSE_DISK_HFE)

#include <debug.h>
#include <computer.h>
#if defined(SSE_FLOPPYBRIDGE)
#include <floppybridge_drive.h>
#endif


#define IMAGE_SIZE image_size //member variable
#define NUM_SIDES 2
#define NUM_TRACKS file_header->number_of_track
#define MFM_SIDE_BLOCK_SIZE 128
#define TRACKBYTES FloppyDisk[Id].TrackBytes

#define LOGSECTION LOGSECTION_IMAGE_INFO

// HFEv3 opcodes, as stored in the file (bit-reversed)
#define HFEV3_OPCODE_MASK 0x0F
#define HFEV3_NOP         0x0F
#define HFEV3_INDEX       0x8F
#define HFEV3_BITRATE     0x4F
#define HFEV3_SKIPBITS    0xCF
#define HFEV3_RAND        0x2F
#define HFEV3_DEFAULT_CELL 72 // 1/36 us units -> 2us, 250kbps DD
#ifndef ATARIST_DD_FLOPPYMODE
#define ATARIST_DD_FLOPPYMODE 0x02
#endif

static BYTE ReverseByte(BYTE b) {
  BYTE r=0;
  for(int i=0;i<8;i++)
  {
    r=(BYTE)((r<<1)|(b&1));
    b>>=1;
  }
  return r;
}


static WORD RandomWord() {
  // RAND_MAX may be only 0x7FFF (MSVC)
  return (WORD)(((rand()&0xFF)<<8)|(rand()&0xFF));
}


// weak/not weak changes within the gap between two flux transitions
struct THfeV3Gap {
  DWORD start[16]; // time (since last transition) where the state changes
  BYTE weak[16];
  int n;
  THfeV3Gap():n(0) {}
  void Note(DWORD t,BYTE w) {
    if(!n || (weak[n-1]!=w && n<16))
    {
      start[n]=t;
      weak[n]=w;
      n++;
    }
  }
  BYTE At(DWORD t) const {
    BYTE w=0;
    for(int i=0;i<n && start[i]<=t;i++)
      w=weak[i];
    return w;
  }
};


// growable bit buffer used while decoding a v3 track (one byte per bit)
struct THfeV3Bits {
  BYTE *bit,*weak;
  WORD *cyc; // per-cell flux time, 4.4 fixed point cycles (256 = nominal)
  int n,size;
  THfeV3Bits():bit(NULL),weak(NULL),cyc(NULL),n(0),size(0) {}
  ~THfeV3Bits() { free(bit); free(weak); free(cyc); }
  bool Add(BYTE b,BYTE w,WORD c) {
    if(n==size)
    {
      int new_size=size ? size*2 : 65536;
      BYTE *nb=(BYTE*)realloc(bit,new_size);
      if(!nb)
        return false;
      bit=nb;
      BYTE *nw=(BYTE*)realloc(weak,new_size);
      if(!nw)
        return false;
      weak=nw;
      WORD *nc=(WORD*)realloc(cyc,new_size*sizeof(WORD));
      if(!nc)
        return false;
      cyc=nc;
      size=new_size;
    }
    bit[n]=b;
    weak[n]=w;
    cyc[n]=c;
    n++;
    return true;
  }
};


static bool MakeSavePath(const char *path,char *save_path,size_t size) {
/*  Game.hfe -> Game.sav.hfe, as the GTi names its saves. Returns false when
    path is already a save copy (Game.sav.hfe, Game.sav.XXXX.hfe), or if the
    name is too long.
*/
  const char *name=path;
  for(const char *p=path;*p;p++)
    if(*p=='/'||*p=='\\'||*p==':')
      name=p+1;
  const char *dot=strrchr(name,'.');
  size_t base_len=dot ? (size_t)(dot-path) : strlen(path);
  // already a save copy?
  for(const char *p=name;p+4<=path+base_len;p++)
    if(p[0]=='.' && (p[1]=='s'||p[1]=='S') && (p[2]=='a'||p[2]=='A')
      && (p[3]=='v'||p[3]=='V') && (p+4==path+base_len || p[4]=='.'))
      return false;
  if(strlen(path)+4>=size)
    return false;
  memcpy(save_path,path,base_len);
  strcpy(save_path+base_len,".sav");
  strcat(save_path,path+base_len); // extension, if any
  return true;
}


TImageHFE::TImageHFE() {
#if defined(SSE_FLOPPYBRIDGE)
  BridgeError[0]=0;
#endif
  Init();
}


TImageHFE::~TImageHFE() {
  Close();
}


void TImageHFE::Close() {
  bool bridge=false; // a real drive: nothing to save
#if defined(SSE_FLOPPYBRIDGE)
  if(Bridge)
  {
    TRACE_LOG("FloppyBridge drive close\n");
    BridgeFlush(NULL); // what the ST wrote and isn't saved yet
    FloppyBridgeClose(Bridge);
    Bridge=NULL;
    bridge=true;
  }
#endif
  if(fCurrentImage)
  {
    TRACE_LOG("HFE %s image\n",FloppyDisk[Id].WrittenTo?"save and close":"close");
    if(IsV3 && V3Tracks && ImageData && FloppyDisk[Id].WrittenTo && !bridge)
    {
      // re-encode written tracks into the v3 stream before saving
      for(int tr=0;tr<NUM_TRACKS;tr++)
        if(V3Tracks[tr*2].Dirty || V3Tracks[tr*2+1].Dirty)
          EncodeV3Track((BYTE)tr);
    }
    if(ImageData && FloppyDisk[Id].WrittenTo && !bridge)
    {
      if(SavePath[0]) // first write: the original stays untouched
      {
        FILE *fp=fopen(SavePath,"wb");
        if(fp)
        {
          FWRITE(ImageData,1,IMAGE_SIZE,fp);
          fclose(fp);
        }
        TRACE_LOG("HFE save copy %s %s\n",SavePath,fp?"written":"can't be created");
      }
      else // the save copy itself: update it
      {
        FSEEK(fCurrentImage,0,SEEK_SET); // rewind
        FWRITE(ImageData,1,IMAGE_SIZE,fCurrentImage); //save
      }
    }
    fclose(fCurrentImage);
    FreeV3Tracks(); // before ImageData, which holds the track count
    free(ImageData);
  }
  FreeV3Tracks();
  Init();
}


bool TImageHFE::Create(char *path) {
  // utility called by Disk manager
  bool ok=false;
  Close();
  fCurrentImage=fopen(path,"wb+"); // create new image
  if(fCurrentImage)
  {
    // copy overhead of a HFE file
#if 1 && defined(SSE_FILES_IN_RC)
    // first look for the file, if it isn't there, use internal resource
    EasyStr filename=RunDir+SLASH+SSE_PLUGIN_DIR1+SLASH+DISK_HFE_BOOT_FILENAME;
    FILE *fp=fopen(filename.Text,"rb"); // open boot
    if(!fp)
    {
      filename=RunDir+SLASH+SSE_PLUGIN_DIR2+SLASH+DISK_HFE_BOOT_FILENAME;
      fp=fopen(filename.Text,"rb"); // open boot
    }
    if(!fp)
    {
      filename=RunDir+SLASH+DISK_HFE_BOOT_FILENAME;
      fp=fopen(filename.Text,"rb"); // open boot
    }
    if(fp)
    {
      //ASSERT(GetFileLength(fp)==1024);
      for(int i=0;i<1024;i++)
        FPUTC(FGETC(fp),fCurrentImage);
      for(int i=0;i<84*SECTOR_SIZE*49;i++)
        FPUTC(rand()&0xff,fCurrentImage); //random data (unformatted)
      ok=true;
      fclose(fp);
    }
    else 
    {
#ifdef WIN32
      HRSRC rc=FindResource(hInstance,MAKEINTRESOURCE(IDR_HFEBOOT),RT_RCDATA);
      ASSERTB(rc);
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
          BYTE *pdata=(BYTE*)Get_HFE_boot_bin();
          BYTE *pdata_end=(BYTE*)Get_HFE_boot_bin_end();
          size_t size=pdata_end-pdata;
#endif
          if(pdata && size==1024) //1Kb
          {
            FWRITE((void*)pdata,size,1,fCurrentImage);
            ok=true;
          }
        }
      }//if(rc)
    }
    for(int i=0;i<84*SECTOR_SIZE*49;i++)
      FPUTC(rand()&0xff,fCurrentImage); //random data (unformatted)
    Close();
#else//#if defined(SSE_FILES_IN_RC)
    EasyStr filename=RunDir+SLASH+"plugins"+SLASH+DISK_HFE_BOOT_FILENAME;
    FILE *fp=fopen(filename.Text,"rb"); // open boot
    if(!fp)
    {
      filename=RunDir+SLASH+DISK_HFE_BOOT_FILENAME;
      fp=fopen(filename.Text,"rb"); // open boot
    }
    if(fp)
    {
      ASSERTB(GetFileLength(fp)==1024);
      for(int i=0;i<1024;i++) // copy boot: we use a file to reduce overhead
        FPUTC(FGETC(fp),fCurrentImage);  //in Steem itself
      for(int i=0;i<84*SECTOR_SIZE*49;i++)
        FPUTC(rand()&0xff,fCurrentImage); //random data (unformatted)
      ok=true;
      fclose(fp);
      Close(); 
    }
#endif//#if defined(SSE_FILES_IN_RC)
  }
  TRACE_LOG("HFE create %s %s\n",CHECKPATH(path),ok?"OK":"failed");  
  return ok;
}


int TImageHFE::ComputeIndex(WORD position) {
/*
"  A track data is a table containing the bit stream of a track of the floppy disk. A track can
contain a MFM / FM / GCR or a custom encoding.
The track is divided in block of 512bytes and each block contains a part of the Side 0
track and a part of the Side 1 track:
Figure 1 : A track data
The bits transmitting order to the FDC is :
Bit 0-> Bit 1-> Bit 2-> Bit 3-> Bit 4-> Bit 5-> Bit 6-> Bit 7->(next byte)"

->  HFE format is confusing with interleaved sides:
    256 (MFM) bytes for side 0 then 256 bytes for side 1...
    Each MFM word (clock + data) is reversed : Bit 0-> ... -> Bit 15
*/

  BYTE &current_side=FloppyDisk[Id].current_side;
  int block=(position/MFM_SIDE_BLOCK_SIZE)*2+current_side;
  int index=(position%MFM_SIDE_BLOCK_SIZE)+block*MFM_SIDE_BLOCK_SIZE;
  //ASSERT( index>=0 && index< TRACKBYTES*2+128*current_side );
  return index;
}


WORD TImageHFE::GetImageWord(WORD position) {
  if(IsV3)
  {
    WORD mfm_data=0;
    if(V3Current && V3Current->Data && V3Current->nWords)
    {
      int index=position%V3Current->nWords;
      mfm_data=V3Current->Data[index];
      if(V3Current->Weak && V3Current->Weak[index])
      {
        WORD mask=V3Current->Weak[index];
        mfm_data=(WORD)((mfm_data&~mask)|(RandomWord()&mask));
      }
    }
    return mfm_data;
  }
  int index=ComputeIndex(position);
  WORD mfm_data=0;
  if(TrackData)
  {
    WORD mirror_mfm=TrackData[index];
    mfm_data=MirrorMFM(mirror_mfm);
  }
  return mfm_data;
}


WORD TImageHFE::GetMfmData(WORD position) {
  WORD mfm_data=TImageMfm::GetMfmData(position);
  if(!LowLevel)
    IncPosition();
  return mfm_data;
}


int TImageHFE::GetNextTransition(WORD& us_to_next_flux) {
  DWORD bit=0;
  us_to_next_flux=0;
  // For a variable-density v3 track, Cyc gives the real flux time of each cell
  // (4.4 fixed-point 8MHz cycles, 256 = nominal) so the DPLL sees the true
  // timing. Nominal tracks (Cyc==NULL) and v1 images keep the 16cy/2us path.
  const WORD *cyc=(IsV3 && V3Current)?V3Current->Cyc:NULL;
  int ncells=(cyc && V3Current->nWords)?V3Current->nWords*16:0;
  DWORD fp=0; // flux time since last transition, 4.4 fixed-point cycles
  do
  {
    fp+=(cyc && ncells)?cyc[BitPosition%ncells]:256;
    WORD mfm=GetImageWord(Position);
    DWORD index=15-(BitPosition%16); // most significant bit read first
    bit=(mfm>>index)&1;
    IncBitPosition();
  } while(bit==0);
  int cycles=(int)((fp+8)>>4);
  us_to_next_flux=(WORD)((cycles+4)/8);
  return cycles;
}


int TImageHFE::VarByteCycles(WORD byteindex,int nominal_cpb) {
  // Only variable-density v3 tracks override the standard byte timing. The
  // byte's 16 cells carry a real duration in Cyc (4.4 fixed point, 256 =
  // nominal cell); scaled against a nominal byte (16*256) it gives the real
  // CPU-cycle length of that byte, so a slower (lower-density) sector - e.g.
  // the Rob Northen Copylock sector 6 - reads measurably slower, as on a
  // real drive. Nominal tracks return -1 and keep the fixed byte timing.
  if(!IsV3 || !V3Current || !V3Current->Cyc || !V3Current->nWords)
    return -1;
  int w=byteindex%V3Current->nWords;
  DWORD fp=0;
  for(int k=0;k<16;k++)
    fp+=V3Current->Cyc[w*16+k];
  // nominal byte = 16 cells * 256 = 4096 -> nominal_cpb cycles
  return (int)(((long long)nominal_cpb*fp+2048)/4096);
}


void TImageHFE::IncBitPosition() {
  BitPosition++;
  if(BitPosition/16>Position)
  {
    IncPosition();
    if(!Position)
      BitPosition=0;
  }
}


void TImageHFE::Init() {
  fCurrentImage=NULL;
  ImageData=NULL;
  TrackData=NULL;
  file_header=NULL;
  track_header=NULL;
  V3Tracks=NULL;
  V3Current=NULL;
  IsV3=false;
  SavePath[0]=0;
  IMAGE_SIZE=0;
#if defined(SSE_FLOPPYBRIDGE)
  Bridge=NULL;
  BridgeDiskIn=false;
  // BridgeError is kept: the disk manager reports it after a failed Open
#endif
}


void TImageHFE::FreeV3TrackData(THfeV3Track &t) {
  free(t.Data);
  free(t.Weak);
  free(t.Cyc);
  t.Data=t.Weak=t.Cyc=NULL;
  t.nWords=0;
  t.Dirty=t.Unread=t.Formatted=false;
}


void TImageHFE::FreeV3Tracks() {
  if(V3Tracks && file_header)
  {
    for(int i=0;i<NUM_TRACKS*2;i++)
      FreeV3TrackData(V3Tracks[i]);
  }
  free(V3Tracks);
  V3Tracks=NULL;
  V3Current=NULL;
}


bool TImageHFE::DecodeV3Track(BYTE side,BYTE track,THfeV3Track &t) {
/*  Decode one side of a v3 track into a plain array of MFM words.
    Bit timing is converted to the nominal cell length given by the header
    bit rate: each flux transition (1 bit) is placed at the nearest nominal
    cell. When there's no BITRATE opcode this is an exact copy of the bits.
*/
  pictrack &th=track_header[track];
  int base=th.offset*SECTOR_SIZE;
  int side_len=th.track_len/2; // bytes of stream data for this side
  // don't read past the end of the file
  while(side_len>0 && base+((side_len-1)/256)*512+side*256+(side_len-1)%256
    >=IMAGE_SIZE)
    side_len--;
  int nominal=HFEV3_DEFAULT_CELL;
  if(file_header->bitRate>=50 && file_header->bitRate<=2000)
    nominal=(18000+file_header->bitRate/2)/file_header->bitRate;
  int cell=nominal;
  DWORD acc=0; // time since last flux transition, 1/36 us units
  THfeV3Gap gap; // where weak bits are between two transitions
  int index_bit=-1;
  int skip=0;
  const int max_bits=0xFFFF*16;
  THfeV3Bits bits;
  bool ok=true;
  bool density=false; // true if any cell's flux time differs from nominal
  for(int i=0;i<side_len && ok && bits.n<max_bits;)
  {
    BYTE raw=ImageData[base+(i/256)*512+side*256+i%256];
    i++;
    BYTE weak=0;
    if((raw&HFEV3_OPCODE_MASK)==HFEV3_OPCODE_MASK)
    {
      switch(raw) {
      case HFEV3_BITRATE:
        if(i<side_len)
        {
          BYTE x=ReverseByte(ImageData[base+(i/256)*512+side*256+i%256]);
          i++;
          if(x)
            cell=x;
        }
        skip=0;
        continue;
      case HFEV3_SKIPBITS:
        if(i<side_len)
        {
          skip=ReverseByte(ImageData[base+(i/256)*512+side*256+i%256])&7;
          i++;
        }
        continue;
      case HFEV3_INDEX:
        if(index_bit<0)
          index_bit=bits.n+(int)((acc+nominal/2)/nominal);
        skip=0;
        continue;
      case HFEV3_RAND:
        raw=(BYTE)rand();
        weak=1;
        break;
      case HFEV3_NOP:
      default: // unknown opcodes are ignored, like FlashFloppy does
        skip=0;
        continue;
      }
    }
    for(int k=skip;k<8 && ok;k++)
    {
      gap.Note(acc,weak);
      acc+=cell;
      if((raw>>k)&1) // bit 0 is transmitted first
      {
        int n=(int)((acc+nominal/2)/nominal);
        if(n<1)
          n=1;
        // Real flux time of this gap, spread over the n nominal cells, in
        // 4.4 fixed-point 8MHz cycles (one nominal cell = 72 units = 256).
        // The flux/DPLL reader uses this so density is honoured; the word
        // array keeps nominal spacing for the word path and writes.
        DWORD gap_fp=(acc*256)/nominal;
        WORD per0=(WORD)(n>0?gap_fp/n:gap_fp);
        WORD per1=(WORD)(gap_fp-(DWORD)(n-1)*per0); // last cell takes remainder
        if(gap_fp!=(DWORD)n*256)
          density=true;
        // zero cells take the weak state of the input at their centre
        for(int j=1;j<n && ok;j++)
          ok=bits.Add(0,gap.At((DWORD)((2*j-1)*nominal/2)),per0);
        if(ok)
          ok=bits.Add(1,weak,per1);
        acc=0;
        gap.n=0;
      }
    }
    skip=0;
  }
  // remaining cells after the last transition
  int n_end=(int)((acc+nominal/2)/nominal);
  DWORD end_fp=n_end>0?(acc*256)/nominal:0;
  WORD end_per=(WORD)(n_end>0?end_fp/n_end:256);
  if(n_end>0 && end_fp!=(DWORD)n_end*256)
    density=true;
  for(int j=1;j<=n_end && ok && bits.n<max_bits;j++)
    ok=bits.Add(0,gap.At((DWORD)((2*j-1)*nominal/2)),end_per);
  if(!ok)
  {
    TRACE_LOG("HFEv3 out of memory decoding side %d track %d\n",side,track);
    return false;
  }
  int total=bits.n;
  if(total>max_bits)
    total=max_bits;
  bool unformatted=(total<16);
  if(unformatted)
    total=DISK_BYTES_PER_TRACK*16; // no data: random bits
  t.nWords=(WORD)((total+15)/16);
  t.Data=(WORD*)calloc(t.nWords,sizeof(WORD));
  t.Weak=(WORD*)calloc(t.nWords,sizeof(WORD));
  t.Cyc=NULL;
  // Only variable-density tracks carry a timing array (one WORD per cell);
  // nominal tracks leave Cyc NULL and keep the fast nominal flux path.
  if(density && !unformatted)
    t.Cyc=(WORD*)calloc((size_t)t.nWords*16,sizeof(WORD));
  if(!t.Data || !t.Weak)
  {
    free(t.Data);
    free(t.Weak);
    free(t.Cyc);
    t.Data=t.Weak=t.Cyc=NULL;
    t.nWords=0;
    return false;
  }
  // rotate so that the index is at the start of the array, Steem's index
  int rotate=(index_bit>0 && index_bit<total) ? index_bit : 0;
  bool any_weak=unformatted;
  for(int i=0;i<t.nWords*16;i++)
  {
    BYTE b,w; WORD c=256;
    if(unformatted)
    {
      b=(BYTE)(rand()&1);
      w=1;
    }
    else if(i<total)
    {
      int j=(i+rotate)%total;
      b=bits.bit[j];
      w=bits.weak[j];
      c=bits.cyc[j];
    }
    else // pad the last word with MFM zeros ($AAAA)
    {
      b=(BYTE)((0xAAAA>>(15-i%16))&1);
      w=0;
    }
    if(b)
      t.Data[i/16]|=(WORD)(1<<(15-i%16));
    if(w)
    {
      t.Weak[i/16]|=(WORD)(1<<(15-i%16));
      any_weak=true;
    }
    if(t.Cyc)
      t.Cyc[i]=c ? c : 256;
  }
  if(!any_weak)
  {
    free(t.Weak);
    t.Weak=NULL;
  }
  t.Dirty=false;
  TRACE_LOG("HFEv3 decode side %d track %d: %d stream bytes -> %d MFM words, index at bit %d%s\n",
    side,track,side_len,t.nWords,index_bit,t.Weak?", weak bits":"");
  return true;
}


bool TImageHFE::MakeUnformattedTrack(THfeV3Track &t) {
  // a track of weak (random) bits, what an unformatted track reads as
  t.nWords=DISK_BYTES_PER_TRACK;
  t.Data=(WORD*)malloc(t.nWords*sizeof(WORD));
  t.Weak=(WORD*)malloc(t.nWords*sizeof(WORD));
  t.Cyc=NULL;
  t.Dirty=t.Formatted=false;
  if(!t.Data || !t.Weak)
  {
    free(t.Data);
    free(t.Weak);
    t.Data=t.Weak=NULL;
    t.nWords=0;
    return false;
  }
  for(int i=0;i<t.nWords;i++)
  {
    t.Data[i]=RandomWord();
    t.Weak[i]=0xFFFF;
  }
  return true;
}


bool TImageHFE::BitsToV3Track(const BYTE *mfm,const WORD *speed,
  const BYTE *weak,int nbits,THfeV3Track &t) {
/*  Store one revolution read from a real drive (bridge mode) as a decoded v3
    track. The bits are already cells (FloppyBridge's PLL did that) and start
    at the index, like a decoded v3 track. speed[] is the measured time of
    each byte, 1000 = nominal; it becomes the per-cell flux time in Cyc
    (256 = nominal) so the density-aware reader and the byte pacing see the
    disk's real timing, as for a v3 image with BITRATE opcodes. When every
    byte reads at exactly nominal speed Cyc stays NULL (nominal path).
    weak[] marks bits that read differently on each revolution: they become
    the track's weak bits (random each time they're read, as for a v3 image
    with RAND opcodes); Weak stays NULL if there are none.
*/
  t.Data=t.Weak=t.Cyc=NULL;
  t.nWords=0;
  t.Dirty=t.Unread=t.Formatted=false;
  if(!mfm || nbits<16)
    return false;
  if(nbits>0xFFFF*16)
    nbits=0xFFFF*16;
  int nbytes=(nbits+7)/8;
  bool density=false;
  if(speed)
    for(int i=0;i<nbytes && !density;i++)
      if(speed[i]!=1000)
        density=true;
  t.nWords=(WORD)((nbits+15)/16);
  t.Data=(WORD*)calloc(t.nWords,sizeof(WORD));
  if(density)
    t.Cyc=(WORD*)malloc((size_t)t.nWords*16*sizeof(WORD));
  if(!t.Data || (density && !t.Cyc))
  {
    free(t.Data);
    free(t.Cyc);
    t.Data=t.Cyc=NULL;
    t.nWords=0;
    return false;
  }
  for(int i=0;i<t.nWords*16;i++)
  {
    BYTE b;
    WORD c=256;
    if(i<nbits)
    {
      b=(BYTE)((mfm[i/8]>>(7-i%8))&1);
      if(density)
      {
        DWORD sp=speed[i/8];
        c=(WORD)((sp*256+500)/1000);
        if(!c)
          c=256;
      }
    }
    else // pad the last word with MFM zeros ($AAAA)
      b=(BYTE)((0xAAAA>>(15-i%16))&1);
    if(b)
      t.Data[i/16]|=(WORD)(1<<(15-i%16));
    if(t.Cyc)
      t.Cyc[i]=c;
  }
  if(weak)
  {
    for(int i=0;i<nbits;i++)
      if((weak[i/8]>>(7-i%8))&1)
      {
        if(!t.Weak)
          t.Weak=(WORD*)calloc(t.nWords,sizeof(WORD));
        if(!t.Weak)
          break;
        t.Weak[i/16]|=(WORD)(1<<(15-i%16));
      }
  }
  return true;
}


#if defined(SSE_FLOPPYBRIDGE)

bool TImageHFE::OpenBridge(const char *descriptor,const char *path) {
  // the descriptor file stays open, like an image, but nothing is written;
  // the log goes next to it: Greaseweazle.hfe -> Greaseweazle.log
  char log_path[MAX_PATH+8];
  log_path[0]=0;
  if(path && strlen(path)<MAX_PATH)
  {
    strcpy(log_path,path);
    char *dot=strrchr(log_path,'.');
    char *slash=strrchr(log_path,'\\');
    if(dot && (!slash || dot>slash))
      *dot=0;
    strcat(log_path,".log");
  }
  Bridge=FloppyBridgeOpen(descriptor,log_path,BridgeError,sizeof(BridgeError));
  if(!Bridge)
  {
    TRACE_LOG("FloppyBridge open failed: %s\n",BridgeError);
    return false;
  }
  int cylinders=FloppyBridgeMaxCylinder(Bridge)+1; // highest one, or count+1
  if(cylinders<80)
    cylinders=80;
  if(cylinders>84)
    cylinders=84;
  memset(&BridgeHeader,0,sizeof(BridgeHeader));
  memcpy(BridgeHeader.HEADERSIGNATURE,"HXCHFEV3",8);
  BridgeHeader.number_of_track=(BYTE)cylinders;
  BridgeHeader.number_of_side=2;
  BridgeHeader.bitRate=250;
  BridgeHeader.floppyRPM=300;
  BridgeHeader.floppyinterfacemode=ATARIST_DD_FLOPPYMODE;
  BridgeHeader.write_protected=1;
  file_header=&BridgeHeader;
  track_header=NULL;
  IsV3=true;
  SavePath[0]=0;
  V3Tracks=(THfeV3Track*)calloc(NUM_TRACKS*2,sizeof(THfeV3Track));
  BridgeDiskIn=FloppyBridgeDiskInDrive(Bridge);
  BridgePollCount=0;
  BridgeVerify=false;
  BridgeLastWP=FloppyBridgeWriteProtected(Bridge);
  BridgeLastSide=BridgeLastTrack=-1;
  TRACE_LOG("FloppyBridge drive open, %d cylinders, disk %s\n",cylinders,
    BridgeDiskIn?"in":"out");
  return V3Tracks!=NULL;
}


void TImageHFE::BridgeMotorOff() {
  if(Bridge)
  {
    BridgeFlush(NULL); // the ST has finished: save what it wrote
    FloppyBridgeMotorOff(Bridge);
    BridgeVerify=true; // the disk may be swapped while the drive is idle
  }
}


bool TImageHFE::BridgeCanWrite() {
  return Bridge && FloppyBridgeCanWrite(Bridge);
}


void TImageHFE::BridgePoll() {
/*  Called every VBL (run.cpp). A real disk taken out or put in is passed on
    to the ST, so the disk can be swapped without re-inserting
    Greaseweazle.hfe. Checked every 8 VBLs, these are flags in the DLL.
    - FloppyBridge's disk-in flag, from the drive's disk-change line (pin 34
      of a PC drive). Without that line it always says "disk in".
    - The write-protect sensor: it changes as a disk goes in or out, which
      is also how the ST itself sees a disk change. Then the next read
      checks the disk (BridgeVerifyDisk).
    - And after the drive has been idle (motor off), the next read checks
      the disk too.
*/
  if(!Bridge || (++BridgePollCount&7))
    return;
  bool disk_in=FloppyBridgeDiskInDrive(Bridge);
  if(disk_in!=BridgeDiskIn)
    BridgeDiskChanged(disk_in);
  bool wp=FloppyBridgeWriteProtected(Bridge);
  if(wp!=BridgeLastWP)
  {
    BridgeLastWP=wp;
    FloppyBridgeLogMessage(Bridge,wp ? "write-protect sensor on (disk out, or protected)"
      : "write-protect sensor off (disk in)");
    BridgeVerify=true;
    if(Id<2)
    {
      floppy_mediach[Id]=FLOPPY_MEDIACH_VBL; // as the ST would see it
      FloppyDisk[Id].ReadOnly=!BridgeCanWrite();
      FloppyDisk[Id].WriteProtect=FloppyDisk[Id].ReadOnly;
    }
  }
}


void TImageHFE::BridgeVerifyDisk(BYTE side,BYTE track,THfeV3Track &t) {
/*  The drive was idle (or the write-protect sensor changed): before using
    what was read, check that it's still the same disk. FloppyBridge's
    cache is cleared, and a track we have is read again from the disk and
    compared, sector by sector. Another disk: everything read is dropped
    and the ST gets a disk change. Costs a revolution at spin-up, which
    happens then anyway.
*/
  BridgeVerify=false;
  THfeV3Track *ref=NULL;
  int rs=side,rt=track;
  if(t.Data && !t.Unread && !t.Dirty)
    ref=&t;
  else if(BridgeLastTrack>=0)
  {
    THfeV3Track &l=V3Tracks[BridgeLastTrack*2+BridgeLastSide];
    if(l.Data && !l.Unread && !l.Dirty)
    {
      ref=&l;
      rs=BridgeLastSide;
      rt=BridgeLastTrack;
    }
  }
  FloppyBridgeForget(Bridge);
  if(!ref)
  {
    // nothing to compare with: don't trust what was read before
    for(int i=0;i<NUM_TRACKS*2;i++)
      if(!V3Tracks[i].Dirty)
        FreeV3TrackData(V3Tracks[i]);
    return;
  }
  const int max_bytes=0xFFFF*2;
  BYTE *mfm=(BYTE*)malloc(max_bytes);
  WORD *speed=(WORD*)malloc(max_bytes*sizeof(WORD));
  BYTE *weak=(BYTE*)malloc(max_bytes);
  BYTE *old=(BYTE*)malloc(ref->nWords*2);
  int nbits=(mfm && speed && weak && old)
    ? FloppyBridgeReadTrack(Bridge,rs,rt,mfm,speed,max_bytes,weak) : 0;
  if(nbits<16)
  {
    // no disk (or unreadable): drop what we have, check again next time
    FloppyBridgeLogMessage(Bridge,"disk check: no data, the disk may be out");
    BridgeDiskChanged(BridgeDiskIn);
    BridgeVerify=true;
  }
  else
  {
    for(int k=0;k<ref->nWords;k++)
    {
      old[k*2]=(BYTE)(ref->Data[k]>>8);
      old[k*2+1]=(BYTE)(ref->Data[k]&0xFF);
    }
    if(!FloppyBridgeSameTrack(old,ref->nWords*16,mfm,nbits))
    {
      char msg[120];
      sprintf(msg,"disk check (track %d side %d): ANOTHER DISK - disk change for the ST",
        rt,rs);
      FloppyBridgeLogMessage(Bridge,msg);
      BridgeDiskChanged(true);
      BitsToV3Track(mfm,speed,weak,nbits,V3Tracks[rt*2+rs]); // keep the new read
    }
    else
      FloppyBridgeLogMessage(Bridge,"disk check: same disk");
  }
  free(mfm);
  free(speed);
  free(weak);
  free(old);
}


void TImageHFE::BridgeDiskChanged(bool disk_in) {
  // what was read belongs to the other disk; tracks the ST wrote to that
  // weren't saved yet are lost (the disk is gone)
  int lost=0;
  for(int i=0;i<NUM_TRACKS*2;i++)
  {
    if(V3Tracks[i].Dirty)
      lost++;
    FreeV3TrackData(V3Tracks[i]);
  }
  if(lost)
  {
    char msg[160];
    sprintf(msg,"disk removed before %d track(s) written by the ST could be saved: they're lost",
      lost);
    FloppyBridgeLogMessage(Bridge,msg);
  }
  BridgeDiskIn=disk_in;
  TRACE_LOG("FloppyBridge disk %s\n",disk_in?"inserted":"removed");
  if(Id<2)
  {
    // the ST sees a disk change as the write-protect signal changing while
    // the disk passes the sensor: same as inserting a disk in Steem
    floppy_mediach[Id]=FLOPPY_MEDIACH_VBL;
    if(disk_in)
    { // this disk may have its write-protect tab in another position
      FloppyDisk[Id].ReadOnly=!BridgeCanWrite();
      FloppyDisk[Id].WriteProtect=FloppyDisk[Id].ReadOnly;
    }
  }
}


void TImageHFE::BridgeFlush(const THfeV3Track *keep) {
/*  Tracks the ST wrote to are written to the real disk when the head leaves
    them, when the emulated motor stops, when Steem stops and when the disk
    is ejected. The whole track is rewritten from the index, at the normal
    density (see FloppyBridgeWriteTrack, which refuses tracks it couldn't
    read). The ST keeps seeing its changes either way.
*/
  if(!Bridge || !V3Tracks)
    return;
  for(int i=0;i<NUM_TRACKS*2;i++)
  {
    THfeV3Track &w=V3Tracks[i];
    if(&w==keep || !w.Dirty || !w.Data || !w.nWords)
      continue;
    int flags=(w.Formatted ? FLOPPYBRIDGE_WRITE_FORMATTED : 0)
      |(w.Cyc ? FLOPPYBRIDGE_WRITE_VARIABLE : 0)
      |(w.Unread ? FLOPPYBRIDGE_WRITE_UNREAD : 0);
    BYTE *mfm=(BYTE*)malloc(w.nWords*2);
    if(!mfm)
      continue;
    for(int k=0;k<w.nWords;k++)
    {
      mfm[k*2]=(BYTE)(w.Data[k]>>8);
      mfm[k*2+1]=(BYTE)(w.Data[k]&0xFF);
    }
    int r=FloppyBridgeWriteTrack(Bridge,i%2,i/2,mfm,w.nWords*16,flags);
    free(mfm);
    TRACE_LOG("FloppyBridge write side %d track %d: %d\n",i%2,i/2,r);
    w.Dirty=false; // tried: don't try again (it's in the log)
    if(r==1)
    {
      w.Unread=false;
      if(w.Cyc)
      { // rewritten at the normal density
        free(w.Cyc);
        w.Cyc=NULL;
      }
      w.Formatted=false;
    }
  }
}


bool TImageHFE::LoadBridgeTrack(BYTE side,BYTE track,THfeV3Track &t) {
  // the head moved to another track: save the ones the ST wrote to
  BridgeFlush(&t);
  bool disk_in=FloppyBridgeDiskInDrive(Bridge);
  if(disk_in!=BridgeDiskIn)
    BridgeDiskChanged(disk_in); // disk taken out or put in
  if(BridgeVerify)
    BridgeVerifyDisk(side,track,t); // same disk as before the drive was idle?
  BridgeLastSide=side;
  BridgeLastTrack=track;
  if(t.Data && (t.Dirty || !t.Unread))
    return true; // already read from this disk, or written by the ST
  FreeV3TrackData(t);
  // with the motor off FloppyBridge may not know yet that a disk is in:
  // always ask the drive (it gives up quickly if there's really none)
  {
    const int max_bytes=0xFFFF*2; // as much as a track can hold
    BYTE *mfm=(BYTE*)malloc(max_bytes);
    WORD *speed=(WORD*)malloc(max_bytes*sizeof(WORD));
    BYTE *weak=(BYTE*)malloc(max_bytes);
    int nbits=(mfm && speed && weak)
      ? FloppyBridgeReadTrack(Bridge,side,track,mfm,speed,max_bytes,weak) : 0;
    bool ok=(nbits>=16 && BitsToV3Track(mfm,speed,weak,nbits,t));
    free(mfm);
    free(speed);
    free(weak);
    TRACE_LOG("FloppyBridge read side %d track %d: %d bits%s%s\n",side,track,
      nbits,(ok && t.Cyc)?", variable density":"",(ok && t.Weak)?", weak bits":"");
    if(ok)
      return true;
  }
  // no disk, or nothing could be read: random bits, try again next time
  bool ok=MakeUnformattedTrack(t);
  t.Unread=true;
  return ok;
}

#endif//#if defined(SSE_FLOPPYBRIDGE)


bool TImageHFE::EncodeV3Track(BYTE track) {
/*  Write decoded sides back into the v3 stream of a track. A written side is
    stored as plain data (an INDEX opcode first, weak bytes as RAND opcodes),
    an unwritten side is kept as it was. Both sides are padded with NOPs to
    the same length. The track can't grow beyond the blocks it already has
    in the file.
*/
  pictrack &th=track_header[track];
  int base=th.offset*SECTOR_SIZE;
  int blocks=(th.track_len+511)/512;
  while(blocks>0 && base+blocks*512>IMAGE_SIZE)
    blocks--;
  int cap=blocks*256; // bytes per side
  int orig_len=th.track_len/2;
  if(orig_len>cap)
    orig_len=cap;
  int len=orig_len;
  for(int s=0;s<NUM_SIDES;s++)
  {
    THfeV3Track &t=V3Tracks[track*2+s];
    if(t.Dirty && t.Data && 1+t.nWords*2>len)
      len=1+t.nWords*2;
  }
  if(len>cap)
  {
    TRACE_LOG("HFEv3 track %d too long (%d bytes), truncated to %d\n",track,len,cap);
    len=cap;
  }
  if(len<=0)
    return false;
  for(int s=0;s<NUM_SIDES;s++)
  {
    THfeV3Track &t=V3Tracks[track*2+s];
    bool written=(t.Dirty && t.Data);
    for(int i=(written?0:orig_len);i<len;i++)
    {
      BYTE raw=HFEV3_NOP;
      if(written && i==0)
        raw=HFEV3_INDEX;
      else if(written && i-1<t.nWords*2)
      {
        int w=(i-1)/2;
        int shift=((i-1)&1) ? 0 : 8; // high byte goes first
        BYTE data=(BYTE)(t.Data[w]>>shift);
        BYTE weak=t.Weak ? (BYTE)(t.Weak[w]>>shift) : 0;
        if(weak==0xFF)
          raw=HFEV3_RAND;
        else
        {
          raw=ReverseByte(data);
          // a run of 4 1s at the start of a byte would look like an opcode,
          // this can't happen in valid MFM
          if((raw&HFEV3_OPCODE_MASK)==HFEV3_OPCODE_MASK)
            raw^=2;
        }
      }
      ImageData[base+(i/256)*512+s*256+i%256]=raw;
    }
  }
  TRACE_LOG("HFEv3 encode track %d: %d -> %d bytes per side\n",track,th.track_len/2,len);
  th.track_len=(WORD)(len*2);
  return true;
}


bool TImageHFE::LoadTrack(BYTE side,BYTE track,bool) {
  //ASSERT(Id==0||Id==1);
  bool ok=false;
  if(IsV3)
  {
    if(file_header && side<NUM_SIDES && track<NUM_TRACKS && ImageData
      && V3Tracks)
    {
      THfeV3Track &t=V3Tracks[track*2+side];
#if defined(SSE_FLOPPYBRIDGE)
      if(Bridge ? LoadBridgeTrack(side,track,t)
        : (t.Data || DecodeV3Track(side,track,t)))
#else
      if(t.Data || DecodeV3Track(side,track,t))
#endif
      {
        V3Current=&t;
        TRACKBYTES=t.nWords;
        FloppyDisk[Id].current_side=side;
        FloppyDisk[Id].current_track=track;
        ok=true;
      }
    }
#if defined(SSE_ENABLE_TRACE_LOG)
    if(!ok)
      TRACE_LOG("HFEv3 can't load side %d track %d\n",side,track);
#endif
    return ok;
  }
  if(file_header && side<NUM_SIDES && track<NUM_TRACKS && ImageData)
  {
    int position= track_header[track].offset*SECTOR_SIZE;
    TRACKBYTES=track_header[track].track_len/4; // same for both sides
    //ASSERT(TRACKBYTES>=0 && TRACKBYTES<6500);
    //ASSERT(Id==DRIVE);
#if defined(SSE_ENABLE_TRACE_LOG)
    if(TrackData!=(WORD*)(ImageData+position)) //only once
      TRACE_LOG("HFE LoadTrack side %d track %d offset %d position %d len %d bytes %d\n", side,track,track_header[track].offset,position,track_header[track].track_len,TRACKBYTES);
#endif
    TrackData=(WORD*)(ImageData+position);
    FloppyDisk[Id].current_side=side;
    FloppyDisk[Id].current_track=track;
    ok=true;
  }
#if defined(SSE_ENABLE_TRACE_LOG)
  else
    TRACE_LOG("HFE can't load side %d track %d from %p\n",side,track,ImageData);
#endif
  return ok;
}


WORD TImageHFE::MirrorMFM(WORD mfm_word) {
/*  Because it's easier so for HxC hardware, bits are in the
    reverse order each word.  eg  $4489 reads $9122
    Needed some time to figure this out :)
*/
  WORD mirror_mfm=0;
  for(int i=0;i<16;i++)
  {
    int bit=mfm_word&1;
    mirror_mfm|=bit; // bit is 0 otherwise (init)
    mfm_word>>=1;
    if(i<15)
      mirror_mfm<<=1;
  }
  return mirror_mfm;
}


bool TImageHFE::Open(char *path) {
  bool ok=false;
  Close(); // make sure previous image is correctly closed
  // use the save copy if there is one, otherwise remember where it goes
  char save_path[sizeof(SavePath)];
  char *open_path=path;
  if(MakeSavePath(path,save_path,sizeof(save_path)))
  {
    FILE *fp=fopen(save_path,"rb");
    if(fp)
    {
      fclose(fp);
      open_path=save_path;
      TRACE_LOG("HFE %s: using save copy %s\n",path,save_path);
    }
    else
      strcpy(SavePath,save_path);
  }
  if(SavePath[0])
    fCurrentImage=fopen(open_path,"rb"); // the original is never written
  else
  {
    fCurrentImage=fopen(open_path,"rb+"); // try to open existing file
    if(!fCurrentImage) // maybe it's read-only
      fCurrentImage=fopen(open_path,"rb");
  }
  //ASSERT(fCurrentImage);
  if(fCurrentImage) // image exists
  {
    IMAGE_SIZE=(int)GetFileLength(fCurrentImage);
    //ASSERT(IMAGE_SIZE>0);
    ImageData=(BYTE*)malloc(IMAGE_SIZE+1);
    //ASSERT(ImageData);
    if(ImageData)
    {
      FREAD(ImageData,1,IMAGE_SIZE,fCurrentImage); // read all in memory (2MB OK)
      ImageData[IMAGE_SIZE]=0; // so a bridge descriptor is a C string
#if defined(SSE_FLOPPYBRIDGE)
      BridgeError[0]=0;
      char *text=(char*)ImageData;
      if(IMAGE_SIZE>=3 && !memcmp(text,"\xEF\xBB\xBF",3))
        text+=3; // UTF-8 BOM from a text editor
      if(IMAGE_SIZE>=(int)strlen(FLOPPYBRIDGE_SIGNATURE)+(int)(text-(char*)ImageData)
        && !strncmp(text,FLOPPYBRIDGE_SIGNATURE,strlen(FLOPPYBRIDGE_SIGNATURE)))
        ok=OpenBridge(text,path);
      else
#endif
      {
      file_header=(picfileformatheader*)ImageData;
      if(IMAGE_SIZE>=(int)sizeof(picfileformatheader))
        IsV3=!strncmp("HXCHFEV3",(char*)file_header->HEADERSIGNATURE,8);
      if(IMAGE_SIZE>=(int)sizeof(picfileformatheader)
        && (IsV3||!strncmp("HXCPICFE",(char*)file_header->HEADERSIGNATURE,8))
        && file_header->track_list_offset*SECTOR_SIZE
        +NUM_TRACKS*(int)sizeof(pictrack)<=IMAGE_SIZE)  // it's HFE
      {
        if(IsV3)
          V3Tracks=(THfeV3Track*)calloc(NUM_TRACKS*2,sizeof(THfeV3Track));
#if defined(SSE_ENABLE_TRACE_LOG) //lots of info
        TRACE_LOG("Open HFE size %d v%d sides %d tracks %d encoding %X mode %X bitRate %d\n",
  IMAGE_SIZE,file_header->formatrevision,file_header->number_of_side,
  file_header->number_of_track,file_header->track_encoding,
  file_header->floppyinterfacemode,file_header->bitRate);
TRACE_LOG("RPM %d  WP %d WA %X offset %d step %X TR0/1 %X%X TR1/1 %X%X\n",
  file_header->floppyRPM,file_header->write_protected,file_header->write_allowed,
  file_header->track_list_offset,file_header->single_step,file_header->track0s0_altencoding,
  file_header->track0s0_encoding,file_header->track0s1_altencoding,file_header->track0s1_encoding);
#endif

        track_header=(pictrack*)(ImageData+file_header->track_list_offset*SECTOR_SIZE);
        ok=(!IsV3 || V3Tracks!=NULL);
      }
      }
    }
  }//if(fCurrentImage)
  if(!ok)
    Close();
  else
  {
    FloppyDrive[Id].MfmManager=this;
    // HFE tracks are a plain bit stream: sync marks and bytes needn't start on
    // a 16-bit word boundary (they rarely do in images made by HxC, Gotek,
    // GTi...), so the word-based reader never finds the sector IDs and TOS
    // keeps retrying the boot sector. The bit-level path (DPLL) handles any
    // alignment, like a real drive.
    LowLevel=true;
  }
  return ok;
}


void TImageHFE::SetImageWord(WORD position,WORD mfm_data) {
  if(IsV3)
  {
    if(V3Current && V3Current->Data && V3Current->nWords)
    {
#if defined(SSE_FLOPPYBRIDGE)
      // real drive: WRITE TRACK rewrites the whole track, so it can be
      // written even if it couldn't be read or is protected (formatting)
      if(Bridge && Fdc.CommandType()==3)
        V3Current->Formatted=true;
#endif
      int index=position%V3Current->nWords;
      V3Current->Data[index]=mfm_data;
      if(V3Current->Weak)
        V3Current->Weak[index]=0; // written bits are no longer weak
      V3Current->Dirty=true;
    }
    return;
  }
  int index=ComputeIndex(position);
  WORD mirror_mfm=MirrorMFM(mfm_data); // reverse bits
  if(TrackData)//&&index<nTrackBytes)
    TrackData[index]=mirror_mfm;
}


void TImageHFE::SetMfmData(WORD position,WORD mfm_data) {
/*  Write Sector: the bit-level reader has just found the sector ID and read
    the gap after it, so the head is exactly at BitPosition. Starting the
    write from the drive's byte counter instead (position) puts the data
    field a few bits off, since HFE syncs aren't word-aligned; that could
    overwrite the sector's own ID, and the next access to it failed with
    Record Not Found (Crazy Cars high scores).
*/
  if(LowLevel && position!=0xFFFF && Fdc.CommandType()==2)
    position=0xFFFF; // carry on from where the reader left the head
  TImageMfm::SetMfmData(position,mfm_data);
  if(!LowLevel)
  {
    SetImageWord(Position,mfm_data);
    Fdc.Mfm.encoded=0;
    IncPosition();
  }
}


#undef IMAGE_SIZE
#undef NUM_SIDES
#undef NUM_TRACKS
#undef MFM_SIDE_BLOCK_SIZE
#undef TRACKBYTES
#undef LOGSECTION

#endif//#if defined(SSE_DISK_HFE)
