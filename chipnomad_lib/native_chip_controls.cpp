#include "project.h"
#include "sid_patch.h"
#include "monk_patch.h"
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <initializer_list>

static const InstrumentModDestination* directModDestination(InstrumentType type,int index);

int instrumentFMOperatorCount(const Instrument* i) {
  if(!i)return 0;
  switch(i->type) {
    case InstrumentType::OPLL:case InstrumentType::VRC7:case InstrumentType::OPL2:return 2;
    case InstrumentType::OPL3:return i->chip.opl.topology==OPLTopology::twoOperator?2:4;
    case InstrumentType::GenesisFM:case InstrumentType::ArcadeFM:return 4;
    case InstrumentType::DX7:return 6;
    default:return 0;
  }
}

bool instrumentNativeFXInfo(const Instrument* i,int fx,NativeFXInfo* out,int op) {
  if(!i||!out)return false;
  if (i->type == InstrumentType::SoundFont) {
    const char* label = nullptr;
    int value = 255;
    switch (fx) {
      case fxFPT: *out={48, std::clamp(int(i->chip.soundfont.controls.tuneCents)/100+24,0,48), false,0,"Pitch"}; return true;
      case fxFVL: label="Volume"; value=i->volume; break;
      case fxFPN: label="Pan"; value=i->pan; break;
      case fxFCF: label="Cutoff"; break;
      case fxFRS: label="Resonance"; break;
      case fxFAT: label="Attack"; break;
      case fxFDC: label="Decay"; break;
      case fxFRL: label="Release"; break;
      case fxFHD: label="Hold"; break;
      case fxFSU: label="Sustain"; break;
      default: return false;
    }
    *out = {255, value, false, 0, label}; return true;
  }
  if (i->type == InstrumentType::Monk) {
    const auto& p = i->chip.monk;
    for (int parameter = 0; parameter < monkParameterCount; ++parameter) {
      const auto& control = monkControls[parameter];
      if (control.fx == fx) {
        *out = {control.maximum, std::clamp(monkValue(p, parameter), control.minimum, control.maximum),
                false, control.minimum, control.name};
        return true;
      }
    }
    return false;
  }

  if(fx>=fxOAR&&fx<=fxLEN)return instrumentDirectFMInfo(i,fx,out,op);
  if(fx==fxFBK) {
    if(!instrumentFMOperatorCount(i))return false;
    int value=0;
    if(i->type==InstrumentType::OPLL||i->type==InstrumentType::VRC7)value=i->chip.opll.patch[3]&7;
    else if(i->type==InstrumentType::OPL2||i->type==InstrumentType::OPL3)value=i->chip.opl.feedback[0];
    else if(i->type==InstrumentType::DX7)value=i->chip.dx7.voice[135];
    else value=i->chip.fourOp.feedback;
    const auto* tone=instrumentFMToneSettings(const_cast<Instrument*>(i));
    if(tone->feedback)value=tone->feedback-1;
    *out={7,value,false};return true;
  }
  if(fx>=fxOL1&&fx<=fxOL6) {
    int op=fx-fxOL1;
    if(op>=instrumentFMOperatorCount(i))return false;
    int maximum=63,value=0;
    switch(i->type) {
      case InstrumentType::OPLL:case InstrumentType::VRC7:
        maximum=op?15:63;value=op?15:63-(i->chip.opll.patch[2]&63);break;
      case InstrumentType::OPL2:case InstrumentType::OPL3:value=63-i->chip.opl.operators[op].level;break;
      case InstrumentType::GenesisFM:case InstrumentType::ArcadeFM:
        maximum=127;value=127-i->chip.fourOp.operators[op].level;break;
      case InstrumentType::DX7:maximum=99;value=i->chip.dx7.voice[(5-op)*21+16];break;
      default:return false;
    }
    *out={maximum,value,false};return true;
  }
  if(!instrumentFXAvailableForInstrument(i,fx))return false;
  for(int g=genericModFMBrightness;g<genericModTotalCount;++g) {
    const auto* d=instrumentNativeModDestination(i->type,g);
    if(!d||d->fx!=fx)continue;
    int value=instrumentNativeControlValue(i,g);
    *out={d->range,value,false};
    if(fx==fxSMR||fx==fxSWV||fx==fxSPR) {
      out->minimum=1;
      out->label=fx==fxSMR?"SID macro speed":fx==fxSWV?"SID waveform":"SID partner ratio";
    }
    return true;
  }
  return false;
}

