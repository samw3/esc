
#include "chip.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include "blip_buf.h"
#include "console.h"

#define PATTERN_LEN 32
#define NUM_CHANNELS 4

enum {
  WF_PUL = 0x01,
  WF_SAW = 0x02,
  WF_TRI = 0x04,
  WF_NOI = 0x08,
  WF_WAV = 0xF0,
};

struct PatternLine {
  u8 note;
  u8 instr;
  u8 cmd[2];
  u8 param[2];
};

struct Pattern {
  struct PatternLine line[PATTERN_LEN];
};

struct InstrumentLine {
  u8 cmd;
  u8 param;
};

struct Instrument {
  char name[256];
  int length;
  struct InstrumentLine line[256];
};

struct SongLine {
  u8 track[NUM_CHANNELS];
  u8 transp[NUM_CHANNELS];
};

struct VolumeTable {
  int length;
  u8 column[256];
};

struct DutyTable {
  int length;
  u8 column[256];
};

struct PanTable {
  int length;
  u8 column[256];
};

static blip_t *sBlipBuffer[2];

static struct Instrument instrument[256];
static struct Pattern track[256];
static struct SongLine song[256];
static char sFilename[1024];
static int songlen = 1;
static struct VolumeTable volumeTable[16];
static struct DutyTable dutyTable[16];
static struct PanTable panTable[16];
static u8 waveTable[16][32];
static u8 sTempo = 6;
static u8 trackwait;
static u8 trackpos;
static u8 songpos;
static u8 playsong;
static u8 playtrack;

static const u16 waveStep[8 * 12] = {
    0x5448, 0x4f8d, 0x4b16, 0x46df, 0x42e5, 0x3f24, 0x3b98, 0x3840,
    0x3518, 0x321d, 0x2f4d, 0x2ca5, 0x2a24, 0x27c6, 0x258b, 0x2370,
    0x2172, 0x1f92, 0x1dcc, 0x1c20, 0x1a8c, 0x190f, 0x17a7, 0x1653,
    0x1512, 0x13e3, 0x12c5, 0x11b8, 0x10b9, 0x0fc9, 0x0ee6, 0x0e10,
    0x0d46, 0x0c87, 0x0bd3, 0x0b29, 0x0a89, 0x09f2, 0x0963, 0x08dc,
    0x085d, 0x07e4, 0x0773, 0x0708, 0x06a3, 0x0644, 0x05ea, 0x0595,
    0x0544, 0x04f9, 0x04b1, 0x046e, 0x042e, 0x03f2, 0x03ba, 0x0384,
    0x0351, 0x0322, 0x02f5, 0x02ca, 0x02a2, 0x027c, 0x0259, 0x0237,
    0x0217, 0x01f9, 0x01dd, 0x01c2, 0x01a9, 0x0191, 0x017a, 0x0165,
    0x0151, 0x013e, 0x012c, 0x011b, 0x010c, 0x00fd, 0x00ee, 0x00e1,
    0x00d4, 0x00c8, 0x00bd, 0x00b3, 0x00a9, 0x009f, 0x0096, 0x008e,
    0x0086, 0x007e, 0x0077, 0x0071, 0x006a, 0x0064, 0x005f, 0x0059,
};

static const s8 sinetable[] = {
    0, 12, 25, 37, 49, 60, 71, 81,
    90, 98, 106, 112, 117, 122, 125, 126,
    127, 126, 125, 122, 117, 112, 106, 98,
    90, 81, 71, 60, 49, 37, 25, 12,
    0, -12, -25, -37, -49, -60, -71, -81,
    -90, -98, -106, -112, -117, -122, -125, -126,
    -127, -126, -125, -122, -117, -112, -106, -98,
    -90, -81, -71, -60, -49, -37, -25, -12
};

static volatile struct oscillator {
  s32 freq;
  u16 phase;
  u16 lastPhase;
  u16 duty;
  u8 waveform;
  u8 volume;  // 0-255
  u8 pan;     // 0-16
  u32 buzzseed;
  s16 lastLeft;
  s16 lastRight;
  size_t lastTime;
} osc[NUM_CHANNELS];

static struct channel {
  u8 tnum;
  s8 transp;
  u8 tnote;
  u8 lastinstr;
  u8 inum;
  u8 iptr;
  u8 iwait;
  u8 inote;
  s8 bendd;
  s16 bend;
  s8 volumed;
  u8 vdepth;
  u8 vrate;
  u8 vpos;
  s16 inertia;
  u16 slur;
  u8 volumeTable;
  u8 volumeStretch;
  u8 volumeStretchCounter;
  u8 volumePtr;
  u8 dutyTable;
  u8 dutyStretch;
  u8 dutyStretchCounter;
  u8 dutyPtr;
  u8 panTable;
  u8 panStretch;
  u8 panStretchCounter;
  u8 panPtr;
  u8 filterLow;
  u8 filterHigh;
} channel[NUM_CHANNELS];

// FILTER_LENGTH MUST BE ODD
#define FILTER_LENGTH 31
#define SAMPLE_RATE 44100.0
#define CLOCK_RATE (44100.0 * 256.0)
#define SAMPLES_PER_PLAYROUTINE (SAMPLE_RATE / 60)
#define CLOCKS_PER_PLAYROUTINE (CLOCK_RATE / 60)


static float filter_freqKernels[256][FILTER_LENGTH];
static float filter_kernel[NUM_CHANNELS][FILTER_LENGTH];
static float filter_buffer[NUM_CHANNELS][2][FILTER_LENGTH];
static uint_fast16_t filter_bufferPos[NUM_CHANNELS];

void filter_setFreqs(const u8 _channelNum, u8 _low, u8 _high) {
  if (_low == 0) {
    for (size_t i = 0; i < FILTER_LENGTH; i++) {
      filter_kernel[_channelNum][i] = filter_freqKernels[_high][i];
    }
  } else {
    for (size_t i = 0; i < FILTER_LENGTH; i++) {
      filter_kernel[_channelNum][i] = filter_freqKernels[_high][i] - filter_freqKernels[_low][i];
    }
  }
}

