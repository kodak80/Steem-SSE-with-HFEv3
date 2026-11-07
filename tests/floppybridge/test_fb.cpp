// FloppyBridge wrapper tests: build an ST track as MFM, check what the
// sector checker finds; speed smoothing; descriptor parsing
#include "../../steem/floppybridge_drive.cpp"
#include <string>

static int checks=0,fails=0;
#define CHECK(c,...) do{checks++;if(!(c)){fails++;printf("FAIL %s:%d: ",__FILE__,__LINE__);printf(__VA_ARGS__);printf("\n");}}while(0)

struct Mfm {
  std::vector<int> bits; int last=0;
  void Byte(int b){ for(int k=7;k>=0;k--){int d=(b>>k)&1;int c=(!d && !last)?1:0;bits.push_back(c);bits.push_back(d);last=d;} }
  void Sync(){ // A1 with a missing clock: $4489
    for(int k=15;k>=0;k--) bits.push_back((0x4489>>k)&1); last=1; }
  void Fill(int b,int n){ for(int i=0;i<n;i++) Byte(b); }
  std::vector<BYTE> Pack() const { std::vector<BYTE> v((bits.size()+7)/8);
    for(size_t i=0;i<bits.size();i++) if(bits[i]) v[i/8]|=(BYTE)(0x80>>(i%8)); return v; }
};

static WORD Crc(const std::vector<int>&b){WORD c=0xFFFF;for(int x:b)c=Crc16(c,(BYTE)x);return c;}

// a standard 9 x 512 ST track; corrupt=sector whose data CRC is wrong
static Mfm StTrack(int track,int side,int corrupt=0,int lead_bits=0){
  Mfm m; for(int i=0;i<lead_bits;i++) m.bits.push_back(i&1);
  m.Fill(0x4E,60);
  for(int s=1;s<=9;s++){
    m.Fill(0,12); m.Sync();m.Sync();m.Sync();
    std::vector<int> id={0xA1,0xA1,0xA1,0xFE,track,side,s,2};
    WORD c=Crc(id); for(int k=3;k<8;k++) m.Byte(id[k]); m.Byte(c>>8); m.Byte(c&0xFF);
    m.Fill(0x4E,22); m.Fill(0,12); m.Sync();m.Sync();m.Sync();
    std::vector<int> d={0xA1,0xA1,0xA1,0xFB}; for(int i=0;i<512;i++) d.push_back((i*7+s)&0xFF);
    c=Crc(d); if(s==corrupt) c^=1;
    for(size_t k=3;k<d.size();k++) m.Byte(d[k]); m.Byte(c>>8); m.Byte(c&0xFF);
    m.Fill(0x4E,40);
  }
  while(m.bits.size()<100000) m.Byte(0x4E);
  return m;
}

