#include "blip_wrap.h"
#include "blargg/Blip_Buffer.h"

enum { VOLUME_MIN = -0x200 };
enum { VOLUME_MAX = +0x200 - 1 };


struct blip_wrap_t
{
    Blip_Buffer buf[2];
    Blip_Synth<blip_med_quality, VOLUME_MAX - VOLUME_MIN> synth_med;
    Blip_Synth<blip_good_quality, VOLUME_MAX - VOLUME_MIN> synth_good;
};

extern "C" {

blip_wrap_t* blip_wrap_new(double sample_rate)
{
    return new blip_wrap_t();
}

void blip_wrap_delete(blip_wrap_t* b)
{
    delete b;
}

int blip_wrap_set_rates(blip_wrap_t* b, double clock_rate, double sample_rate)
{
    b->buf[0].clock_rate(clock_rate);
    b->buf[1].clock_rate(clock_rate);
    if (b->buf[0].set_sample_rate(sample_rate)) {
        return -1;
    }
    if (b->buf[1].set_sample_rate(sample_rate)) {
        return -1;
    }

    return 0;
}

void blip_wrap_clear(blip_wrap_t* b)
{
    b->buf[0].clear();
    b->buf[1].clear();
}

void blip_wrap_add_delta(blip_wrap_t* b, unsigned clock_time, int delta, int lr)
{
    b->synth_good.offset_inline(clock_time, delta, &b->buf[lr]);
}

void blip_wrap_add_delta_fast(blip_wrap_t* b, unsigned clock_time, int delta, int lr)
{
    b->synth_med.offset_inline(clock_time, delta, &b->buf[lr]);
}

int blip_wrap_clocks_needed(const blip_wrap_t* b, int sample_count)
{
    return b->buf[0].count_clocks(sample_count / 2);
}

void blip_wrap_end_frame(blip_wrap_t* b, unsigned clock_duration)
{
    b->buf[0].end_frame(clock_duration);
    b->buf[1].end_frame(clock_duration);
}

int blip_wrap_samples_avail(const blip_wrap_t* b)
{
    return b->buf[0].samples_avail() * 2;
}

int blip_wrap_read_samples(blip_wrap_t* b, short out[], int count)
{
    b->buf[0].read_samples(out + 0, count / 2, 1);
    return b->buf[1].read_samples(out + 1, count / 2, 1) * 2;
}

int blip_apply_volume_to_sample(blip_wrap_t*, int sample, float volume)
{
    return sample * volume;
}

void blip_wrap_set_volume(blip_wrap_t* b, float volume)
{
    b->synth_med.volume(volume);
    b->synth_good.volume(volume);
}

// only available when using blip_buffer.cpp
void blip_wrap_set_bass(blip_wrap_t* b, int frequency)
{
    b->buf[0].bass_freq(frequency);
    b->buf[1].bass_freq(frequency);
}

void blip_wrap_set_treble(blip_wrap_t* b, double treble_db)
{
    b->synth_med.treble_eq(treble_db);
    b->synth_good.treble_eq(treble_db);
}

} // extern "C"
