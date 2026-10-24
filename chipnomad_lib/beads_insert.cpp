#include "beads_insert.h"
#include "external/mutable/beads/dsp/processor.h"
#include "external/mutable/beads/dsp/host_random.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>

namespace {
// Push-driven, bounded stereo FIR. Conversion is independent of host callback
// size; coefficients are calculated once, outside the audio callback.
struct Converter {
  float coefficients[64][32]{};
  beads::FloatFrame history[32]{};
  double ratio = 1, phase = 0;
  int position = 0;
  void init(double source, double destination) {
    ratio = destination / source;
    double cutoff = .46 * std::min(1.0, ratio);
    for (int p = 0; p < 64; ++p) {
      double sum = 0;
      for (int t = 0; t < 32; ++t) {
        double x = t - 15.0 - p / 64.0;
        double sinc = std::abs(x) < 1e-9 ? 2 * cutoff :
            std::sin(6.283185307179586 * cutoff * x) / (3.141592653589793 * x);
        coefficients[p][t] = sinc * (.5 + .5 * std::cos(3.141592653589793 * x / 16));
        sum += coefficients[p][t];
      }
      for (float& c : coefficients[p]) c /= sum;
    }
    reset();
  }
  void reset() { phase = 0; position = 0; std::memset(history, 0, sizeof(history)); }
  template<class Emit> void push(beads::FloatFrame input, Emit emit) {
    position = (position + 1) & 31;
    history[position] = input;
    phase += ratio;
    while (phase >= 1.0) {
      phase -= 1.0;
      int p = std::min(63, int(phase / ratio * 64));
      beads::FloatFrame out{};
      for (int t = 0; t < 32; ++t) {
        const auto& h = history[(position - t) & 31];
        out.l += h.l * coefficients[p][t];
        out.r += h.r * coefficients[p][t];
      }
      emit(out);
    }
  }
};
}

struct BeadsInsert::Impl {
  static constexpr int quantum = beads::kHostBlockSize;
  beads::Processor processor;
  alignas(4) uint8_t recording[3][262144]{};
  uint16_t reverb[32768]{};
  Converter input, output;
  beads::FloatFrame in[quantum]{}, out[quantum]{}, queue[256]{};
  bool frozen[quantum]{};
  int fill = 0, read = 0, write = 0, available = 0;
  float hostRate, mix = 0;
  uint32_t random = 0x42656164;
  explicit Impl(float sr) : hostRate(std::clamp(sr, 8000.0f, 192000.0f)) {
    input.init(hostRate, 48000);
    output.init(48000, hostRate);
    reset();
  }
  void reset() {
    // Upstream constructors assume zero-initialized firmware globals. Rebuild
    // the object in preallocated storage so resets also clear every history.
    processor.~Processor();
    std::memset(&processor, 0, sizeof(processor));
    new (&processor) beads::Processor;
    uint8_t* banks[] = {recording[0], recording[1], recording[2]};
    processor.Init(banks, reverb);
    input.reset();
    output.reset();
    std::memset(queue, 0, sizeof(queue));
    fill = read = 0;
    // One DSP quantum of initial silence absorbs output bursts at every rate.
    available = int(std::ceil(quantum * hostRate / processor.sample_rate())) + 2;
    write = available;
    mix = 0;
    random = 0x42656164;
  }
  void render(const float* p) {
    beads::Parameters controls{};
    controls.time.base = p[0];
    controls.size.base = p[1];
    controls.pitch.base = p[3];
    controls.shape.base = (int(p[6]) & 127) / 127.0f;
    controls.time.attenuverter = p[4];
    controls.size.attenuverter = p[4] * .5f;
    controls.pitch.attenuverter = 0;
    controls.shape.attenuverter = 0;
    beads::ModulatedParameter density{p[2], 0};
    beads::Processor::MixParameters balance{p[5], 1.0f, 0.0f, false};
    beads::HostRandom::Scope randomScope(random);
    processor.Process(beads::SEED_BUTTON_STATE_LOCKED, nullptr,
        frozen, density, controls, balance, in, out, quantum);
    for (const auto& frame : out) output.push(frame, [this](beads::FloatFrame f) {
      queue[write] = f;
      write = (write + 1) & 255;
      ++available;
    });
    fill = 0;
  }
  void process(float* l, float* r, int frames, const float* p) {
    float step = 1.0f / (hostRate * .005f);
    for (int i = 0; i < frames; ++i) {
      beads::FloatFrame source{std::clamp(l[i], -1.0f, 1.0f), std::clamp(r[i], -1.0f, 1.0f)};
      input.push(source, [this, p](beads::FloatFrame f) {
        in[fill] = f;
        frozen[fill] = (int(p[6]) & 128) != 0;
        if (++fill == quantum) render(p);
      });
      beads::FloatFrame wet{};
      if (available) {
        wet = queue[read];
        read = (read + 1) & 255;
        --available;
      }
      mix += std::clamp(p[7] - mix, -step, step);
      l[i] += mix * (wet.l - l[i]);
      r[i] += mix * (wet.r - r[i]);
    }
  }
};
BeadsInsert::BeadsInsert(float rate) : impl_(new (std::nothrow) Impl(rate)) {}
BeadsInsert::~BeadsInsert() { delete impl_; }
void BeadsInsert::reset() { if (impl_) impl_->reset(); }
void BeadsInsert::process(float* l, float* r, int frames, const float* p) {
  if (impl_) impl_->process(l, r, frames, p);
}
