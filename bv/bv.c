#include "../chip.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "../console.h"

// #include "../resid/sid.h"

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
    struct {
      unsigned controlTriangle: 1;
      unsigned controlSawtooth: 1;
      unsigned controlPulse: 1;
      unsigned controlNoise: 1;
      unsigned controlGate: 1;
      unsigned controlReset: 1;
      Cmd6Bit cmd: 2; // Cmd6Bit_Control
    } __attribute__((packed)) control;
    // Inc
    struct {
      int incAmount: 5;
      unsigned incMode: 1; // 0 = PulesWidth, 1 = Frequency
      Cmd6Bit cmd: 2; // Cmd6Bit_Inc
    } __attribute__((packed)) inc;
    // Set
    struct {
      unsigned setValue: 5;
      unsigned setMode: 1; // 0 = PulesWidth, 1 = Frequency
      Cmd6Bit cmd: 2; // Cmd6Bit_Set
    } __attribute__((packed)) set;
    // Extra
    struct {
      // Arpeggio = 0
      unsigned arpeggioNote: 4;
      Cmd4Bit cmd: 4; // Cmd4Bit_Arpeggio
    } __attribute__((packed)) arpeggio;
    struct {
      // Loop = 1
      unsigned loopPosition: 4;
      Cmd4Bit cmd: 4; // Cmd4Bit_Loop
    } __attribute__((packed)) loop;
    struct {
      // Wait
      unsigned waitFrames: 3;
      Cmd3Bit cmd: 5; // Cmd3Bit_Wait
    } __attribute__((packed)) wait;
    struct {
      // Vibrato
      unsigned vibratoDepth: 3;
      Cmd3Bit cmd: 5; // Cmd3Bit_Vibrato
    } __attribute__((packed)) vibrato;
    struct {
      // Sync
      unsigned syncEnable: 1;
      Cmd1Bit cmd: 7; // Cmd1Bit_Sync
    } __attribute__((packed)) sync;
    struct {
      // Ring
      unsigned ringEnable: 1;
      Cmd1Bit cmd: 7; // Cmd1Bit_Ring
    } __attribute__((packed)) ring;
    struct {
      // External Voice Flag
      unsigned externalVoiceFlag: 1;
      Cmd1Bit cmd: 7; // Cmd1Bit_ExternalVoiceFlag
    } __attribute__((packed)) externalVoiceFlag;
    struct {
      // Note off jmp position
      unsigned noteOffJmpPosition: 1;
      Cmd1Bit cmd: 7; // Cmd1Bit_NoteOffJmpPosition
    } __attribute__((packed)) noteOffJmpPosition;
    u8 raw;
  } __attribute__((packed));
} __attribute__((packed)) InstrumentInstruction;

typedef struct Instrument_t {
  unsigned attack: 4;
  unsigned decay: 4;
  unsigned sustain: 4;
  unsigned release: 4;
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

static Song song;
static Instrument instruments[8];

static const char *getChipId() {
  return "BV";
}

static ChipError newSong() {
  memset(&song, 0, sizeof(Song));

  song.instrumentSet = 0;
  song.ch0Octave = SongOctave_Bass;
  song.ch1Octave = SongOctave_Alto;
  song.ch2Octave = SongOctave_Treble;
  song.meter = SongMeter_4_4;
  song.loopSong = SongLoop_Stop;
  song.tempo = 2;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 20; j++) {
      song.tracks[i][j].pattern = 0;
      song.tracks[i][j].speed = SongTrackSpeed_Normal;
      song.tracks[i][j].options = SongTrackOptions_PlayOnce;
    }
  }
  memset(&song.pattern44, 0, sizeof(song.pattern44));

  // Init intstruments
  for (int i = 0; i < 8; i++) {
    instruments[i].attack = 0;
    instruments[i].decay = 0;
    instruments[i].sustain = 0;
    instruments[i].release = 0;
    instruments[i].program[0].raw = 0;
    instruments[i].program[0].control.cmd = Cmd6Bit_Control;
    instruments[i].program[0].control.controlPulse = 1;
    instruments[i].program[0].control.controlGate = 1;
    for (int j = 1; j < 30; j++) {
      instruments[i].program[j].loop.cmd = Cmd4Bit_Loop;
      instruments[i].program[j].loop.loopPosition = 0;
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
  newSong();

  con_msgf("Size of song: %d", sizeof(song));
  con_msgf("Size of instruments: %d", sizeof(instruments));
  con_msgf("Size of SongTrack: %d", sizeof(SongTrack));
  con_msgf("Size of Instrument: %d", sizeof(Instrument));

  return NO_ERR;
}

static ChipError shutdown() {
  return NO_ERR;
}

static ChipError loadSong(const char *_filename) {
  return NO_ERR;
}

static ChipError saveSong(const char *filename) {
  return NO_ERR;
}

static ChipError insertSongRow(u8 _atSongRow) {
  return NO_ERR;
}

static ChipError addSongRow() {
  return NO_ERR;
}

static ChipError deleteSongRow(u8 _songRow) {
  return NO_ERR;
}

static ChipError insertPatternRow(u8 _channelNum, u8 _patternNum, u8 _atPatternRow) {
  return NO_ERR;
}

static ChipError addPatternRow(u8 _channelNum, u8 _patternNum) {
  return NO_ERR;
}

static ChipError deletePatternRow(u8 _channelNum, u8 _patternNum, u8 _patternRow) {
  return NO_ERR;
}

static ChipError insertInstrumentRow(u8 _instrument, u8 _atInstrumentRow) {
  return NO_ERR;
}

static ChipError addInstrumentRow(u8 _instrument) {
  return NO_ERR;
}

static ChipError deleteInstrumentRow(u8 _instrument, u8 _instrumentRow) {
  return NO_ERR;
}

static ChipError insertTableColumn(u8 _tableKind, u8 _table, u8 _atColumn) { return NO_ERR; }

static ChipError addTableColumn(u8 _tableKind, u8 _table) { return NO_ERR; }

static ChipError deleteTableColumn(u8 _tableKind, u8 _table, u8 _atColumn) { return NO_ERR; }

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
    case 0:return GETLO(song.tracks[_channelNum][_songRow].pattern);
    case 2:return GETLO(song.tracks[_channelNum][_songRow].speed);
    case 4:return GETLO(song.tracks[_channelNum][_songRow].options);
    default:return ':';
  }
  return 0;
}

