
#include "../chip.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define TRACKLEN 32

enum {
  WF_TRI,
  WF_SAW,
  WF_PUL,
  WF_NOI
};

struct trackline {
  u8 note;
  u8 instr;
  u8 cmd[2];
  u8 param[2];
};

struct track {
  struct trackline line[TRACKLEN];
};

struct instrline {
  u8 cmd;
  u8 param;
};

struct instrument {
  int length;
  struct instrline line[256];
};

struct songline {
  u8 track[4];
  u8 transp[4];
};

static struct instrument instrument[256];
static struct track track[256];
static struct songline song[256];
static char sFilename[1024];
static int songlen = 1;

static volatile u16 callbackwait;
static u16 noiseseedwait = 0;

static u8 trackwait;
static u8 trackpos;
static u8 songpos;

static u8 playsong;
static u8 playtrack;

/*static const u16 freqtable[] = {
  0x010b, 0x011b, 0x012c, 0x013e, 0x0151, 0x0165, 0x017a, 0x0191, 0x01a9,
  0x01c2, 0x01dd, 0x01f9, 0x0217, 0x0237, 0x0259, 0x027d, 0x02a3, 0x02cb,
  0x02f5, 0x0322, 0x0352, 0x0385, 0x03ba, 0x03f3, 0x042f, 0x046f, 0x04b2,
  0x04fa, 0x0546, 0x0596, 0x05eb, 0x0645, 0x06a5, 0x070a, 0x0775, 0x07e6,
  0x085f, 0x08de, 0x0965, 0x09f4, 0x0a8c, 0x0b2c, 0x0bd6, 0x0c8b, 0x0d4a,
  0x0e14, 0x0eea, 0x0fcd, 0x10be, 0x11bd, 0x12cb, 0x13e9, 0x1518, 0x1659,
  0x17ad, 0x1916, 0x1a94, 0x1c28, 0x1dd5, 0x1f9b, 0x217c, 0x237a, 0x2596,
  0x27d3, 0x2a31, 0x2cb3, 0x2f5b, 0x322c, 0x3528, 0x3851, 0x3bab, 0x3f37,
  0x42f9, 0x46f5, 0x4b2d, 0x4fa6, 0x5462, 0x5967, 0x5eb7, 0x6459, 0x6a51,
  0x70a3, 0x7756, 0x7e6f
};*/

static const u16 freqtable[] = {
    0x0085, 0x008d, 0x0096, 0x009f, 0x00a8, 0x00b2, 0x00bd, 0x00c8, 0x00d4, 0x00e1, 0x00ee, 0x00fc, 0x010b,
    0x011b, 0x012c, 0x013e, 0x0151, 0x0165, 0x017a, 0x0191, 0x01a9, 0x01c2, 0x01dd, 0x01f9, 0x0217, 0x0237,
    0x0259, 0x027d, 0x02a3, 0x02cb, 0x02f5, 0x0322, 0x0352, 0x0385, 0x03ba, 0x03f3, 0x042f, 0x046f, 0x04b2,
    0x04fa, 0x0546, 0x0596, 0x05eb, 0x0645, 0x06a5, 0x070a, 0x0775, 0x07e6, 0x085f, 0x08de, 0x0965, 0x09f4,
    0x0a8c, 0x0b2c, 0x0bd6, 0x0c8b, 0x0d4a, 0x0e14, 0x0eea, 0x0fcd, 0x10be, 0x11bd, 0x12cb, 0x13e9, 0x1518,
    0x1659, 0x17ad, 0x1916, 0x1a94, 0x1c28, 0x1dd5, 0x1f9b, 0x217c, 0x237a, 0x2596, 0x27d3, 0x2a31, 0x2cb3,
    0x2f5b, 0x322c, 0x3528, 0x3851, 0x3bab, 0x3f37
};

static const s8 sinetable[] = {
    0, 12, 25, 37, 49, 60, 71, 81, 90, 98, 106, 112, 117, 122, 125, 126, 127, 126, 125, 122, 117, 112, 106, 98,
    90, 81, 71, 60, 49, 37, 25, 12, 0, -12, -25, -37, -49, -60, -71, -81, -90, -98, -106, -112, -117, -122,
    -125, -126, -127, -126, -125, -122, -117, -112, -106, -98, -90, -81, -71, -60, -49, -37, -25, -12
};

