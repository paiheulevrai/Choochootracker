#include "four_op_patch.h"
#include "project_io_common.h"
#include "fm_amp.h"
#include <cstring>
#include <cstdlib>
#include <cerrno>
bool isFourOp(InstrumentType t){return t==InstrumentType::GenesisFM||t==InstrumentType::ArcadeFM;}
bool validFourOp(InstrumentType t,const InstrumentFourOp& p){
  if(!validFMControls(p.amp,p.tone)||!isFourOp(t)||p.schema!=1||p.algorithm>7||p.feedback>7||p.pan<1||p.pan>3||p.amplitudeSensitivity>3||p.pitchSensitivity>7||p.lfoEnabled>1||p.lfoWave>3||p.amplitudeDepth>127||p.pitchDepth>127||p.operatorMask>15||p.fineTune < -100||p.fineTune>100||!memchr(p.presetName,0,64))return false;
  if(t==InstrumentType::GenesisFM&&(p.lfoRate>7||p.lfoWave||p.amplitudeDepth||p.pitchDepth))return false;
  for(const auto& o:p.operators){if(o.multiplier>15||o.detune>7||o.level>127||o.keyScale>3||o.attack>31||o.decay>31||o.sustainRate>31||o.release>15||o.sustainLevel>15||o.ssg>15||o.detune2>3||o.amplitudeMod>1)return false;
    if((t==InstrumentType::GenesisFM&&o.detune2)||(t==InstrumentType::ArcadeFM&&o.ssg))return false;}
  return true;
}
void initFourOpPatch(InstrumentFourOp* p){
  *p={};p->schema=1;p->algorithm=4;p->pan=3;p->operatorMask=15;
  p->operators[0]={1,0,30,1,31,10,3,7,5,0,0,0};
  p->operators[1]={1,0,8,1,31,8,2,7,4,0,0,0};
  p->operators[2]={3,0,40,1,31,12,4,7,6,0,0,0};
  p->operators[3]={1,0,12,1,31,8,2,7,4,0,0,0};strcpy(p->presetName,"Twin Reed");
}
static bool csv(const char* text,int* out,int n){
  for(int i=0;i<n;++i){char* end;errno=0;long v=strtol(text,&end,10);if(errno||end==text||v < -100||v>65535)return false;out[i]=int(v);text=end;if(i+1<n&&*text++!=',')return false;}return !*text;
}
int loadFourOpData(FILE* file,Instrument* instrument){
  InstrumentFourOp p{};unsigned seen=0;bool ampSeen=false,toneSeen=false;
  while(char* line=peekLine(file)){
    if(line[0]=='#')break;int v[15]{};
    if(!strncmp(line,"- FourOp base: ",15)){
      if((seen&1)||!csv(line+15,v,15))return 1;
      for(int i=0;i<15;++i)if(v[i]<(i==12?-100:0)||v[i]>(i==12?100:i>=13?65535:255))return 1;
      p.schema=v[0];p.algorithm=v[1];p.feedback=v[2];p.pan=v[3];p.amplitudeSensitivity=v[4];p.pitchSensitivity=v[5];p.lfoEnabled=v[6];p.lfoRate=v[7];p.lfoWave=v[8];p.amplitudeDepth=v[9];p.pitchDepth=v[10];p.operatorMask=v[11];p.fineTune=v[12];p.bankId=v[13];p.sourceProgram=v[14];seen|=1;
    }else if(!strncmp(line,"- FourOp name: ",15)){
      if((seen&2)||strlen(line+15)>=64)return 1;strcpy(p.presetName,line+15);seen|=2;
    }else if(!strncmp(line,"- FourOp op",11)&&line[11]>='0'&&line[11]<='3'&&line[12]==':'&&line[13]==' '){
      int i=line[11]-'0';unsigned bit=4u<<i;if((seen&bit)||!csv(line+14,v,12))return 1;
      for(int j=0;j<12;++j)if(v[j]<0||v[j]>255)return 1;
      p.operators[i]={(uint8_t)v[0],(uint8_t)v[1],(uint8_t)v[2],(uint8_t)v[3],(uint8_t)v[4],(uint8_t)v[5],(uint8_t)v[6],(uint8_t)v[7],(uint8_t)v[8],(uint8_t)v[9],(uint8_t)v[10],(uint8_t)v[11]};seen|=bit;
    }else if(loadFMAmpSetting(line,p.amp,ampSeen)!=1 && loadFMToneSetting(line,p.tone,toneSeen)!=1)return 1;consumeLine(file);
  }
  if(seen!=63||!validFourOp(instrument->type,p))return 1;instrument->chip.fourOp=p;return 0;
}
int saveFourOpData(FILE* file,const Instrument* instrument){
  const auto& p=instrument->chip.fourOp;
  fprintf(file,"- FourOp base: %u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u\n",p.schema,p.algorithm,p.feedback,p.pan,p.amplitudeSensitivity,p.pitchSensitivity,p.lfoEnabled,p.lfoRate,p.lfoWave,p.amplitudeDepth,p.pitchDepth,p.operatorMask,p.fineTune,p.bankId,p.sourceProgram);
  fprintf(file,"- FourOp name: %s\n",p.presetName);
  for(int i=0;i<4;++i){const auto& o=p.operators[i];fprintf(file,"- FourOp op%d: %u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",i,o.multiplier,o.detune,o.level,o.keyScale,o.attack,o.decay,o.sustainRate,o.release,o.sustainLevel,o.ssg,o.detune2,o.amplitudeMod);}
  saveFMAmpSetting(file,p.amp);saveFMToneSetting(file,p.tone);return ferror(file)?1:0;
}