void filter_init() {
  // Init kernels
  for (size_t f = 0; f < 256; f++) {
    float filtFreq = 16.42 + pow(1.029410639, 90 + f);
    if (f == 255) {
      filtFreq = 22050.0;
    }
    int j = 0;
    for (int i = (-(FILTER_LENGTH - 1) / 2); i <= ((FILTER_LENGTH - 1) / 2); i++) {
      // const float w = 0.5 * (1.0 - cos(2.0 * M_PI * ((float)j + 1.0) / ((float)FILTER_LENGTH)));
      const float w = 0.5 - 0.5 * cos((2.0 * M_PI * ((float) j + 1.0)) / (float) FILTER_LENGTH);
      const float b = 2.0 * filtFreq * ((float) i) / SAMPLE_RATE;
      const float sinc_b = (b == 0) ? 1.0 : sin(M_PI * b) / (M_PI * b);
      const float lowPass = w * (2.0 * filtFreq / SAMPLE_RATE) * sinc_b;
      filter_freqKernels[f][j] = lowPass;
      j++;
    }
  }
  // Init buffers
  for (size_t x = 0; x < NUM_CHANNELS; x++) {
    for (size_t y = 0; y < 2; y++) {
      for (size_t z = 0; z < FILTER_LENGTH; z++) {
        filter_buffer[x][y][z] = 0;
      }
    }
    filter_setFreqs(x, 0, 255);
    filter_bufferPos[x] = 0;
  }
}

s16 filter_sample(const s16 _input, const u8 _channelNum, const u8 _zeroForLeft) {
  float out = 0;
  uint_fast16_t *pos = &filter_bufferPos[_channelNum];
  float *buf = filter_buffer[_channelNum][_zeroForLeft];

  buf[(*pos)++] = _input;
  if (*pos == FILTER_LENGTH) {
    *pos = 0;
  }
  for (size_t i = 0; i < FILTER_LENGTH; i++) {
    out += buf[(*pos)++] * filter_kernel[_channelNum][i];
    if (*pos == FILTER_LENGTH) {
      *pos = 0;
    }
  }
  return (s16) out;
}

static void readsong(int pos, int ch, u8 *dest) {
  dest[0] = song[pos].track[ch];
  dest[1] = song[pos].transp[ch];
}

static void readtrack(int num, int pos, struct PatternLine *tl) {
  tl->note = track[num].line[pos].note;
  tl->instr = track[num].line[pos].instr;
  tl->cmd[0] = track[num].line[pos].cmd[0];
  tl->cmd[1] = track[num].line[pos].cmd[1];
  tl->param[0] = track[num].line[pos].param[0];
  tl->param[1] = track[num].line[pos].param[1];
}

static void readinstr(int num, int pos, u8 *il) {
  if (pos >= instrument[num].length) {
    il[0] = 0;
    il[1] = 0;
  } else {
    il[0] = instrument[num].line[pos].cmd;
    il[1] = instrument[num].line[pos].param;
  }
}

static void silence() {
  u8 i;
  for (i = 0; i < 4; i++) {
    osc[i].volume = 0;
    channel[i].volumeTable = 0;
    channel[i].volumed = 0;
  }
  playsong = 0;
  playtrack = 0;
}

static void runcmd(u8 ch, u8 cmd, u8 param) {
  switch (cmd) {
    case 0:channel[ch].inum = 0;
      break;
    case 'd': // Duty
      if (param > 15) {
        channel[ch].dutyTable = param >> 4;
        const u8 stretch = param & 0xf;
        channel[ch].dutyStretch = stretch;
        channel[ch].dutyStretchCounter = stretch;
        channel[ch].dutyPtr = 0;
      } else {
        channel[ch].dutyTable = 0;
        osc[ch].duty = param;
      }
      break;
    case 'f': // Fade
      channel[ch].volumed = param;
      break;
    case 'h': // High filter
      channel[ch].filterHigh = param;
      filter_setFreqs(ch, channel[ch].filterHigh, channel[ch].filterLow);
      break;
    case 'i': // Inertia
      channel[ch].inertia = param << 1;
      break;
    case 'j': // Jump
      channel[ch].iptr = param;
      break;
    case 'l': // Low filter
      channel[ch].filterLow = param;
      filter_setFreqs(ch, channel[ch].filterHigh, channel[ch].filterLow);
      break;
    case 'o': // vibratO
      if (channel[ch].vdepth != (param >> 4)) {
        channel[ch].vpos = 0;
      }
      channel[ch].vdepth = param >> 4;
      channel[ch].vrate = param & 15;
      break;
    case 'p': // Pan
      if (param > 15) {
        channel[ch].panTable = param >> 4;
        const u8 stretch = param & 0xf;
        channel[ch].panStretch = stretch;
        channel[ch].panStretchCounter = stretch;
        channel[ch].panPtr = 0;
      } else {
        channel[ch].panTable = 0;
        osc[ch].pan = param;
      }
      break;
    case 's': // Slide
      channel[ch].bendd = param;
      break;
    case 't': // wait xx Time units
      channel[ch].iwait = param;
      break;
    case 'v': // Volume
      if (param > 15) {
        const u8 table = param >> 4;
        channel[ch].volumeTable = table;
        const u8 stretch = param & 0xf;
        channel[ch].volumeStretch = stretch;
        channel[ch].volumeStretchCounter = stretch;
        channel[ch].volumePtr = 0;
        osc[ch].volume = volumeTable[table].column[0] << 4;
      } else {
        channel[ch].volumeTable = 0;
        osc[ch].volume = param << 4;
      }
      break;
    case 'w': // Waveform
      osc[ch].waveform = param;
      break;
    case '+': // Relative note
      channel[ch].inote = param + channel[ch].tnote - 12 * 4;
      break;
    case '=': // Absolute note
      channel[ch].inote = param;
      break;
  } /* switch */
} /* runcmd */

static void startplaytrack(int t) {
  channel[0].tnum = t;
  channel[1].tnum = 0;
  channel[2].tnum = 0;
  channel[3].tnum = 0;
  trackpos = 0;
  trackwait = 0;
  playtrack = 1;
  playsong = 0;
}

static void startplaysong(int p) {
  songpos = p;
  trackpos = 0;
  trackwait = 0;
  playtrack = 0;
  playsong = 1;
}

