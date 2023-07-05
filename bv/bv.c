#include "../chip.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "../console.h"

#define CHIPS_IMPL

#include "../m6581.h"

typedef enum Cmd6Bit_t : unsigned {
  Cmd6Bit_Control = 0,
  Cmd6Bit_Inc = 1,
  Cmd6Bit_Set = 2,
  Cmd6Bit_Extra = 3
} Cmd6Bit;

typedef enum Cmd4Bit_t : unsigned {
  Cmd4Bit_Arpeggio = 0 | (Cmd6Bit_Extra << 2),
  Cmd4Bit_Loop = 1 | (Cmd6Bit_Extra << 2),
  Cmd4Bit_3Bit = 2 | (Cmd6Bit_Extra << 2),
  Cmd4Bit_1Bit = 3 | (Cmd6Bit_Extra << 2)
} Cmd4Bit;

typedef enum Cmd3Bit_t : unsigned {
  Cmd3Bit_Wait = 0 | (Cmd4Bit_3Bit << 1),
  Cmd3Bit_Vibrato = 1 | (Cmd4Bit_3Bit << 1),
} Cmd3Bit;

typedef enum Cmd1Bit_t : unsigned {
  Cmd1Bit_Sync = 0 | (Cmd4Bit_1Bit << 3),
  Cmd1Bit_Ring = 1 | (Cmd4Bit_1Bit << 3),
  Cmd1Bit_ExternalVoiceFlag = 2 | (Cmd4Bit_1Bit << 3),
  Cmd1Bit_Reserved3 = 3 | (Cmd4Bit_1Bit << 3),
  Cmd1Bit_Reserved4 = 4 | (Cmd4Bit_1Bit << 3),
  Cmd1Bit_Reserved5 = 5 | (Cmd4Bit_1Bit << 3),
  Cmd1Bit_Reserved6 = 6 | (Cmd4Bit_1Bit << 3),
  Cmd1Bit_NoteOffJmpPosition = 7 | (Cmd4Bit_1Bit << 3),
} Cmd1Bit;

typedef struct InstrumentInstruction_t {
  union {
    // Control
    struct __attribute__((packed)) {
      unsigned controlReset: 1;
      unsigned controlTriangle: 1;
      unsigned controlSawtooth: 1;
      unsigned controlPulse: 1;
      unsigned controlNoise: 1;
      unsigned controlGate: 1;
      Cmd6Bit cmd: 2; // Cmd6Bit_Control
    } control;
    // Inc
    struct __attribute__((packed)) {
      int incAmount: 5;
      unsigned incMode: 1; // 0 = PulseWidth, 1 = Frequency
      Cmd6Bit cmd: 2; // Cmd6Bit_Inc
    } inc;
    // Set
    struct __attribute__((packed)) {
      unsigned setValue: 5;
      unsigned setMode: 1; // 0 = PulseWidth, 1 = Frequency
      Cmd6Bit cmd: 2; // Cmd6Bit_Set
    } set;
    // Extra
    struct __attribute__((packed)) {
      // Arpeggio = 0
      unsigned arpeggioNote: 4;
      Cmd4Bit cmd: 4; // Cmd4Bit_Arpeggio
    } arpeggio;
    struct __attribute__((packed)) {
      // Loop = 1
      unsigned loopPosition: 4;
      Cmd4Bit cmd: 4; // Cmd4Bit_Loop
    } loop;
    struct __attribute__((packed)) {
      // Wait
      unsigned waitFrames: 3;
      Cmd3Bit cmd: 5; // Cmd3Bit_Wait
    } wait;
    struct __attribute__((packed)) {
      // Vibrato
      unsigned vibratoDepth: 2;
      unsigned incMode: 1; // 0 = PulseWidth, 1 = Frequency
      Cmd3Bit cmd: 5; // Cmd3Bit_Vibrato
    } vibrato;
    struct __attribute__((packed)) {
      // Sync
      unsigned syncEnable: 1;
      Cmd1Bit cmd: 7; // Cmd1Bit_Sync
    } sync;
    struct __attribute__((packed)) {
      // Ring
      unsigned ringEnable: 1;
      Cmd1Bit cmd: 7; // Cmd1Bit_Ring
    } ring;
    struct __attribute__((packed)) {
      // External Voice Flag
      unsigned externalVoiceFlag: 1;
      Cmd1Bit cmd: 7; // Cmd1Bit_ExternalVoiceFlag
    } externalVoiceFlag;
    struct __attribute__((packed)) {
      // Note off jmp position
      unsigned noteOffJmpPosition: 1;
      Cmd1Bit cmd: 7; // Cmd1Bit_NoteOffJmpPosition
    } noteOffJmpPosition;
    struct __attribute__((packed)) {
      unsigned value6: 6;
      Cmd6Bit cmd6: 2;
    };
    struct __attribute__((packed)) {
      unsigned value4: 4;
      Cmd4Bit cmd4: 4;
    };
    struct __attribute__((packed)) {
      unsigned value3: 3;
      Cmd3Bit cmd3: 5;
    };
    struct __attribute__((packed)) {
      unsigned value1: 1;
      Cmd1Bit cmd1: 7;
    };
    u8 raw;
  } __attribute__((packed));
} __attribute__((packed)) InstrumentInstruction;

typedef struct Instrument_t {
  unsigned decay: 4;
  unsigned attack: 4;
  unsigned release: 4;
  unsigned sustain: 4;
  InstrumentInstruction program[30];
} __attribute__((packed)) Instrument;

typedef enum SongTrackSpeed_t : unsigned {
  SongTrackSpeed_Normal = 0,
  SongTrackSpeed_Half = 1,
  SongTrackSpeed_Quarter = 2,
  SongTrackSpeed_Eighth = 3
} SongTrackSpeed;

typedef enum SongTrackOptions_t : unsigned {
  SongTrackOptions_PlayOnce = 0,
  SongTrackOptions_PlayTwice = 1,
  SongTrackOptions_Silence = 2,
  SongTrackOptions_EndLoop = 3
} SongTrackOptions;

typedef struct SongTrack_t {
  unsigned pattern: 4;
  /*
   * 0 - normal
   * 1 - 1/2 speed
   * 2 - 1/4 speed
   * 3 - 1/8 speed
   */
  SongTrackSpeed speed: 2;
  /*
   * 0 - play once
   * 1 - play twice
   * 2 - silence
   * 3 - end/loop
   */
  SongTrackOptions options: 2;
} __attribute__((packed)) SongTrack;

typedef struct SongLine_t {
  unsigned note: 5; // 0 = note off
  unsigned instrument: 3;
} __attribute__((packed)) SongLine;

