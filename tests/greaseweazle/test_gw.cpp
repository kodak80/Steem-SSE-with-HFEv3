// Greaseweazle flux mode tests: encode a track as a Greaseweazle flux
// stream, decode it, cut the revolutions, find the sectors again
#include "../../steem/floppybridge_drive.cpp"
#include "../../steem/greaseweazle_flux.cpp"
#include <string>
#include <cmath>
#include <algorithm>

static int checks=0,fails=0;
#define CHECK(c,...) do{checks++;if(!(c)){fails++;printf("FAIL %s:%d: ",__FILE__,__LINE__);printf(__VA_ARGS__);printf("\n");}}while(0)

struct Mfm {
  std::vector<int> bits; int last=0;
  void Byte(int b){ for(int k=7;k>=0;k--){int d=(b>>k)&1;int c=(!d && !last)?1:0;bits.push_back(c);bits.push_back(d);last=d;} }
  void Sync(){ for(int k=15;k>=0;k--) bits.push_back((0x4489>>k)&1); last=1; }
  void Fill(int b,int n){ for(int i=0;i<n;i++) Byte(b); }
};
static WORD Crc(const std::vector<int>&b){WORD c=0xFFFF;for(int x:b)c=Crc16(c,(BYTE)x);return c;}
static Mfm StTrack(int track,int side){
  Mfm m; m.Fill(0x4E,60);
  for(int s=1;s<=10;s++){
    m.Fill(0,12); m.Sync();m.Sync();m.Sync();
    std::vector<int> id={0xA1,0xA1,0xA1,0xFE,track,side,s,2};
    WORD c=Crc(id); for(int k=3;k<8;k++) m.Byte(id[k]); m.Byte(c>>8); m.Byte(c&0xFF);
    m.Fill(0x4E,22); m.Fill(0,12); m.Sync();m.Sync();m.Sync();
    std::vector<int> d={0xA1,0xA1,0xA1,0xFB}; for(int i=0;i<512;i++) d.push_back((i*7+s)&0xFF);
    c=Crc(d); for(size_t k=3;k<d.size();k++) m.Byte(d[k]); m.Byte(c>>8); m.Byte(c&0xFF);
    m.Fill(0x4E,40);
  }
  while(m.bits.size()<100000) m.Byte(0x4E);
  m.bits.resize(100000);
  return m;
}

// Greaseweazle stream encoding of a tick interval
static void Put(std::vector<BYTE>&s,unsigned v){
  if(v<250) s.push_back((BYTE)v);
  else if(v<1525){ unsigned x=v-250; s.push_back((BYTE)(250+x/255)); s.push_back((BYTE)(1+x%255)); }
  else { // space + short transition
    unsigned sp=v-249; s.push_back(255); s.push_back(2);
    s.push_back((BYTE)(1|(sp<<1))); s.push_back((BYTE)(1|(sp>>6))); s.push_back((BYTE)(1|(sp>>13))); s.push_back((BYTE)(1|(sp>>20)));
    s.push_back(249); }
}
static void Put28(std::vector<BYTE>&s,int op,unsigned v){
  s.push_back(255); s.push_back((BYTE)op);
  s.push_back((BYTE)(1|(v<<1))); s.push_back((BYTE)(1|(v>>6))); s.push_back((BYTE)(1|(v>>13))); s.push_back((BYTE)(1|(v>>20)));
}