const InstrumentModDestination* instrumentNativeModDestination(InstrumentType t, int g) {
  if(g==genericModFMBrightness||(g>=genericModFMTime&&g<=genericModFMLFODepth))return nullptr;
  static const InstrumentModDestination controls[] = {
    {"Brightness",fxFBR,255,InstrumentMotionValue::raw},
    {"Feedback",fxFBK,7,InstrumentMotionValue::raw},
    {"Mode",fxCMD,3,InstrumentMotionValue::raw},
    {"Noise rate",fxCNR,3,InstrumentMotionValue::raw},
    {"Noise divisor",fxCND,7,InstrumentMotionValue::raw},
    {"Noise shift",fxCNS,13,InstrumentMotionValue::raw},
    {"Sweep period",fxCSP,7,InstrumentMotionValue::raw},
    {"Sweep shift",fxCSS,7,InstrumentMotionValue::raw},
    {"Sweep down",fxCSD,1,InstrumentMotionValue::raw},
    {"GB env level",fxCEI,15,InstrumentMotionValue::raw},
    {"GB env period",fxCEP,7,InstrumentMotionValue::raw},
    {"GB env rise",fxCED,1,InstrumentMotionValue::raw},
  };
  static const InstrumentModDestination segaMode={"Tone / Noise",fxCMD,2,InstrumentMotionValue::raw};
  static const InstrumentModDestination pulseMode={"Duty",fxCMD,3,InstrumentMotionValue::raw};
  static const InstrumentModDestination noiseMode={"Noise width",fxCMD,1,InstrumentMotionValue::raw};
  if(g<genericModFMBrightness||g>=genericModTotalCount)return nullptr;
  static const InstrumentModDestination sid[] = {
    {"Pulse width",fxSCP,255,InstrumentMotionValue::raw},
    {"SID cutoff",fxSCT,255,InstrumentMotionValue::raw},
    {"SID resonance",fxSRN,15,InstrumentMotionValue::raw},
    {"SID waveform",fxSWV,8,InstrumentMotionValue::raw},
    {"SID filter mode",fxSFTY,7,InstrumentMotionValue::raw},
    {"Macro speed",fxSMR,200,InstrumentMotionValue::raw},
    {"Ring modulation",fxSRG,1,InstrumentMotionValue::raw},
    {"Hard sync",fxSSY,1,InstrumentMotionValue::raw},
  };
  if(g>=genericModSIDPulse&&g<=genericModSIDSync)return t==InstrumentType::SID?&sid[g-genericModSIDPulse]:nullptr;
  bool fm=t==InstrumentType::OPLL||t==InstrumentType::VRC7||t==InstrumentType::OPL2||t==InstrumentType::OPL3||t==InstrumentType::GenesisFM||t==InstrumentType::ArcadeFM||t==InstrumentType::DX7;
  static const InstrumentModDestination sidEnvelope[] = {
    {"SID attack",fxSAT,15,InstrumentMotionValue::raw},
    {"SID decay",fxSDE,15,InstrumentMotionValue::raw},
    {"SID sustain",fxSSU,15,InstrumentMotionValue::raw},
    {"SID release",fxSRL,15,InstrumentMotionValue::raw},
    {"SID partner ratio",fxSPR,16,InstrumentMotionValue::raw},
  };
  if(g>=genericModFirstDirectFM)return directModDestination(t,g-genericModFirstDirectFM);
  if(g>=genericModSIDAttack)return t==InstrumentType::SID?&sidEnvelope[g-genericModSIDAttack]:nullptr;
  if(g>=genericModFMOperator1) {
    return fm?directModDestination(t,78+g-genericModFMOperator1):nullptr;
  }
  if(g<=genericModFMFeedback)return fm?&controls[g-genericModFMBrightness]:nullptr;
  if(g==genericModChipMode)return t==InstrumentType::SegaPSG?&segaMode:t==InstrumentType::GBPulse?&pulseMode:t==InstrumentType::GBNoise?&noiseMode:nullptr;
  bool valid=g==genericModChipNoiseRate?t==InstrumentType::SegaPSG:
    g<=genericModChipNoiseShift?t==InstrumentType::GBNoise:
    g<=genericModChipSweepDirection?t==InstrumentType::GBPulse:
    (t==InstrumentType::GBPulse||t==InstrumentType::GBNoise);
  return valid?&controls[g-genericModFMBrightness]:nullptr;
}