static volatile struct oscillator {
  s32 freq;
  u16 phase;
  u16 duty;
  u8 waveform;
  u8 volume; // 0-255
} osc[4];

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
  s16 dutyd;
  u8 vdepth;
  u8 vrate;
  u8 vpos;
  s16 inertia;
  u16 slur;
} channel[4];

static void readsong(int pos, int ch, u8 *dest) {
  dest[0] = song[pos].track[ch];
  dest[1] = song[pos].transp[ch];
}

static void readtrack(int num, int pos, struct trackline *tl) {
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
  }
  playsong = 0;
  playtrack = 0;
}

static void runcmd(u8 ch, u8 cmd, u8 param) {
  switch (cmd) {
    case 0:channel[ch].inum = 0;
      break;
    case 'd':osc[ch].duty = param << 8;
      break;
    case 'f':channel[ch].volumed = param;
      break;
    case 'i':channel[ch].inertia = param << 1;
      break;
    case 'j':channel[ch].iptr = param;
      break;
    case 'l':channel[ch].bendd = param;
      break;
    case 'm':channel[ch].dutyd = param << 6;
      break;
    case 't':channel[ch].iwait = param;
      break;
    case 'v':osc[ch].volume = param;
      break;
    case 'w':osc[ch].waveform = param;
      break;
    case '+':channel[ch].inote = param + channel[ch].tnote - 12 * 4;
      break;
    case '=':channel[ch].inote = param;
      break;
    case '~':
      if (channel[ch].vdepth != (param >> 4)) {
        channel[ch].vpos = 0;
      }
      channel[ch].vdepth = param >> 4;
      channel[ch].vrate = param & 15;
      break;
  } /* switch */
}   /* runcmd */

/*
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
*/

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
      trackwait = 4;
      if (!trackpos) {
        if (playsong) {
          if (songpos >= songlen) {
            playsong = 0;
          } else {
            for (ch = 0; ch < 4; ch++) {
              u8 tmp[2];

              readsong(songpos, ch, tmp);
              channel[ch].tnum = tmp[0];
              channel[ch].transp = tmp[1];
            }
            songpos++;
          }
        }
      }
      if (playtrack || playsong) {
        for (ch = 0; ch < 4; ch++) {
          if (channel[ch].tnum) {
            struct trackline tl;
            u8 instr = 0;

            readtrack(channel[ch].tnum, trackpos, &tl);
            if (tl.note) {
              channel[ch].tnote = tl.note + channel[ch].transp;
              instr = channel[ch].lastinstr;
            }
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
              channel[ch].dutyd = 0;
              channel[ch].vdepth = 0;
            }
            if (tl.cmd[0]) {
              runcmd(ch, tl.cmd[0], tl.param[0]);
            }
            /*if(tl.cmd[1])
              runcmd(ch, tl.cmd[1], tl.param[1]);*/
          }
        }
        trackpos++;
        trackpos &= 31;
      }
    }
  }
  for (ch = 0; ch < 4; ch++) {
    s16 vol;
    u16 duty;
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
      diff = freqtable[channel[ch].inote] - slur;
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
      slur = freqtable[channel[ch].inote];
    }
    osc[ch].freq = slur + channel[ch].bend + ((channel[ch].vdepth * sinetable[channel[ch].vpos & 63]) >> 2);
    channel[ch].bend += channel[ch].bendd;
    vol = osc[ch].volume + channel[ch].volumed;
    if (vol < 0) {
      vol = 0;
    }
    if (vol > 255) {
      vol = 255;
    }
    osc[ch].volume = vol;

    duty = osc[ch].duty + channel[ch].dutyd;
    if (duty > 0xe000) {
      duty = 0x2000;
    }
    if (duty < 0x2000) {
      duty = 0xe000;
    }
    osc[ch].duty = duty;

    channel[ch].vpos += channel[ch].vrate;
  }
} /* playroutine */

static const char *getChipId() {
  return "LFT";
}