static void playroutine() { // called at 50 Hz
  u8 ch;
  if (playtrack || playsong) {
    if (trackwait) {
      trackwait--;
    } else {
      trackwait = sTempo;
      if (!trackpos) {
        if (playsong) {
          if (songpos >= songlen) {
            songpos = 0;
            trackpos = 0;

            // playsong = 0;
          }
          for (ch = 0; ch < 4; ch++) {
            u8 tmp[2];

            readsong(songpos, ch, tmp);
            channel[ch].tnum = tmp[0];
            channel[ch].transp = tmp[1];
          }
          songpos++;
        }
      }
      if (playtrack || playsong) {
        for (ch = 0; ch < 4; ch++) {
          if (channel[ch].tnum) {
            struct PatternLine tl;
            u8 instr = 0;

            readtrack(channel[ch].tnum, trackpos, &tl);
            if (tl.note == 255) {
              // Note cut
              channel[ch].volumeTable = 0;
              osc[ch].volume = 0;
              channel[ch].volumed = 0;
            } else if (tl.note) {
              channel[ch].tnote = tl.note + channel[ch].transp;
              instr = channel[ch].lastinstr;
              if (tl.instr) {
                instr = tl.instr;
              }
              if (instr) {
                channel[ch].lastinstr = instr;
                channel[ch].inum = instr;
                channel[ch].iptr = 0;
                channel[ch].iwait = 0;
                channel[ch].bend = 0;
                channel[ch].bendd = 0;
                channel[ch].volumed = 0;
                channel[ch].vdepth = 0;
              }
              if (tl.cmd[0]) {
                runcmd(ch, tl.cmd[0], tl.param[0]);
              }
              /*if(tl.cmd[1])
                runcmd(ch, tl.cmd[1], tl.param[1]);*/
            }
          }
        }
        trackpos++;
        trackpos &= 31;
      }
    }
  }
  for (ch = 0; ch < NUM_CHANNELS; ch++) {
    s16 vol;
    u16 slur;
    while (channel[ch].inum && !channel[ch].iwait) {
      u8 il[2];

      readinstr(channel[ch].inum, channel[ch].iptr, il);
      channel[ch].iptr++;

      runcmd(ch, il[0], il[1]);
    }
    if (channel[ch].iwait) {
      channel[ch].iwait--;
    }
    if (channel[ch].inertia) {
      s16 diff;

      slur = channel[ch].slur;
      diff = waveStep[channel[ch].inote] - slur;
      // diff >>= channel[ch].inertia;
      if (diff > 0) {
        if (diff > channel[ch].inertia) {
          diff = channel[ch].inertia;
        }
      } else if (diff < 0) {
        if (diff < -channel[ch].inertia) {
          diff = -channel[ch].inertia;
        }
      }
      slur += diff;
      channel[ch].slur = slur;
    } else {
      slur = waveStep[channel[ch].inote];
    }
    osc[ch].freq = slur + channel[ch].bend + ((channel[ch].vdepth * sinetable[channel[ch].vpos & 63]) >> 2);
    channel[ch].bend += channel[ch].bendd << 3;
    if (channel[ch].volumeTable == 0) {
      vol = osc[ch].volume + channel[ch].volumed;
      if (vol < 0) {
        vol = 0;
      }
      if (vol > 255) {
        vol = 255;
      }
      osc[ch].volume = vol;
    } else {
      if (channel[ch].volumeStretchCounter > 0) {
        channel[ch].volumeStretchCounter--;
      } else {
        channel[ch].volumeStretchCounter = channel[ch].volumeStretch;
        if (channel[ch].volumePtr < volumeTable[channel[ch].volumeTable].length - 1) {
          channel[ch].volumePtr++;
          osc[ch].volume = volumeTable[channel[ch].volumeTable].column[channel[ch].volumePtr] << 4;
        }
      }
    }
    channel[ch].vpos += channel[ch].vrate;
  }
} /* playroutine */

static const char *getChipId() {
  return "P1XL";
}

static ChipError init() {
  for (size_t i = 0; i < 2; i++) {
    sBlipBuffer[i] = blip_new(SAMPLES_PER_PLAYROUTINE * 2);
    blip_set_rates(sBlipBuffer[i], CLOCK_RATE, SAMPLE_RATE);
    blip_clear(sBlipBuffer[i]);
  }
  trackwait = 0;
  trackpos = 0;
  playsong = 0;
  playtrack = 0;
  for (size_t i = 0; i < NUM_CHANNELS; i++) {
    osc[i].volume = 0;
    osc[i].buzzseed = 1;
    osc[i].pan = 8;
    channel[i].inum = 0;
    channel[i].filterLow = 0;
    channel[i].filterHigh = 255;
  }
  for (int i = 1; i < 256; i++) {
    sprintf(instrument[i].name, "INSTR %02X", i);
    instrument[i].length = 1;
    instrument[i].line[0].cmd = '0';
    instrument[i].line[0].param = 0;
  }
  for (size_t i = 0; i < 16; i++) {
    volumeTable[i].length = 1;
    volumeTable[i].column[0] = 0;

    dutyTable[i].length = 1;
    dutyTable[i].column[0] = 0;

    panTable[i].length = 1;
    panTable[i].column[0] = 8;
    for (size_t j = 0; j < 32; j++) {
      waveTable[i][j] = 8;
    }
  }
  return NO_ERR;
} /* init */

static ChipError shutdown() {
  for (size_t j = 0; j < 2; j++) {
    blip_delete(sBlipBuffer[j]);
  }
  return NO_ERR;
}

static ChipError newSong() {
  return NO_ERR;
}

static ChipError loadSong(const char *_filename) {
  FILE *f;
  char buf[1024];
  char str[1024];
  int cmd[3];
  int i1, i2, trk[4], transp[4], param[3], note, instr;
  int i;

  snprintf(sFilename, sizeof(sFilename), "%s", _filename);

  f = fopen(_filename, "r");
  if (!f) {
    return "Cannot load file.";
  }
  songlen = 1;
  while (!feof(f) && fgets(buf, sizeof(buf), f)) {
    if (9 == sscanf(buf, "song %x %x %x %x %x %x %x %x %x", &i1,
                    &trk[0], &transp[0],
                    &trk[1], &transp[1],
                    &trk[2], &transp[2],
                    &trk[3], &transp[3])) {
      for (i = 0; i < 4; i++) {
        song[i1].track[i] = trk[i];
        song[i1].transp[i] = transp[i];
      }
      if (songlen <= i1) {
        songlen = i1 + 1;
      }
    } else if (8 == sscanf(buf, "pattern %x %x %x %x %x %x %x %x",
                           &i1, &i2,
                           &note, &instr,
                           &cmd[0], &param[0],
                           &cmd[1], &param[1])) {
      track[i1].line[i2].note = note;
      track[i1].line[i2].instr = instr;
      for (i = 0; i < 2; i++) {
        track[i1].line[i2].cmd[i] = cmd[i];
        track[i1].line[i2].param[i] = param[i];
      }
    } else if (2 == sscanf(buf, "instrumentName %x %255[^\n]s", &i1, str)) {
      strncpy(instrument[i1].name, str, 255);
    } else if (4 == sscanf(buf, "instrument %x %x %x %x", &i1, &i2, &cmd[0], &param[0])) {
      instrument[i1].line[i2].cmd = cmd[0];
      instrument[i1].line[i2].param = param[0];
      if (instrument[i1].length <= i2) {
        instrument[i1].length = i2 + 1;
      }
    } else if (3 == sscanf(buf, "volume %x %x %x", &i1, &i2, &cmd[0])) {
      volumeTable[i1].column[i2] = cmd[0];
      if (volumeTable[i1].length <= i2) {
        volumeTable[i1].length = i2 + 1;
      }
    } else if (3 == sscanf(buf, "duty %x %x %x", &i1, &i2, &cmd[0])) {
      dutyTable[i1].column[i2] = cmd[0];
      if (dutyTable[i1].length <= i2) {
        dutyTable[i1].length = i2 + 1;
      }
    } else if (3 == sscanf(buf, "pan %x %x %x", &i1, &i2, &cmd[0])) {
      panTable[i1].column[i2] = cmd[0];
      if (panTable[i1].length <= i2) {
        panTable[i1].length = i2 + 1;
      }
    } else if (3 == sscanf(buf, "wave %x %x %x", &i1, &i2, &cmd[0])) {
      waveTable[i1][i2] = cmd[0];
    }
  }
  fclose(f);
  return NO_ERR;
} /* loadSong */