typedef enum SongMeter_t : unsigned {
  SongMeter_4_4 = 0,
  SongMeter_3_4 = 1
} SongMeter;

typedef enum SongLoop_t : unsigned {
  SongLoop_Stop = 0,
  SongLoop_Loop = 1
} SongLoop;

typedef enum SongOctave_t : unsigned {
  SongOctave_Disabled = 0,
  SongOctave_Bass = 1,
  SongOctave_Alto = 2,
  SongOctave_Treble = 3
} SongOctave;

typedef struct Song_t {
  u8 instrumentSet;
  // Channel octaves, 0 = disable channel
  SongOctave ch0Octave: 2;
  SongOctave ch1Octave: 2;
  SongOctave ch2Octave: 2;
  SongMeter meter: 1;
  SongLoop loopSong: 1; // 0 = Stop, 1 = Loop
  /*
   *  0 = 250(PAL) / 300(NTSC)
   *  1 = 187.5(PAL) / 225(NTSC)
   *  2 = 150(PAL) / 180(NTSC)
   *  3 = 125(PAL) / 150(NTSC)
   *  4 = 107.14(PAL) / 128.57(NTSC)
   *  5 = 93.75(PAL) / 112.5(NTSC)
   *  6 = 83.33(PAL) / 100(NTSC)
   *  7 = 75(PAL) / 90(NTSC)
   */
  unsigned tempo: 3;
  unsigned reserved: 5;
  u8 reserved2;
  SongTrack tracks[3][20];
  union {
    SongLine pattern44[12][16]; // 4/4 time
    SongLine pattern34[16][12]; // 3/4 time
  } __attribute__((packed));
} __attribute__((packed)) Song;

static Song sSong;
static Instrument sInstruments[8];
static u8 sSidRegisters[0x20] = {0};

//////////////////////////////////////////////////
// Playroutine

void sidReset();

#define Cmd1BitMask 0xFE
#define Cmd3BitMask 0xF8
#define Cmd4BitMask 0xF0
#define Cmd6BitMask 0xC0

#define CmdControl 0x00 // 00000000
#define CmdInc 0x40 // 01000000
#define CmdSet 0x80 // 10000000
#define CmdArpeggio 0xC0 // 11000000
#define CmdLoop 0xD0 // 11010000
#define CmdWait 0xE0 // 11100000
#define CmdVibrato 0xE8 // 11101000
#define CmdSync 0xF0 // 11110000
#define CmdRing 0xF2 // 11110010
#define CmdExternalVoiceFlag 0xF4 // 11110100
#define CmdNoteOffJmpPos 0xFF // 11111111

#define InstrPosMask 0x1F

static const u8 sFreqTablePalLo[96] = {
    // Bass
    0x17, 0x27, 0x39, 0x4b, 0x5f, 0x74, 0x8a, 0xa1, 0xba, 0xd4, 0xf0, 0x0e, 0x2d, 0x4e, 0x71, 0x96,
    0xbe, 0xe8, 0x14, 0x43, 0x74, 0xa9, 0xe1, 0x1c, 0x5a, 0x9c, 0xe2, 0x2d, 0x7c, 0xcf, 0x28, 0x85,
    // Alto
    0xe8, 0x52, 0xc1, 0x37, 0xb4, 0x39, 0xc5, 0x5a, 0xf7, 0x9e, 0x4f, 0x0a, 0xd1, 0xa3, 0x82, 0x6e,
    0x68, 0x71, 0x8a, 0xb3, 0xee, 0x3c, 0x9e, 0x15, 0xa2, 0x46, 0x04, 0xdc, 0xd0, 0xe2, 0x14, 0x67,
    // Treble
    0xdd, 0x79, 0x3c, 0x29, 0x44, 0x8d, 0x08, 0xb8, 0xa1, 0xc5, 0x28, 0xcd, 0xba, 0xf1, 0x78, 0x53,
    0x87, 0x1a, 0x10, 0x71, 0x42, 0x89, 0x4f, 0x9b, 0x74, 0xe2, 0xf0, 0xa6, 0x0e, 0x33, 0x20, 0xff
};
static const u8 sFreqTablePalHi[96] = {
    // Bass
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x06, 0x06,
    // Alto
    0x06, 0x07, 0x07, 0x08, 0x08, 0x09, 0x09, 0x0a, 0x0a, 0x0b, 0x0c, 0x0d, 0x0d, 0x0e, 0x0f, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x17, 0x18, 0x1a, 0x1b, 0x1d, 0x1f, 0x20, 0x22, 0x24, 0x27, 0x29,
    // Treble
    0x2b, 0x2e, 0x31, 0x34, 0x37, 0x3a, 0x3e, 0x41, 0x45, 0x49, 0x4e, 0x52, 0x57, 0x5c, 0x62, 0x68,
    0x6e, 0x75, 0x7c, 0x83, 0x8b, 0x93, 0x9c, 0xa5, 0xaf, 0xb9, 0xc4, 0xd0, 0xdd, 0xea, 0xf8, 0xff
};
static const u8 sWaitLookup[8] = {1, 2, 4, 8, 16, 32, 64, 128};
static const s8 sVibratoLookup[16] = {
    0, 12, 22, 29, 31, 29, 22, 12, 0, -12, -22, -29, -31, -29, -22, -12
};

static bool sIsPlaying = false;
static u8 sLineCounter[3] = {0};
static SongTrack *sTrack[3] = {0};
static SongLine *sLine[3] = {0};
static u8 sInstrumentPos[3] = {0};
static u8 sNote[3] = {0};
static u8 sArpNote[3] = {0};
static u16 sFreqDelta[3] = {0};
static u16 sPwDelta[3] = {0};
static u8 sVibratoDepth[3] = {0};
static bool sVibratoMode[3] = {true};
static bool sExternalVoiceFlag[3] = {false};

static u8 sSongSpeeds[4];
static u8 sWaitCounter[3] = {0};
static u8 sFrameCounter = 0;
static u8 *sInstrumentSet = (u8 *) &(sInstruments[0]);

void playerInit() {
  u8 songSpeed = (sSong.tempo + 3) << 2;
  for (s8 i = 3; i >= 0; --i) sSongSpeeds[i] = songSpeed << sSong.tracks[i][0].speed;
  for (s8 ch = 2; ch >= 0; --ch) {
    if (sSong.meter == SongMeter_4_4) {
      sTrack[ch] = &sSong.tracks[ch][0];
      sLine[ch] = sSong.pattern44[sTrack[ch]->pattern];
    } else {
      sTrack[ch] = &sSong.tracks[ch][0];
      sLine[ch] = sSong.pattern34[sTrack[ch]->pattern];
    }
    sInstrumentPos[ch] = 0;
    sLineCounter[ch] = songSpeed << sSong.tracks[ch][0].speed;
  }
}