static ChipError init() {
  trackwait = 0;
  trackpos = 0;
  playsong = 0;
  playtrack = 0;

  osc[0].volume = 0;
  channel[0].inum = 0;
  osc[1].volume = 0;
  channel[1].inum = 0;
  osc[2].volume = 0;
  channel[2].inum = 0;
  osc[3].volume = 0;
  channel[3].inum = 0;
  for (int i = 1; i < 256; i++) {
    instrument[i].length = 1;
    instrument[i].line[0].cmd = '0';
    instrument[i].line[0].param = 0;
  }
  return NO_ERR;
}

static ChipError shutdown() {
  return NO_ERR;
}

static ChipError newSong() {
  return NO_ERR;
}

static ChipError loadSong(const char *_filename) {
  FILE *f;
  char buf[1024];
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
    if (9 == sscanf(buf, "songline %x %x %x %x %x %x %x %x %x", &i1, &trk[0], &transp[0], &trk[1], &transp[1],
                    &trk[2],
                    &transp[2], &trk[3], &transp[3])) {
      for (i = 0; i < 4; i++) {
        song[i1].track[i] = trk[i];
        song[i1].transp[i] = transp[i];
      }
      if (songlen <= i1) {
        songlen = i1 + 1;
      }
    } else if (8 ==
               sscanf(buf, "trackline %x %x %x %x %x %x %x %x", &i1, &i2, &note, &instr, &cmd[0], &param[0],
                      &cmd[1],
                      &param[1])) {
      track[i1].line[i2].note = note;
      track[i1].line[i2].instr = instr;
      for (i = 0; i < 2; i++) {
        track[i1].line[i2].cmd[i] = cmd[i];
        track[i1].line[i2].param[i] = param[i];
      }
    } else if (4 == sscanf(buf, "instrumentline %x %x %x %x", &i1, &i2, &cmd[0], &param[0])) {
      instrument[i1].line[i2].cmd = cmd[0];
      instrument[i1].line[i2].param = param[0];
      if (instrument[i1].length <= i2) {
        instrument[i1].length = i2 + 1;
      }
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
    fprintf(f, "songline %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, song[i].track[0],
            song[i].transp[0], song[i].track[1], song[i].transp[1], song[i].track[2], song[i].transp[2],
            song[i].track[3], song[i].transp[3]);
  }
  fprintf(f, "\n");
  for (i = 1; i < 256; i++) {
    for (j = 0; j < TRACKLEN; j++) {
      struct trackline *tl = &track[i].line[j];
      if (tl->note || tl->instr || tl->cmd[0] || tl->cmd[1]) {
        fprintf(f, "trackline %02x %02x %02x %02x %02x %02x %02x %02x\n", i, j, tl->note, tl->instr,
                tl->cmd[0], tl->param[0], tl->cmd[1], tl->param[1]);
      }
    }
  }
  fprintf(f, "\n");
  for (i = 1; i < 256; i++) {
    if (instrument[i].length > 1) {
      for (j = 0; j < instrument[i].length; j++) {
        fprintf(f, "instrumentline %02x %02x %02x %02x\n", i, j, instrument[i].line[j].cmd,
                instrument[i].line[j].param);
      }
    }
  }
  fclose(f);
  return NO_ERR;
}

static ChipError insertSongRow(u8 _channelNum, u8 _atSongRow) {
  if (songlen < 256) {
    memmove(&song[_atSongRow + 1],
            &song[_atSongRow + 0],
            sizeof(struct songline) * (songlen - _atSongRow));
    songlen++;
    memset(&song[_atSongRow], 0, sizeof(struct songline));
    return NO_ERR;
  } else {
    return "Song is full.";
  }
}

static ChipError addSongRow() {
  if (songlen < 256) {
    memset(&song[songlen], 0, sizeof(struct songline));
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
            sizeof(struct songline) * (songlen - _songRow - 1));
    songlen--;
  }
  return NO_ERR;
}

static ChipError insertPatternRow(u8 _channelNum, u8 _patternNum, u8 _atPatternRow) {
  memmove(&(track[_patternNum].line[_atPatternRow + 1]),
          &(track[_patternNum].line[_atPatternRow + 0]),
          sizeof(struct trackline) * (TRACKLEN - _atPatternRow));
  memset(&(track[_patternNum].line[_atPatternRow]), 0, sizeof(struct trackline));
  return NO_ERR;
}

static ChipError addPatternRow(u8 _channelNum, u8 _patternNum) {
  return NO_ERR;
}