static ChipError saveSong(const char *filename) {
  FILE *f;
  int i, j;

  f = fopen(filename, "w");
  if (!f) {
    return "save error!\n";
  }
  fprintf(f, "musicchip tune\n");
  fprintf(f, "version 1\n");
  fprintf(f, "\n");
  for (i = 0; i < songlen; i++) {
    fprintf(f, "song %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", i,
            song[i].track[0], song[i].transp[0],
            song[i].track[1], song[i].transp[1],
            song[i].track[2], song[i].transp[2],
            song[i].track[3], song[i].transp[3]);
  }
  fprintf(f, "\n");
  for (i = 1; i < 256; i++) {
    for (j = 0; j < PATTERN_LEN; j++) {
      struct PatternLine *tl = &track[i].line[j];
      if (tl->note || tl->instr || tl->cmd[0] || tl->cmd[1]) {
        fprintf(f, "pattern %02x %02x %02x %02x %02x %02x %02x %02x\n", i, j,
                tl->note, tl->instr,
                tl->cmd[0], tl->param[0],
                tl->cmd[1], tl->param[1]);
      }
    }
  }
  fprintf(f, "\n");
  for (i = 1; i < 256; i++) {
    if (instrument[i].length > 1) {
      fprintf(f, "instrumentName %02x %s\n", i, instrument[i].name);
      for (j = 0; j < instrument[i].length; j++) {
        fprintf(f, "instrument %02x %02x %02x %02x \n", i, j,
                instrument[i].line[j].cmd, instrument[i].line[j].param);
      }
    }
  }
  fprintf(f, "\n");
  for (i = 1; i < 16; i++) {
    if (volumeTable[i].length > 1) {
      for (j = 0; j < volumeTable[i].length; j++) {
        fprintf(f, "volume %02x %02x %02x\n", i, j, volumeTable[i].column[j]);
      }
    }
  }
  fprintf(f, "\n");
  for (i = 1; i < 16; i++) {
    if (dutyTable[i].length > 1) {
      for (j = 0; j < dutyTable[i].length; j++) {
        fprintf(f, "duty %02x %02x %02x\n", i, j, dutyTable[i].column[j]);
      }
    }
  }
  fprintf(f, "\n");
  for (i = 1; i < 16; i++) {
    if (panTable[i].length > 1) {
      for (j = 0; j < panTable[i].length; j++) {
        fprintf(f, "pan %02x %02x %02x\n", i, j, panTable[i].column[j]);
      }
    }
  }
  fprintf(f, "\n");
  for (i = 1; i < 16; i++) {
    bool used = false;
    for (j = 0; j < 32; j++) {
      if (waveTable[i][j] != 8) {
        used = true;
        break;
      }
    }
    if (used) {
      for (j = 0; j < 32; j++) {
        fprintf(f, "wave %02x %02x %02x\n", i, j, waveTable[i][j]);
      }
    }
  }
  fclose(f);
  return NO_ERR;
} /* saveSong */

static ChipError insertSongRow(u8 _channelNum, u8 _atSongRow) {
  if (songlen < 256) {
    memmove(&song[_atSongRow + 1],
            &song[_atSongRow + 0],
            sizeof(struct SongLine) * (songlen - _atSongRow));
    songlen++;
    memset(&song[_atSongRow], 0, sizeof(struct SongLine));
    return NO_ERR;
  } else {
    return "Song is full.";
  }
}

static ChipError addSongRow() {
  if (songlen < 256) {
    memset(&song[songlen], 0, sizeof(struct SongLine));
    songlen++;
    return NO_ERR;
  } else {
    return "Song is full.";
  }
}

static ChipError deleteSongRow(u8 _channelNum, u8 _songRow) {
  if (songlen > 1) {
    memmove(&song[_songRow + 0],
            &song[_songRow + 1],
            sizeof(struct SongLine) * (songlen - _songRow - 1));
    songlen--;
  }
  return NO_ERR;
}

static ChipError insertPatternRow(u8 _channelNum, u8 _patternNum, u8 _atPatternRow) {
  memmove(&(track[_patternNum].line[_atPatternRow + 1]),
          &(track[_patternNum].line[_atPatternRow + 0]),
          sizeof(struct PatternLine) * (PATTERN_LEN - _atPatternRow));
  memset(&(track[_patternNum].line[_atPatternRow]), 0, sizeof(struct PatternLine));
  return NO_ERR;
}

static ChipError addPatternRow(u8 _channelNum, u8 _patternNum) {
  return NO_ERR;
}

static ChipError deletePatternRow(u8 _channelNum, u8 _patternNum, u8 _patternRow) {
  memmove(&(track[_patternNum].line[_patternRow + 0]),
          &(track[_patternNum].line[_patternRow + 1]),
          sizeof(struct PatternLine) * (PATTERN_LEN - _patternRow - 1));
  memset(&(track[_patternNum].line[PATTERN_LEN - 1]), 0, sizeof(struct PatternLine));
  return NO_ERR;
}

static ChipError insertInstrumentRow(u8 _instrument, u8 _atInstrumentRow) {
  struct Instrument *in = &instrument[_instrument];
  if (in->length < 256) {
    memmove(&in->line[_atInstrumentRow + 1],
            &in->line[_atInstrumentRow + 0],
            sizeof(struct InstrumentLine) * (in->length - _atInstrumentRow));
    in->length++;
    in->line[_atInstrumentRow].cmd = '0';
    in->line[_atInstrumentRow].param = 0;
    return NO_ERR;
  } else {
    return "Instrument is full.";
  }
}

static ChipError addInstrumentRow(u8 _instrument) {
  struct Instrument *in = &instrument[_instrument];
  if (in->length < 256) {
    in->line[in->length].cmd = '0';
    in->line[in->length].param = 0;
    in->length++;
    return NO_ERR;
  } else {
    return "Instrument is full.";
  }
}