void playerPlayNote(u8 _channel, u8 _note, u8 _instrument, bool _isDown) {
  if (_isDown) {
    sNote[_channel] = _note;
    sInstrumentPos[_channel] = (_instrument << 5) + 2;
    sWaitCounter[_channel] = 0;
    sArpNote[_channel] = 0;
    sVibratoDepth[_channel] = 0;
    sSidRegisters[0x05 + _channel * 7] = sInstrumentSet[sInstrumentPos[_channel] - 2]; // AD
    sSidRegisters[0x06 + _channel * 7] = sInstrumentSet[sInstrumentPos[_channel] - 1]; // SR
  } else {
    sInstrumentPos[_channel] = (_instrument << 5) + 2;
    u8 end = sInstrumentPos[_channel] + 30;
    for (u8 i = sInstrumentPos[_channel]; i < end; ++i) {
      if (sInstrumentSet[i] == CmdNoteOffJmpPos) {
        sInstrumentPos[_channel] = i + 1;
        return;
      }
    }
    sInstrumentPos[_channel] = 0;
    sWaitCounter[_channel] = 0;
    sSidRegisters[0x04 + _channel * 7] &= 0xFE;
  }
}

void playerTick() {
  if (sIsPlaying) {
    // Trigger intruments from song lines
  }
  // Tick instruments
  for (u8 ch = 0; ch < 3; ++ch) {
    // Check end of instrument
    if (sInstrumentPos[ch] == 0) continue;

    // Wait if necessary
    if (sWaitCounter[ch] != 0) {
      sWaitCounter[ch]--;
      continue;
    }

    // Process instrument instructions
    u8 cmd = sInstrumentSet[sInstrumentPos[ch]];
    u8 value = cmd;
    if (cmd == CmdNoteOffJmpPos) {

    } else {
      cmd &= Cmd1BitMask;
      if (cmd == CmdSync) {
        // Sync
        if (value & 1) {
          sSidRegisters[0x04 + ch * 7] |= 0x02;
        } else {
          sSidRegisters[0x04 + ch * 7] &= 0xFD;
        }
      } else if (cmd == CmdRing) {
        // Ring
        if (value & 1) {
          sSidRegisters[0x04 + ch * 7] |= 0x04;
        } else {
          sSidRegisters[0x04 + ch * 7] &= 0xFB;
        }
      } else if (cmd == CmdExternalVoiceFlag) {
        // External voice flag
        sExternalVoiceFlag[ch] = value & 1;
      } else {
        cmd &= Cmd3BitMask;
        if (cmd == CmdWait) {
          // Wait
          sWaitCounter[ch] = sWaitLookup[value & 0x07];
        } else if (cmd == CmdVibrato) {
          // Vibrato
          sVibratoDepth[ch] = value & 0x03;
          sVibratoMode[ch] = value & 0x04;
        } else {
          cmd &= Cmd4BitMask;
          if (cmd == CmdArpeggio) {
            // Arpeggio
            sArpNote[ch] = value & 0x0F;
          } else if (cmd == CmdLoop) {
            // Loop
            if (value == Cmd4Bit_Loop << 4) {
              sInstrumentPos[ch] = 0;
              return;
            }
            sInstrumentPos[ch] -= ((value & 0x0F) + 1);
          } else {
            cmd &= Cmd6BitMask;
            if (cmd == CmdControl) {
              // Control
              value &= 0x3F;
              value = (value << 3) | (value >> 5); // ROL*3
              sSidRegisters[0x04 + ch * 7] &= ~0xF9;
              sSidRegisters[0x04 + ch * 7] |= value;
            } else if (cmd == CmdInc) {
              // Inc
              if (value & 0x20) {
                // Frequency
                sFreqDelta[ch] += (value & 0x10) ? value | 0xE0 : value & 0x1F;
              } else {
                // Pulse width
                sPwDelta[ch] += (value & 0x10) ? value | 0xE0 : value & 0x1F;
              }
            } else if (cmd == CmdSet) {
              // Set
              if (value & 0x20) {
                // Frequency
                sFreqDelta[ch] = (value & 0x1F) << 11;
              } else {
                // Pulse width
                sPwDelta[ch] = (value & 0x1F) << 7;
              }
            }
          }
        }
      }
    }

    // Set frequency
    u8 note = sNote[ch] + sArpNote[ch];
    if (ch == 0) {
      note += ((sSong.ch0Octave - 1) << 5);
    } else if (ch == 1) {
      note += ((sSong.ch1Octave - 1) << 5);
    } else {
      note += ((sSong.ch2Octave - 1) << 5);
    }
    u16 *freq = (u16 *) &sSidRegisters[0x00 + ch * 7];
    *freq = (sFreqTablePalLo[note] | sFreqTablePalHi[note] << 8) + sFreqDelta[ch];

    // Set pulse width
    u16 *pw = (u16 *) &sSidRegisters[0x02 + ch * 7];
    *pw = sPwDelta[ch];

    // Apply Vibrato
    if (sVibratoDepth[ch] != 0) {
      s16 vibrato = (sVibratoLookup[sFrameCounter & 0x0F] << sVibratoDepth[ch]);;
      if (sVibratoMode[ch]) {
        *freq += vibrato;
      } else {
        *pw += vibrato;
      }
    }

    // Increment instrument position
    sInstrumentPos[ch] = ((sInstrumentPos[ch] + 1) & 0x1F) | (sInstrumentPos[ch] & 0xE0);
  } // end for
  sFrameCounter++;
}

void playerPlay() {
  playerInit();
  sIsPlaying = true;
}

void playerStop() {
  sIsPlaying = false;
  sidReset();
}


//////////////////////////////////////////////////

#define C64_FREQUENCY (985248) // clock frequency in Hz
#define C64_VBLANK (C64_FREQUENCY / 50) // 50 Hz
m6581_t sid;
uint64_t pins = 0;
u32 clocks = 0;

void sidReset() {
  m6581_reset(&sid);
  pins = 0;
  for (int i = 0; i < 32; ++i) {
    sSidRegisters[i] = 0;
  }
  // Set volume
  sSidRegisters[0x18] = 0x0F;
/*
  // Test tone
  sSidRegisters[0x00] = 0x68;
  sSidRegisters[0x01] = 0x11;
  sSidRegisters[0x04] = 0x11;
  sSidRegisters[0x05] = 0x00;
  sSidRegisters[0x06] = 0xF0;
*/
}