InstrumentFMTone* instrumentFMToneSettings(Instrument* i) {
  switch(i->type) {
    case InstrumentType::OPLL:case InstrumentType::VRC7:return &i->chip.opll.tone;
    case InstrumentType::OPL2:case InstrumentType::OPL3:return &i->chip.opl.tone;
    case InstrumentType::GenesisFM:case InstrumentType::ArcadeFM:return &i->chip.fourOp.tone;
    case InstrumentType::DX7:return &i->chip.dx7.tone;
    default:return nullptr;
  }
}
int instrumentNativeControlValue(const Instrument* i,int g) {
  // This access is read-only; the mutable overload also serves the UI editor.
  const auto* tone=instrumentFMToneSettings(const_cast<Instrument*>(i));
  if(tone) {
    int fx,op;
    if(nativeFMModTarget(g,&fx,&op)) { NativeFXInfo info{};return instrumentNativeFXInfo(i,fx,&info,op)?info.preset:0; }


    if(g==genericModFMFeedback){NativeFXInfo info{};return instrumentNativeFXInfo(i,fxFBK,&info)?info.preset:0;}
    return g==genericModFMBrightness?fmBrightnessToByte(tone->brightness):0;
  }
  if(i->type==InstrumentType::SID) {
    const auto* v=i->chip.sid.value;
    switch(g){
      case genericModSIDPulse:return (v[sidPulse]*255+2047)/4095;
      case genericModSIDCutoff:return (v[sidCutoff]*255+1023)/2047;
      case genericModSIDResonance:return v[sidResonance];
      case genericModSIDWave:return v[sidWave];
      case genericModSIDFilterMode:return v[sidFilterMode];
      case genericModSIDMacroRate:return v[sidMacroRate];
      case genericModSIDRing:return v[sidRing];
      case genericModSIDSync:return v[sidSync];
      case genericModSIDAttack:return v[sidAttack];
      case genericModSIDDecay:return v[sidDecay];
      case genericModSIDSustain:return v[sidSustain];
      case genericModSIDRelease:return v[sidRelease];
      case genericModSIDPartner:return v[sidPartnerRatio];
      default:return 0;
    }
  }
  const auto& p=i->chip.simpleChip;
  switch(g) {
    case genericModChipMode:return p.mode;
    case genericModChipNoiseRate:return p.noiseRate;
    case genericModChipNoiseDivisor:return p.noiseDivisor;
    case genericModChipNoiseShift:return p.noiseShift;
    case genericModChipSweepPeriod:return p.sweepPeriod;
    case genericModChipSweepShift:return p.sweepShift;
    case genericModChipSweepDirection:return p.sweepNegate;
    case genericModChipEnvelopeInitial:return p.envelopeInitial;
    case genericModChipEnvelopePeriod:return p.envelopePeriod;
    case genericModChipEnvelopeDirection:return p.envelopeIncrease;
    default:return 0;
  }
}

const char* directFMName(int fx) {
  static const char* names[]={"OAR","ODR","OSR","ORR","OSL","ODT","OMU","OFI","OFM","OE1","OE2","OE4","LFR","LAD","LPD","LAS","LPS","LEN"};
  return fx>=fxOAR&&fx<=fxLEN?names[fx-fxOAR]:"";
}

