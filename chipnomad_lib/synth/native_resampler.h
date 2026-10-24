#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
// Bounded streaming FIR for native FM rates. Coefficients prepared off callback.
class NativeResampler {
 public:
  void init(double nativeRate, double hostRate) {
    ratio_ = nativeRate / std::max(8000.0, hostRate);
    const double cutoff = .46 * std::min(1.0, 1.0 / ratio_);
    for (int p=0;p<64;++p) {
      double sum=0;
      for(int t=0;t<24;++t) {
        double x=t-11.0+p/64.0;
        double sinc=std::abs(x)<1e-9?2*cutoff:std::sin(2*3.141592653589793*cutoff*x)/(3.141592653589793*x);
        filter_[p][t]=sinc*(.5+.5*std::cos(3.141592653589793*x/12));sum+=filter_[p][t];
      }
      for(float& c:filter_[p])c/=sum;
    }
    reset();
  }
  void reset() { phase_=0;position_=0;std::memset(history_,0,sizeof(history_)); }
  template<class Source> void next(Source source, float& left, float& right) {
    phase_+=ratio_;
    while(phase_>=1) { phase_-=1;position_=(position_+1)&31;source(history_[0][position_],history_[1][position_]); }
    int p=std::min(63,int(phase_*64));left=right=0;
    for(int t=0;t<24;++t) { left+=history_[0][(position_-t)&31]*filter_[p][t];right+=history_[1][(position_-t)&31]*filter_[p][t]; }
  }
 private:
  double ratio_=1,phase_=0;
  int position_=0;
  float filter_[64][24]{},history_[2][32]{};
};
