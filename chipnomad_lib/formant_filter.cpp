// Inspired by JM Vowel Filter, Copyright 2023 Józef Maćkowiak (MIT).
// Native implementation of standard RBJ peaking EQ equations; no Tale source
// or JSFX runtime copied. Two +18 dB peaks, -18 dB wet compensation, Q 2..16.
#include "formant_filter.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr int period=32;
constexpr float positions[]={0,32,64,96,127};
constexpr FormantFrequencies vowels[]={{850,1610},{390,2300},{240,2400},{360,640},{250,595}};
float bounded(float v,float lo,float hi) { return std::isfinite(v)?std::clamp(v,lo,hi):lo; }
float logInterpolate(float a,float b,float t) { return std::exp(std::log(a)+t*std::log(b/a)); }
float protectedWet(double value) {
  if (!std::isfinite(value)) return 0;
  const double x=std::abs(value);
  // Unity below the knee; asymptotic +/-1.2 protection for coincident formants.
  if (x<=.9) return float(value);
  return float(std::copysign(.9+.3*(x-.9)/(.3+x-.9),value));
}
}
FormantFrequencies formantVowelFrequencies(float value) {
  value=bounded(value,0,127);
  for(int i=0;i<5;++i) if(value==positions[i]) return vowels[i];
  int i=0;while(i<3 && value>positions[i+1])++i;
  const float t=(value-positions[i])/(positions[i+1]-positions[i]);
  return {logInterpolate(vowels[i].f1,vowels[i+1].f1,t),
          logInterpolate(vowels[i].f2,vowels[i+1].f2,t)};
}
float formantCustomFrequency(int formant,float value) {
  const float x=bounded(value,0,255)/255;
  return formant ? logInterpolate(500,2800,x) : logInterpolate(200,1200,x);
}
float formantResonance(float value) { return logInterpolate(2,16,bounded(value,0,255)/255); }
void FormantFilter::reset(float sampleRate) {
  *this=FormantFilter{};
  rate_=bounded(sampleRate,1000,384000);
  slew_=1-std::exp(-period/(rate_*.008f));
  mixSlew_=1-std::exp(-1/(rate_*.005f));
}
void FormantFilter::set(const uint8_t v[8]) {
  const auto f=v[0] ? FormantFrequencies{formantCustomFrequency(0,v[2]),formantCustomFrequency(1,v[3])}
                    : formantVowelFrequencies(v[1]);
  targetLog_[0]=std::log(std::clamp(f.f1,20.f,rate_*.45f));
  targetLog_[1]=std::log(std::clamp(f.f2,20.f,rate_*.45f));
  targetLog_[2]=std::log(formantResonance(v[4]));
  targetMix_=v[5]/255.f;
  if(!initialized_) {
    for(int i=0;i<3;++i)currentLog_[i]=targetLog_[i];
    mix_=targetMix_;
    update();
    for(int i=0;i<2;++i) { coefficients_[i]=destination_[i];delta_[i]={0,0,0,0,0}; }
    initialized_=true;
  }
}
void FormantFilter::update() {
  bool changed = !initialized_;
  for(int i=0;i<3;++i) if(currentLog_[i] != targetLog_[i]) {
    changed = true;
    currentLog_[i] += slew_*(targetLog_[i]-currentLog_[i]);
    if(std::abs(currentLog_[i]-targetLog_[i]) < 1e-5f) currentLog_[i]=targetLog_[i];
  }
  if(!changed) {
    for(auto& d:delta_) d={0,0,0,0,0};
    controlRemaining_=period;
    return;
  }
  const double q=std::exp(currentLog_[2]);
  constexpr double A=2.8183829312644537; // 10^(18/40)
  for(int i=0;i<2;++i) {
    const double w=6.283185307179586*std::exp(currentLog_[i])/rate_;
    const double alpha=std::sin(w)/(2*q),c=std::cos(w),a0=1+alpha/A;
    auto& t=destination_[i];
    t={(1+alpha*A)/a0,-2*c/a0,(1-alpha*A)/a0,-2*c/a0,(1-alpha/A)/a0};
    const auto& b=coefficients_[i];
    delta_[i]={(t.b0-b.b0)/period,(t.b1-b.b1)/period,(t.b2-b.b2)/period,
               (t.a1-b.a1)/period,(t.a2-b.a2)/period};
  }
  controlRemaining_=period;
}
void FormantFilter::process(float* left,float* right,int frames) {
  if(!initialized_)return;
  float* channels[]={left,right};
  for(int n=0;n<frames;++n) {
    if(!controlRemaining_)update();
    for(int i=0;i<2;++i) {
      auto& c=coefficients_[i];const auto& d=delta_[i];
      c.b0+=d.b0;c.b1+=d.b1;c.b2+=d.b2;c.a1+=d.a1;c.a2+=d.a2;
    }
    mix_+=mixSlew_*(targetMix_-mix_);
    if(std::abs(targetMix_-mix_)<1e-6f)mix_=targetMix_;
    for(int ch=0;ch<2;++ch) {
      const float dry=std::isfinite(channels[ch][n])?channels[ch][n]:0;
      double x=dry;
      for(int i=0;i<2;++i) {
        const auto& c=coefficients_[i];auto& h=history_[ch][i];
        double y=c.b0*x+c.b1*h.x1+c.b2*h.x2-c.a1*h.y1-c.a2*h.y2;
        if(!std::isfinite(y) || std::abs(y)>1e8) { h={};y=0; }
        if(std::abs(y)<1e-24)y=0;
        h.x2=h.x1;h.x1=x;h.y2=h.y1;h.y1=y;x=y;
      }
      const float wet=protectedWet(x*.12589254117941673);
      channels[ch][n]=mix_==0 ? dry : dry+mix_*(wet-dry);
    }
    if(!--controlRemaining_)for(int i=0;i<2;++i)coefficients_[i]=destination_[i];
  }
}