void sidInit() {
  m6581_init(&sid, &(m6581_desc_t) {
      .tick_hz = C64_FREQUENCY,
      .sound_hz = 44100,
      .magnitude = 1.0f,
  });
  sidReset();
}

void sidTick(ChipSample *_buf, int _len) {
  int pos = 0;
  while (pos < _len) {
    if (clocks == 0) {
      // Call playroutine
      playerTick();
    }
    if (clocks < 0x20) {
      // Copy shadow registers to SID
      pins |= M6581_CS;
      pins &= ~M6581_RW;
      pins &= ~M6581_ADDR_MASK;
      pins |= clocks;
      M6581_SET_DATA(pins, sSidRegisters[clocks]);
      pins = m6581_tick(&sid, pins);
      // con_msgf("%02X > %02X", clocks, sSidRegisters[clocks]);
    } else {
      // Do nothing to SID
      pins &= ~M6581_CS;
      pins |= M6581_RW;
      M6581_SET_DATA(pins, 0);
      pins = m6581_tick(&sid, pins);
    }
    if (pins & M6581_SAMPLE) {
      _buf[pos].left = (s16) (sid.sample * INT16_MAX);
      _buf[pos].right = (s16) (sid.sample * INT16_MAX);
      _buf[pos] = chip_expandSample(_buf[pos]);
      pos++;
    }
    clocks = (clocks + 1) % C64_VBLANK;
  }
}

static const char *getChipId() {
  return "BV";
}

static ChipError newSong() {
  memset(&sSong, 0, sizeof(Song));

  sSong.instrumentSet = 0;
  sSong.ch0Octave = SongOctave_Bass;
  sSong.ch1Octave = SongOctave_Alto;
  sSong.ch2Octave = SongOctave_Treble;
  sSong.meter = SongMeter_4_4;
  sSong.loopSong = SongLoop_Stop;
  sSong.tempo = 2;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 20; j++) {
      sSong.tracks[i][j].pattern = 0;
      sSong.tracks[i][j].speed = SongTrackSpeed_Normal;
      sSong.tracks[i][j].options = SongTrackOptions_PlayOnce;
    }
  }
  memset(&sSong.pattern44, 0, sizeof(sSong.pattern44));

  // Init intstruments
  for (int i = 0; i < 8; i++) {
    sInstruments[i].attack = 0;
    sInstruments[i].decay = 0;
    sInstruments[i].sustain = 0xf;
    sInstruments[i].release = 0;
    sInstruments[i].program[0].raw = 0;
    sInstruments[i].program[0].control.cmd = Cmd6Bit_Control;
    sInstruments[i].program[0].control.controlTriangle = 1;
    sInstruments[i].program[0].control.controlGate = 1;
    for (int j = 1; j < 30; j++) {
      sInstruments[i].program[j].loop.cmd = Cmd4Bit_Loop;
      sInstruments[i].program[j].loop.loopPosition = 0;
    }
  }

  return NO_ERR;
}

static u8 labelString(const char *label, u8 col) {
  if (col < strlen(label)) {
    return label[col];
  } else {
    return ' ';
  }
}

static ChipError init() {
  sidInit();
  newSong();

  con_msgf("Size of song: %d", sizeof(sSong));
  con_msgf("Size of instruments: %d", sizeof(sInstrumentSet));
  con_msgf("Size of SongTrack: %d", sizeof(SongTrack));
  con_msgf("Size of Instrument: %d", sizeof(Instrument));
  con_msgf("Size of InstrumentInstruction: %d", sizeof(InstrumentInstruction));


  return NO_ERR;
}

static ChipError shutdown() {
  return NO_ERR;
}

static ChipError loadSong(const char *_filename) {
  FILE *file = fopen(_filename, "rb");
  if (file == NULL) {
    return ERR_FILE_NOT_FOUND;
  }
  size_t res;
  res = fread(&sSong, sizeof(Song), 1, file);
  if (res != 1) {
    return ERR_FILE_READ;
  }
  res = fread(&sInstruments, sizeof(sInstruments), 1, file);
  if (res != 1) {
    return ERR_FILE_READ;
  }
  fclose(file);
  return NO_ERR;
}

static ChipError saveSong(const char *filename) {
  FILE *file = fopen(filename, "wb");
  if (file == NULL) {
    return ERR_FILE_NOT_FOUND;
  }
  size_t res;
  res = fwrite(&sSong, sizeof(Song), 1, file);
  if (res != 1) {
    return ERR_FILE_WRITE;
  }
  res = fwrite(&sInstruments, sizeof(sInstruments), 1, file);
  if (res != 1) {
    return ERR_FILE_WRITE;
  }
  fclose(file);
  return NO_ERR;
}

static ChipError insertSongRow(u8 _atSongRow) {
  // TODO: Implement
  return NO_ERR;
}

static ChipError addSongRow() {
  // TODO: Implement
  return NO_ERR;
}

static ChipError deleteSongRow(u8 _songRow) {
  // TODO: Implement
  return NO_ERR;
}

static ChipError insertPatternRow(u8 _channelNum, u8 _patternNum, u8 _atPatternRow) {
  // TODO: Implement
  return NO_ERR;
}

static ChipError addPatternRow(u8 _channelNum, u8 _patternNum) {
  // TODO: Implement
  return NO_ERR;
}

static ChipError deletePatternRow(u8 _channelNum, u8 _patternNum, u8 _patternRow) {
  // TODO: Implement
  return NO_ERR;
}

static ChipError insertInstrumentRow(u8 _instrument, u8 _atInstrumentRow) {
  con_msgf("insertInstrumentRow(%d, %d)", _instrument, _atInstrumentRow);
  if (_atInstrumentRow < 4) return ERR_NOT_SUPPORTED;
  if (_atInstrumentRow > 31) return ERR_NOT_SUPPORTED;
  if (_instrument > 7) return ERR_NOT_SUPPORTED;
  u8 start = _atInstrumentRow - 4;
  u8 end = 30 - 4;
  for (int i = end; i >= start; i--) {
    sInstruments[_instrument].program[i + 1] = sInstruments[_instrument].program[i];
  }
  sInstruments[_instrument].program[start].loop.cmd = Cmd4Bit_Loop;
  sInstruments[_instrument].program[start].loop.loopPosition = 0;
  return NO_ERR;
}

static ChipError addInstrumentRow(u8 _instrument) {
  return ERR_NOT_SUPPORTED;
}