int main(){
  // 1. stream decoding
  { std::vector<BYTE> s; Put(s,100); Put(s,300); Put28(s,1,40); Put(s,2000); Put(s,5);
    std::vector<unsigned long long> f,ix;
    bool ok=GwDecodeFluxStream(s.data(),(int)s.size(),f,ix);
    CHECK(ok && f.size()==4,"decode: 4 transitions (%d)",(int)f.size());
    CHECK(f.size()==4 && f[0]==100 && f[1]==400 && f[2]==2400 && f[3]==2405,"decode: times %llu %llu %llu %llu",f[0],f[1],f[2],f[3]);
    CHECK(ix.size()==1 && ix[0]==440,"decode: index after the 2nd transition (%llu)",ix.size()?ix[0]:0);
    std::vector<BYTE> bad={255,1,3}; CHECK(!GwDecodeFluxStream(bad.data(),3,f,ix),"decode: truncated opcode refused");
  }
  // 2. a whole track: 3 revolutions at 72MHz, with drift; split + PLL + sectors
  const unsigned freq=72000000; // ticks per second
  Mfm m=StTrack(5,1);
  std::vector<BYTE> s; double t=0; unsigned long long last=0; unsigned long long abs_ticks=0;
  std::vector<double> ip; // index times (s)
  double cell=2e-6*1.01; // drive 1% slow
  // lead-in: start 30ms before the first index
  double pos=-0.03; int nrev=3; double rev_time=cell*100000;
  std::vector<double> trans;
  for(int r=-1;r<=nrev;r++) for(size_t i=0;i<m.bits.size();i++) if(m.bits[i]){
    double tt=r*rev_time+i*cell + ((int)((i*7919)%13)-6)*2e-9; // small jitter
    if(tt>=pos && tt<nrev*rev_time+0.001) trans.push_back(tt); }
  std::sort(trans.begin(),trans.end());
  double prev=pos; size_t ti=0; int next_ip=0;
  for(;ti<trans.size();ti++){
    while(next_ip<=nrev && next_ip*rev_time<trans[ti]){
      unsigned v=(unsigned)((next_ip*rev_time-prev)*freq); Put28(s,1,v); next_ip++; }
    unsigned v=(unsigned)llround((trans[ti]-prev)*freq);
    prev+= v/(double)freq; Put(s,v);
  }
  std::vector<unsigned long long> f,ix;
  bool ok=GwDecodeFluxStream(s.data(),(int)s.size(),f,ix);
  CHECK(ok && ix.size()==4,"track: 4 index pulses (%d)",(int)ix.size());
  TGwFluxRev revs[GWFLUX_MAX_REVS];
  int n=GwSplitRevolutions(f,ix,freq,revs,GWFLUX_MAX_REVS);
  CHECK(n==3,"track: 3 revolutions (%d)",n);
  int ones=0; for(int b:m.bits) ones+=b;
  CHECK(n==3 && abs((int)revs[1].size()-ones-1)<=1,"track: transitions per rev %d vs %d (+1 for continuity)",(int)revs[1].size(),ones);
  unsigned rev_units=(unsigned)((ix[1]-ix[0])*40000000ull/freq);
  CHECK(abs((int)rev_units-(int)(rev_time*40e6))<=2,"track: rev %u units",rev_units);
  CHECK(n==3 && revs[0].back()>rev_units,"track: last transition after the next index");
  bool increasing=true; for(size_t i=1;i<revs[0].size();i++) if(revs[0][i]<=revs[0][i-1]) increasing=false;
  CHECK(increasing,"track: times increase");
  static BYTE mfm[32768];
  int nb=GwFluxToBits(revs[0],rev_units,mfm,sizeof(mfm));
  TFloppyBridgeCheck c; FloppyBridgeCheckTrack(mfm,nb,c);
  CHECK(abs(nb-100000)<=8,"pll: %d bits",nb);
  CHECK(c.ids==10 && c.good==10 && c.bad==0 && c.id_track==5 && c.id_side==1,"pll: 10 sectors OK (ids %d good %d bad %d)",c.ids,c.good,c.bad);
  // 3. a 10% slow then 10% fast region (split bit-rate) is still decoded
  { TGwFluxRev r; double tt=0; unsigned u=0; std::vector<int> bits;
    Mfm a=StTrack(1,0); int k=0;
    for(size_t i=0;i<a.bits.size();i++){ double c2=(i>20000&&i<24000)?2.2:(i>=24000&&i<28000)?1.8:2.0; tt+=c2*40; if(a.bits[i]) r.push_back((unsigned)tt); }
    int nb2=GwFluxToBits(r,(unsigned)tt,mfm,sizeof(mfm)); TFloppyBridgeCheck c2; FloppyBridgeCheckTrack(mfm,nb2,c2);
    CHECK(c2.ids==10,"split rate: all IDs found (%d)",c2.ids);
  }
  printf("%d checks, %d failures\n",checks,fails);
  return fails?1:0;
}
