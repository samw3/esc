
#ifndef CHIP_H
#define CHIP_H

#include "types.h"

struct TextEdit;
typedef struct TextEdit TextEdit;

#define SETLO(v, x) v = ((v) & 0xf0) | (x)
#define SETHI(v, x) v = ((v) & 0x0f) | ((x) << 4)
#define GETLO(v) ((v) & 0xf)
#define GETHI(v) (((v) >> 4) & 0xf)
#define GETBYTE(v, b) ((v >> (b * 8)) & 0xff)
#define HEXCHAR(x) ((x) < 10 ? '0' + (x) : 'A' + ((x) - 10))

typedef enum {
  CDT_LABEL, CDT_NOTE, CDT_HEX, CDT_ASCII
} ChipDataType;

typedef enum {
  CMDT_STRING, CMDT_HEX, CMDT_DECIMAL, CMDT_OPTIONS
} ChipMetaDataType;

typedef enum {
  CTS_BOTTOM, CTS_CENTER
} ChipTableStyle;

typedef struct {
  const char *name;
  ChipMetaDataType type;
  int min;
  int max;
  int value;
  const char *options[256];
  char stringValue[40];
  TextEdit *textEdit;
} ChipMetaDataEntry;

typedef struct {
  s16 left;
  s16 right;
} ChipSample;

typedef const char *ChipError;

#define NO_ERR (NULL)

typedef struct {
  // Tracker Commands
  const char *(*getChipId)();

  ChipError (*init)();

  ChipError (*shutdown)();

  ChipError (*newSong)();

  ChipError (*loadSong)(const char *filename);

  ChipError (*saveSong)(const char *filename);

  ChipError (*insertSongRow)(u8 _atSongRow);

  ChipError (*addSongRow)();

  ChipError (*deleteSongRow)(u8 _songRow);

  ChipError (*insertPatternRow)(u8 _channelNum, u8 _patternNum, u8 _atPatternRow);

  ChipError (*addPatternRow)(u8 _channelNum, u8 _patternNum);

  ChipError (*deletePatternRow)(u8 _channelNum, u8 _patternNum, u8 _patternRow);

  ChipError (*insertInstrumentRow)(u8 _instrument, u8 _atInstrumentRow);

  ChipError (*addInstrumentRow)(u8 _instrument);

  ChipError (*deleteInstrumentRow)(u8 _instrument, u8 _instrumentRow);

  ChipError (*insertTableColumn)(u8 _tableKind, u8 _table, u8 _atColumn);

  ChipError (*addTableColumn)(u8 _tableKind, u8 _table);

  ChipError (*deleteTableColumn)(u8 _tableKind, u8 _table, u8 _atColumn);

  // Metadata
  u8 (*getNumMetaData)();

  ChipMetaDataEntry *(*getMetaData)(u8 _index);

  ChipError (*setMetaData)(u8 _index, ChipMetaDataEntry *entry);

  // Save Options
  u8 (*getNumSaveOptions)();

  ChipMetaDataEntry *(*getSaveOptions)(u8 _index);

  ChipError (*setSaveOptions)(u8 _index, ChipMetaDataEntry *entry);

  // Song Data
  u16 (*getNumSongRows)();

  u8 (*getNumSongDataColumns)(u8 _channelNum);

  ChipDataType (*getSongDataType)(u8 _songRow, u8 _channelNum, u8 _songDataColumn);

  u8 (*getSongData)(u8 _songRow, u8 _channelNum, u8 _songDataColumn);

  u8 (*clearSongData)(u8 _songRow, u8 _channelNum, u8 _songDataColumn);

  u8 (*setSongData)(u8 _songRow, u8 _channelNum, u8 _songDataColumn, u8 _data);

  void (*setSongPattern)(u8 _songRow, u8 _channelNum, u8 _pattern);

  const char *(*getSongHelp)(u8 _songRow, u8 _channelNum, u8 _songDataColumn);

  // Channels
  u8 (*getNumChannels)();

  const char *(*getChannelName)(u8 _patternNum, u8 _stringWidth);

  // Patterns
  u16 (*getNumPatterns)();

  u8 (*getPatternNum)(u8 _songRow, u8 _channelNum);

  u8 (*getPatternLen)(u8 _patternNum);

  u8 (*getNumPatternDataColumns)(u8 _channelNum, u8 _patternNum, u8 _patternRow);

  ChipDataType (*getPatternDataType)(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn);

  u8 (*getPatternData)(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn);

  const char *(*getPatternHelp)(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn);

  u8 (*clearPatternData)(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn);

  u8 (*setPatternData)(u8 _channelNum, u8 _patternNum, u8 _patternRow, u8 _patternColumn, u8 _instrument,
                       u8 _data);

  u8 (*getMinOctave)();

  u8 (*getMaxOctave)();

  // Instruments
  const char *(*getInstrumentName)(u8 _instrument, u8 _stringWidth);

  void (*setInstrumentName)(u8 _instrument, char *_instrName);

  u8 (*instrumentNameLength)(u8 _instrument);

  u16 (*getNumInstruments)();

  u8 (*getInstrumentLen)(u8 _instrument);

  u8 (*getNumInstrumentParams)(u8 _instrument);

  const char *(*getInstrumentParamName)(u8 _instrument, u8 _instrumentParam, u8 _stringWidth);

  u8 (*getNumInstrumentData)(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow);

  ChipDataType (*getInstrumentDataType)(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow,
                                        u8 _instrumentColumn);

  u8 (*getInstrumentData)(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn);

  const char *(*getInstrumentHelp)(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn);

  const char *(*getInstrumentLabel)(u8 _instrument, u8 _instrumentRow);

  u8 (*clearInstrumentData)(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn);

  bool (*setInstrumentData)(u8 _instrument, u8 _instrumentParam, u8 _instrumentRow, u8 _instrumentColumn,
                            u8 _data);

  void (*swapInstrumentRow)(u8 _instrument, u8 _instrumentRow1, u8 _instrumentRow2);

  // Tables
  bool (*useTables)();

  u8 (*getNumTableKinds)();

  const char *(*getTableKindName)(u8 _tableKind);

  ChipTableStyle (*getTableStyle)(u8 _tableKind);

  u16 (*getMinTable)(u8 _tableKind);

  u16 (*getNumTables)(u8 _tableKind);

  u8 (*getTableDataLen)(u8 _tableKind, u16 _table);

  u8 (*getTableData)(u8 _tableKind, u16 _table, u8 _tableColumn);

  u8 (*setTableData)(u8 _tableKind, u16 _table, u8 _tableColumn, u8 _data);

  // Player
  u8 (*getPlayerSongRow)();

  u8 (*getPlayerPatternRow)();

  void (*plonk)(u8 _note, u8 _channelNum, u8 _instrument);

  void (*playSongFrom)(u8 _songRow, u8 _songColumn, u8 _patternRow, u8 _patternColumn);

  void (*playPatternFrom)(u8 _songRow, u8 _songColumn, u8 _patternRow, u8 _patternColumn);

  bool (*isPlaying)();

  void (*stop)();

  void (*silence)();

  void (*getSamples)(ChipSample *_buf, int _len);

  void (*preferredWindowSize)(u32 *_width, u32 *_height);
} ChipInterface;

extern ChipInterface *chips[];

ChipSample chip_expandSample(ChipSample _sample);

#endif // ifndef CHIP_H

