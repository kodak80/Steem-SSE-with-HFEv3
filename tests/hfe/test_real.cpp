// Decode every sector of HxC-generated HFE v1/v3 images through TImageHFE
// and compare with the source .st image.
#include "pch.h"
#include "disk_hfe.h"
#include <vector>

int g_trace=0;
EasyStr RunDir(".");
TFloppyDisk FloppyDisk[3];
TFloppyDrive FloppyDrive[3];
TFdcMock Fdc;
TImageHFE ImageHFE[3];
long GetFileLength(FILE*f){long p=ftell(f);fseek(f,0,SEEK_END);long l=ftell(f);fseek(f,p,SEEK_SET);return l;}
WORD TImageMfm::GetMfmData(WORD p){if(p!=0xFFFF)Position=p%FloppyDisk[Id].TrackBytes;return GetImageWord(Position);}
void TImageMfm::IncPosition(){Position=(Position+1)%FloppyDisk[Id].TrackBytes;}
void TImageMfm::SetMfmData(WORD,WORD){FloppyDisk[Id].WrittenTo=true;}

// decode MFM bit stream (twice around, for sectors crossing index) into sectors
static int DecodeSectors(const std::vector<BYTE>&bits,int track,int side,const BYTE*st,int spt,int *bad){
  int n=(int)bits.size(),found=0; std::vector<BYTE> seen(spt+1,0);
  BYTE id_trk=0,id_sid=0,id_sec=0; bool have_id=false;
  unsigned sh=0;
  for(int i=0;i<n*2;i++){
    sh=(sh<<1)|bits[i%n];
    if((sh&0xFFFF)!=0x4489) continue;
    // read following bytes from i+1
    int p=i+1;
    std::vector<BYTE> by;
    // skip further A1s
    int nb=(have_id?520:8);
    for(int b=0;b<nb;b++){BYTE v=0;for(int k=0;k<8;k++){v=(v<<1)|bits[(p+1+2*k)%n];}by.push_back(v);p+=16;}
    if(by[0]==0xA1) continue; // more sync marks, next match handles
    if(by[0]==0xFE){id_trk=by[1];id_sid=by[2];id_sec=by[3];have_id=true;continue;}
    if(by[0]==0xFB && have_id){
      have_id=false;
      if(id_trk!=track||id_sid!=side||id_sec<1||id_sec>spt){(*bad)++;continue;}
      if(seen[id_sec]) continue;
      seen[id_sec]=1;found++;
      const BYTE*ref=st+((track*2+side)*spt+id_sec-1)*512;
      if(memcmp(&by[1],ref,512)) (*bad)++;
    }
  }
  return found;
}

static int Run(const char*hfe,const char*stpath,int spt){
  FILE*f=fopen(stpath,"rb");std::vector<BYTE> st(80*2*spt*512);fread(&st[0],1,st.size(),f);fclose(f);
  TImageHFE&im=ImageHFE[0];
  if(!im.Open((char*)hfe)){printf("%s: open failed\n",hfe);return 1;}
  int total=0,bad=0,minw=99999,maxw=0;
  for(int t=0;t<80;t++)for(int s=0;s<2;s++){
    if(!im.LoadTrack(s,t)){printf("load fail %d/%d\n",t,s);bad++;continue;}
    int nw=FloppyDisk[0].TrackBytes; if(nw<minw)minw=nw; if(nw>maxw)maxw=nw;
    std::vector<BYTE> bits;
    for(int w=0;w<nw;w++){WORD m=im.GetImageWord(w);for(int k=15;k>=0;k--)bits.push_back((m>>k)&1);}
    total+=DecodeSectors(bits,t,s,&st[0],spt,&bad);
  }
  printf("%-10s v3=%d sectors %d/%d bad %d, words/track %d-%d\n",hfe,(int)im.IsV3,total,80*2*spt,bad,minw,maxw);
  im.Close();
  return (total==80*2*spt && !bad)?0:1;
}

int main(int argc,char**argv){
  int r=0;
  for(int i=1;i+2<argc;i+=3) r|=Run(argv[i],argv[i+1],atoi(argv[i+2]));
  return r;
}
