// Unit tests for Steem SSE disk_hfe.cpp (HFE v1 + v3), run against mocks.
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
WORD TImageMfm::GetMfmData(WORD position){
  if(position!=0xFFFF) Position=position%FloppyDisk[Id].TrackBytes;
  return Fdc.Mfm.encoded=GetImageWord(Position);}
void TImageMfm::IncPosition(){Position=(Position+1)%FloppyDisk[Id].TrackBytes;}
void TImageMfm::SetMfmData(WORD position,WORD){
  FloppyDisk[Id].WrittenTo=true;
  if(position!=0xFFFF) Position=position%FloppyDisk[Id].TrackBytes;}

static int fails=0,checks=0;
#define CHECK(c,...) do{checks++; if(!(c)){fails++;printf("FAIL %s:%d: ",__FILE__,__LINE__);printf(__VA_ARGS__);printf("\n");}}while(0)

typedef std::vector<WORD> Words;
typedef std::vector<BYTE> Bytes;
typedef std::vector<BYTE> Bits;

static BYTE rev8(BYTE b){BYTE r=0;for(int i=0;i<8;i++){r=(r<<1)|(b&1);b>>=1;}return r;}

// MFM-encode random data bytes -> words (MSB first)
static Words RandomMfmTrack(int nbytes,unsigned seed){
  srand(seed); Words w; int last=0;
  for(int i=0;i<nbytes;i++){
    BYTE d=(BYTE)rand(); WORD m=0;
    for(int k=7;k>=0;k--){int b=(d>>k)&1;int c=(!b&&!last)?1:0;m=(m<<2)|(c<<1)|b;last=b;}
    w.push_back(m);
  }
  return w;
}
static Bits WordsToBits(const Words&w){Bits b;for(size_t i=0;i<w.size();i++)for(int k=15;k>=0;k--)b.push_back((w[i]>>k)&1);return b;}
// pack 8 transmitted bits (first bit first) into a raw HFE byte (bit0 first)
static BYTE RawByte(const Bits&b,size_t at){BYTE r=0;for(int k=0;k<8;k++)if(b[at+k])r|=1<<k;return r;}
static Bytes WordsToRaw(const Words&w){Bytes r;for(size_t i=0;i<w.size();i++){r.push_back(rev8(w[i]>>8));r.push_back(rev8(w[i]&0xFF));}return r;}

// build an HFE file: streams[track][side]
static void WriteHfe(const char*path,bool v3,const std::vector<Bytes>&s0,const std::vector<Bytes>&s1,int bitrate=250){
  int ntr=(int)s0.size();
  Bytes f(1024,0xFF);
  memcpy(&f[0],v3?"HXCHFEV3":"HXCPICFE",8);
  picfileformatheader*h=(picfileformatheader*)&f[0];
  h->formatrevision=0;h->number_of_track=ntr;h->number_of_side=2;h->track_encoding=0;
  h->bitRate=bitrate;h->floppyRPM=300;h->floppyinterfacemode=7;h->write_protected=0;
  h->track_list_offset=1;h->write_allowed=0xFF;h->single_step=0xFF;
  int block=2;
  for(int t=0;t<ntr;t++){
    size_t len=s0[t].size()>s1[t].size()?s0[t].size():s1[t].size();
    int blocks=(int)((len*2+511)/512);
    pictrack*th=(pictrack*)&f[512+t*4];
    th->offset=block;th->track_len=(WORD)(len*2);
    f.resize((block+blocks)*512,v3?0x0F:0x00);
    for(int s=0;s<2;s++){const Bytes&d=s?s1[t]:s0[t];
      for(size_t i=0;i<len;i++){BYTE v=i<d.size()?d[i]:(v3?0x0F:0xAA);
        f[block*512+(i/256)*512+s*256+i%256]=v;}}
    block+=blocks;
  }
  FILE*fp=fopen(path,"wb");fwrite(&f[0],1,f.size(),fp);fclose(fp);
}