static ChipError deleteInstrumentRow(u8 _instrument, u8 _instrumentRow) {
  struct Instrument *in = &instrument[_instrument];
  if (in->length > 1) {
    memmove(&in->line[_instrumentRow + 0],
            &in->line[_instrumentRow + 1],
            sizeof(struct InstrumentLine) * (in->length - _instrumentRow - 1));
    in->length--;
  }
  return NO_ERR;
}

static ChipError insertTableColumn(u8 _tableKind, u8 _table, u8 _atColumn) {
  switch (_tableKind) {
    case 0: // VOLUME
    {
      struct VolumeTable *t = &volumeTable[_table];
      if (t->length < 256) {
        memmove(&t->column[_atColumn + 1],
                &t->column[_atColumn + 0],
                sizeof(struct VolumeTable) * (t->length - _atColumn));
        t->length++;
        t->column[_atColumn] = 0;
        return NO_ERR;
      } else {
        return "Table is full.";
      }
    }

    case 1: // DUTY
    {
      struct DutyTable *t = &dutyTable[_table];
      if (t->length < 256) {
        memmove(&t->column[_atColumn + 1],
                &t->column[_atColumn + 0],
                sizeof(struct DutyTable) * (t->length - _atColumn));
        t->length++;
        t->column[_atColumn] = 0;
        return NO_ERR;
      } else {
        return "Table is full.";
      }
    }

    case 2: // PAN
    {
      struct PanTable *t = &panTable[_table];
      if (t->length < 256) {
        memmove(&t->column[_atColumn + 1],
                &t->column[_atColumn + 0],
                sizeof(struct PanTable) * (t->length - _atColumn));
        t->length++;
        t->column[_atColumn] = 0;
        return NO_ERR;
      } else {
        return "Table is full.";
      }
    }

    case 3: // WAVE
      return "WAVE tables are a fixed size.";
  } /* switch */
  return NO_ERR;
} /* insertTableColumn */

static ChipError addTableColumn(u8 _tableKind, u8 _table) {
  switch (_tableKind) {
    case 0: // VOLUME
    {
      struct VolumeTable *t = &volumeTable[_table];
      if (t->length < 256) {
        t->column[t->length] = 0;
        t->length++;
        return NO_ERR;
      } else {
        return "Table is full.";
      }
    }

    case 1: // DUTY
    {
      struct DutyTable *t = &dutyTable[_table];
      if (t->length < 256) {
        t->column[t->length] = 0;
        t->length++;
        return NO_ERR;
      } else {
        return "Table is full.";
      }
    }

    case 2: // PAN
    {
      struct PanTable *t = &panTable[_table];
      if (t->length < 256) {
        t->column[t->length] = 0;
        t->length++;
        return NO_ERR;
      } else {
        return "Table is full.";
      }
    }

    case 3: // WAVE
      return "WAVE tables are a fixed size.";
  }
  return NO_ERR;
} /* addTableColumn */

static ChipError deleteTableColumn(u8 _tableKind, u8 _table, u8 _atColumn) {
  switch (_tableKind) {
    case 0: // VOLUME
    {
      struct VolumeTable *t = &volumeTable[_table];
      if (t->length > 1) {
        memmove(&t->column[_atColumn + 0],
                &t->column[_atColumn + 1],
                sizeof(struct VolumeTable) * (t->length - _atColumn - 1));
        t->length--;
      }
      return NO_ERR;
    }

    case 1: // DUTY
    {
      struct DutyTable *t = &dutyTable[_table];
      if (t->length > 1) {
        memmove(&t->column[_atColumn + 0],
                &t->column[_atColumn + 1],
                sizeof(struct DutyTable) * (t->length - _atColumn - 1));
        t->length--;
      }
      return NO_ERR;
    }

    case 2: // PAN
    {
      struct PanTable *t = &panTable[_table];
      if (t->length > 1) {
        memmove(&t->column[_atColumn + 0],
                &t->column[_atColumn + 1],
                sizeof(struct PanTable) * (t->length - _atColumn - 1));
        t->length--;
      }
      return NO_ERR;
    }

    case 3: // WAVE
      return "WAVE tables are a fixed size.";
  }
  return NO_ERR;
} /* deleteTableColumn */

static u8 getNumMetaData() {
  return 0;
}

static ChipMetaDataEntry *getMetaData(u8 _index) {
  return NULL;
}

static ChipError setMetaData(u8 _index, ChipMetaDataEntry *entry) {
  return NO_ERR;
}

static u8 getNumSaveOptions() {
  return 0;
}

static ChipMetaDataEntry *getSaveOptions(u8 _index) {
  return NULL;
}

static ChipError setSaveOptions(u8 _index, ChipMetaDataEntry *entry) {
  return NO_ERR;
}

static u16 getNumSongRows() {
  return songlen;
}

static u8 getNumSongDataColumns(u8 _channelNum) {
  return 5;
}

static ChipDataType getSongDataType(u8 _songRow, u8 _channelNum, u8 _songDataColumn) {
  if (_songDataColumn == 2) {
    return CDT_LABEL;
  }
  return CDT_HEX;
}

static u8 getSongData(u8 _songRow, u8 _channelNum, u8 _songDataColumn) {
  switch (_songDataColumn) {
    case 0:return GETHI(song[_songRow].track[_channelNum]);
    case 1:return GETLO(song[_songRow].track[_channelNum]);
    case 2:return ':';
    case 3:return GETHI(song[_songRow].transp[_channelNum]);
    case 4:return GETLO(song[_songRow].transp[_channelNum]);
  }
  return 0;
}

static u8 clearSongData(u8 _songRow, u8 _channelNum, u8 _songDataColumn) {
  switch (_songDataColumn) {
    case 0:return SETHI(song[_songRow].track[_channelNum], 0);
    case 1:return SETLO(song[_songRow].track[_channelNum], 0);
    case 3:return SETHI(song[_songRow].transp[_channelNum], 0);
    case 4:return SETLO(song[_songRow].transp[_channelNum], 0);
  }
  return 0;
}

static u8 setSongData(u8 _songRow, u8 _channelNum, u8 _songDataColumn, u8 _data) {
  switch (_songDataColumn) {
    case 0:return SETHI(song[_songRow].track[_channelNum], _data);
    case 1:return SETLO(song[_songRow].track[_channelNum], _data);
    case 3:return SETHI(song[_songRow].transp[_channelNum], _data);
    case 4:return SETLO(song[_songRow].transp[_channelNum], _data);
  }
  return _data;
}

static void setSongPattern(u8 _songRow, u8 _channelNum, u8 _pattern) {
  song[_songRow].track[_channelNum] = _pattern;
}

static u8 getNumChannels() {
  return 4;
}

static const char *getChannelName(u8 _patternNum, u8 _width) {
  static char buf[256];
  if (_width == 0) {
    _width = 255;
  }
  snprintf(buf, _width, "CH%d", _patternNum);
  buf[255] = 0;
  return (const char *) buf;
}