static u8 clearSongData(u8 _songRow, u8 _channelNum, u8 _songDataColumn) {
  switch (_songDataColumn) {
    case 0:return SETLO(song.tracks[_channelNum][_songRow].pattern, 0);
    case 2:return SETLO(song.tracks[_channelNum][_songRow].speed, 0);
    case 4:return SETLO(song.tracks[_channelNum][_songRow].options, 0);
  }
  return 0;
}

static u8 setSongData(u8 _songRow, u8 _channelNum, u8 _songDataColumn, u8 _data) {
  switch (_songDataColumn) {
    case 0:return SETLO(song.tracks[_channelNum][_songRow].pattern, _data);
    case 2:return SETLO(song.tracks[_channelNum][_songRow].speed, _data);
    case 4:return SETLO(song.tracks[_channelNum][_songRow].options, _data);
  }
  return _data;
}

static void setSongPattern(u8 _songRow, u8 _channelNum, u8 _pattern) {
  SETLO(song.tracks[_channelNum][_songRow].pattern, _pattern);
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
  return song.meter == SongMeter_4_4 ? 12 : 16;
}

static u8 getPatternNum(u8 _songRow, u8 _channelNum) {
  return song.tracks[_channelNum][_songRow].pattern;
}

static u8 getPatternLen() {
  return song.meter == SongMeter_4_4 ? 16 : 12;
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
  if (song.meter == SongMeter_4_4) {
    line = &song.pattern44[_patternNum][_patternRow];
  } else {
    line = &song.pattern34[_patternNum][_patternRow];
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
  if (song.meter == SongMeter_4_4) {
    line = &song.pattern44[_patternNum][_patternRow];
  } else {
    line = &song.pattern34[_patternNum][_patternRow];
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
  if (song.meter == SongMeter_4_4) {
    line = &song.pattern44[_patternNum][_patternRow];
  } else {
    line = &song.pattern34[_patternNum][_patternRow];
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
  return 34; // TODO: Implement
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
        case 0: return instruments[_instrument].attack;
        case 1: return instruments[_instrument].decay;
        case 2: return instruments[_instrument].sustain;
        case 3: return instruments[_instrument].release;
        default: {
          if (_instrumentRow > 33) return ' ';
          InstrumentInstruction *instr = &instruments[_instrument].program[_instrumentRow - 4];
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
          InstrumentInstruction *instr = &instruments[_instrument].program[_instrumentRow - 4];
          Cmd6Bit cmd6Bit = (instr->raw >> 6) & 0x3;
          switch (cmd6Bit) {
            case Cmd6Bit_Control: {
              switch (_instrumentColumn) {
                case 0: return 'C';
                case 1: return ':';
                case 2: return instr->control.controlReset ? 'R' : '.';
                case 3: return instr->control.controlGate ? 'G' : '.';
                case 4: return instr->control.controlNoise ? 'N' : '.';
                case 5: return instr->control.controlPulse ? 'P' : '.';
                case 6: return instr->control.controlSawtooth ? 'S' : '.';
                case 7: return instr->control.controlTriangle ? 'T' : '.';
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

  return 0; // TODO: Implement
}

static u8 clearInstrumentData(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn) {
  return 0; // TODO: Implement
}

static bool setInstrumentData(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn,
                              u8 _data) {
  switch (_instrumentRow) {
    case 0:instruments[_instrument].attack = _data;
      break;
    case 1:instruments[_instrument].decay = _data;
      break;
    case 2:instruments[_instrument].sustain = _data;
      break;
    case 3:instruments[_instrument].release = _data;
      break;
    default: {
      if (_instrumentRow > 33) return false;
      if (_instrumentParam == 0) {
        InstrumentInstruction *instr = &instruments[_instrument].program[_instrumentRow - 4];
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
  return true; // TODO: Implement
}

static void swapInstrumentRow(u8 _instrument, u8 _instrumentRow1, u8 _instrumentRow2) {
/*
    struct instrline temp = instrument[_instrument].line[_instrumentRow1];
    instrument[_instrument].line[_instrumentRow1] = instrument[_instrument].line[_instrumentRow2];
    instrument[_instrument].line[_instrumentRow2] = temp;
*/
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

static void plonk(u8 _note, u8 _channelNum, u8 _instrument) {
}

static void playSongFrom(u8 _songRow, u8 _songColumn, u8 _patternRow, u8 _patternColumn) {
}

static void playPatternFrom(u8 _songRow, u8 _songColumn, u8 _patternRow, u8 _patternColumn) {}

static bool isPlaying() {
  return false; // TODO: Implement
}

static void silence() {
}

static void stop() {
  silence();
}

static void getSamples(ChipSample *_buf, int _len) {
  memset(_buf, 0, _len * sizeof(ChipSample));
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
