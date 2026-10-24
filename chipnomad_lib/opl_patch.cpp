#include "opl_patch.h"
#include "project_io_common.h"
#include "fm_amp.h"
#include <cstring>
#include <cstdlib>
#include <cerrno>

bool isOPL(InstrumentType t){return t==InstrumentType::OPL2||t==InstrumentType::OPL3;}
bool validOPL(InstrumentType type,const InstrumentOPL& p){
  if(!validFMControls(p.amp,p.tone)||!isOPL(type)||p.schema!=1||int(p.topology)>2||p.deepVibrato>1||p.deepTremolo>1||p.percussion>1||p.fixedNote>1||p.drumKey>127||p.volumeModel>11||p.fineTune < -100||p.fineTune>100)return false;
  if(type==InstrumentType::OPL2&&p.topology!=OPLTopology::twoOperator)return false;
  for(int i=0;i<2;++i)if(p.feedback[i]>7||p.connection[i]>1||p.pan[i]<1||p.pan[i]>3||p.noteOffset[i]<-127||p.noteOffset[i]>127)return false;
  for(const auto& o:p.operators)if(o.multiplier>15||o.level>63||o.attack>15||o.decay>15||o.sustain>15||o.release>15||o.waveform>(type==InstrumentType::OPL2?3:7)||o.keyScale>3||o.vibrato>1||o.tremolo>1||o.sustained>1||o.rateScale>1)return false;
  return memchr(p.presetName,0,sizeof(p.presetName));
}
void initOPLPatch(InstrumentOPL* p){
  *p={};p->schema=1;p->pan[0]=p->pan[1]=3;p->drumKey=60;
  p->operators[0]={1,24,15,4,6,5,0,0,0,0,0,1};
  p->operators[1]={1,0,15,3,5,5,0,0,0,0,0,1};
  strcpy(p->presetName,"Soft FM Keys");
}
static bool csv(const char* text,int* out,int n){
  for(int i=0;i<n;++i){char* end;errno=0;long v=strtol(text,&end,10);if(errno||end==text||v < -32768||v>65535)return false;out[i]=int(v);text=end;
    if(i+1<n){if(*text++!=',')return false;}}
  return *text==0;
}
int loadOPLData(FILE* file,Instrument* instrument){
  InstrumentOPL p{};unsigned seen=0;bool ampSeen=false,toneSeen=false;
  while(char* line=peekLine(file)){
    if(line[0]=='#')break;
    int v[24]{};
    if(!strncmp(line,"- OPL base: ",12)){
      if((seen&1)||!csv(line+12,v,24))return 1;
      // Reject before narrowing, including signed source attributes.
      for(int i=0;i<24;++i){int lo=(i>=13&&i<=17)?-128:0;int hi=(i>=13&&i<=17)?127:(i==18||i==19||i==20||i==21||i==22)?65535:255;if(v[i]<lo||v[i]>hi)return 1;}
      p.schema=v[0];p.topology=(OPLTopology)v[1];p.feedback[0]=v[2];p.feedback[1]=v[3];p.connection[0]=v[4];p.connection[1]=v[5];p.pan[0]=v[6];p.pan[1]=v[7];p.deepVibrato=v[8];p.deepTremolo=v[9];p.percussion=v[10];p.fixedNote=v[11];p.drumKey=v[12];p.noteOffset[0]=v[13];p.noteOffset[1]=v[14];p.secondDetune=v[15];p.velocityOffset=v[16];p.fineTune=v[17];p.keyOnDuration=v[18];p.keyOffDuration=v[19];p.bankId=v[20];p.sourceBank=v[21];p.sourceProgram=v[22];p.volumeModel=v[23];seen|=1;
    }else if(!strncmp(line,"- OPL name: ",12)){
      if((seen&2)||strlen(line+12)>=sizeof(p.presetName))return 1;strcpy(p.presetName,line+12);seen|=2;
    }else if(!strncmp(line,"- OPL op",8)&&line[8]>='0'&&line[8]<='3'&&line[9]==':'&&line[10]==' '){
      int i=line[8]-'0';unsigned bit=4u<<i;if((seen&bit)||!csv(line+11,v,12))return 1;
      for(int j=0;j<12;++j)if(v[j]<0||v[j]>255)return 1;
      p.operators[i]={(uint8_t)v[0],(uint8_t)v[1],(uint8_t)v[2],(uint8_t)v[3],(uint8_t)v[4],(uint8_t)v[5],(uint8_t)v[6],(uint8_t)v[7],(uint8_t)v[8],(uint8_t)v[9],(uint8_t)v[10],(uint8_t)v[11]};seen|=bit;
    }else if(loadFMAmpSetting(line,p.amp,ampSeen)!=1 && loadFMToneSetting(line,p.tone,toneSeen)!=1)return 1;
    consumeLine(file);
  }
  if(seen!=63||!validOPL(instrument->type,p))return 1;instrument->chip.opl=p;return 0;
}
int saveOPLData(FILE* file,const Instrument* instrument){
  const auto& p=instrument->chip.opl;
  fprintf(file,"- OPL base: %u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%u,%u,%u,%u,%u,%u\n",p.schema,unsigned(p.topology),p.feedback[0],p.feedback[1],p.connection[0],p.connection[1],p.pan[0],p.pan[1],p.deepVibrato,p.deepTremolo,p.percussion,p.fixedNote,p.drumKey,p.noteOffset[0],p.noteOffset[1],p.secondDetune,p.velocityOffset,p.fineTune,p.keyOnDuration,p.keyOffDuration,p.bankId,p.sourceBank,p.sourceProgram,p.volumeModel);
  fprintf(file,"- OPL name: %s\n",p.presetName);
  for(int i=0;i<4;++i){const auto& o=p.operators[i];fprintf(file,"- OPL op%d: %u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",i,o.multiplier,o.level,o.attack,o.decay,o.sustain,o.release,o.waveform,o.keyScale,o.vibrato,o.tremolo,o.sustained,o.rateScale);}
  saveFMAmpSetting(file,p.amp);saveFMToneSetting(file,p.tone);return ferror(file)?1:0;
}
