#ifdef SNDSTATION_BUILD

#include "sndstation_volume.h"
#include "../../src/screens/screens.h"
#include <SDL2/SDL.h>
#include <alsa/asoundlib.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace {
int inputFd = -1;
bool held[2] = {};
Uint32 repeatAt[2] = {};
bool dropped = false;
volatile sig_atomic_t exitSignal = 0;
struct sigaction oldInt, oldTerm;
bool signalsInstalled = false;
snd_mixer_t* mixer = nullptr;
snd_mixer_elem_t* control = nullptr;
int cardIndex = -1;
long saved[2], ceiling[2], minimumDb;
bool changed = false;
int initialRoute[3];

void handleSignal(int signal) { exitSignal = signal; }

void error(const char* message, int code = 0) {
  if (code < 0) fprintf(stderr, "SNDStation volume: %s: %s\n", message, snd_strerror(code));
  else fprintf(stderr, "SNDStation volume: %s\n", message);
  screenMessage(MESSAGE_TIME_ERROR, "Volume: %s", message);
}

bool hasKey(const unsigned long* bits, int key) {
  constexpr int wordBits = sizeof(unsigned long) * 8;
  return (bits[key / wordBits] & (1UL << (key % wordBits))) != 0;
}

int openButtons() {
  DIR* directory = opendir("/dev/input");
  if (!directory) return -1;
  int result = -1;
  struct dirent* entry;
  while ((entry = readdir(directory)) != nullptr) {
    if (strncmp(entry->d_name, "event", 5) != 0) continue;
    char path[512], name[256] = {};
    if (snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name) >= (int)sizeof(path)) continue;
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) continue;
    constexpr int wordBits = sizeof(unsigned long) * 8;
    unsigned long keys[(KEY_MAX + wordBits) / wordBits] = {};
    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0 &&
        strcmp(name, "5070800.lradc") == 0 &&
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) >= 0 &&
        hasKey(keys, KEY_VOLUMEUP) && hasKey(keys, KEY_VOLUMEDOWN)) {
      result = fd;
      break;
    }
    close(fd);
  }
  closedir(directory);
  return result;
}

bool ownsCodecPlayback() {
  if (cardIndex < 0) return false;
  char path[128], line[128];
  snprintf(path, sizeof(path), "/proc/asound/card%d/pcm0p/sub0/status", cardIndex);
  FILE* file = fopen(path, "r");
  if (!file) return false;
  bool found = false;
  while (fgets(line, sizeof(line), file)) {
    long owner;
    if (sscanf(line, "owner_pid : %ld", &owner) == 1) {
      found = owner == (long)getpid();
      break;
    }
  }
  fclose(file);
  return found;
}

bool readRoute(int route[3]) {
  const char* names[] = {"Speaker", "Headphone"};
  snd_mixer_selem_id_t* id;
  snd_mixer_selem_id_alloca(&id);
  snd_mixer_selem_id_set_index(id, 0);
  for (int i = 0; i < 2; ++i) {
    snd_mixer_selem_id_set_name(id, names[i]);
    snd_mixer_elem_t* element = snd_mixer_find_selem(mixer, id);
    if (!element || !snd_mixer_selem_has_playback_switch(element) ||
        snd_mixer_selem_get_playback_switch(element, SND_MIXER_SCHN_MONO, &route[i]) < 0) {
      error("cannot read audio route");
      return false;
    }
  }
  snd_ctl_t* ctl = nullptr;
  int result = snd_ctl_open(&ctl, "hw:Codec", SND_CTL_NONBLOCK);
  if (result >= 0) {
    snd_ctl_elem_id_t* jack;
    snd_ctl_elem_value_t* value;
    snd_ctl_elem_id_alloca(&jack);
    snd_ctl_elem_value_alloca(&value);
    snd_ctl_elem_id_set_interface(jack, SND_CTL_ELEM_IFACE_CARD);
    snd_ctl_elem_id_set_name(jack, "Headphone Jack");
    snd_ctl_elem_value_set_id(value, jack);
    result = snd_ctl_elem_read(ctl, value);
    if (result >= 0) route[2] = snd_ctl_elem_value_get_boolean(value, 0);
    snd_ctl_close(ctl);
  }
  if (result < 0) error("cannot read headphone jack", result);
  return result >= 0;
}