static ChipError deleteInstrumentRow(u8 _instrument, u8 _instrumentRow) {
  if (_instrumentRow < 4) return ERR_NOT_SUPPORTED;
  if (_instrumentRow > 31) return ERR_NOT_SUPPORTED;
  if (_instrument > 7) return ERR_NOT_SUPPORTED;
  u8 start = _instrumentRow - 4;
  u8 end = 30 - 4;
  for (int i = start; i < end; i++) {
    sInstruments[_instrument].program[i] = sInstruments[_instrument].program[i + 1];
  }
  sInstruments[_instrument].program[end].loop.cmd = Cmd4Bit_Loop;
  sInstruments[_instrument].program[end].loop.loopPosition = 0;
  return NO_ERR;
}

static ChipError insertTableColumn(u8 _tableKind, u8 _table, u8 _atColumn) { return NO_ERR; }

static ChipError addTableColumn(u8 _tableKind, u8 _table) { return NO_ERR; }

static ChipError deleteTableColumn(u8 _tableKind, u8 _table, u8 _atColumn) { return NO_ERR; }

static ChipMetaDataEntry metaData[] =
    {
        {
            .name = "Ch.0 Octave",
            .type = CMDT_OPTIONS,
            .min = 0,
            .max = 3,
            .value = 1,
            .options = {"Disabled", "Bass", "Alto", "Treble"},
            .stringValue = {},
            .textEdit = NULL,
        },
        {
            .name = "Ch.1 Octave",
            .type = CMDT_OPTIONS,
            .min = 0,
            .max = 3,
            .value = 2,
            .options = {"Disabled", "Bass", "Alto", "Treble"},
            .stringValue = {},
            .textEdit = NULL,
        },
        {
            .name = "Ch.2 Octave",
            .type = CMDT_OPTIONS,
            .min = 0,
            .max = 3,
            .value = 3,
            .options = {"Disabled", "Bass", "Alto", "Treble"},
            .stringValue = {},
            .textEdit = NULL,
        },
        {
            .name = "Meter",
            .type = CMDT_OPTIONS,
            .min = 0,
            .max = 1,
            .value = 0,
            .options = {"4/4", "3/4"},
            .stringValue = {},
            .textEdit = NULL,
        },
        {
            .name = "Loop",
            .type = CMDT_OPTIONS,
            .min = 0,
            .max = 1,
            .value = 0,
            .options = {"Stop", "Loop"},
            .stringValue = {},
            .textEdit = NULL,
        },
    };

static u8 getNumMetaData() {
  return sizeof(metaData) / sizeof(ChipMetaDataEntry);
}

static ChipMetaDataEntry *getMetaData(u8 _index) {
  return &metaData[_index];
}

static ChipError setMetaData(u8 _index, ChipMetaDataEntry *entry) {
  switch (_index) {
    case 0:sSong.ch0Octave = entry->value;
      break;
    case 1:sSong.ch1Octave = entry->value;
      break;
    case 2:sSong.ch2Octave = entry->value;
      break;
    case 3:sSong.meter = entry->value;
      break;
    case 4:sSong.loopSong = entry->value;
      break;
  }
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
  return 20;
}

static u8 getNumSongDataColumns(u8 _channelNum) {
  return 5;
}

static ChipDataType getSongDataType(u8 _songRow, u8 _channelNum, u8 _songDataColumn) {
  switch (_songDataColumn) {
    case 0:
    case 2:
    case 4: return CDT_HEX;
    default: return CDT_LABEL;
  }
}

static const char *getSongHelp(u8 _songRow, u8 _channelNum, u8 _songDataColumn) {
  switch (_songDataColumn) {
    case 0: return "PATTERN";
    case 2: return "SPEED:0=1/1,1=1/2,2=1/4,3=1/8";
    case 4: return "OPTIONS:0=ONCE,1=TWICE,2=SILENCE,3=END/LOOP";
    default: return "";
  }
}

static u8 getSongData(u8 _songRow, u8 _channelNum, u8 _songDataColumn) {
  switch (_songDataColumn) {
    case 0:return GETLO(sSong.tracks[_channelNum][_songRow].pattern);
    case 2:return GETLO(sSong.tracks[_channelNum][_songRow].speed);
    case 4:return GETLO(sSong.tracks[_channelNum][_songRow].options);
    default:return ':';
  }
  return 0;
}

static u8 clearSongData(u8 _songRow, u8 _channelNum, u8 _songDataColumn) {
  switch (_songDataColumn) {
    case 0:return SETLO(sSong.tracks[_channelNum][_songRow].pattern, 0);
    case 2:return SETLO(sSong.tracks[_channelNum][_songRow].speed, 0);
    case 4:return SETLO(sSong.tracks[_channelNum][_songRow].options, 0);
  }
  return 0;
}

static u8 setSongData(u8 _songRow, u8 _channelNum, u8 _songDataColumn, u8 _data) {
  switch (_songDataColumn) {
    case 0:return SETLO(sSong.tracks[_channelNum][_songRow].pattern, _data);
    case 2:return SETLO(sSong.tracks[_channelNum][_songRow].speed, _data);
    case 4:return SETLO(sSong.tracks[_channelNum][_songRow].options, _data);
  }
  return _data;
}

static void setSongPattern(u8 _songRow, u8 _channelNum, u8 _pattern) {
  SETLO(sSong.tracks[_channelNum][_songRow].pattern, _pattern);
}

static u8 getNumChannels() {
  return 3;
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
  return sSong.meter == SongMeter_4_4 ? 12 : 16;
}

static u8 getPatternNum(u8 _songRow, u8 _channelNum) {
  return sSong.tracks[_channelNum][_songRow].pattern;
}

static u8 getPatternLen(u8 _patternNum) {
  return sSong.meter == SongMeter_4_4 ? 16 : 12;
}

static u8 getNumPatternDataColumns(u8 _channelNum, u8 _patternNum, u8 _patternRow) {
  return 3;
}

static ChipDataType getPatternDataType(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn) {
  switch (_patternColumn) {
    case 0: // Note
      return CDT_NOTE;

    case 2: // Instrument
      return CDT_HEX;

    default:return CDT_LABEL;
  }
}

static u8 getPatternData(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn) {
  SongLine *line;
  if (sSong.meter == SongMeter_4_4) {
    line = &sSong.pattern44[_patternNum][_patternRow];
  } else {
    line = &sSong.pattern34[_patternNum][_patternRow];
  }
  switch (_patternColumn) {
    case 0: return line->note;
    case 2: return line->instrument;
    default:return ' ';
  } /* switch */
}   /* getPatternData */

