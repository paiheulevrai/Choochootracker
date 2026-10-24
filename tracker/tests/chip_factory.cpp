// Host-only IR -> canonical native instrument writer and reload check.
#include "project.h"
#include "project_io_common.h"
#include "opl_patch.h"
#include "four_op_patch.h"
#include "dx7_patch.h"
#include "opll_presets.h"
#include "simple_chip_presets.h"
#include <filesystem>
#include <memory>
#include <cstring>
#include <cstdio>
int main(int argc,char** argv){
  if(argc!=3)return 2;
  if(!strcmp(argv[1],"--validate")) {
    auto p=std::make_unique<Project>();projectInit(p.get());fillFXNames();
    int result=instrumentLoad(p.get(),argv[2],0);projectFree(p.get());return result;
  }
  if(!strcmp(argv[1],"--dx7-default")) {
    auto p=std::make_unique<Project>();projectInit(p.get());getInstrumentFunctions(InstrumentType::DX7).init(&p->instruments[0]);
    p->instruments[0].chip.dx7.bankId=100;int result=instrumentSave(p.get(),argv[2],0);projectFree(p.get());return result;
  }
  if(!strcmp(argv[1],"--builtins")) {
    auto p=std::make_unique<Project>(),q=std::make_unique<Project>();projectInit(p.get());projectInit(q.get());fillFXNames();
    std::filesystem::path output=argv[2];FILE* manifest=fopen((output/"builtins.tsv").string().c_str(),"wb");if(!manifest)return 2;
    fputs("CCT-CHIP-CATALOG\t1\n",manifest);
    for(auto type:{InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::SegaPSG,InstrumentType::GBPulse,InstrumentType::GBNoise}) {
      int first=isOPLL(type)?1:0,count=isOPLL(type)?15:simpleChipPresetCount(type);
      for(int n=first;n<first+count;++n){getInstrumentFunctions(type).init(&p->instruments[0]);
        const char* name=isOPLL(type)?opllPresetName(type,n):simpleChipPresetName(type,n);
        if(isOPLL(type))opllApplyPreset(&p->instruments[0],n);else simpleChipApplyPreset(&p->instruments[0],n);
        char filename[80];snprintf(filename,sizeof(filename),"builtin-%02d-%02d.cni",int(type),n);
        if(instrumentSave(p.get(),(output/filename).string().c_str(),0)||instrumentLoad(q.get(),(output/filename).string().c_str(),0)||memcmp(&p->instruments[0].chip,&q->instruments[0].chip,sizeof(InstrumentChipData)))return 4;
        fprintf(manifest,"%d\t%d\t%s\tUnsorted\t%s\t%s\n",int(type),300+int(type),getInstrumentDefinition(type)->uiName,name,filename);
      }
    }
    fclose(manifest);projectFree(p.get());projectFree(q.get());return 0;
  }
  FILE* f=fopen(argv[1],"rb");if(!f)return 2;
  char header[80];int version=0,type=0;
  if(!fgets(header,sizeof(header),f)||sscanf(header,"CHOOCHOO-OPL-IR %d %d",&version,&type)!=2||version!=1||(!isOPL((InstrumentType)type)&&!isFourOp((InstrumentType)type)&&type!=int(InstrumentType::DX7))){fclose(f);return 2;}
  auto p=std::make_unique<Project>(),q=std::make_unique<Project>();projectInit(p.get());projectInit(q.get());fillFXNames();
  p->instruments[0].type=(InstrumentType)type;getInstrumentFunctions((InstrumentType)type).init(&p->instruments[0]);
  resetPeekConsume();int result=isFourOp((InstrumentType)type)?loadFourOpData(f,&p->instruments[0]):type==int(InstrumentType::DX7)?loadDX7Data(f,&p->instruments[0]):loadOPLData(f,&p->instruments[0]);fclose(f);
  if(result){fprintf(stderr,"Invalid IR: %s\n",argv[1]);return 3;}
  strncpy(p->instruments[0].name,isFourOp((InstrumentType)type)?p->instruments[0].chip.fourOp.presetName:type==int(InstrumentType::DX7)?p->instruments[0].chip.dx7.presetName:p->instruments[0].chip.opl.presetName,PROJECT_INSTRUMENT_NAME_LENGTH);
  if(instrumentSave(p.get(),argv[2],0)||instrumentLoad(q.get(),argv[2],0))return 4;
  if(memcmp(&p->instruments[0].chip,&q->instruments[0].chip,sizeof(InstrumentChipData)))return 5;
  projectFree(p.get());projectFree(q.get());return 0;
}