bool routeUnchanged() {
  int current[3];
  if (!readRoute(current)) return false;
  if (memcmp(current, initialRoute, sizeof(current)) != 0) {
    error("audio route changed");
    return false;
  }
  return true;
}

bool openMixer() {
  cardIndex = snd_card_get_index("Codec");
  if (cardIndex < 0) {
    error("built-in codec unavailable");
    return false;
  }
  snd_ctl_t* ctl = nullptr;
  int result = snd_ctl_open(&ctl, "hw:Codec", SND_CTL_NONBLOCK);
  if (result < 0) { error("cannot open codec", result); return false; }
  snd_ctl_card_info_t* info;
  snd_ctl_card_info_alloca(&info);
  result = snd_ctl_card_info(ctl, info);
  bool isCodec = result >= 0 && strcmp(snd_ctl_card_info_get_longname(info), "a100-audio-codec") == 0;
  snd_ctl_close(ctl);
  if (!isCodec) { error("unsupported codec", result); return false; }

  result = snd_mixer_open(&mixer, SND_CTL_NONBLOCK);
  if (result >= 0) result = snd_mixer_attach(mixer, "hw:Codec");
  if (result >= 0) result = snd_mixer_selem_register(mixer, nullptr, nullptr);
  if (result >= 0) result = snd_mixer_load(mixer);
  if (result < 0) {
    error("cannot open mixer", result);
    if (mixer) snd_mixer_close(mixer);
    mixer = nullptr;
    return false;
  }
  snd_mixer_selem_id_t* id;
  snd_mixer_selem_id_alloca(&id);
  snd_mixer_selem_id_set_index(id, 0);
  snd_mixer_selem_id_set_name(id, "DAC Digital");
  control = snd_mixer_find_selem(mixer, id);
  long maximumDb = 0;
  if (!control || !snd_mixer_selem_has_playback_volume(control) ||
      !snd_mixer_selem_has_playback_channel(control, SND_MIXER_SCHN_FRONT_LEFT) ||
      !snd_mixer_selem_has_playback_channel(control, SND_MIXER_SCHN_FRONT_RIGHT) ||
      snd_mixer_selem_get_playback_dB_range(control, &minimumDb, &maximumDb) < 0) {
    error("DAC control unavailable");
    snd_mixer_close(mixer);
    mixer = nullptr;
    control = nullptr;
    return false;
  }
  const snd_mixer_selem_channel_id_t channels[] = {
    SND_MIXER_SCHN_FRONT_LEFT, SND_MIXER_SCHN_FRONT_RIGHT
  };
  for (int i = 0; i < 2; ++i) {
    result = snd_mixer_selem_get_playback_volume(control, channels[i], &saved[i]);
    if (result >= 0) result = snd_mixer_selem_get_playback_dB(control, channels[i], &ceiling[i]);
    if (result < 0 || ceiling[i] > 0 || ceiling[i] < minimumDb) {
      error("unsafe initial DAC level", result);
      snd_mixer_close(mixer);
      mixer = nullptr;
      control = nullptr;
      return false;
    }
  }
  if (!readRoute(initialRoute)) {
    snd_mixer_close(mixer);
    mixer = nullptr;
    control = nullptr;
    return false;
  }
  return true;
}

void stepVolume(int direction) {
  if (!mixer || !ownsCodecPlayback()) {
    error(mixer ? "built-in output not active" : "volume control unavailable");
    held[0] = held[1] = false;
    return;
  }
  int result = snd_mixer_handle_events(mixer);
  if (result < 0 && result != -EAGAIN) {
    error("cannot read mixer", result);
    held[0] = held[1] = false;
    return;
  }
  if (!routeUnchanged()) {
    held[0] = held[1] = false;
    return;
  }
  const snd_mixer_selem_channel_id_t channels[] = {
    SND_MIXER_SCHN_FRONT_LEFT, SND_MIXER_SCHN_FRONT_RIGHT
  };
  long previous[2], current[2], target[2];
  for (int i = 0; i < 2; ++i) {
    result = snd_mixer_selem_get_playback_volume(control, channels[i], &previous[i]);
    if (result >= 0) result = snd_mixer_selem_get_playback_dB(control, channels[i], &current[i]);
    if (result < 0) { error("cannot read DAC level", result); held[0] = held[1] = false; return; }
    target[i] = sndstationVolumeStep(current[i], minimumDb, ceiling[i], direction);
  }
  for (int i = 0; i < 2; ++i) {
    if (target[i] == current[i]) continue;
    changed = true;
    result = snd_mixer_selem_set_playback_dB(control, channels[i], target[i], -1);
    if (result < 0) {
      for (int j = 0; j < i; ++j) {
        int restore = snd_mixer_selem_set_playback_volume(control, channels[j], previous[j]);
        if (restore < 0) error("cannot restore DAC level", restore);
      }
      error("cannot set DAC level", result);
      held[0] = held[1] = false;
      return;
    }
  }
  screenMessage(MESSAGE_TIME, "Volume %.1f dB", target[0] / 100.0);
}
}