static Words ReadTrack(TImageHFE&im,BYTE side,BYTE track,bool*ok=NULL){
  Words w; bool r=im.LoadTrack(side,track); if(ok)*ok=r; if(!r)return w;
  int n=FloppyDisk[0].TrackBytes;
  for(int i=0;i<n;i++) w.push_back(im.GetImageWord((WORD)i));
  return w;
}
static Bytes ReadFile(const char*path){Bytes b;FILE*f=fopen(path,"rb");if(!f)return b;int c;while((c=fgetc(f))!=EOF)b.push_back((BYTE)c);fclose(f);return b;}
static bool Exists(const char*path){FILE*f=fopen(path,"rb");if(f)fclose(f);return f!=NULL;}
static int Diff(const Words&a,const Words&b){if(a.size()!=b.size())return -2;for(size_t i=0;i<a.size();i++)if(a[i]!=b[i])return (int)i;return -1;}

// mock FloppyBridge drive (bridge mode)
struct TMockBridge {
  bool open_ok,disk_in;
  int max_cyl,reads,fail_reads,opens,closes;
  Bits bits;            // one revolution, first bit first
  std::vector<WORD> speed; // per byte, 1000 = nominal
} g_fb={true,true,81,0,0,0,0};
static int g_fb_handle;
void *FloppyBridgeOpen(const char*descriptor,char*error,int error_size){
  g_fb.opens++;
  if(!g_fb.open_ok){snprintf(error,error_size,"mock: no Greaseweazle");return NULL;}
  if(strncmp(descriptor,"FLOPPYBRIDGE",12)){snprintf(error,error_size,"bad descriptor");return NULL;}
  return &g_fb_handle;}
void FloppyBridgeClose(void*h){if(h==&g_fb_handle)g_fb.closes++;}
int FloppyBridgeMaxCylinder(void*){return g_fb.max_cyl;}
bool FloppyBridgeDiskInDrive(void*){return g_fb.disk_in;}
int FloppyBridgeReadTrack(void*,int side,int track,unsigned char*mfm,unsigned short*speed,int max_bytes){
  g_fb.reads++;
  if(g_fb.fail_reads>0){g_fb.fail_reads--;return 0;}
  int n=(int)g_fb.bits.size(); if(n>max_bytes*8) n=max_bytes*8;
  memset(mfm,0,(n+7)/8);
  for(int i=0;i<n;i++) if(g_fb.bits[i]) mfm[i/8]|=(unsigned char)(0x80>>(i%8));
  for(int i=0;i<(n+7)/8;i++) speed[i]=i<(int)g_fb.speed.size()?g_fb.speed[i]:1000;
  (void)side;(void)track;
  return n;}

