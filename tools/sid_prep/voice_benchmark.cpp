// Isolated production SID adapter cost, not a hardware callback measurement.
#include "../../chipnomad_lib/synth/sid_voice.h"
#include <chrono>
#include <cstdio>
#include <vector>
int main(){
 printf("voices,ring_sync,mean_us,p95_us,worst_us,cpu_percent\n");
 for(int complex:{0,1})for(int n:{1,2,4}){
  std::vector<SIDVoice>voices(n);InstrumentSID p;initSIDPatch(&p);p.value[sidSustain]=15;p.value[sidRing]=complex;p.value[sidSync]=complex;
  for(auto& v:voices){v.init(48000);v.configure(&p,6000,.25);v.noteOn();}
  float audio[512];std::vector<double> times;
  for(int block=0;block<600;++block){auto t=std::chrono::steady_clock::now();for(auto& v:voices)v.render(audio,512);if(block>=100)times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count());}
  double sum=0;for(auto t:times)sum+=t;std::sort(times.begin(),times.end());printf("%d,%d,%.3f,%.3f,%.3f,%.2f\n",n,complex,sum/times.size(),times[times.size()*95/100],times.back(),sum/times.size()/106.666667);
 }
}