static const char *getPatternHelp(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn) {
  switch (_patternColumn) {
    case 0: return "NOTE";
    case 2: return "INSTRUMENT";
    default:return "";
  }
}   /* getPatternHelp */

static u8 clearPatternData(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn) {
  SongLine *line;
  if (sSong.meter == SongMeter_4_4) {
    line = &sSong.pattern44[_patternNum][_patternRow];
  } else {
    line = &sSong.pattern34[_patternNum][_patternRow];
  }
  switch (_patternColumn) {
    case 0: {
      // Note
      line->note = 0;
      line->instrument = 0;
      break;
    }
    case 2: {
      // Instrument
      line->instrument = 0;
      break;
    }
    default:break;
  } /* switch */
  return 0;
}

static u8 setPatternData(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn, u8 _instrument,
                         u8 _data) {
  SongLine *line;
  if (sSong.meter == SongMeter_4_4) {
    line = &sSong.pattern44[_patternNum][_patternRow];
  } else {
    line = &sSong.pattern34[_patternNum][_patternRow];
  }
  switch (_patternColumn) {
    case 0: {
      // Note
      line->note = _data & 0x1f;
      line->instrument = _instrument & 0x7;
      break;
    }
    case 2: {
      // Instrument
      line->instrument = _data & 0x7;
      break;
    }
    default:break;
  } /* switch */
  return 0;
}

static u8 getMinOctave() {
  return 0;
}

static u8 getMaxOctave() {
  return 2;
}

static const char *getInstrumentName(u8 _instrument, u8 _stringWidth) {
  static char buf[256] = {0};
  if (_stringWidth == 0) {
    _stringWidth = 3;
  }
  snprintf(buf, _stringWidth, "I%d", _instrument);
  buf[2] = 0;
  return (const char *) buf;
}

static void setInstrumentName(u8 _instrument, char *_instrName) {}

static u8 instrumentNameLength(u8 _instrument) {
  return 2;
}

static u16 getNumInstruments() {
  return 8;
}

static u8 getInstrumentLen(u8 _instrument) {
  return 34;
}

static u8 getNumInstrumentParams(u8 _instrument) {
  return 2;
}

static const char *getInstrumentParamName(u8 _instrument, u8 _instrumentParam, u8 _stringWidth) {
  switch (_instrumentParam) {
    case 0:return "Cmd";
    case 1:return "Info";
    default:return " ";
  }
}

static u8 getNumInstrumentData(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow) {
  switch (_instrumentParam) {
    case 0: {
      if (_instrumentRow < 4) {
        return 1;
      } else {
        return 3;
      }
    }
    case 1: {
      return 8;
    }
    default:return 0;
  }
}

static ChipDataType
getInstrumentDataType(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn) {
  if (_instrumentParam == 0) {
    if (_instrumentRow < 4) {
      return CDT_HEX;
    } else if (_instrumentColumn == 0) {
      return CDT_ASCII;
    } else {
      return CDT_HEX;
    }
  } else {
    return CDT_LABEL;
  }
}

static const char *getInstrumentLabel(u8 _instrument, u8 _instrumentRow) {
  static char buf[3];
  switch (_instrumentRow) {
    case 0:return " A";
    case 1:return " D";
    case 2:return " S";
    case 3:return " R";
    default:snprintf(buf, 3, "%02X", _instrumentRow - 4);
      return buf;
  }
}

