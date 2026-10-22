#pragma once
struct TFloppyDisk { WORD TrackBytes; BYTE current_side,current_track; bool WrittenTo,ReadOnly; WORD current_byte; };
extern TFloppyDisk FloppyDisk[3];
struct TImageMfm {
  TImageMfm(){Id=0;fCurrentImage=NULL;Position=0;LowLevel=false;}
  virtual void Close()=0;
  virtual WORD GetMfmData(WORD position);
  virtual bool LoadTrack(BYTE side,BYTE track,bool reload=false)=0;
  virtual void SetMfmData(WORD position,WORD mfm_data);
  virtual void IncPosition();
  virtual WORD GetImageWord(WORD position)=0;
  virtual int GetNextTransition(WORD&){return 0;}
  virtual void SetImageWord(WORD position,WORD mfm_data)=0;
  FILE *fCurrentImage; WORD Position; BYTE Id; DWORD BitPosition; bool LowLevel;
  virtual void IncBitPosition()=0;
};
struct TFloppyDrive { TImageMfm *MfmManager; };
extern TFloppyDrive FloppyDrive[3];
struct TFdcMock { struct {WORD encoded;} Mfm; };
extern TFdcMock Fdc;