static ChipError deletePatternRow(u8 _channelNum, u8 _patternNum, u8 _patternRow) {
  memmove(&(track[_patternNum].line[_patternRow + 0]),
          &(track[_patternNum].line[_patternRow + 1]),
          sizeof(struct trackline) * (TRACKLEN - _patternRow - 1));
  memset(&(track[_patternNum].line[TRACKLEN - 1]), 0, sizeof(struct trackline));
  return NO_ERR;
}

static ChipError insertInstrumentRow(u8 _instrument, u8 _atInstrumentRow) {
  struct instrument *in = &instrument[_instrument];
  if (in->length < 256) {
    memmove(&in->line[_atInstrumentRow + 1],
            &in->line[_atInstrumentRow + 0],
            sizeof(struct instrline) * (in->length - _atInstrumentRow));
    in->length++;
    in->line[_atInstrumentRow].cmd = '0';
    in->line[_atInstrumentRow].param = 0;
    return NO_ERR;
  } else {
    return "Instrument is full.";
  }
}

static ChipError addInstrumentRow(u8 _instrument) {
  struct instrument *in = &instrument[_instrument];
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
  struct instrument *in = &instrument[_instrument];
  if (in->length > 1) {
    memmove(&in->line[_instrumentRow + 0],
            &in->line[_instrumentRow + 1],
            sizeof(struct instrline) * (in->length - _instrumentRow - 1));
    in->length--;
  }
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

static u8 getPatternLen(u8 _patternNum) {
  return TRACKLEN;
}

static u8 getNumPatternDataColumns(u8 _channelNum, u8 _patternNum, u8 _patternRow) {
  return 12;
}

static ChipDataType getPatternDataType(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn) {
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
      return cmd;
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

      // Command 1
    case 9: {
      int cmd = track[_patternNum].line[_patternRow].cmd[1];
      if (cmd == 0) {
        return '.';
      }
      return cmd;
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
  switch (_patternColumn) {
    // Note
    case 0:track[_patternNum].line[_patternRow].instr = _instrument;
      return track[_patternNum].line[_patternRow].note = _data + 1;

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
}

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
  snprintf(buf, _stringWidth, "INSTR");
  buf[255] = 0;
  return (const char *) buf;
}

static void setInstrumentName(u8 _instrument, char *_instrName) {}

static u8 instrumentNameLength(u8 _instrument) {
  return 0;
}

static u16 getNumInstruments() {
  return 256;
}

static u8 getInstrumentLen(u8 _instrument) {
  return instrument[_instrument].length;
}

static u8 getNumInstrumentParams(u8 _instrument) {
  return 1;
}

static const char *getInstrumentParamName(u8 _instrument, u8 _instrumentParam, u8 _stringWidth) {
  static char buf[256];
  if (_stringWidth == 0) {
    _stringWidth = 255;
  }
  snprintf(buf, _stringWidth, "P%d", _instrumentParam);
  buf[255] = 0;
  return (const char *) buf;
}

static u8 getNumInstrumentData(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow) {
  u8 cmd = instrument[_instrument].line[_instrumentRow].cmd;
  if (cmd == '+' || cmd == '=') {
    return 2;
  }
  return 3;
}

static const char *getInstrumentLabel(u8 _instrument, u8 _instrumentRow) {
  static char buf[3];
  snprintf(buf, 3, "%02X", _instrumentRow);
  return buf;
}

static ChipDataType getInstrumentDataType(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow,
                                          u8 _instrumentColumn) {
  if (_instrumentColumn == 0) {
    return CDT_ASCII;
  }
  u8 cmd = instrument[_instrument].line[_instrumentRow].cmd;
  if (cmd == '+' || cmd == '=') {
    return CDT_NOTE;
  }
  return CDT_HEX;
}

static u8 getInstrumentData(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn) {
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
}

static const char *getInstrumentHelp(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn) {
  return ""; // TODO: Implement
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

static char *validcmds = "0dfijlmtvw~+=";

static bool setInstrumentData(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn,
                              u8 _data) {
  u8 cmd = instrument[_instrument].line[_instrumentRow].cmd;
  if (_instrumentColumn == 0) {
    u8 ascii = _data;
    if (ascii >= 'A' && ascii <= 'Z') {
      ascii = tolower(_data);
    }
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
      } else {
        SETLO(instrument[_instrument].line[_instrumentRow].param, _data);
      }
    }
  }
  return true;
}

static void swapInstrumentRow(u8 _instrument, u8 _instrumentRow1, u8 _instrumentRow2) {
  struct instrline temp = instrument[_instrument].line[_instrumentRow1];
  instrument[_instrument].line[_instrumentRow1] = instrument[_instrument].line[_instrumentRow2];
  instrument[_instrument].line[_instrumentRow2] = temp;
}

// Tables
static bool useTables() { return false; }

static u8 getNumTableKinds() { return 0; }

static const char *getTableKindName(u8 _tableKind) { return ""; }

static u16 getMinTable(u8 _tableKind) { return 0; }

static u16 getNumTables(u8 _tableKind) { return 0; }

static ChipTableStyle getTableStyle(u8 _tableKind) { return CTS_BOTTOM; }

static u8 getTableDataLen(u8 _tableKind, u16 _table) { return 0; }

static u8 getTableData(u8 _tableKind, u16 _table, u8 _tableColumn) { return 0; }

static u8 setTableData(u8 _tableKind, u16 _table, u8 _tableColumn, u8 _data) { return 0; }

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
  channel[_channelNum].tnote = _note + 1;
  channel[_channelNum].inum = _instrument;
  channel[_channelNum].iptr = 0;
  channel[_channelNum].iwait = 0;
  channel[_channelNum].bend = 0;
  channel[_channelNum].bendd = 0;
  channel[_channelNum].volumed = 0;
  channel[_channelNum].dutyd = 0;
  channel[_channelNum].vdepth = 0;
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

static ChipSample getSample() {
  u8 i;
  ChipSample acc;
  static u32 noiseseed = 1;
  u8 newbit;
  if (noiseseedwait) {
    noiseseedwait--;
  } else {
    newbit = 0;
    if (noiseseed & 0x80000000L) {
      newbit ^= 1;
    }
    if (noiseseed & 0x01000000L) {
      newbit ^= 1;
    }
    if (noiseseed & 0x00000040L) {
      newbit ^= 1;
    }
    if (noiseseed & 0x00000200L) {
      newbit ^= 1;
    }
    noiseseed = (noiseseed << 1) | newbit;
    noiseseedwait = 3;
  }
  if (callbackwait) {
    callbackwait--;
  } else {
    playroutine();
    callbackwait = 496 - 1;
  }
  acc.left = 0;
  acc.right = 0;
  for (i = 0; i < 4; i++) {
    s8 value; // [-32,31]
    switch (osc[i].waveform) {
      case WF_TRI:
        if (osc[i].phase < 0x8000) {
          value = -32 + (osc[i].phase >> 9);
        } else {
          value = 31 - ((osc[i].phase - 0x8000) >> 9);
        }
        break;
      case WF_SAW:value = -32 + (osc[i].phase >> 10);
        break;
      case WF_PUL:value = (osc[i].phase > osc[i].duty) ? -32 : 31;
        break;
      case WF_NOI:value = (noiseseed & 63) - 32;
        break;
      default:value = 0;
        break;
    }
    if (osc[i].freq < 0) {
      osc[i].freq = 0;
    }
    osc[i].phase += (osc[i].freq / 2.75625);
    if ((i & 2) == 0) {
      acc.left += value * osc[i].volume;  // rhs = [-8160,7905]
    } else {
      acc.right += value * osc[i].volume; // rhs = [-8160,7905]
    }
  }
  acc.left = (acc.left + acc.right * 0.8f) * 0.8f;
  acc.right = (acc.right + acc.left * 0.8f) * 0.8f;

  // acc [-32640,31620]
  // return 128 + (acc >> 8); // [1,251]
  return chip_expandSample(acc);
} /* getSample */

static void getSamples(ChipSample *_buf, int _len) {
  for (size_t i = 0; i < _len; i++) {
    _buf[i] = getSample();
  }
}

static const char *getSongHelp(u8 _songRow, u8 _channelNum, u8 _songDataColumn) {
  switch (_songDataColumn) {
    default: return "";
  }
}

static const char *getPatternHelp(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn) {
  switch (_patternColumn) {
    default:return "";
  }
}   /* getPatternHelp */


ChipInterface chip_lft = {
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

