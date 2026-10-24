#ifndef _BLIP_WRAP_H_
#define _BLIP_WRAP_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct blip_wrap_t blip_wrap_t;

blip_wrap_t* blip_wrap_new(double sample_rate);

int blip_wrap_set_rates(blip_wrap_t*, double clock_rate, double sample_rate);
void blip_wrap_clear(blip_wrap_t*);
void blip_wrap_add_delta(blip_wrap_t*, unsigned clock_time, int delta, int lr);
void blip_wrap_add_delta_fast(blip_wrap_t*, unsigned clock_time, int delta, int lr);
int blip_wrap_clocks_needed(const blip_wrap_t*, int sample_count);
void blip_wrap_end_frame(blip_wrap_t*, unsigned clock_duration);
int blip_wrap_samples_avail(const blip_wrap_t*);
int blip_wrap_read_samples(blip_wrap_t*, short out [], int count);
void blip_wrap_delete(blip_wrap_t*);
int blip_apply_volume_to_sample(blip_wrap_t*, int sample, float volume);
void blip_wrap_set_volume(blip_wrap_t*, float volume);

// only available when using blip_buffer.cpp
void blip_wrap_set_bass(blip_wrap_t*, int frequency);
void blip_wrap_set_treble(blip_wrap_t*, double treble_db);

#ifdef __cplusplus
}
#endif
#endif