int main(){
  // sector check
  { Mfm m=StTrack(3,1); auto v=m.Pack(); TFloppyBridgeCheck c;
    FloppyBridgeCheckTrack(&v[0],(int)m.bits.size(),c);
    CHECK(c.ids==9 && c.good==9 && c.bad==0 && c.bad_ids==0,"9 good sectors (%d %d %d)",c.ids,c.good,c.bad);
    CHECK(c.id_track==3 && c.id_side==1 && c.id_list[0]==1 && c.id_list[8]==9,"IDs read");
    char s[300]; FormatCheck(c,s,sizeof(s));
    CHECK(strstr(s,"9 IDs (trk 3 side 1, sectors 1 2 3 4 5 6 7 8 9), data OK 9, bad 0")!=NULL,"log text: %s",s); }
  { Mfm m=StTrack(0,0); auto v=m.Pack(); TFloppyBridgeCheck c; // per-sector speed
    FloppyBridgeCheckTrack(&v[0],(int)m.bits.size(),c);
    CHECK(c.secs==9 && c.sec_id[5]==6 && c.sec_end[5]-c.sec_start[5]==512*16,"data fields located");
    int nb=(int)v.size(); std::vector<WORD> sp(nb,1000);
    for(int i=c.sec_start[5]/8;i<c.sec_end[5]/8;i++) sp[i]=1052; // Copylock-style sector 6
    char s[400]; FloppyBridgeSectorSpeeds(c,&sp[0],nb,s,sizeof(s));
    CHECK(strstr(s,"5:99")!=NULL && strstr(s," 6:10")!=NULL,"%s",s);
    int s6=atoi(strstr(s," 6:")+3),s5=atoi(strstr(s,"5:")+2);
    CHECK(s6-s5>=45 && s6-s5<=55,"sector 6 ~5%% slower than 5: %s",s); }
  { Mfm m=StTrack(0,0,5); auto v=m.Pack(); TFloppyBridgeCheck c;
    FloppyBridgeCheckTrack(&v[0],(int)m.bits.size(),c);
    CHECK(c.good==8 && c.bad==1 && c.bad_list[0]==5,"bad data CRC found in sector 5");
    char s[300]; FormatCheck(c,s,sizeof(s)); CHECK(strstr(s,"bad 1 (5)")!=NULL,"%s",s); }
  { Mfm m=StTrack(0,0,0,3); auto v=m.Pack(); TFloppyBridgeCheck c; // not on a byte boundary
    FloppyBridgeCheckTrack(&v[0],(int)m.bits.size(),c);
    CHECK(c.ids==9 && c.good==9,"bit-shifted track (%d %d)",c.ids,c.good); }
  { std::vector<BYTE> v(12500); srand(1); for(auto&b:v) b=(BYTE)rand(); TFloppyBridgeCheck c;
    FloppyBridgeCheckTrack(&v[0],100000,c);
    CHECK(c.good==0,"random bits: no good sector"); }

  // speed smoothing
  const int N=12500;
  std::vector<WORD> sp(N);
  srand(2); for(int i=0;i<N;i++) sp[i]=(WORD)(1000+rand()%101-50); // +-5% jitter
  int mn,mx; FloppyBridgeSmoothSpeeds(&sp[0],N,FLOPPYBRIDGE_DENSITY_AUTO,&mn,&mx);
  bool all=true; for(int i=0;i<N;i++) if(sp[i]!=1000) all=false;
  CHECK(all && mn>=970 && mx<=1030,"jitter only -> nominal (%d-%d)",mn,mx);
  for(int i=0;i<N;i++) sp[i]=(WORD)(1020+rand()%61-30); // drive 2% slow + jitter
  FloppyBridgeSmoothSpeeds(&sp[0],N,FLOPPYBRIDGE_DENSITY_AUTO,&mn,&mx);
  all=true; for(int i=0;i<N;i++) if(sp[i]!=1000) all=false;
  CHECK(all,"slow drive -> nominal (%d-%d)",mn,mx);
  { // decisions on a real ST track, values as measured with a Greaseweazle
    Mfm m=StTrack(0,0); auto v=m.Pack(); TFloppyBridgeCheck c;
    FloppyBridgeCheckTrack(&v[0],(int)m.bits.size(),c);
    int nb=(int)v.size(); std::vector<WORD> t(nb);
    // Total Recall disk 1 track 0: sector 6 5.5% slower, plus jitter
    for(int i=0;i<nb;i++) t[i]=(WORD)(1000+rand()%61-30);
    for(int i=c.sec_start[5]/8;i<c.sec_end[5]/8;i++) t[i]+=55;
    CHECK(FloppyBridgeSectorDeviation(c,&t[0],nb)>=48,"Copylock deviation %d",FloppyBridgeSectorDeviation(c,&t[0],nb));
    std::vector<WORD> u=t; FloppyBridgeSmoothSpeeds(&u[0],nb,FLOPPYBRIDGE_DENSITY_AUTO,&mn,&mx,&c);
    int mid=(c.sec_start[5]+c.sec_end[5])/16, other=(c.sec_start[1]+c.sec_end[1])/16;
    CHECK(u[mid]-u[other]>=40,"Copylock sector kept (%d vs %d)",u[mid],u[other]);
    // ordinary track with drive wobble: sectors up to 3.3% apart, gap spikes to 7%
    for(int i=0;i<nb;i++) t[i]=(WORD)(1000+rand()%61-30);
    for(int k=0;k<9;k++){int w=(k==1||k==2)?30:(k==6?-20:0); for(int i=c.sec_start[k]/8;i<c.sec_end[k]/8;i++) t[i]+=w;}
    for(int i=100;i<140;i++) t[i]=1070;
    u=t; FloppyBridgeSmoothSpeeds(&u[0],nb,FLOPPYBRIDGE_DENSITY_AUTO,&mn,&mx,&c);
    bool flat=true; for(int i=0;i<nb;i++) if(u[i]!=1000) flat=false;
    CHECK(flat && mx>1030,"wobble + gap spike -> nominal (max %d, dev %d)",mx,FloppyBridgeSectorDeviation(c,&t[0],nb));
    // fewer than 3 sectors: 8% rule
    TFloppyBridgeCheck none; memset(&none,0,sizeof(none));
    u=t; FloppyBridgeSmoothSpeeds(&u[0],nb,FLOPPYBRIDGE_DENSITY_AUTO,&mn,&mx,&none);
    CHECK(u[0]==1000 && u[nb/2]==1000,"no sectors, <8%% -> nominal");
    for(int i=5000;i<5600;i++) t[i]=1100;
    u=t; FloppyBridgeSmoothSpeeds(&u[0],nb,FLOPPYBRIDGE_DENSITY_AUTO,&mn,&mx,&none);
    CHECK(u[5300]>1070,"no sectors, 10%% slow area kept (%d)",u[5300]);
  }
  for(int i=0;i<N;i++) sp[i]=1234;
  FloppyBridgeSmoothSpeeds(&sp[0],N,FLOPPYBRIDGE_DENSITY_OFF,&mn,&mx);
  CHECK(sp[0]==1000 && sp[N-1]==1000,"density=off");
  sp[7]=950; FloppyBridgeSmoothSpeeds(&sp[0],N,FLOPPYBRIDGE_DENSITY_RAW,&mn,&mx);
  CHECK(sp[7]==950 && mn==950 && mx==1000,"density=raw");

  // descriptor
  { TFloppyBridgeSettings s; ParseDescriptor("FLOPPYBRIDGE\r\ndriver = DrawBridge # x\r\nport=COM5\r\ndrive=b\r\nlog=off\r\ndensity=raw ; y\r\n",s);
    CHECK(!strcmp(s.driver,"DrawBridge") && !strcmp(s.port,"COM5") && !strcmp(s.drive,"b")
      && !strcmp(s.log,"off") && !strcmp(s.density,"raw"),"descriptor keys"); }
  { TFloppyBridgeSettings s; ParseDescriptor("FLOPPYBRIDGE\nconfig=a;b#c\n",s);
    CHECK(!strcmp(s.config,"a;b#c") && !strcmp(s.log,"on") && !strcmp(s.density,"auto") && !strcmp(s.cache,"off"),"config kept whole, defaults"); }
  { TFloppyBridgeSettings s; ParseDescriptor("FLOPPYBRIDGE\ncache = on ; faster\n",s);
    CHECK(!strcmp(s.cache,"on"),"cache=on"); }
  { FILE*f=fopen("../../../extras/Greaseweazle.hfe","rb"); CHECK(f!=NULL,"sample descriptor");
    if(f){ char b[4096]; size_t n=fread(b,1,4095,f); b[n]=0; fclose(f);
      TFloppyBridgeSettings s; ParseDescriptor(b,s);
      CHECK(!strcmp(s.driver,"greaseweazle") && !strcmp(s.port,"auto") && !strcmp(s.drive,"A") && !strcmp(s.cache,"off"),"sample keys");
      CHECK(!strncmp(b,"FLOPPYBRIDGE",12),"sample signature"); } }

  // no DLL here: open fails cleanly, and the log says why
  { char err[256]=""; void*h=FloppyBridgeOpen("FLOPPYBRIDGE\n","t.log",err,sizeof(err));
    CHECK(!h && strstr(err,"not found"),"no DLL: %s",err);
    FILE*f=fopen("t.log","rb"); char b[1000]=""; if(f){fread(b,1,999,f);fclose(f);}
    CHECK(strstr(b,"driver=greaseweazle") && strstr(b,"not found") && strstr(b,"inserted ----"),"log written: %s",b);
    FloppyBridgeOpen("FLOPPYBRIDGE\n","t.log",err,sizeof(err)); // second insert: appended
    f=fopen("t.log","rb"); char b2[2000]=""; if(f){fread(b2,1,1999,f);fclose(f);}
    const char*first=strstr(b2,"inserted ----"); CHECK(first && strstr(first+5,"inserted ----"),"log appended");
    remove("t.log"); }

  printf("%d checks, %d failures\n",checks,fails);
  return fails!=0;
}