void sndstationVolumeStart(void) {
  struct sigaction action = {};
  action.sa_handler = handleSignal;
  sigemptyset(&action.sa_mask);
  if (sigaction(SIGINT, &action, &oldInt) != 0) {
    error("cannot install exit handler");
    return;
  }
  if (sigaction(SIGTERM, &action, &oldTerm) != 0) {
    sigaction(SIGINT, &oldInt, nullptr);
    error("cannot install exit handler");
    return;
  }
  signalsInstalled = true;
  inputFd = openButtons();
  if (inputFd < 0) error("buttons unavailable");
  else openMixer();
}

void sndstationVolumePoll(void) {
  if (inputFd < 0) return;
  struct input_event events[16];
  ssize_t size;
  while ((size = read(inputFd, events, sizeof(events))) > 0) {
    for (size_t i = 0; i < (size_t)size / sizeof(*events); ++i) {
      const input_event& event = events[i];
      if (event.type == EV_SYN && event.code == SYN_DROPPED) {
        dropped = true;
        held[0] = held[1] = false;
      } else if (dropped && event.type == EV_SYN && event.code == SYN_REPORT) {
        dropped = false;
      } else if (!dropped && event.type == EV_KEY &&
                 (event.code == KEY_VOLUMEUP || event.code == KEY_VOLUMEDOWN)) {
        const int index = event.code == KEY_VOLUMEUP ? 0 : 1;
        if (event.value == 1 && !held[index]) {
          held[index] = true;
          repeatAt[index] = SDL_GetTicks() + 400;
          stepVolume(index == 0 ? 1 : -1);
        } else if (event.value == 0) held[index] = false;
      }
    }
  }
  if (size == 0 || (size < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
    error("buttons disconnected");
    close(inputFd);
    inputFd = -1;
    held[0] = held[1] = false;
    return;
  }
  Uint32 now = SDL_GetTicks();
  for (int i = 0; i < 2; ++i) {
    if (held[i] && (int32_t)(now - repeatAt[i]) >= 0) {
      repeatAt[i] = now + 150;
      stepVolume(i == 0 ? 1 : -1);
    }
  }
}

bool sndstationVolumeHandlesButtons(void) { return inputFd >= 0 && mixer != nullptr; }
bool sndstationVolumeExitRequested(void) { return exitSignal != 0; }

void sndstationVolumeStop(void) {
  int mixerEvents = mixer && changed ? snd_mixer_handle_events(mixer) : 0;
  if (mixer && control && changed && (!ownsCodecPlayback() ||
      (mixerEvents < 0 && mixerEvents != -EAGAIN) || !routeUnchanged())) {
    error("cannot restore: output changed");
  } else if (mixer && control && changed) {
    const snd_mixer_selem_channel_id_t channels[] = {
      SND_MIXER_SCHN_FRONT_LEFT, SND_MIXER_SCHN_FRONT_RIGHT
    };
    for (int i = 0; i < 2; ++i) {
      int result = snd_mixer_selem_set_playback_volume(control, channels[i], saved[i]);
      if (result < 0) error("cannot restore DAC level", result);
    }
  }
  if (mixer) snd_mixer_close(mixer);
  mixer = nullptr;
  control = nullptr;
  changed = false;
  if (inputFd >= 0) close(inputFd);
  inputFd = -1;
  if (signalsInstalled) {
    sigaction(SIGINT, &oldInt, nullptr);
    sigaction(SIGTERM, &oldTerm, nullptr);
    signalsInstalled = false;
  }
}

#endif