bool instrumentDirectFMInfo(const Instrument* i,int fx,NativeFXInfo* out,int op) {
  if(!i||!out||fx<fxOAR||fx>fxLEN)return false;
  int count=instrumentFMOperatorCount(i);
  if(!count)return false;
  if(fx<fxLFR&&(op<0||op>=count))return false;
  int value=0,maximum=0;
  const char* label=nullptr;
  bool dx=i->type==InstrumentType::DX7;
  bool four=i->type==InstrumentType::GenesisFM||i->type==InstrumentType::ArcadeFM;
  bool opl=i->type==InstrumentType::OPL2||i->type==InstrumentType::OPL3;
  if(fx>=fxLFR) {
    if(!dx&&!four)return false;
    if(dx) {
      const auto* p=i->chip.dx7.voice;
      switch(fx) {
        case fxLFR:value=p[137];maximum=99;label="LFO rate";break;
        case fxLAD:value=p[140];maximum=99;label="LFO amplitude depth";break;
        case fxLPD:value=p[139];maximum=99;label="LFO pitch depth";break;
        case fxLPS:value=p[143];maximum=7;label="LFO pitch sensitivity";break;
        default:return false;
      }
    } else {
      const auto& p=i->chip.fourOp;bool genesis=i->type==InstrumentType::GenesisFM;
      switch(fx) {
        case fxLFR:value=p.lfoRate;maximum=genesis?7:255;label="LFO rate";break;
        case fxLAD:if(genesis)return false;value=p.amplitudeDepth;maximum=127;label="LFO amplitude depth";break;
        case fxLPD:if(genesis)return false;value=p.pitchDepth;maximum=127;label="LFO pitch depth";break;
        case fxLAS:value=p.amplitudeSensitivity;maximum=3;label="LFO amplitude sensitivity";break;
        case fxLPS:value=p.pitchSensitivity;maximum=7;label="LFO pitch sensitivity";break;
        case fxLEN:value=p.lfoEnabled;maximum=1;label="LFO enabled";break;
        default:return false;
      }
    }
  } else if(dx) {
    const auto* p=i->chip.dx7.voice+(5-op)*21;
    maximum=99;
    switch(fx) {
      case fxOAR:value=p[0];label="Envelope rate 1";break;
      case fxODR:value=p[1];label="Envelope rate 2";break;
      case fxOSR:value=p[2];label="Envelope rate 3";break;
      case fxORR:value=p[3];label="Envelope rate 4";break;
      case fxOSL:value=p[6];label="Envelope level 3";break;
      case fxODT:value=p[20];maximum=14;label="Operator detune";break;
      case fxOMU:value=p[18];maximum=31;label="Frequency coarse";break;
      case fxOFI:value=p[19];label="Frequency fine";break;
      case fxOFM:value=p[17];maximum=1;label="Frequency mode (0=ratio)";break;
      case fxOE1:value=p[4];label="Envelope level 1";break;
      case fxOE2:value=p[5];label="Envelope level 2";break;
      case fxOE4:value=p[7];label="Envelope level 4";break;
      default:return false;
    }
  } else if(four) {
    const auto& p=i->chip.fourOp.operators[op];
    switch(fx) {
      case fxOAR:value=p.attack;maximum=31;label="Attack rate";break;
      case fxODR:value=p.decay;maximum=31;label="Decay rate";break;
      case fxOSR:value=p.sustainRate;maximum=31;label="Sustain rate";break;
      case fxORR:value=p.release;maximum=15;label="Release rate";break;
      case fxOSL:value=p.sustainLevel;maximum=15;label="Sustain attenuation";break;
      case fxODT:value=p.detune;maximum=7;label="Detune (native encoding)";break;
      case fxOMU:value=p.multiplier;maximum=15;label="Frequency multiplier";break;
      default:return false;
    }
  } else if(opl) {
    const auto& p=i->chip.opl.operators[op];maximum=15;
    switch(fx) {
      case fxOAR:value=p.attack;label="Attack rate";break;
      case fxODR:value=p.decay;label="Decay rate";break;
      case fxORR:value=p.release;label="Release rate";break;
      case fxOSL:value=p.sustain;label="Sustain attenuation";break;
      case fxOMU:value=p.multiplier;label="Frequency multiplier";break;
      default:return false;
    }
  } else {
    const auto* p=i->chip.opll.patch;maximum=15;
    switch(fx) {
      case fxOAR:value=p[4+op]>>4;label="Attack rate";break;
      case fxODR:value=p[4+op]&15;label="Decay rate";break;
      case fxORR:value=p[6+op]&15;label="Release rate";break;
      case fxOSL:value=p[6+op]>>4;label="Sustain attenuation";break;
      case fxOMU:value=p[op]&15;label="Frequency multiplier";break;
      default:return false;
    }
  }
  *out={maximum,value,false,0,label};return true;
}


bool nativeFMModTarget(int g,int* fx,int* op) {
  if(g>=genericModFMOperator1&&g<=genericModFMOperator6) {*op=g-genericModFMOperator1;*fx=fxOL1+*op;return true;}
  int n=g-genericModFirstDirectFM;
  if(n<0||n>=78)return false;
  *op=n<72?n/12:0;*fx=n<72?fxOAR+n%12:fxLFR+n-72;return true;
}

static const InstrumentModDestination* directModDestination(InstrumentType type,int index) {
  struct Cache {
    InstrumentModDestination values[int(InstrumentType::totalCount)][84]{};
    char names[int(InstrumentType::totalCount)][84][16]{};
    Cache() {
      for(auto t:{InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7}) {
        Instrument i{};getInstrumentFunctions(t).init(&i);if(t==InstrumentType::OPL3)i.chip.opl.topology=OPLTopology::fourOperator;
        for(int n=0;n<84;++n) {
          int op=n<72?n/12:n>=78?n-78:0;
          int fx=n<72?fxOAR+n%12:n>=78?fxOL1+n-78:fxLFR+n-72;
          NativeFXInfo info{};if(!instrumentNativeFXInfo(&i,fx,&info,op))continue;
          auto* name=names[int(t)][n];
          if(n>=78)snprintf(name,16,"OP%d Level",op+1);
          else if(n<72)snprintf(name,16,"OP%d %s",op+1,directFMName(fx));
          else snprintf(name,16,"%s",directFMName(fx));
          values[int(t)][n]={name,uint8_t(fx),uint16_t(info.maximum),InstrumentMotionValue::raw};
        }
      }
    }
  };
  static const Cache cache;
  if(index<0||index>=84||int(type)<0||int(type)>=int(InstrumentType::totalCount))return nullptr;
  const auto* d=&cache.values[int(type)][index];return d->name?d:nullptr;
}