static u16 getNumPatterns() {
  return 256;
}

static u8 getPatternNum(u8 _songRow, u8 _channelNum) {
  return song[_songRow].track[_channelNum];
}

static u8 getPatternLen(u8 patternNum) {
  return PATTERN_LEN;
}

static u8 getNumPatternDataColumns(u8 _channelNum, u8 _patternNum, u8 _patternRow) {
  if (_patternNum == 0) {
    return 14;
  }
  return 12;
}

static ChipDataType getPatternDataType(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn) {
  if (_patternNum == 0) {
    return CDT_LABEL;
  }
  switch (_patternColumn) {
    // Note
    case 0:return CDT_NOTE;

      // Instrument
    case 2:
    case 3:return CDT_HEX;

      // Command 1
    case 5:return CDT_ASCII;

      // Param
    case 6:
    case 7:return CDT_HEX;

      // Command 1
    case 9:return CDT_ASCII;

      // Param
    case 10:
    case 11:return CDT_HEX;

    default:return CDT_LABEL;
  }
}

static u8 getPatternData(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn) {
  if (_patternNum == 0) {
    return ' ';
  }
  switch (_patternColumn) {
    // Note
    case 0:return track[_patternNum].line[_patternRow].note;

      // Instrument
    case 2:return GETHI(track[_patternNum].line[_patternRow].instr);
    case 3:return GETLO(track[_patternNum].line[_patternRow].instr);

      // Command 1
    case 5: {
      int cmd = track[_patternNum].line[_patternRow].cmd[0];
      if (cmd == 0) {
        return '.';
      }
      return toupper(cmd);
    }

      // Param
    case 6:
      if (track[_patternNum].line[_patternRow].cmd[0] == 0) {
        return '.';
      }
      return GETHI(track[_patternNum].line[_patternRow].param[0]);
    case 7:
      if (track[_patternNum].line[_patternRow].cmd[0] == 0) {
        return '.';
      }
      return GETLO(track[_patternNum].line[_patternRow].param[0]);

      // Command 2
    case 9: {
      int cmd = track[_patternNum].line[_patternRow].cmd[1];
      if (cmd == 0) {
        return '.';
      }
      return toupper(cmd);
    }

      // Param
    case 10:
      if (track[_patternNum].line[_patternRow].cmd[1] == 0) {
        return '.';
      }
      return GETHI(track[_patternNum].line[_patternRow].param[1]);
    case 11:
      if (track[_patternNum].line[_patternRow].cmd[1] == 0) {
        return '.';
      }
      return GETLO(track[_patternNum].line[_patternRow].param[1]);

    default:return ' ';
  } /* switch */
}   /* getPatternData */

static u8 clearPatternData(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn) {
  if (_patternNum == 0) {
    return ' ';
  }
  u8 ret;
  switch (_patternColumn) {
    // Note
    case 0:SETHI(track[_patternNum].line[_patternRow].instr, 0);
      SETLO(track[_patternNum].line[_patternRow].instr, 0);
      return track[_patternNum].line[_patternRow].note = 0;

      // Instrument
    case 2:ret = SETHI(track[_patternNum].line[_patternRow].instr, 0);
      if (track[_patternNum].line[_patternRow].instr == 0) {
        track[_patternNum].line[_patternRow].note = 0;
      }
      return ret;
    case 3:ret = SETLO(track[_patternNum].line[_patternRow].instr, 0);
      if (track[_patternNum].line[_patternRow].instr == 0) {
        track[_patternNum].line[_patternRow].note = 0;
      }
      return ret;

      // Command 1
    case 5:track[_patternNum].line[_patternRow].param[0] = 0;
      return track[_patternNum].line[_patternRow].cmd[0] = 0;

      // Param
    case 6:return SETHI(track[_patternNum].line[_patternRow].param[0], 0);
    case 7:return SETLO(track[_patternNum].line[_patternRow].param[0], 0);

      // Command 1
    case 9:track[_patternNum].line[_patternRow].param[1] = 0;
      return track[_patternNum].line[_patternRow].cmd[1] = 0;

      // Param
    case 10:return SETHI(track[_patternNum].line[_patternRow].param[1], 0);
    case 11:return SETLO(track[_patternNum].line[_patternRow].param[1], 0);

    default:return ' ';
  } /* switch */
}   /* getPatternData */

static u8 setPatternData(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn, u8 _instrument,
                         u8 _data) {
  if (_patternNum == 0) {
    return ' ';
  }
  switch (_patternColumn) {
    // Note
    case 0:track[_patternNum].line[_patternRow].instr = _instrument;
      if (_data == 255) {
        return track[_patternNum].line[_patternRow].note = 255;
      } else {
        return track[_patternNum].line[_patternRow].note = _data + 1;
      }
      // Instrument
    case 2:return SETHI(track[_patternNum].line[_patternRow].instr, _data);
    case 3:return SETLO(track[_patternNum].line[_patternRow].instr, _data);

      // Command 1
    case 5:return track[_patternNum].line[_patternRow].cmd[0] = _data;

      // Param
    case 6:return SETHI(track[_patternNum].line[_patternRow].param[0], _data);
    case 7:return SETLO(track[_patternNum].line[_patternRow].param[0], _data);

      // Command 1
    case 9:return track[_patternNum].line[_patternRow].cmd[1] = _data;

      // Param
    case 10:return SETHI(track[_patternNum].line[_patternRow].param[1], _data);
    case 11:return SETLO(track[_patternNum].line[_patternRow].param[1], _data);

    default:return ' ';
  }
} /* setPatternData */

static u8 getMinOctave() {
  return 0;
}

static u8 getMaxOctave() {
  return 7;
}

static const char *getInstrumentName(u8 _instrument, u8 _stringWidth) {
  static char buf[256];
  if (_stringWidth == 0) {
    _stringWidth = 255;
  }
  strncpy(buf, instrument[_instrument].name, _stringWidth);
  buf[255] = 0;
  return (const char *) buf;
}

static void setInstrumentName(u8 _instrument, char *_instrName) {
  strncpy(instrument[_instrument].name, _instrName, 255);
  instrument[_instrument].name[255] = 0;
}

static u8 instrumentNameLength(u8 _instrument) {
  return false;
}

static u16 getNumInstruments() {
  return 256;
}

static u8 getInstrumentLen(u8 _instrument) {
  return instrument[_instrument].length;
}

static u8 getNumInstrumentParams(u8 _instrument) {
  return 2;
}

static const char *getInstrumentParamName(u8 _instrument, u8 _instrumentParam, u8 _stringWidth) {
  if (_instrumentParam == 0) {
    return "CMD";
  }
  if (_instrumentParam == 1) {
    return "V";
  }
  return "";
}