int main(){
  const int NB=6250; // data bytes per side -> 6250 MFM words
  Words X=RandomMfmTrack(NB,1),Y=RandomMfmTrack(NB,2);
  TImageHFE &im=ImageHFE[0];

  // A. v1 regression
  {
    std::vector<Bytes> s0(2,WordsToRaw(X)),s1(2,WordsToRaw(Y));
    WriteHfe("t_v1.hfe",false,s0,s1);
    CHECK(im.Open((char*)"t_v1.hfe"),"v1 open");
    CHECK(!im.IsV3,"v1 not v3");
    Words a=ReadTrack(im,0,1),b=ReadTrack(im,1,1);
    CHECK(Diff(a,X)==-1,"v1 side0 diff %d",Diff(a,X));
    CHECK(Diff(b,Y)==-1,"v1 side1 diff %d",Diff(b,Y));
    im.Close();
  }

  // B. v3 plain with INDEX first and NOPs sprinkled
  {
    Bytes raw=WordsToRaw(X),st;st.push_back(0x8F);
    for(size_t i=0;i<raw.size();i++){if(i%97==5)st.push_back(0x0F);st.push_back(raw[i]);}
    std::vector<Bytes> s0(1,st),s1(1,WordsToRaw(Y));
    WriteHfe("t_v3b.hfe",true,s0,s1);
    CHECK(im.Open((char*)"t_v3b.hfe"),"v3 open");
    CHECK(im.IsV3,"is v3");
    Words a=ReadTrack(im,0,0),b=ReadTrack(im,1,0);
    CHECK(Diff(a,X)==-1,"v3 nop side0 diff %d (n=%d)",Diff(a,X),(int)a.size());
    CHECK(Diff(b,Y)==-1,"v3 side1 (no opcodes) diff %d",Diff(b,Y));
    CHECK(im.V3Tracks[0].Weak==NULL,"no weak mask");
    im.Close();
  }

  // C. index in the middle -> rotation
  {
    Bytes raw=WordsToRaw(X),st;int k=1234*2;
    st.insert(st.end(),raw.begin()+k,raw.end());st.push_back(0x8F);st.insert(st.end(),raw.begin(),raw.begin()+k);
    std::vector<Bytes> s0(1,st),s1(1,WordsToRaw(Y));
    WriteHfe("t_v3c.hfe",true,s0,s1);
    CHECK(im.Open((char*)"t_v3c.hfe"),"open c");
    Words a=ReadTrack(im,0,0);
    CHECK(Diff(a,X)==-1,"index rotation diff %d",Diff(a,X));
    im.Close();
  }

  // D. SKIPBITS: two skips of garbage that keep stream byte aligned
  {
    Bits b=WordsToBits(X);Bytes st;size_t p=0;
    while(p<b.size()){
      if(p==8000){ // skip 3 garbage bits, take 5 real bits
        st.push_back(0xCF);st.push_back(rev8(3));
        BYTE r=0x07; for(int k=0;k<5;k++) if(b[p+k]) r|=1<<(3+k); st.push_back(r); p+=5;
        st.push_back(0xCF);st.push_back(rev8(5)); // skip 5 garbage bits, take 3
        r=0x15; for(int k=0;k<3;k++) if(b[p+k]) r|=1<<(5+k); st.push_back(r); p+=3;
        continue;
      }
      st.push_back(RawByte(b,p));p+=8;
    }
    std::vector<Bytes> s0(1,st),s1(1,WordsToRaw(Y));
    WriteHfe("t_v3d.hfe",true,s0,s1);
    CHECK(im.Open((char*)"t_v3d.hfe"),"open d");
    Words a=ReadTrack(im,0,0);
    CHECK(Diff(a,X)==-1,"skipbits diff %d n=%d",Diff(a,X),(int)a.size());
    im.Close();
  }

  // E. BITRATE: a stretch at double rate, each nominal cell = 2 fast cells (0,b)
  {
    Bits b=WordsToBits(X);Bytes st;size_t p=0;
    while(p<b.size()){
      if(p==16000){
        st.push_back(0x4F);st.push_back(rev8(36));
        for(int n=0;n<400;n++){ // 400 raw bytes = 1600 nominal bits
          BYTE r=0; for(int k=0;k<4;k++) if(b[p+k]) r|=1<<(2*k+1); st.push_back(r); p+=4; }
        st.push_back(0x4F);st.push_back(rev8(72));
        continue;
      }
      st.push_back(RawByte(b,p));p+=8;
    }
    std::vector<Bytes> s0(1,st),s1(1,WordsToRaw(Y));
    WriteHfe("t_v3e.hfe",true,s0,s1);
    CHECK(im.Open((char*)"t_v3e.hfe"),"open e");
    Words a=ReadTrack(im,0,0);
    CHECK(Diff(a,X)==-1,"bitrate diff %d n=%d",Diff(a,X),(int)a.size());
    im.Close();
  }

  // F. RAND weak bytes, then write-back
  {
    Bytes raw=WordsToRaw(X),st;
    for(size_t i=0;i<raw.size();i++) st.push_back(i>=3000&&i<3010?0x2F:raw[i]);
    std::vector<Bytes> s0(2,st),s1(2,WordsToRaw(Y));
    remove("t_v3f.sav.hfe");
    WriteHfe("t_v3f.hfe",true,s0,s1);
    Bytes orig=ReadFile("t_v3f.hfe");
    CHECK(im.Open((char*)"t_v3f.hfe"),"open f");
    Words a1=ReadTrack(im,0,1),a2=ReadTrack(im,0,1);
    CHECK(a1.size()==X.size(),"rand size");
    int same=0,diffw=0;
    for(size_t i=0;i<X.size();i++){
      bool weak=(i>=1500&&i<1505);
      if(!weak && a1[i]==X[i] && a2[i]==X[i]) same++;
      if(weak && a1[i]!=a2[i]) diffw++;
    }
    CHECK(same==(int)X.size()-5,"non-weak words intact (%d)",same);
    CHECK(diffw>=3,"weak words vary between reads (%d of 5)",diffw);
    CHECK(im.V3Tracks[2].Weak && im.V3Tracks[2].Weak[1500]==0xFFFF,"weak mask set");

    // write: overwrite words 100..199 on side 0 track 1, leave the weak area
    im.LoadTrack(0,1);
    Words Z=X; for(int i=100;i<200;i++){Z[i]=0x4489;im.SetImageWord((WORD)i,0x4489);}
    FloppyDisk[0].WrittenTo=true;
    im.Close();
    FloppyDisk[0].WrittenTo=false;

    CHECK(ReadFile("t_v3f.hfe")==orig,"original image untouched");
    Bytes sav=ReadFile("t_v3f.sav.hfe");
    CHECK(sav.size()==orig.size() && !memcmp(&sav[0],"HXCHFEV3",8),"save copy written as v3 (%d bytes)",(int)sav.size());
    CHECK(im.Open((char*)"t_v3f.hfe"),"reopen f (loads the save copy)");
    Words r1=ReadTrack(im,0,1),r2=ReadTrack(im,0,1);
    int bad=0,weakvar=0;
    for(size_t i=0;i<Z.size()&&i<r1.size();i++){
      bool weak=(i>=1500&&i<1505);
      if(!weak && (r1[i]!=Z[i]||r2[i]!=Z[i])) bad++;
      if(weak && r1[i]!=r2[i]) weakvar++;
    }
    CHECK(r1.size()==Z.size() && bad==0,"written track read back (size %d bad %d)",(int)r1.size(),bad);
    CHECK(weakvar>=3,"weak bytes preserved as RAND after save (%d)",weakvar);
    Words s=ReadTrack(im,1,1),t0=ReadTrack(im,0,0);
    CHECK(Diff(s,Y)==-1,"other side untouched diff %d",Diff(s,Y));
    CHECK(t0.size()==X.size(),"other track size");
    int bad0=0; for(size_t i=0;i<X.size();i++) if(!(i>=1500&&i<1505)&&t0[i]!=X[i]) bad0++;
    CHECK(bad0==0,"other track untouched (%d)",bad0);
    im.Close();
  }

  // G. write-back where the written side is longer than the original stream
  {
    Bytes raw=WordsToRaw(X); Bytes shortside(raw.begin(),raw.begin()+6000*2); // 6000 words
    std::vector<Bytes> s0(1,shortside),s1(1,shortside);
    remove("t_v3g.sav.hfe");
    WriteHfe("t_v3g.hfe",true,s0,s1);
    CHECK(im.Open((char*)"t_v3g.hfe"),"open g");
    Words a=ReadTrack(im,0,0);
    CHECK(a.size()==6000,"short track size %d",(int)a.size());
    im.SetImageWord(5,0x5224);
    FloppyDisk[0].WrittenTo=true;im.Close();FloppyDisk[0].WrittenTo=false;
    CHECK(im.Open((char*)"t_v3g.hfe"),"reopen g");
    Words b=ReadTrack(im,0,0),c=ReadTrack(im,1,0);
    CHECK(b.size()==6000 && b[5]==0x5224 && b[6]==X[6],"rewritten side ok (%d)",(int)b.size());
    Words xs(X.begin(),X.begin()+6000);
    CHECK(Diff(c,xs)==-1,"unwritten side padded with NOPs reads same, diff %d",Diff(c,xs));
    im.Close();
  }

  // I. save copies, GTi style, also for v1
  {
    remove("t_v1i.sav.hfe");
    std::vector<Bytes> s0(1,WordsToRaw(X)),s1(1,WordsToRaw(Y));
    WriteHfe("t_v1i.hfe",false,s0,s1);
    Bytes orig=ReadFile("t_v1i.hfe");
    // no write -> no save copy
    CHECK(im.Open((char*)"t_v1i.hfe"),"open i");
    ReadTrack(im,0,0); im.Close();
    CHECK(!Exists("t_v1i.sav.hfe"),"no save copy without writes");
    // first write goes to the save copy
    CHECK(im.Open((char*)"t_v1i.hfe"),"open i 2");
    im.LoadTrack(0,0); im.SetImageWord(10,0x1234);
    FloppyDisk[0].WrittenTo=true;im.Close();FloppyDisk[0].WrittenTo=false;
    CHECK(ReadFile("t_v1i.hfe")==orig,"v1 original untouched");
    CHECK(Exists("t_v1i.sav.hfe"),"v1 save copy created");
    // the save copy is loaded from now on, and updated in place
    CHECK(im.Open((char*)"t_v1i.hfe"),"open i 3");
    Words a=ReadTrack(im,0,0);
    CHECK(a.size()>10 && a[10]==0x1234,"save copy loaded");
    im.SetImageWord(11,0x5678);
    FloppyDisk[0].WrittenTo=true;im.Close();FloppyDisk[0].WrittenTo=false;
    CHECK(!Exists("t_v1i.sav.sav.hfe"),"save copy updated in place, no .sav.sav");
    CHECK(ReadFile("t_v1i.hfe")==orig,"v1 original still untouched");
    CHECK(im.Open((char*)"t_v1i.sav.hfe"),"save copy opens directly");
    a=ReadTrack(im,0,0);
    CHECK(a[10]==0x1234 && a[11]==0x5678,"both writes in save copy");
    im.Close();
    // deleting the save copy goes back to the original
    remove("t_v1i.sav.hfe");
    CHECK(im.Open((char*)"t_v1i.hfe"),"open i 4");
    a=ReadTrack(im,0,0);
    CHECK(a[10]==X[10],"original after deleting save copy");
    im.Close();
    // per-dongle style names (Game.sav.XXXX.hfe) count as saves too
    rename("t_v1i.hfe","t_v1i.sav.80FE.hfe");
    CHECK(im.Open((char*)"t_v1i.sav.80FE.hfe"),"open tagged save");
    im.LoadTrack(0,0); im.SetImageWord(12,0x9ABC);
    FloppyDisk[0].WrittenTo=true;im.Close();FloppyDisk[0].WrittenTo=false;
    CHECK(!Exists("t_v1i.sav.80FE.sav.hfe"),"tagged save updated in place");
    remove("t_v1i.sav.80FE.hfe");
  }

  // H. empty / truncated tracks don't crash
  {
    std::vector<Bytes> s0(2,Bytes()),s1(2,Bytes());
    WriteHfe("t_v3h.hfe",true,s0,s1);
    CHECK(im.Open((char*)"t_v3h.hfe"),"open h");
    bool ok;Words a=ReadTrack(im,0,0,&ok);
    CHECK(ok && a.size()==6256,"empty track -> unformatted (%d)",(int)a.size());
    CHECK(!im.LoadTrack(0,5),"track out of range rejected");
    im.Close();
    FILE*fp=fopen("t_v3h.hfe","wb");fwrite("HXCHFEV3",1,8,fp);fclose(fp);
    CHECK(!im.Open((char*)"t_v3h.hfe"),"truncated header rejected");
  }

  // I. bridge mode: a real drive through FloppyBridge (mocked)
  {
    const char*desc="\xEF\xBB\xBF" "FLOPPYBRIDGE\r\n; comment\r\ndriver=greaseweazle\nport=auto\ndrive=A\n";
    FILE*fp=fopen("t_gw.hfe","wb");fputs(desc,fp);fclose(fp);
    // a revolution that isn't a whole number of words, with a slow area
    Bits b=WordsToBits(X); b.resize(b.size()-5);
    g_fb.bits=b; g_fb.speed.assign((b.size()+7)/8,1000);
    for(int i=2000;i<2400;i++) g_fb.speed[i]=1050; // 5% slower bytes
    CHECK(im.Open((char*)"t_gw.hfe"),"bridge open");
    CHECK(im.Bridge!=NULL && im.IsV3,"bridge mode is a v3 track store");
    CHECK(im.file_header->number_of_track==82,"cylinders from the drive (%d)",im.file_header->number_of_track);
    CHECK(im.file_header->floppyinterfacemode==0x02,"DD drive (ATARIST_DD_FLOPPYMODE)");
    int r0=g_fb.reads;
    bool ok;Words a=ReadTrack(im,1,3,&ok);
    CHECK(ok && g_fb.reads==r0+1,"track read from the drive");
    CHECK(a.size()==X.size(),"word count %d",(int)a.size());
    int bad=0;for(size_t i=0;i+1<a.size();i++)if(a[i]!=X[i])bad++;
    CHECK(bad==0,"bits match the drive (%d bad words)",bad);
    // last word: 11 real bits then $AAAA padding
    CHECK((a.back()&0xFFE0)==(X.back()&0xFFE0) && (a.back()&0x1F)==(0xAAAA&0x1F),"last word padded %04X",a.back());
    TImageHFE::THfeV3Track&t=im.V3Tracks[3*2+1];
    CHECK(t.Cyc!=NULL,"measured density kept");
    CHECK(t.Cyc && t.Cyc[2000*8]==269 && t.Cyc[100*8]==256,"cell times %d %d",t.Cyc?t.Cyc[16000]:0,t.Cyc?t.Cyc[800]:0);
    int nominal=im.VarByteCycles(10,256),slow=im.VarByteCycles(1100,256);
    CHECK(nominal==256 && slow==269,"byte pacing follows density (%d %d)",nominal,slow);
    a=ReadTrack(im,1,3,&ok);
    CHECK(g_fb.reads==r0+1,"cached: not read again");
    // a track at exactly nominal speed keeps the nominal path
    g_fb.speed.assign(g_fb.speed.size(),1000);
    ReadTrack(im,0,3,&ok);
    CHECK(ok && im.V3Tracks[3*2].Cyc==NULL,"nominal track has no Cyc");
    // writes are ignored, nothing saved
    im.SetImageWord(5,0x1234);
    CHECK(im.V3Tracks[3*2].Data[5]==X[5] && !im.V3Tracks[3*2].Dirty,"write ignored");
    // disk removed: random bits, the cache is dropped
    g_fb.disk_in=false; r0=g_fb.reads;
    a=ReadTrack(im,0,4,&ok);
    CHECK(ok && g_fb.reads==r0 && a.size()==6256 && im.V3Tracks[4*2].Unread,"no disk -> unformatted, not read");
    CHECK(im.V3Tracks[3*2+1].Data==NULL,"cache dropped when the disk goes");
    // disk back: read again
    g_fb.disk_in=true;
    a=ReadTrack(im,0,4,&ok);
    CHECK(ok && g_fb.reads==r0+1 && !im.V3Tracks[4*2].Unread && a.size()==X.size(),"disk back -> read");
    // read failure: random for now, retried next time
    g_fb.fail_reads=1; r0=g_fb.reads;
    a=ReadTrack(im,0,5,&ok);
    CHECK(ok && im.V3Tracks[5*2].Unread,"failed read -> unformatted");
    a=ReadTrack(im,0,5,&ok);
    CHECK(g_fb.reads==r0+2 && !im.V3Tracks[5*2].Unread && a.size()==X.size(),"retried");
    CHECK(!im.LoadTrack(0,82),"beyond the drive rejected");
    FloppyDisk[0].WrittenTo=true; int c0=g_fb.closes; im.Close(); FloppyDisk[0].WrittenTo=false;
    CHECK(g_fb.closes==c0+1,"drive closed");
    CHECK(!Exists("t_gw.sav.hfe"),"no save copy for a drive");
    CHECK(ReadFile("t_gw.hfe").size()==strlen(desc),"descriptor untouched");
    // can't connect: Open fails with a message
    g_fb.open_ok=false;
    CHECK(!im.Open((char*)"t_gw.hfe") && strstr(im.BridgeError,"no Greaseweazle"),"open error reported");
    g_fb.open_ok=true;
    remove("t_gw.hfe");
  }

  printf("%d checks, %d failures\n",checks,fails);
  return fails?1:0;
}
