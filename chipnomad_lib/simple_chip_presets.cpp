#include "simple_chip_presets.h"
#include "chip_program.h"
#include <cstring>
#include <iterator>
// Original tracker recipes (MIT), no game extractions. Software ADSR values
// use the existing squared 0..5-second mapping; native GB envelope runs at 64Hz.
struct Recipe {const char* name;uint8_t mode,rate,divisor,shift,initial,period,increase,sweep,steps,negate,attack,decay,sustain,release;};
static const Recipe sega[]={
 {"Bright Pluck",0,0,0,0,15,0,0,0,0,0,0,55,0,35},
 {"Round Bass",0,0,0,0,15,0,0,0,0,0,0,75,80,35},
 {"Square Hold",0,0,0,0,15,0,0,0,0,0,0,0,255,35},
 {"Soft Pad",0,0,0,0,15,0,0,0,0,0,80,70,190,100},
 {"Short Key",0,0,0,0,15,0,0,0,0,0,0,30,0,20},
 {"Organ Tone",0,0,0,0,15,0,0,0,0,0,12,0,255,15},
 {"Long Pluck",0,0,0,0,15,0,0,0,0,0,0,115,0,65},
 {"Noise Hat",1,0,0,0,15,0,0,0,0,0,0,22,0,10},
 {"Open Hat",1,0,0,0,15,0,0,0,0,0,0,70,0,25},
 {"Noise Snare",1,1,0,0,15,0,0,0,0,0,0,44,0,25},
 {"Low Burst",1,2,0,0,15,0,0,0,0,0,0,70,0,30},
 {"Metal Tick",2,0,0,0,15,0,0,0,0,0,0,35,0,20},
 {"Motor",2,3,0,0,15,0,0,0,0,0,0,0,255,40},
 {"Wind Rise",1,2,0,0,15,0,0,0,0,0,140,40,200,130},
 {"Linked Static",1,3,0,0,15,0,0,0,0,0,0,0,255,50},
 {"Tuned Snare",1,3,0,0,15,0,0,0,0,0,0,60,0,22},
 {"Periodic Bass",2,3,0,0,15,0,0,0,0,0,0,100,110,40},
 {"Buzz Pluck",2,3,0,0,15,0,0,0,0,0,0,65,0,25},
 {"Clock Tick",2,1,0,0,15,0,0,0,0,0,0,18,0,8},
 {"Clock Drone",2,1,0,0,15,0,0,0,0,0,35,0,255,65},
 {"Low Clock",2,2,0,0,15,0,0,0,0,0,0,105,100,60},
 {"Buzz Swell",2,3,0,0,15,0,0,0,0,0,140,0,255,110},
 {"Static Wash",1,0,0,0,15,0,0,0,0,0,80,0,255,120},
 {"Wood Knock",0,0,0,0,15,0,0,0,0,0,0,18,0,8},
};
static const Recipe pulse[]={
 {"Hollow Lead",1,0,0,0,15,0,0,0,0,0,0,0,255,35},
 {"Square Bass",2,0,0,0,15,3,0,0,0,0,0,90,100,30},
 {"Thin Pluck",0,0,0,0,15,2,0,0,0,0,0,75,0,30},
 {"Wide Key",3,0,0,0,13,3,0,0,0,0,0,95,0,45},
 {"Pulse Organ",2,0,0,0,12,0,0,0,0,0,8,0,255,20},
 {"Soft Pulse",1,0,0,0,10,0,0,0,0,0,75,40,200,95},
 {"Down Sweep",2,0,0,0,15,2,0,3,2,1,0,80,0,25},
 {"Up Blip",0,0,0,0,12,2,0,2,3,0,0,55,0,20},
 {"Rising Pulse",3,0,0,0,3,3,1,0,0,0,0,0,255,60},
 {"Short Click",1,0,0,0,15,1,0,0,0,0,0,20,0,10},
 {"Deep Drop",2,0,0,0,15,2,0,1,1,1,0,85,0,25},
 {"Laser Fall",0,0,0,0,15,1,0,2,2,1,0,80,0,20},
 {"Slow Fall",1,0,0,0,13,4,0,7,3,1,0,120,0,50},
 {"Coin Rise",2,0,0,0,14,2,0,1,4,0,0,50,0,20},
 {"Arcade Zap",3,0,0,0,15,1,0,1,2,0,0,35,0,15},
 {"Chirp",1,0,0,0,15,1,0,3,4,0,0,30,0,10},
 {"Narrow Reed",0,0,0,0,12,0,0,0,0,0,15,0,255,55},
 {"Hollow Pluck",1,0,0,0,15,3,0,0,0,0,0,105,0,50},
 {"Soft Square",2,0,0,0,8,0,0,0,0,0,110,0,255,110},
 {"Wide Swell",3,0,0,0,1,5,1,0,0,0,0,0,255,100},
 {"Narrow Swell",0,0,0,0,1,3,1,0,0,0,0,0,255,80},
 {"Slow Pitch Pad",1,0,0,0,10,0,0,7,7,1,90,0,255,110},
 {"Falling Bell",2,0,0,0,15,6,0,6,6,1,0,135,0,80},
 {"Tiny Kick",2,0,0,0,15,1,0,1,2,1,0,45,0,15},
};
static const Recipe noise[]={
 {"Closed Hat",0,0,1,0,12,1,0,0,0,0,0,25,0,10},
 {"Open Hat",0,0,2,1,14,3,0,0,0,0,0,85,0,35},
 {"Snare Burst",0,0,3,3,15,2,0,0,0,0,0,65,0,25},
 {"Brush",0,0,5,2,9,4,0,0,0,0,10,100,0,50},
 {"Metal Hat",1,0,1,1,12,2,0,0,0,0,0,45,0,20},
 {"Rattle",1,0,4,4,15,3,0,0,0,0,0,100,0,35},
 {"Low Noise",0,0,7,5,15,4,0,0,0,0,0,120,0,60},
 {"Noise Click",0,0,0,0,15,1,0,0,0,0,0,16,0,8},
 {"Rising Wind",0,0,3,4,1,4,1,0,0,0,0,0,255,100},
 {"Chip Alarm",1,0,2,5,12,0,0,0,0,0,0,0,255,30},
 {"Tight Snare",0,0,2,3,15,1,0,0,0,0,0,45,0,15},
 {"Deep Snare",0,0,5,4,15,2,0,0,0,0,0,70,0,25},
 {"Clap Burst",0,0,4,2,15,1,0,0,0,0,0,45,0,20},
 {"Shaker",0,0,3,1,10,2,0,0,0,0,8,45,0,30},
 {"Crash Wash",0,0,1,2,15,7,0,0,0,0,0,180,0,100},
 {"Surf",0,0,6,3,8,0,0,0,0,0,140,0,255,130},
 {"Steam",0,0,0,1,9,0,0,0,0,0,55,0,255,65},
 {"Thunder",0,0,7,7,15,5,0,0,0,0,30,170,0,90},
 {"Low Crackle",0,0,5,9,13,4,0,0,0,0,0,180,0,80},
 {"Digital Tick",1,0,0,0,15,1,0,0,0,0,0,22,0,10},
 {"Metal Ping",1,0,2,3,15,3,0,0,0,0,0,100,0,55},
 {"Small Tom",1,0,3,5,15,2,0,0,0,0,0,90,0,40},
 {"Low Tom",1,0,6,6,15,3,0,0,0,0,0,110,0,55},
 {"Buzz Drone",1,0,5,3,12,0,0,0,0,0,30,0,255,65},
 {"Robot Growl",1,0,7,6,12,0,0,0,0,0,65,0,255,100},
 {"Power Rise",1,0,1,4,1,3,1,0,0,0,0,0,255,55},
 {"Static Rise",0,0,2,2,1,2,1,0,0,0,0,0,255,55},
 {"Bit Rain",1,0,4,8,15,6,0,0,0,0,20,180,0,90},
};
bool isSimpleChip(InstrumentType t){return t==InstrumentType::SegaPSG||t==InstrumentType::GBPulse||t==InstrumentType::GBNoise;}
int simpleChipPresetCount(InstrumentType t){return t==InstrumentType::SegaPSG?std::size(sega):t==InstrumentType::GBPulse?std::size(pulse):t==InstrumentType::GBNoise?std::size(noise):0;}
static const Recipe* recipe(InstrumentType t,int n){if(n<0||n>=simpleChipPresetCount(t))return nullptr;return &(t==InstrumentType::SegaPSG?sega:t==InstrumentType::GBPulse?pulse:noise)[n];}
const char* simpleChipPresetName(InstrumentType t,int n){auto* r=recipe(t,n);return r?r->name:"Custom";}
bool simpleChipApplyPreset(Instrument* i,int n){auto* r=recipe(i->type,n);if(!r)return false;
 auto& p=i->chip.simpleChip;p={};p.schema=1;p.preset=n;p.mode=r->mode;p.noiseRate=r->rate;p.noiseDivisor=r->divisor;p.noiseShift=r->shift;p.envelopeInitial=r->initial;p.envelopePeriod=r->period;p.envelopeIncrease=r->increase;p.sweepPeriod=r->sweep;p.sweepShift=r->steps;p.sweepNegate=r->negate;p.attack=r->attack;p.decay=r->decay;p.sustain=r->sustain;p.release=r->release;p.filterCutoffHz=20000;p.segaBassExtension=i->type==InstrumentType::SegaPSG;
 strncpy(i->name,r->name,PROJECT_INSTRUMENT_NAME_LENGTH);i->name[PROJECT_INSTRUMENT_NAME_LENGTH]=0;return true;}
bool validSimpleChip(InstrumentType t,const InstrumentSimpleChip& p){
 if(!validChipProgram(p.program)||p.program.format>1)return false;return isSimpleChip(t)&&p.schema==1&&p.mode<=(t==InstrumentType::SegaPSG?2:t==InstrumentType::GBPulse?3:1)&&p.noiseRate<=3&&p.noiseDivisor<=7&&p.noiseShift<=13&&p.envelopeInitial<=15&&p.envelopePeriod<=7&&p.envelopeIncrease<=1&&p.sweepPeriod<=7&&p.sweepShift<=7&&p.sweepNegate<=1&&p.segaBassExtension<=1&&p.fineTune>=-100&&p.fineTune<=100;}