static u8 getNumInstrumentData(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow) {
  if (_instrumentParam == 0) {
    u8 cmd = instrument[_instrument].line[_instrumentRow].cmd;
    if (cmd == '+' || cmd == '=') {
      return 2;
    }
    return 3;
  } else {
    return 1;
  }
}

static ChipDataType getInstrumentDataType(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow,
                                          u8 _instrumentColumn) {
  if (_instrumentParam == 0) {
    if (_instrumentColumn == 0) {
      return CDT_ASCII;
    }
    u8 cmd = instrument[_instrument].line[_instrumentRow].cmd;
    if (cmd == '+' || cmd == '=') {
      return CDT_NOTE;
    }
    return CDT_HEX;
  } else {
    return CDT_ASCII;
  }
}

static u8 getInstrumentData(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn) {
  if (_instrumentParam == 0) {
    u8 cmd = instrument[_instrument].line[_instrumentRow].cmd;
    if (_instrumentColumn == 0) {
      return toupper(cmd);
    }
    if (cmd == '+' || cmd == '=') {
      return instrument[_instrument].line[_instrumentRow].param;
    }
    if (_instrumentColumn == 1) {
      return GETHI(instrument[_instrument].line[_instrumentRow].param);
    }
    return GETLO(instrument[_instrument].line[_instrumentRow].param);
  } else {
    return '0';
  }
}

static u8 clearInstrumentData(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn) {
  u8 cmd = instrument[_instrument].line[_instrumentRow].cmd;
  if (_instrumentColumn == 0) {
    return 0;
  }
  if (cmd == '+' || cmd == '=') {
    return 0;
  }
  if (_instrumentColumn == 1) {
    return 0;
  }
  return 0;
}

static char *validcmds = "0dfhijlopstvw+=";

static bool setInstrumentData(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn,
                              u8 _data) {
  u8 cmd = instrument[_instrument].line[_instrumentRow].cmd;
  if (_instrumentColumn == 0) {
    u8 ascii = _data;
    /*
    if(ascii >= 'A' && ascii <= 'Z') {
      ascii = tolower(_data);
    }
    */
    if (strchr(validcmds, ascii) != 0) {
      instrument[_instrument].line[_instrumentRow].cmd = tolower(_data);
      return true;
    } else {
      return false;
    }
  } else {
    if (cmd == '+' || cmd == '=') {
      instrument[_instrument].line[_instrumentRow].param = _data + 1;
      return true;
    } else {
      if (_instrumentColumn == 1) {
        SETHI(instrument[_instrument].line[_instrumentRow].param, _data);
        return true;
      } else {
        SETLO(instrument[_instrument].line[_instrumentRow].param, _data);
        return true;
      }
    }
  }
}

static void swapInstrumentRow(u8 _instrument, u8 _instrumentRow1, u8 _instrumentRow2) {
  struct InstrumentLine temp = instrument[_instrument].line[_instrumentRow1];
  instrument[_instrument].line[_instrumentRow1] = instrument[_instrument].line[_instrumentRow2];
  instrument[_instrument].line[_instrumentRow2] = temp;
}

// Tables
static bool useTables() {
  return true;
}

static u8 getNumTableKinds() {
  return 4;
}

const char *getTableKindName(u8 _tableKind) {
  static const char *kinds[] = {
      "VOLUME", "DUTY", "PAN", "WAVE"
  };
  return kinds[_tableKind];
}

static ChipTableStyle getTableStyle(u8 _tableKind) {
  if (_tableKind < 2) {
    return CTS_BOTTOM;
  }
  return CTS_CENTER;
}

static u16 getMinTable(u8 _tableKind) {
  return 1;
}

static u16 getNumTables(u8 _tableKind) {
  return 16;
}

static u8 getTableDataLen(u8 _tableKind, u16 _table) {
  switch (_tableKind) {
    case 0: // VOLUME
      return volumeTable[_table].length;
    case 1: // DUTY
      return dutyTable[_table].length;
    case 2: // PAN
      return panTable[_table].length;
    case 3: // WAVE
      return 32;
  }
  return 0;
}

static u8 getTableData(u8 _tableKind, u16 _table, u8 _tableColumn) {
  switch (_tableKind) {
    case 0: // VOLUME
      return volumeTable[_table].column[_tableColumn];
    case 1: // DUTY
      return dutyTable[_table].column[_tableColumn];
    case 2: // PAN
      return panTable[_table].column[_tableColumn];
    case 3: // WAVE
      return waveTable[_table][_tableColumn];
  }
  return 0;
}

static u8 setTableData(u8 _tableKind, u16 _table, u8 _tableColumn, u8 _data) {
  switch (_tableKind) {
    case 0: // VOLUME
      return volumeTable[_table].column[_tableColumn] = _data;
    case 1: // DUTY
      return dutyTable[_table].column[_tableColumn] = _data;
    case 2: // PAN
      return dutyTable[_table].column[_tableColumn] = _data;
    case 3: // WAVE
      return waveTable[_table][_tableColumn] = _data;
  }
  return 0;
}

static u8 getPlayerSongRow(u8 _channel) {
  if (songpos == 0) {
    return 0;
  }
  return songpos - 1;
}

static u8 getPlayerPatternRow(u8 _channel) {
  return trackpos;
}

static u8 getPlayerPattern(u8 _channelNum) {
  return 0; // TODO: Implement
}

static u8 getPlayerInstrumentRow(u8 _channelNum) {
  return 0; // TODO: Implement
}

static u8 getPlayerInstrument(u8 _channelNum) {
  return 0; // TODO: Implement
}

static void plonk(u8 _note, u8 _channelNum, u8 _instrument, bool _isDown) {
  con_msgf("%d", _note);
  if (_note == 255) {
    channel[_channelNum].volumeTable = 0;
    channel[_channelNum].volumed = 0;
    osc[_channelNum].volume = 0;
  } else {
    channel[_channelNum].tnote = _note + 1;
    channel[_channelNum].inum = _instrument;
    channel[_channelNum].iptr = 0;
    channel[_channelNum].iwait = 0;
    channel[_channelNum].bend = 0;
    channel[_channelNum].bendd = 0;
    channel[_channelNum].volumed = 0;
    channel[_channelNum].vdepth = 0;
  }
}

static void playSongFrom(u8 _songRow, u8 _songColumn, u8 _patternRow, u8 _patternColumn) {
  startplaysong(_songRow);
}

static void playPatternFrom(u8 _songRow, u8 _songColumn, u8 _patternRow, u8 _patternColumn) {}

static bool isPlaying() {
  return playsong != 0;
}

static void stop() {
  silence();
}

