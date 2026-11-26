#define SNDSTATION_BUILD
#include "../platforms/sdl2/sndstation_volume.cpp"
#include <cassert>

static int messages = 0;
void screenMessage(int, const char*, ...) { ++messages; }

int main() {
  assert(sndstationVolumeStep(0, -11925, 0, -1) == -150);
  assert(sndstationVolumeStep(-150, -11925, 0, 1) == 0);
  assert(sndstationVolumeStep(0, -11925, 0, 1) == 0);
  assert(sndstationVolumeStep(-11900, -11925, 0, -1) == -11925);
  assert(sndstationVolumeStep(-11925, -11925, 0, 1) == -11775);
  assert(sndstationVolumeStep(-300, -11925, -600, 1) == -600);
  assert(sndstationVolumeStep(-750, -11925, -600, 1) == -600);

  int pipeFds[2];
  assert(pipe(pipeFds) == 0);
  assert(fcntl(pipeFds[0], F_SETFL, O_NONBLOCK) == 0);
  inputFd = pipeFds[0];
  held[0] = true;
  const input_event events[] = {
    {{}, EV_SYN, SYN_DROPPED, 0},
    {{}, EV_KEY, KEY_VOLUMEUP, 1},
    {{}, EV_SYN, SYN_REPORT, 0},
    {{}, EV_KEY, KEY_VOLUMEUP, 2},
    {{}, EV_KEY, KEY_VOLUMEDOWN, 1},
    {{}, EV_KEY, KEY_VOLUMEDOWN, 0}
  };
  assert(write(pipeFds[1], events, sizeof(events)) == sizeof(events));
  sndstationVolumePoll();
  assert(!held[0] && !held[1] && !dropped);
  assert(messages == 1); // Only the fresh press after SYN_REPORT is handled.
  close(pipeFds[1]);
  held[0] = true;
  sndstationVolumePoll();
  assert(inputFd == -1 && !held[0] && messages == 2);
  sndstationVolumeStop();
}
