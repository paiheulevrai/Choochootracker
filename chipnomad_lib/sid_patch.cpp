#include "sid_patch.h"
#include "chip_program.h"
#include "project_io_common.h"
#include <cstring>
#include <cstdlib>
#include <cerrno>
void initSIDPatch(InstrumentSID* p){
 *p={};p->schema=1;p->bankId=600;strcpy(p->presetName,"Moving Pulse 1");
 auto* v=p->value;v[sidWave]=4;v[sidAttack]=1;v[sidDecay]=6;v[sidSustain]=10;v[sidRelease]=5;
 v[sidPulse]=512;v[sidCutoff]=800;v[sidResonance]=8;v[sidFilterMode]=1;
 v[sidPartnerRatio]=2;v[sidMacroRate]=50;v[sidPulseDepth]=128;v[sidPulseRate]=3;
 v[sidCutoffTarget]=65535;
}
bool validSID(const InstrumentSID& p){
 static const unsigned maximum[]={0,8,15,15,15,15,4095,2047,15,7,1,1,16,200,1000,65535,65535,1000,1,2047,1000,4095,1000,65535,4};
 if(!validChipProgram(p.program)||p.schema!=1||!memchr(p.presetName,0,sizeof(p.presetName)))return false;
 for(int i=0;i<sidParameterCount;++i)if(p.value[i]>maximum[i])return false;
 return p.value[sidWave]>0&&p.value[sidMacroRate]>0&&p.value[sidPartnerRatio]>0&&
   (p.value[sidCutoffTarget]<=2047||p.value[sidCutoffTarget]==65535);
}
int loadSIDData(FILE* f,Instrument* i){
 InstrumentSID p{};unsigned seen=0;bool programSeen=false;
 while(char* line=peekLine(f)){
  if(*line=='#')break;
  if(!strncmp(line,"- SID parameters: ",18)){
   if(seen&1)return 1;const char* c=line+18;
   for(int n=0;n<sidParameterCount;++n){char* end;errno=0;long v=strtol(c,&end,10);
    if(c==end||errno||v<0||v>65535||(*end!=(n+1==sidParameterCount?'\0':',')))return 1;
    p.value[n]=v;c=end+1;
   }seen|=1;
  }else if(!strncmp(line,"- SID identity: ",16)){
   int schema,bank;char tail;if((seen&2)||sscanf(line+16,"%d,%d %c",&schema,&bank,&tail)!=2||schema!=1||bank<0||bank>65535)return 1;
   p.schema=schema;p.bankId=bank;seen|=2;
  }else if(!strncmp(line,"- SID name: ",12)){
   if((seen&4)||strlen(line+12)>=sizeof(p.presetName))return 1;strcpy(p.presetName,line+12);seen|=4;
  }else if(loadChipProgramLine(line,p.program,programSeen)!=1)return 1;
  consumeLine(f);
 }
 if(seen!=7||!validSID(p))return 1;i->chip.sid=p;return 0;
}
void saveSIDData(FILE* f,const Instrument* i){
 const auto& p=i->chip.sid;fprintf(f,"- SID identity: %u,%u\n- SID name: %s\n- SID parameters: ",p.schema,p.bankId,p.presetName);
 for(int n=0;n<sidParameterCount;++n)fprintf(f,"%s%u",n?",":"",p.value[n]);fputc('\n',f);saveChipProgram(f,p.program);
}

static bool hasMacro(const InstrumentSID& patch, unsigned id) {
  const auto& p = patch.program;
  if (p.format != 1) return false;
  for (unsigned pos = 0; pos + 8 <= p.size;) {
    if (p.data[pos] == id) return true;
    pos += 8 + 4 * p.data[pos + 1];
  }
  return false;
}
const char* sidImportedControlReason(const InstrumentSID& patch, int parameter) {
  if (patch.program.format == 2) {
    switch (parameter) {
      case sidWave: return "Waveform: imported Goat wave table";
      case sidPulse: return "Pulse width: imported Goat pulse table";
      case sidCutoff: case sidResonance: case sidFilterMode:
        return "Filter: imported Goat filter table";
      case sidAttack: case sidDecay: case sidSustain: case sidRelease:
        return "ADSR: imported Goat instrument/wave table";
      default: return "Goat program bypasses this recipe control";
    }
  }
  switch (parameter) {
    case sidWave:
      if (hasMacro(patch, 3)) return "Waveform: imported waveform macro";
      break;
    case sidPulse: case sidPulseDepth: case sidPulseRate:
      if (hasMacro(patch, 2)) return "Pulse width: imported pulse macro";
      break;
    case sidCutoff: case sidCutoffTarget:
      if (hasMacro(patch, 5)) return "Cutoff: imported filter macro";
      break;
    case sidFixedFrequency:
      if (hasMacro(patch, 1)) return "Pitch: imported arpeggio macro";
      break;
    default: break;
  }
  return nullptr;
}