static const u32 buzzfeed[] = {
    0x00000C,   // 0 4 bit
    0x000012,   // 1 5 bit
    0x000021,   // 2 6 bit
    0x000044,   // 3 7 bit
    0x00008E,   // 4 8 bit
    0x000108,   // 5 9 bit
    0x000204,   // 6 10 bit
    0x000829,   // 7 12 bit
    0x002015,   // 8 14 bit
    0x008016,   // 9 16 bit
    0x020013,   // A 18 bit
    0x080004,   // B 20 bit
    0x200001,   // C 22 bit
    0x80000D,   // D 24 bit
    0x2000023,  // E 26 bit
    0x80001D8,  // F 28 bit
};

static void fillBlips() {
  u8 i;
  playroutine();
  for (i = 0; i < NUM_CHANNELS; i++) {
    if (osc[i].freq > 0) {
      size_t t = osc[i].lastTime;
      for (; t < CLOCKS_PER_PLAYROUTINE; t += osc[i].freq) {
        s8 value = -1; // [-8,7]
        const u8 phase = osc[i].phase;
        if ((osc[i].waveform & WF_TRI) != 0) {
          if (phase < 8) {
            value = phase & 7;
          } else if (phase < 24) {
            value = 15 - phase;
          } else {
            value = -8 + (phase & 7);
          }
        }
        if ((osc[i].waveform & WF_SAW) != 0) {
          value = -8 + (phase >> 1);
        }
        if ((osc[i].waveform & WF_PUL) != 0) {
          value = ((phase >> 1) > osc[i].duty) ? 7 : -8;
        }
        if ((osc[i].waveform & WF_NOI) != 0) {
          if (osc[i].buzzseed & 1) {
            osc[i].buzzseed = (osc[i].buzzseed >> 1) ^ buzzfeed[osc[i].duty];
          } else {
            osc[i].buzzseed = osc[i].buzzseed >> 1;
          }
          if (osc[i].buzzseed == 0) {
            osc[i].buzzseed = 1;
          }
          value = (osc[i].buzzseed & 15) - 8;
        }
        if ((osc[i].waveform & WF_WAV) != 0) {
          u8 wave = (osc[i].waveform >> 4) & 0xf;
          value = waveTable[wave][osc[i].phase] - 8;
        }
        const s16 panLeft = (osc[i].pan >= 8) ? 15 : osc[i].pan * 2;
        const s16 panRight = (osc[i].pan <= 8) ? 15 : (16 - osc[i].pan) * 2;
        const s16 left = value * (((osc[i].volume) * panLeft) >> 4);
        const s16 right = value * (((osc[i].volume) * panRight) >> 4);
        // acc.left += filter_sample(left, i, 0);    // rhs = [-8160,7905]
        // acc.right += filter_sample(right, i, 1);  // rhs = [-8160,7905]
        if (osc[i].lastLeft != left) {
          const s16 delta = left - osc[i].lastLeft;
          blip_add_delta(sBlipBuffer[0], t, delta);
          osc[i].lastLeft = left;
        }
        if (osc[i].lastRight != right) {
          const s16 delta = right - osc[i].lastRight;
          blip_add_delta(sBlipBuffer[1], t, delta);
          osc[i].lastRight = right;
        }
        if (osc[i].freq < 0) {
          osc[i].freq = 0;
        }
        osc[i].lastPhase = osc[i].phase;
        osc[i].phase = (phase + 1) & 0x1f;
      }
      osc[i].lastTime = t - CLOCKS_PER_PLAYROUTINE;
    }
  }
  blip_end_frame(sBlipBuffer[0], CLOCKS_PER_PLAYROUTINE);
  blip_end_frame(sBlipBuffer[1], CLOCKS_PER_PLAYROUTINE);
} /* fillBlips */

static void getSamples(ChipSample *_buf, int _len) {
  while (blip_samples_avail(sBlipBuffer[0]) < _len) {
    fillBlips();
  }
  for (size_t i = 0; i < 2; i++) {
    blip_read_samples(sBlipBuffer[i], (short *) (((s16 *) _buf) + i), _len, true);
  }
  for (size_t i = 0; i < _len; i++) {
    _buf[i] = chip_expandSample(_buf[i]);
  }
}

static const char *getInstrumentLabel(u8 _instrument, u8 _instrumentRow) {
  static char buf[3];
  snprintf(buf, 3, "%02X", _instrumentRow);
  return buf;
}

static const char *getInstrumentHelp(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn) {
  return ""; // TODO: Implement
}

static const char *getSongHelp(u8 _songRow, u8 _channelNum, u8 _songDataColumn) {
  return ""; // TODO: Implement
}

static const char *getPatternHelp(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn) {
  return ""; // TODO: Implement
}

ChipInterface chip_p1xl = {
    // Tracker Commands
    getChipId,
    init,
    shutdown,
    newSong,
    loadSong,
    saveSong,
    insertSongRow,
    addSongRow,
    deleteSongRow,
    insertPatternRow,
    addPatternRow,
    deletePatternRow,
    insertInstrumentRow,
    addInstrumentRow,
    deleteInstrumentRow,
    insertTableColumn,
    addTableColumn,
    deleteTableColumn,

    // Metadata
    getNumMetaData,
    getMetaData,
    setMetaData,

    // Save Options
    getNumSaveOptions,
    getSaveOptions,
    setSaveOptions,

    // Song Data
    getNumSongRows,
    getNumSongDataColumns,
    getSongDataType,
    getSongData,
    clearSongData,
    setSongData,
    setSongPattern,
    getSongHelp,

    // Channels
    getNumChannels,
    getChannelName,

    // Patterns
    getNumPatterns,
    getPatternNum,
    getPatternLen,
    getNumPatternDataColumns,
    getPatternDataType,
    getPatternData,
    getPatternHelp,
    clearPatternData,
    setPatternData,
    getMinOctave,
    getMaxOctave,

    // Instruments
    getInstrumentName,
    setInstrumentName,
    instrumentNameLength,
    getNumInstruments,
    getInstrumentLen,
    getNumInstrumentParams,
    getInstrumentParamName,
    getNumInstrumentData,
    getInstrumentDataType,
    getInstrumentData,
    getInstrumentHelp,
    getInstrumentLabel,
    clearInstrumentData,
    setInstrumentData,
    swapInstrumentRow,

    // Tables
    useTables,
    getNumTableKinds,
    getTableKindName,
    getTableStyle,
    getMinTable,
    getNumTables,
    getTableDataLen,
    getTableData,
    setTableData,

    // Player
    getPlayerSongRow,
    getPlayerPatternRow,
    getPlayerPattern,
    getPlayerInstrumentRow,
    getPlayerInstrument,
    plonk,
    playSongFrom,
    playPatternFrom,
    isPlaying,
    stop,
    silence,
    getSamples
};