static u8 getInstrumentData(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn) {
  switch (_instrumentParam) {
    case 0:
      switch (_instrumentRow) {
        case 0: return sInstruments[_instrument].attack;
        case 1: return sInstruments[_instrument].decay;
        case 2: return sInstruments[_instrument].sustain;
        case 3: return sInstruments[_instrument].release;
        default: {
          if (_instrumentRow > 33) return ' ';
          InstrumentInstruction *instr = &sInstruments[_instrument].program[_instrumentRow - 4];
          Cmd6Bit cmd6Bit = (instr->raw >> 6) & 0x3;
          switch (cmd6Bit) {
            case Cmd6Bit_Control: {
              if (_instrumentColumn == 0) return 'C';
              if (_instrumentColumn == 1) return GETHI(instr->raw & 0x3f);
              if (_instrumentColumn == 2) return GETLO(instr->raw & 0x3f);
            }
            case Cmd6Bit_Inc: {
              if (_instrumentColumn == 0) return 'I';
              if (_instrumentColumn == 1) return GETHI(instr->raw & 0x3f);
              if (_instrumentColumn == 2) return GETLO(instr->raw & 0x3f);
            }
            case Cmd6Bit_Set: {
              if (_instrumentColumn == 0) return 'S';
              if (_instrumentColumn == 1) return GETHI(instr->raw & 0x3f);
              if (_instrumentColumn == 2) return GETLO(instr->raw & 0x3f);
            }
            case Cmd6Bit_Extra: {
              Cmd4Bit cmd4Bit = instr->raw >> 4;
              switch (cmd4Bit) {
                case Cmd4Bit_Arpeggio: {
                  if (_instrumentColumn == 0) return 'A';
                  if (_instrumentColumn == 1) return GETLO(instr->raw & 0xf);
                  if (_instrumentColumn == 2) return ' ';
                }
                case Cmd4Bit_Loop: {
                  if (_instrumentColumn == 0) return 'L';
                  if (_instrumentColumn == 1) return GETLO(instr->raw & 0xf);
                  if (_instrumentColumn == 2) return ' ';
                }
                case Cmd4Bit_3Bit: {
                  Cmd3Bit cmd3Bit = instr->raw >> 3;
                  switch (cmd3Bit) {
                    case Cmd3Bit_Wait: {
                      if (_instrumentColumn == 0) return 'W';
                      if (_instrumentColumn == 1) return GETLO(instr->raw & 0x7);
                      if (_instrumentColumn == 2) return ' ';
                    }
                    case Cmd3Bit_Vibrato: {
                      if (_instrumentColumn == 0) return 'V';
                      if (_instrumentColumn == 1) return GETLO(instr->raw & 0x7);
                      if (_instrumentColumn == 2) return ' ';
                    }
                  }
                }
                case Cmd4Bit_1Bit: {
                  Cmd1Bit cmd1Bit = instr->raw >> 1;
                  switch (cmd1Bit) {
                    case Cmd1Bit_Sync: {
                      if (_instrumentColumn == 0) return 'Y';
                      if (_instrumentColumn == 1) return GETLO(instr->raw & 0x1);
                      if (_instrumentColumn == 2) return ' ';
                    }
                    case Cmd1Bit_Ring: {
                      if (_instrumentColumn == 0) return 'R';
                      if (_instrumentColumn == 1) return GETLO(instr->raw & 0x1);
                      if (_instrumentColumn == 2) return ' ';
                    }
                    case Cmd1Bit_ExternalVoiceFlag: {
                      if (_instrumentColumn == 0) return 'X';
                      if (_instrumentColumn == 1) return GETLO(instr->raw & 0x1);
                      if (_instrumentColumn == 2) return ' ';
                    }
                    case Cmd1Bit_NoteOffJmpPosition: {
                      if (_instrumentColumn == 0) return 'J';
                    }
                    default: return ' ';
                  }
                }
              }
            }
          }
          return ' ';
        }
      }
    case 1: { // Return Command info
      switch (_instrumentRow) {
        case 0:return labelString("Attack", _instrumentColumn);
        case 1:return labelString("Decay", _instrumentColumn);
        case 2:return labelString("Sustain", _instrumentColumn);
        case 3:return labelString("Release", _instrumentColumn);
        default: {
          if (_instrumentRow > 33) return ' ';
          InstrumentInstruction *instr = &sInstruments[_instrument].program[_instrumentRow - 4];
          Cmd6Bit cmd6Bit = (instr->raw >> 6) & 0x3;
          switch (cmd6Bit) {
            case Cmd6Bit_Control: {
              switch (_instrumentColumn) {
                case 0: return 'C';
                case 1: return ':';
                case 2: return instr->control.controlGate ? 'G' : '.';
                case 3: return instr->control.controlNoise ? 'N' : '.';
                case 4: return instr->control.controlPulse ? 'P' : '.';
                case 5: return instr->control.controlSawtooth ? 'S' : '.';
                case 6: return instr->control.controlTriangle ? 'T' : '.';
                case 7: return instr->control.controlReset ? 'R' : '.';
                default: return ' ';
              }
              return labelString("Control", _instrumentColumn);
            }
            case Cmd6Bit_Inc: {
              if (_instrumentColumn < 3) return labelString("Inc", _instrumentColumn);
              if (_instrumentColumn < 5)
                return labelString(
                    instr->inc.incMode ? "FQ" : "PW",
                    _instrumentColumn - 3
                );
              if (_instrumentColumn == 5) return instr->inc.incAmount < 0 ? '-' : '+';
              if (_instrumentColumn == 6) return HEXCHAR(abs(instr->inc.incAmount & 0x1F) / 10);
              if (_instrumentColumn == 7) return HEXCHAR(abs(instr->inc.incAmount & 0x1F) % 10);
            }
            case Cmd6Bit_Set: {
              if (_instrumentColumn < 3) return labelString("Set", _instrumentColumn);
              if (_instrumentColumn < 6)
                return labelString(
                    instr->inc.incMode ? "FQ" : "PW",
                    _instrumentColumn - 3
                );
              if (_instrumentColumn == 6) return HEXCHAR(GETHI(instr->inc.incAmount & 0x1F));
              if (_instrumentColumn == 7) return HEXCHAR(GETLO(instr->inc.incAmount & 0x1F));
            }
            case Cmd6Bit_Extra: {
              Cmd4Bit cmd4Bit = instr->raw >> 4;
              switch (cmd4Bit) {
                case Cmd4Bit_Arpeggio: return labelString("Arpeggio", _instrumentColumn);
                case Cmd4Bit_Loop: {
                  if (instr->loop.loopPosition == 0) {
                    return labelString("End", _instrumentColumn);
                  } else {
                    if (_instrumentColumn < 5) return labelString("Loop:", _instrumentColumn);
                    int row = (_instrumentRow - 4) - instr->loop.loopPosition;
                    if (row < 0) return labelString("ERR", _instrumentColumn - 5);
                    if (_instrumentColumn == 6) return HEXCHAR(GETHI(row));
                    if (_instrumentColumn == 7) return HEXCHAR(GETLO(row));
                  }
                }
                case Cmd4Bit_3Bit: {
                  Cmd3Bit cmd3Bit = instr->raw >> 3;
                  switch (cmd3Bit) {
                    case Cmd3Bit_Wait: return labelString("Wait", _instrumentColumn);
                    case Cmd3Bit_Vibrato: return labelString("Vibrato", _instrumentColumn);
                  }
                }
                case Cmd4Bit_1Bit: {
                  Cmd1Bit cmd1Bit = instr->raw >> 1;
                  switch (cmd1Bit) {
                    case Cmd1Bit_Sync:
                      return _instrumentColumn < 5
                             ? labelString("Sync:", _instrumentColumn)
                             : labelString(instr->sync.syncEnable ? "On" : "Off",
                                           _instrumentColumn - 5);
                    case Cmd1Bit_Ring:
                      return _instrumentColumn < 5
                             ? labelString("Ring:", _instrumentColumn)
                             : labelString(instr->ring.ringEnable ? "On" : "Off",
                                           _instrumentColumn - 5);
                    case Cmd1Bit_ExternalVoiceFlag:
                      return _instrumentColumn < 5
                             ? labelString("ExtV:", _instrumentColumn)
                             : labelString(
                              instr->externalVoiceFlag.externalVoiceFlag ? "On" : "Off",
                              _instrumentColumn - 5);
                    case Cmd1Bit_NoteOffJmpPosition:return labelString("NoteOff", _instrumentColumn);
                    default: return ' ';
                  }
                }
              }
            }
          }
        }
      }
    }
    default: return 0;
  }
  return 0;
}

static const char *getInstrumentHelp(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn) {
  return "Ctrl Inc Set Arp Loop Wait Vbr sYn Ring eXt (J)noff";
}

static u8 clearInstrumentData(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn) {
  switch (_instrumentRow) {
    case 0: {
      sInstruments[_instrument].attack = 0;
      break;
    }
    case 1: {
      sInstruments[_instrument].decay = 0;
      break;
    }
    case 2: {
      sInstruments[_instrument].sustain = 0;
      break;
    }
    case 3: {
      sInstruments[_instrument].release = 0;
      break;
    }
    default: {
      sInstruments[_instrument].program[_instrumentRow - 4].loop.loopPosition = 0;
    }
  }
  return 0;
}

