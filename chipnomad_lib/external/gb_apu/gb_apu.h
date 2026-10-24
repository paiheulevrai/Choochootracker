#ifndef GB_APU_H
#define GB_APU_H

#ifdef __cplusplus
extern "C" {
#endif

enum GbApuType
{
    GbApuType_DMG,
    GbApuType_CGB,
    GbApuType_AGB,
};

/* TODO: agb filter. */
enum GbApuFilter
{
    GbApuFilter_NONE,
    GbApuFilter_DMG,
    GbApuFilter_CGB,
};

enum GbApuClockRate
{
    GbApuClockRate_DMG = 4194304,
    GbApuClockRate_CGB = GbApuClockRate_DMG,
    GbApuClockRate_AGB = GbApuClockRate_DMG * 4,
};

typedef struct GbApu GbApu;
typedef void(*apu_agb_fifo_dma_request)(void* user, unsigned fifo_num, unsigned time);

/* ------------------------- */
/* ------Initialise Api----- */
/* ------------------------- */
/* ensure you call this at start-up. */
GbApu* apu_init(double clock_rate, double sample_rate);
/* call to free allocated memory by blip buf. */
void apu_quit(GbApu*);
/* clock_rate should be the cpu speed of the system. */
void apu_reset(GbApu*, enum GbApuType type);

/* ------------------------- */
/* ------DMG Functions------ */
/* ------------------------- */
/* reads from io register, unused bits are set to 1. */
unsigned apu_read_io(GbApu*, unsigned addr, unsigned time);
/* writes to an io register. */
void apu_write_io(GbApu*, unsigned addr, unsigned value, unsigned time);
/* call this on the falling edge of bit 4/5 of DIV. */
void apu_frame_sequencer_clock(GbApu*, unsigned time);

/* ------------------------- */
/* ------CGB Functions------ */
/* ------------------------- */
/* returns raw 4-bit sample value for each channel. */
unsigned apu_cgb_read_pcm12(GbApu*, unsigned time);
unsigned apu_cgb_read_pcm34(GbApu*, unsigned time);

/* ------------------------- */
/* ------AGB Functions------ */
/* ------------------------- */
/* translates agb addr to dmg and calls apu_read_io, unused bits are masked. */
unsigned apu_agb_read8_io(GbApu*, unsigned addr, unsigned time);
/* translates agb addr to dmg and calls apu_write_io. */
void apu_agb_write8_io(GbApu*, unsigned addr, unsigned value, unsigned time);
/* same as above for convenience. */
unsigned apu_agb_read16_io(GbApu*, unsigned addr, unsigned time);
void apu_agb_write16_io(GbApu*, unsigned addr, unsigned value, unsigned time);
/* masks unused bits. */
unsigned apu_agb_soundcnt_read(GbApu*, unsigned time);
void apu_agb_soundcnt_write(GbApu*, unsigned value, unsigned time);
/* currently unused. */
unsigned apu_agb_soundbias_read(GbApu*, unsigned time);
void apu_agb_soundbias_write(GbApu*, unsigned value, unsigned time);
/* fifo writes differ based on address alignment. */
void apu_agb_fifo_write8(GbApu*, unsigned addr, unsigned value);
void apu_agb_fifo_write16(GbApu*, unsigned addr, unsigned value);
void apu_agb_fifo_write32(GbApu*, unsigned addr, unsigned value);
/* on timer overflow, the fifo can request for more data by starting a dma. */
void apu_agb_timer_overflow(GbApu*, void* user, apu_agb_fifo_dma_request dma_callback, unsigned timer_num, unsigned time);

/* ------------------------- */
/* ------Advanced Api------- */
/* ------------------------- */
/* returns value of io register, without unused bits applied / masked, */
/* regardless if the apu is enabled or not. */
/* useful for gui io viewer. */
unsigned apu_read_io_raw(const GbApu*, unsigned addr);
unsigned apu_agb_read_io_raw(const GbApu*, unsigned addr);
unsigned apu_agb_soundcnt_read_raw(const GbApu*);
unsigned apu_agb_soundbias_read_raw(const GbApu*);

/* ------------------------- */
/* ------Configure Api------ */
/* ------------------------- */
/* channel volume, max range: 0.0 - 1.0. */
void apu_set_channel_volume(GbApu*, unsigned channel_num, float volume);
/* master volume, max range: 0.0 - 1.0. */
void apu_set_master_volume(GbApu*, float volume);
/* only available with Blip_Buffer. */
void apu_set_bass(GbApu*, int frequency);
/* only available with Blip_Buffer. */
void apu_set_treble(GbApu*, double treble_db);
/* sets the filter that's applied to apu_read_samples(). */
void apu_set_highpass_filter(GbApu*, enum GbApuFilter filter, double clock_rate, double sample_rate);
/* charge factor should be in the range 0.0 - 1.0. */
void apu_set_highpass_filter_custom(GbApu*, double charge_factor, double clock_rate, double sample_rate);
/* enable zombie mode, forcefully disabled in agb mode. */
void apu_set_zombie_mode(GbApu*, unsigned enable);
/* updates timestamp, useful if the time overflows. */
void apu_update_timestamp(GbApu*, int time);

/* ------------------------- */
/* ------Sample Output------ */
/* ------------------------- */
/* returns how many cycles are needed until sample_count == apu_samples_avaliable() */
int apu_clocks_needed(const GbApu*, int sample_count);
/* returns how many samples are available, call apu_end_frame() first. */
int apu_samples_avaliable(const GbApu*);
/* call this when you want to read out samples. */
void apu_end_frame(GbApu*, unsigned time);
/* read stereo samples, returns the amount read. */
int apu_read_samples(GbApu*, short out[], int count);
/* removes all samples. */
void apu_clear_samples(GbApu*);

/* ------------------------- */
/* ------SaveState Api------ */
/* ------------------------- */
/* returns the size needed for savestates. */
unsigned apu_state_size(void);
/* creates a savestate, returns 0 on faliure and apu_state_size() on success. */
unsigned apu_save_state(const GbApu*, void* data, unsigned size);
/* loads a savestate, returns 0 on faliure and apu_state_size() on success. */
unsigned apu_load_state(GbApu*, const void* data, unsigned size);

#ifdef __cplusplus
}
#endif

#endif /* GB_APU_H */
