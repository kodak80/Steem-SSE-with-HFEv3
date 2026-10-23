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
  int n,size;
  THfeV3Bits():bit(NULL),weak(NULL),n(0),size(0) {}
  ~THfeV3Bits() { free(bit); free(weak); }
  bool Add(BYTE b,BYTE w) {
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
      size=new_size;
    }
    bit[n]=b;
    weak[n]=w;
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
  Init();
}


TImageHFE::~TImageHFE() {
  Close();
}


void TImageHFE::Close() {
  if(fCurrentImage)
  {
    TRACE_LOG("HFE %s image\n",FloppyDisk[Id].WrittenTo?"save and close":"close");
    if(IsV3 && V3Tracks && ImageData && FloppyDisk[Id].WrittenTo)
    {
      // re-encode written tracks into the v3 stream before saving
      for(int tr=0;tr<NUM_TRACKS;tr++)
        if(V3Tracks[tr*2].Dirty || V3Tracks[tr*2+1].Dirty)
          EncodeV3Track((BYTE)tr);
    }
    if(ImageData && FloppyDisk[Id].WrittenTo)
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
  int cycles=0;
  us_to_next_flux=0;
  do
  {
    cycles+=(256/16);//TODO
    us_to_next_flux+=2;
    WORD mfm=GetImageWord(Position);
    DWORD index=15-(BitPosition%16); // most significant bit read first
    bit=(mfm>>index)&1;
    IncBitPosition();
  } while(bit==0);
  return cycles;
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
}


void TImageHFE::FreeV3Tracks() {
  if(V3Tracks && file_header)
  {
    for(int i=0;i<NUM_TRACKS*2;i++)
    {
      free(V3Tracks[i].Data);
      free(V3Tracks[i].Weak);
    }
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
        // zero cells take the weak state of the input at their centre
        for(int j=1;j<n && ok;j++)
          ok=bits.Add(0,gap.At((DWORD)((2*j-1)*nominal/2)));
        if(ok)
          ok=bits.Add(1,weak);
        acc=0;
        gap.n=0;
      }
    }
    skip=0;
  }
  // remaining cells after the last transition
  int n_end=(int)((acc+nominal/2)/nominal);
  for(int j=1;j<=n_end && ok && bits.n<max_bits;j++)
    ok=bits.Add(0,gap.At((DWORD)((2*j-1)*nominal/2)));
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
  if(!t.Data || !t.Weak)
  {
    free(t.Data);
    free(t.Weak);
    t.Data=t.Weak=NULL;
    t.nWords=0;
    return false;
  }
  // rotate so that the index is at the start of the array, Steem's index
  int rotate=(index_bit>0 && index_bit<total) ? index_bit : 0;
  bool any_weak=unformatted;
  for(int i=0;i<t.nWords*16;i++)
  {
    BYTE b,w;
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
      if(t.Data || DecodeV3Track(side,track,t))
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
    ImageData=(BYTE*)malloc(IMAGE_SIZE);
    //ASSERT(ImageData);
    if(ImageData)
    {
      FREAD(ImageData,1,IMAGE_SIZE,fCurrentImage); // read all in memory (2MB OK)

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