static bool setInstrumentData(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn,
                              u8 _data) {
  switch (_instrumentRow) {
    case 0:sInstruments[_instrument].attack = _data;
      break;
    case 1:sInstruments[_instrument].decay = _data;
      break;
    case 2:sInstruments[_instrument].sustain = _data;
      break;
    case 3:sInstruments[_instrument].release = _data;
      break;
    default: {
      if (_instrumentRow > 33) return false;
      if (_instrumentParam == 0) {
        InstrumentInstruction *instr = &sInstruments[_instrument].program[_instrumentRow - 4];
        if (_instrumentColumn == 0) {
          switch (_data) {
            case 'c': {
              instr->control.cmd = Cmd6Bit_Control;
              return true;
            }
            case 'i': {
              instr->inc.cmd = Cmd6Bit_Inc;
              return true;
            }
            case 's': {
              instr->set.cmd = Cmd6Bit_Set;
              return true;
            }
            case 'a': {
              instr->arpeggio.cmd = Cmd4Bit_Arpeggio;
              return true;
            }
            case 'l': {
              instr->loop.cmd = Cmd4Bit_Loop;
              return true;
            }
            case 'w': {
              instr->wait.cmd = Cmd3Bit_Wait;
              return true;
            }
            case 'v': {
              instr->vibrato.cmd = Cmd3Bit_Vibrato;
              return true;
            }
            case 'y': {
              instr->sync.cmd = Cmd1Bit_Sync;
              return true;
            }
            case 'r': {
              instr->ring.cmd = Cmd1Bit_Ring;
              return true;
            }
            case 'x': {
              instr->externalVoiceFlag.cmd = Cmd1Bit_ExternalVoiceFlag;
              return true;
            }
            case 'j': {
              instr->noteOffJmpPosition.cmd = Cmd1Bit_NoteOffJmpPosition;
              instr->noteOffJmpPosition.noteOffJmpPosition = 1;
              return true;
            }
            default: {
              return false;
            }
          }
        }
        Cmd6Bit cmd6Bit = (instr->raw >> 6) & 0x3;
        switch (cmd6Bit) {
          case Cmd6Bit_Control:
          case Cmd6Bit_Inc:
          case Cmd6Bit_Set: {
            switch (_instrumentColumn) {
              case 1: {
                instr->raw = (instr->raw & ~0x30) | (_data << 4 & 0x30);
                return true;
              }
              case 2: {
                instr->raw = (instr->raw & ~0x0F) | (_data & 0x0F);
                return true;
              }
              default: return false;
            }
          }
          case Cmd6Bit_Extra: {
            Cmd4Bit cmd4Bit = instr->raw >> 4;
            switch (cmd4Bit) {
              case Cmd4Bit_Arpeggio: {
                switch (_instrumentColumn) {
                  case 1: {
                    instr->raw = (instr->raw & ~0x0F) | (_data & 0x0F);
                    return true;
                  }
                  default: return false;
                }
              }
              case Cmd4Bit_Loop: {
                switch (_instrumentColumn) {
                  case 1: {
                    instr->raw = (instr->raw & ~0x0F) | (_data & 0x0F);
                    return true;
                  }
                  default: return false;
                }
              }
              case Cmd4Bit_3Bit: {
                Cmd3Bit cmd3Bit = instr->raw >> 3;
                switch (cmd3Bit) {
                  case Cmd3Bit_Wait: {
                    switch (_instrumentColumn) {
                      case 1: {
                        instr->raw = (instr->raw & ~0x07) | (_data & 0x07);
                        return true;
                      }
                      default: return false;
                    }
                  }
                  case Cmd3Bit_Vibrato: {
                    switch (_instrumentColumn) {
                      case 1: {
                        instr->raw = (instr->raw & ~0x07) | (_data & 0x07);
                        return true;
                      }
                      default: return false;
                    }
                  }
                }
              }
              case Cmd4Bit_1Bit: {
                Cmd1Bit cmd1Bit = instr->raw >> 1;
                switch (cmd1Bit) {
                  case Cmd1Bit_Sync:
                  case Cmd1Bit_Ring:
                  case Cmd1Bit_ExternalVoiceFlag: {
                    switch (_instrumentColumn) {
                      case 1: {
                        instr->raw = (instr->raw & ~0x01) | (_data & 0x01);
                        return true;
                      }
                      default: return false;
                    }
                  }
                  default: return false;
                }
              }
            }
          }
        }
      }
      break;
    }
  }
  return true;
}

static void swapInstrumentRow(u8 _instrument, u8 _instrumentRow1, u8 _instrumentRow2) {
  if (_instrumentRow1 < 4 || _instrumentRow2 < 4) return;
  if (_instrumentRow1 > 31 || _instrumentRow2 > 31) return;
  if (_instrumentRow1 == _instrumentRow2) return;
  InstrumentInstruction temp = sInstruments[_instrument].program[_instrumentRow1 - 4];
  sInstruments[_instrument].program[_instrumentRow1 - 4] = sInstruments[_instrument].program[_instrumentRow2 - 4];
  sInstruments[_instrument].program[_instrumentRow2 - 4] = temp;
}

static bool useTables() { return false; }

static u8 getNumTableKinds() { return 0; }

static const char *getTableKindName(u8 _tableKind) { return ""; }

static u16 getMinTable(u8 _tableKind) { return 0; }

static u16 getNumTables(u8 _tableKind) { return 0; }

static ChipTableStyle getTableStyle(u8 _tableKind) { return CTS_BOTTOM; }

static u8 getTableDataLen(u8 _tableKind, u16 _table) { return 0; }

static u8 getTableData(u8 _tableKind, u16 _table, u8 _tableColumn) { return 0; }

static u8 setTableData(u8 _tableKind, u16 _table, u8 _tableColumn, u8 _data) { return 0; }

static u8 getPlayerSongRow() {
  return 0; // TODO: Implement
}

static u8 getPlayerPatternRow() {
  return 0; // TODO: Implement
}

static void plonk(u8 _note, u8 _channelNum, u8 _instrument, bool _isDown) {
  playerPlayNote(_channelNum, _note, _instrument, _isDown);
}

static void playSongFrom(u8 _songRow, u8 _songColumn, u8 _patternRow, u8 _patternColumn) {
  // TODO: Implement
}

static void playPatternFrom(u8 _songRow, u8 _songColumn, u8 _patternRow, u8 _patternColumn) {
  // TODO: Implement
}

static bool isPlaying() {
  return sIsPlaying;
}

static void silence() {
  playerStop();
}

static void stop() {
  silence();
}

static void getSamples(ChipSample *_buf, int _len) {
  sidTick(_buf, _len);
}

static void preferredWindowSize(u32 *_width, u32 *_height) {
  *_width = 750;
  *_height = 632;
}

ChipInterface chip_bv = {
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
    plonk,
    playSongFrom,
    playPatternFrom,
    isPlaying,
    stop,
    silence,
    getSamples,

    // GUI Options
    preferredWindowSize
};
