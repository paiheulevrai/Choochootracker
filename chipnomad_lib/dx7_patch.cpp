#include "dx7_patch.h"
#include "project_io_common.h"
#include "fm_amp.h"
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <algorithm>
namespace {
bool validVoice(const uint8_t* v) {
  static const uint8_t maximum[21]={99,99,99,99,99,99,99,99,99,99,99,3,3,7,3,7,99,1,31,99,14};
  for(int op=0;op<6;++op)for(int j=0;j<21;++j)if(v[21*op+j]>maximum[j])return false;
  static const uint8_t globalMax[19]={99,99,99,99,99,99,99,99,31,7,1,99,99,99,99,1,5,7,48};
  for(int j=0;j<19;++j)if(v[126+j]>globalMax[j])return false;
  for(int j=145;j<155;++j)if(v[j]>127)return false;
  return true;
}
void nameFromVoice(InstrumentDX7& p) {
  for(int i=0;i<10;++i)p.presetName[i]=p.voice[145+i]>=32&&p.voice[145+i]<127?p.voice[145+i]:' ';
  for(int i=9;i>=0&&p.presetName[i]==' ';--i)p.presetName[i]=0;
  if(!p.presetName[0])std::strcpy(p.presetName,"Unnamed DX7");
}
bool unpack(const uint8_t* b,uint8_t* v) {
  for(int op=0;op<6;++op) {
    const auto* s=b+17*op;auto* d=v+21*op;
    // Reserved bits are rejected rather than silently masked into a different voice.
    if(s[11]>15||s[12]>119||s[13]>31||s[15]>63)return false;
    std::memcpy(d,s,11);d[11]=s[11]&3;d[12]=s[11]>>2;
    d[13]=s[12]&7;d[20]=s[12]>>3;d[14]=s[13]&3;d[15]=s[13]>>2;
    d[16]=s[14];d[17]=s[15]&1;d[18]=s[15]>>1;d[19]=s[16];
  }
  if(b[111]>15)return false;
  std::memcpy(v+126,b+102,9);v[135]=b[111]&7;v[136]=b[111]>>3;
  std::memcpy(v+137,b+112,4);v[141]=b[116]&1;v[142]=(b[116]>>1)&7;v[143]=b[116]>>4;
  std::memcpy(v+144,b+117,11);return validVoice(v);
}
}
bool validDX7(const InstrumentDX7& p) {
  return validFMControls(p.amp,p.tone)&&p.schema==1&&p.fineTune>=-100&&p.fineTune<=100&&p.velocity>=1&&p.velocity<=127&&
    std::memchr(p.presetName,0,sizeof(p.presetName))&&validVoice(p.voice);
}
void initDX7Patch(InstrumentDX7* p) {
  *p={};p->schema=1;p->velocity=100;auto* v=p->voice;
  // Original three-pair tine patch, not a manufacturer's ROM-bank copy.
  for(int op=0;op<6;++op) {
    auto* o=v+21*op;const uint8_t rates[]={95,uint8_t(op%2?48:65),35,55};
    for(int j=0;j<4;++j)o[j]=rates[j];o[4]=99;o[5]=op%2?80:60;o[6]=op%2?65:25;
    o[8]=45;o[10]=op%2?10:25;o[12]=1;o[13]=2;o[15]=op%2?1:3;
    o[16]=op%2?85:72;o[18]=op%2?1:(op==0?7:op==2?3:1);o[20]=7;
  }
  for(int i=0;i<4;++i){v[126+i]=99;v[130+i]=50;}
  v[134]=4;v[136]=1;v[137]=30;v[141]=1;v[142]=4;v[144]=24;
  std::memcpy(v+145,"TRACK TINE",10);std::strcpy(p->presetName,"Tracker Tine");
}
bool importDX7SysEx(const uint8_t* bytes,size_t size,std::vector<InstrumentDX7>& output,std::string& error) {
  auto fail=[&](const char* why){error=why;return false;};
  if(!bytes||!size||size>1024*1024)return fail("DX7 file is empty or exceeds 1 MiB");
  std::vector<InstrumentDX7> staged;size_t pos=0;
  while(pos<size) {
    if(size-pos<8)return fail("Truncated DX7 SysEx header");
    const auto* m=bytes+pos;
    if(m[0]!=0xf0||m[1]!=0x43||m[2]>15)return fail("Expected Yamaha DX7 bulk dump on channel 1-16");
    size_t payload=m[3]==0?155:m[3]==9?4096:0;
    if(!payload)return fail("Unsupported Yamaha message (DX7 single or 32-voice bulk required)");
    if(m[4]>127||m[5]>127||size_t(m[4])*128+m[5]!=payload)return fail("Incorrect DX7 bulk byte count");
    if(size-pos<payload+8||m[payload+7]!=0xf7)return fail("Truncated DX7 data or missing end marker");
    unsigned sum=0;for(size_t i=0;i<payload;++i){if(m[6+i]>127)return fail("DX7 data must be seven-bit");sum+=m[6+i];}
    if(m[payload+6]>127||((sum+m[payload+6])&127))return fail("DX7 checksum mismatch");
    int count=payload==155?1:32;
    for(int i=0;i<count;++i) {
      InstrumentDX7 patch{};patch.schema=1;patch.velocity=100;patch.sourceProgram=i;
      if(count==1){std::memcpy(patch.voice,m+6,155);if(!validVoice(patch.voice))return fail("DX7 voice parameter outside native range");}
      else if(!unpack(m+6+128*i,patch.voice))return fail("DX7 packed voice contains invalid or reserved values");
      nameFromVoice(patch);staged.push_back(patch);
      if(staged.size()>4096)return fail("DX7 import exceeds 4096 voices");
    }
    pos+=payload+8;
  }
  output.swap(staged);error.clear();return true;
}
int loadDX7Data(FILE* file,Instrument* instrument) {
  InstrumentDX7 p{};unsigned seen=0;bool ampSeen=false,toneSeen=false;
  while(char* line=peekLine(file)) {
    if(line[0]=='#')break;
    if(!strncmp(line,"- DX7 base: ",12)) {
      int a,b,c,d,e;char tail;
      if((seen&1)||sscanf(line+12,"%d,%d,%d,%d,%d %c",&a,&b,&c,&d,&e,&tail)!=5||a!=1||b< -100||b>100||c<1||c>127||d<0||d>65535||e<0||e>65535)return 1;
      p.schema=a;p.fineTune=b;p.velocity=c;p.bankId=d;p.sourceProgram=e;seen|=1;
    }else if(!strncmp(line,"- DX7 name: ",12)) {
      if((seen&2)||strlen(line+12)>=sizeof(p.presetName))return 1;
      strcpy(p.presetName,line+12);seen|=2;
    }else if(!strncmp(line,"- DX7 voice: ",13)) {
      if(seen&4)return 1;const char* text=line+13;
      for(int i=0;i<155;++i){char* end;errno=0;long n=strtol(text,&end,10);if(errno||end==text||n<0||n>127)return 1;p.voice[i]=n;text=end;if(i<154&&*text++!=',')return 1;}
      if(*text)return 1;seen|=4;
    }else if(loadFMAmpSetting(line,p.amp,ampSeen)!=1 && loadFMToneSetting(line,p.tone,toneSeen)!=1)return 1;
    consumeLine(file);
  }
  if(seen!=7||!validDX7(p))return 1;instrument->chip.dx7=p;return 0;
}
int saveDX7Data(FILE* file,const Instrument* instrument) {
  const auto& p=instrument->chip.dx7;
  fprintf(file,"- DX7 base: %u,%d,%u,%u,%u\n- DX7 name: %s\n- DX7 voice: ",p.schema,p.fineTune,p.velocity,p.bankId,p.sourceProgram,p.presetName);
  for(int i=0;i<155;++i)fprintf(file,"%s%u",i?",":"",p.voice[i]);fputc('\n',file);saveFMAmpSetting(file,p.amp);saveFMToneSetting(file,p.tone);return ferror(file)?1:0;
}
