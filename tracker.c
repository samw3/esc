
#include <strings.h>
#include "tracker.h"
#include "console.h"
#include "chip.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

typedef struct {
  int x1;
  int y1;
  int x2;
  int y2;
} Rect;

Rect rectFromPoints(const int _x1, const int _y1, const int _x2, const int _y2) {
  int x1, y1, x2, y2;
  if (_x1 <= _x2) {
    x1 = _x1;
    x2 = _x2;
  } else {
    x1 = _x2;
    x2 = _x1;
  }
  if (_y1 <= _y2) {
    y1 = _y1;
    y2 = _y2;
  } else {
    y1 = _y2;
    y2 = _y1;
  }
  return (Rect) {
      x1, y1, x2, y2
  };
}

Rect rectFromSize(int _x, int _y, int _w, int _h) {
  return (Rect) {
      _x, _y, _x + _w, _y + _h
  };
}

Rect rectLine(int _x, int _y, int _len) {
  return (Rect) {
      _x, _y, _x + _len, _y
  };
}

Rect rectRel(int _len) {
  return (Rect) {
      con_x(), con_y(), con_x() + _len, con_y()
  };
}

bool rectHit(Rect _rect, int _x, int _y) {
  return _rect.x1 <= _x &&
         _rect.x2 >= _x &&
         _rect.y1 <= _y &&
         _rect.y2 >= _y;
}

struct TextEdit;
typedef struct TextEdit TextEdit;

typedef struct {
  Rect hitRect;
  TrackerState validStates;
  Action action;
  TrackerState actionTrackerState;
  TrackerState trackerState;
  int songX;
  int songY;
  int selectedChannel;
  int patternX;
  int patternY;
  int selectedPattern;
  int instrumentX;
  int instrumentY;
  int instrumentParam;
  int selectedInstrument;
  int selectedTableKind;
  int tableX;
  TextEdit *textEdit;
} Hit;

#define NUM_HITS 65536
Hit sHits[NUM_HITS];
u16 sHitPos = 0;

void clearHits() {
  sHitPos = 0;
}

Hit *addHit(Rect _hitRect, TrackerState _validStates) {
  sHits[sHitPos++] = (Hit) {
      _hitRect,
      _validStates,
      ACTION_NONE,
      TRACKER_EDIT_ANY,
      TRACKER_STATE_NONE,
      -1, -1, -1,
      -1, -1, -1,
      -1, -1, -1, -1,
      -1, -1,
      NULL
  };
  return &sHits[sHitPos - 1];
}

typedef void (*TextEditChangeCallback)(TextEdit *_te, TrackerTextEditKey _exitKey);

struct TextEdit {
  u8 xPos;
  u8 yPos;
  u8 maxWidth;
  u8 visibleWidth;
  int cursorPos;
  u8 visibleOffset;
  char *string;
  char *lastString;
  bool isEnabled;
  bool showsEllipsis;
  bool isActive;
  TextEditChangeCallback changeCallback;
  TextEdit *next;
  TextEdit *prev;
  void *userData;
};

TextEdit *TextEdit_new(TextEdit **_root, u8 _x, u8 _y, u8 _maxWidth, u8 _visibleWidth, bool _enabled,
                       bool _ellipsis, char _initialString[], TextEditChangeCallback _changeCallback,
                       void *_userData) {
  TextEdit *te = malloc(sizeof(TextEdit));
  if (!te) {
    return NULL;
  }
  char *buffer = malloc(sizeof(char) * _maxWidth);
  if (!buffer) {
    free(te);
    return NULL;
  }
  char *lastBuffer = malloc(sizeof(char) * _maxWidth);
  if (!lastBuffer) {
    free(te);
    return NULL;
  }
  strncpy(buffer, _initialString, _maxWidth);
  strncpy(lastBuffer, _initialString, _maxWidth);
  int len = strlen(buffer);
  int visibleOffset = (len < _visibleWidth) ? 0 : len - _visibleWidth + 1;
  *te = (TextEdit) {
      .xPos=_x,
      .yPos=_y,
      .maxWidth=_maxWidth,
      .visibleWidth=_visibleWidth,
      .cursorPos=len,
      .visibleOffset = visibleOffset,
      .string = buffer,
      .lastString = lastBuffer,
      .isEnabled = _enabled,
      .showsEllipsis = _ellipsis,
      .isActive = false,
      .changeCallback = _changeCallback,
      .next = *_root,
      .prev = NULL,
      .userData = _userData
  };
  *_root = te;
  return te;
}

void TextEdit_delete(TextEdit **_root, TextEdit *_te) {
  // Unlink
  if (_te->prev == NULL) {
    *_root = _te->next;
  } else {
    _te->prev->next = _te->next;
  }
  if (_te->next != NULL) {
    _te->next->prev = _te->prev;
  }
  // Dealloc
  free(_te->string);
  free(_te);
}

void TextEdit_deleteAll(TextEdit **_root) {
  while (_root != NULL) {
    TextEdit_delete(_root, *_root);
  }
}

void TextEdit_draw(TextEdit *_te) {
  if (_te->isActive) {
    con_gotoXY(_te->xPos, _te->yPos);
    Hit *hit = addHit(rectRel(_te->visibleWidth + 1), TRACKER_EDIT_ANY);
    hit->textEdit = _te;
    con_setAttrib(0x0F);
    con_putc(0xDE);

    const int len = strlen(_te->string);
    const int start = _te->visibleOffset;
    const int end = _te->visibleOffset + _te->visibleWidth;
    for (size_t i = start; i < end; i++) {
      if (i < len) {
        con_setAttrib(0x07);
        con_putc(_te->string[i]);
      } else {
        con_setAttrib(0x80);
        con_putc(0xB2);
      }
    }
    con_setAttrib(0x0F);
    con_putc(0xDD);
    const int x = 1 + _te->xPos + _te->cursorPos - _te->visibleOffset;
    const int y = _te->yPos;
    con_setAttribXY(x, y, 0x100 | con_getAttribXY(x, y));
  } else {
    con_gotoXY(_te->xPos, _te->yPos);
    Hit *hit = addHit(rectRel(_te->visibleWidth + 1), TRACKER_EDIT_ANY);
    hit->textEdit = _te;
    con_setAttrib(0x08);
    con_putc(0xDE);

    const int len = strlen(_te->string);
    const int start = 0;
    const int end = _te->visibleWidth;
    for (size_t i = start; i < end; i++) {
      if (i < len) {
        con_setAttrib(0x07);
        con_putc((_te->string)[i]);
      } else {
        con_setAttrib(0x80);
        con_putc(0xB2);
      }
    }
    con_setAttrib(0x08);
    con_putc(0xDD);
  }
} /* TextEdit_draw */

void TextEdit_drawAll(TextEdit *_root) {
  while (_root != NULL) {
    TextEdit_draw(_root);
    _root = _root->next;
  }
}

void TextEdit_pos(TextEdit *_te, int _x, int _y) {
  _te->xPos = _x;
  _te->yPos = _y;
}

void TextEdit_width(TextEdit *_te, int _visibleWidth) {
  _te->visibleWidth = _visibleWidth;
  int len = strlen(_te->lastString);
  _te->visibleOffset = (len < _te->visibleWidth) ? 0 : len - _te->visibleWidth + 1;
  _te->cursorPos = len;
}

void TextEdit_set(TextEdit *_te, const char *_string) {
  strncpy(_te->string, _string, _te->maxWidth);
  strncpy(_te->lastString, _string, _te->maxWidth);
  int len = strlen(_te->lastString);
  _te->visibleOffset = (len < _te->visibleWidth) ? 0 : len - _te->visibleWidth + 1;
  _te->cursorPos = len;
}

const char *TextEdit_get(TextEdit *_te) {
  return _te->lastString;
}

void TextEdit_activate(TextEdit *_te) {
  _te->isActive = true;
  strncpy(_te->string, _te->lastString, _te->maxWidth);
}

void TextEdit_deactivate(TextEdit *_te) {
  _te->isActive = false;
}

void TextEdit_deactivateAll(TextEdit *_someNode) {
  if (_someNode == NULL) {
    return;
  }
  // Find root
  TextEdit *node = _someNode;
  while (node->prev != NULL) {
    node = node->prev;
  }
  // Deactive all
  while (node->next != NULL) {
    TextEdit_deactivate(node);
    node = node->next;
  }
}

bool TextEdit_handleEditKey(TextEdit **_root, TrackerTextEditKey _key) {
  // Find active TextEdit else return
  TextEdit *te = *_root;
  while (te != NULL) {
    if (te->isActive) {
      break;
    }
    te = te->next;
  }
  if (te == NULL) {
    return false;
  }
  // Handle key
  switch (_key) {
    case TEK_ENTER:
    case TEK_DOWN:
    case TEK_UP: {
      // Save the edit back to the reference string
      strncpy(te->lastString, te->string, te->maxWidth);
      te->changeCallback(te, _key);
      TextEdit_deactivate(te);
      return true;
    }

    case TEK_ESCAPE: {
      strncpy(te->string, te->lastString, te->maxWidth);
      int len = strlen(te->string);
      te->visibleOffset = (len < te->visibleWidth) ? 0 : len - te->visibleWidth + 1;
      te->cursorPos = len;
      te->changeCallback(te, _key);
      TextEdit_deactivate(te);
      return true;
    }

    case TEK_HOME: {
      te->cursorPos = 0;
      te->visibleOffset = 0;
      return true;
    }

    case TEK_END: {
      te->cursorPos = strlen(te->string);
      if (te->cursorPos - te->visibleOffset > te->visibleWidth - 1) {
        te->visibleOffset = te->cursorPos - te->visibleWidth;
      }
      return true;
    }

    case TEK_RIGHT: {
      int len = strlen(te->string);
      if (te->cursorPos < len) {
        te->cursorPos++;
        if (te->cursorPos - te->visibleOffset > te->visibleWidth - 1) {
          te->visibleOffset++;
        }
      }
      return true;
    }

    case TEK_LEFT: {
      if (te->cursorPos > 0) {
        te->cursorPos--;
        if (te->cursorPos - te->visibleOffset < 0) {
          te->visibleOffset--;
        }
      }
      return true;
    }

    case TEK_WORD_RIGHT: {
      int len = strlen(te->string);
      while ((te->string[te->cursorPos] != ' ') && te->cursorPos < len) {
        te->cursorPos++;
        if (te->cursorPos - te->visibleOffset > te->visibleWidth - 1) {
          te->visibleOffset++;
        }
      }
      while ((te->string[te->cursorPos] == ' ') && te->cursorPos < len) {
        te->cursorPos++;
        if (te->cursorPos - te->visibleOffset > te->visibleWidth - 1) {
          te->visibleOffset++;
        }
      }
      return true;
    }

    case TEK_WORD_LEFT: {
      int len = strlen(te->string);
      bool first = true;
      while ((te->string[te->cursorPos] == ' ' || first || te->cursorPos == len) &&
             te->cursorPos > 0) {
        te->cursorPos--;
        if (te->cursorPos - te->visibleOffset < 0) {
          te->visibleOffset--;
        }
        first = false;
      }
      while ((te->string[te->cursorPos] != ' ') &&
             te->cursorPos > 0) {
        te->cursorPos--;
        if (te->cursorPos - te->visibleOffset < 0) {
          te->visibleOffset--;
        }
      }
      while ((te->string[te->cursorPos] == ' ') &&
             te->cursorPos < len &&
             te->cursorPos != 0) {
        te->cursorPos++;
        if (te->cursorPos - te->visibleOffset > te->visibleWidth - 1) {
          te->visibleOffset++;
        }
      }
      return true;
    }

    case TEK_CLEAR: {
      te->string[0] = '\0';
      te->cursorPos = 0;
      te->visibleOffset = 0;
      return true;
    }

    case TEK_DELETE: {
      int len = strlen(te->string);
      if (te->cursorPos < len) {
        memmove(&te->string[te->cursorPos],
                &te->string[te->cursorPos + 1],
                len - te->cursorPos);
      }
      return true;
    }

    case TEK_BACKSPACE: {
      int len = strlen(te->string);
      if (te->cursorPos > 0) {
        memmove(&te->string[te->cursorPos - 1],
                &te->string[te->cursorPos],
                len - te->cursorPos + 1);
        te->cursorPos--;
        if (te->cursorPos - te->visibleOffset < 0) {
          te->visibleOffset--;
        }
      }
      return true;
    }

    case TEK_SPACE: {
      int len = strlen(te->string);
      if (len < te->maxWidth) {
        memmove(&te->string[te->cursorPos + 1],
                &te->string[te->cursorPos],
                len - te->cursorPos + 1);
        te->string[te->cursorPos] = ' ';
      }
      te->cursorPos++;
      if (te->cursorPos - te->visibleOffset > te->visibleWidth - 1) {
        te->visibleOffset++;
      }
      return true;
    }

    case TEK_INSERT: {
      int len = strlen(te->string);
      if (len < te->maxWidth) {
        memmove(&te->string[te->cursorPos + 1],
                &te->string[te->cursorPos],
                len - te->cursorPos + 1);
        te->string[te->cursorPos] = ' ';
      }
      return true;
    }

    default: return false;
  } /* switch */
}   /* TextEdit_handleEditKey */

bool TextEdit_handleAsciiKey(TextEdit **_root, int _key) {
  // Find active TextEdit else return
  TextEdit *te = *_root;
  while (te != NULL) {
    if (te->isActive) {
      break;
    }
    te = te->next;
  }
  if (te == NULL) {
    return false;
  }
  // Handle key
  int len = strlen(te->string);
  if (len < te->maxWidth) {
    memmove(&te->string[te->cursorPos + 1],
            &te->string[te->cursorPos],
            len - te->cursorPos + 1);
    te->string[te->cursorPos] = _key;
    te->cursorPos++;
    if (te->cursorPos - te->visibleOffset > te->visibleWidth - 1) {
      te->visibleOffset++;
    }
  }
  return true;
}

void TextEdit_handleHit(TextEdit *_te, int _x) {
  if (_te->isActive) {
    _te->cursorPos = _x - _te->xPos + _te->visibleOffset - 1;
    int len = strlen(_te->string);
    if (_te->cursorPos > len) {
      _te->cursorPos = len;
    }
    if (_te->cursorPos < 0) {
      _te->cursorPos = 0;
    }
    if (_te->cursorPos < _te->visibleOffset) {
      _te->visibleOffset = _te->cursorPos;
    }
    if (_te->cursorPos - _te->visibleOffset > _te->visibleWidth) {
      _te->visibleOffset = _te->cursorPos - _te->visibleWidth + 1;
    }
  } else {
    TextEdit_deactivateAll(_te);
    _te->isActive = true;
  }
}

TextEdit *sTextEditRoot_Editor;
TextEdit *sTEInstrumentName;

#define HANDLE_ACTION(action, state) if((_action == action) && ((state & _state & sTrackerState) != 0)) \
    ACTION__ ## action ## __ ## state()
#define ACTION(action, state) static void ACTION__ ## action ## __ ## state()

char *sNotenames[] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};
u8 sOctave = 4;
char *sFilename = "";
const char *sChipName;
ChipInterface *sChip;
int sSongX = 0, sSongY = 0;
int sSelectedChannel = 0;
int sPatternX = 0, sPatternY = 0;
int sSelectedPattern = 0;
int sInstrumentX = 0, sInstrumentY = 0, sInstrumentParam = 0;
int sSelectedInstrument = 0;
int sSelectedTableKind = 0;
u16 *sSelectedTable = NULL;
int sTableX = 0;
Rect sTableRect = {0};
int sMetaDataY = 0;
TrackerState sTrackerState = TRACKER_EDIT_SONG;
bool sbEditing = false;
bool sbShowKeys = false;
u8 sPlonkNote = 0;

void tracker_onChangeInstrumentName(TextEdit *_te, TrackerTextEditKey _exitKey) {
  sChip->setInstrumentName(sSelectedInstrument,
                           sTEInstrumentName->lastString);
  if (_exitKey == TEK_UP) {
    tracker_instrumentMoveUp();
  }
}

void tracker_init() {
  sChip->init();
  sChip->loadSong(sFilename);
  sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);

  int count = sChip->getNumTableKinds();
  sSelectedTable = malloc(sizeof(u16) * count);
  for (size_t i = 0; i < count; i++) {
    sSelectedTable[i] = sChip->getMinTable(i);
  }
  sTextEditRoot_Editor = NULL;

  sTEInstrumentName = TextEdit_new(&sTextEditRoot_Editor, 20, 2, 40, 16, true, false, "",
                                   tracker_onChangeInstrumentName, NULL);

  sOctave = sChip->getMaxOctave() / 2;
}

void tracker_destroy() {
  sChip->shutdown();
  free(sSelectedTable);
}

void tracker_setFilename(char *filename) {
  sFilename = filename;
}

void *tracker_setChipName(char *chipName) {
  int i = 0;
  do {
    sChip = chips[i];
    i++;
  } while (sChip != NULL && strcasecmp(chipName, sChip->getChipId()));
  if (sChip != NULL) {
    sChipName = sChip->getChipId();
  }
  return sChip;
}

void tracker_drawNote(u8 _note) {
  if (_note == 0) {
    con_print("---");
  } else if (_note == 255) {
    con_print("***");
  } else {
    con_printf("%s%d", sNotenames[(_note - 1) % 12], (_note - 1) / 12);
  }
}

int tracker_drawPatternEditorCentered(int _x, int _y, int _height) {
  Hit *hit;
  if (sTrackerState == TRACKER_EDIT_PATTERN) {
    con_setAttrib(0x0F);
  } else {
    con_setAttrib(0x07);
  }
  con_printXY(_x, _y, "PATTERN");

  static int patternOffset = 0;
  int numChannels = sChip->getNumChannels();
  int width = 3;
  if (sPatternY < patternOffset) {
    patternOffset = sPatternY;
  }
  if (sPatternY >= patternOffset + _height) {
    patternOffset = sPatternY - _height + 1;
  }

  u8 maxPatternLen = 0;
  for (size_t i = 0; i < numChannels; i++) {
    int patternNum = sChip->getPatternNum(sSongY, i);
    if (patternNum >= 0) {
      u8 patternLen = sChip->getPatternLen(patternNum);
      if (patternLen > maxPatternLen) {
        maxPatternLen = patternLen;
      }
    }
  }

  u8 quarter = maxPatternLen >> 2;

  // Draw pattern row numbers
  int heightOfRows = _height - 1;
  int halfHeightOfRows = heightOfRows / 2;
  for (int j = 0; j < heightOfRows; j++) {
    int row = sPatternY - halfHeightOfRows + j;
    if (row >= 0 && row < maxPatternLen) {
      if (row == sPatternY) {
        con_setAttrib(0x4F);
      } else {
        if (row % quarter == 0) {
          con_setAttrib(0x07);
        } else {
          con_setAttrib(0x08);
        }
      }
      hit = addHit(rectLine(_x, _y + j + 2, 2), TRACKER_EDIT_ANY);
      hit->trackerState = TRACKER_EDIT_PATTERN;
      hit->patternY = row;
      con_printfXY(_x, _y + 2 + j, "%02X", row);
    }
  }
  for (size_t i = 0; i < numChannels; i++) {
    int patternNum = sChip->getPatternNum(sSongY, i);
    int patternLen = sChip->getPatternLen(patternNum);

    // Compute pattern width
    int patternWidth = 0;
    for (int j = 0; j < heightOfRows; j++) {
      int row = sPatternY - halfHeightOfRows + j;
      if (row >= 0 && row < patternLen) {
        int w = sChip->getNumPatternDataColumns(i, patternNum, row);
        int tw = 0;
        for (size_t k = 0; k < w; k++) {
          ChipDataType type = sChip->getPatternDataType(i, patternNum, row, k);
          switch (type) {
            case CDT_LABEL:
            case CDT_HEX:
            case CDT_ASCII: tw++;
              break;
            case CDT_NOTE: tw += 3;
              break;
          }
        }
        patternWidth = (patternWidth > tw) ? patternWidth : tw;
      }
    }
    width += patternWidth + 1;

    // Draw pattern data
    con_setAttrib(0x07);
    con_printfXY(_x + i * (patternWidth + 1) + 3, _y + 1, "%02X", patternNum);
    con_setAttrib(0x08);
    con_printXY(_x + i * (patternWidth + 1) + 6, _y + 1, sChip->getChannelName(i, patternWidth - 2));
    for (int j = 0; j < heightOfRows; j++) {
      int row = sPatternY - halfHeightOfRows + j;
      if (row >= 0 && row < patternLen) {
        if (sPatternY == row) {
          if (sSelectedChannel == i) {
            con_setAttrib(0x4F);
          } else {
            con_setAttrib(0x47);
          }
        } else {
          con_setAttrib(0x07);
        }
        con_gotoXY(_x + i * (patternWidth + 1) + 3, _y + j + 2);
        int w = sChip->getNumPatternDataColumns(i, patternNum, row);
        for (size_t k = 0; k < w; k++) {
          if (sPatternY == row && sSelectedChannel == i) {
            con_setAttrib(0x4F);
            if (k == sPatternX && sTrackerState == TRACKER_EDIT_PATTERN) {
              if (sbEditing) {
                con_setAttrib(0x1F4);
              } else {
                con_setAttrib(0xF4);
              }
            }
          }
          ChipDataType type = sChip->getPatternDataType(i, patternNum, row, k);
          u8 data = sChip->getPatternData(i, patternNum, row, k);
          switch (type) {
            case CDT_LABEL: hit = addHit(rectRel(1), TRACKER_EDIT_ANY);
              con_putc(data);
              break;
            case CDT_NOTE: hit = addHit(rectRel(3), TRACKER_EDIT_ANY);
              tracker_drawNote(data);
              break;
            case CDT_HEX: hit = addHit(rectRel(1), TRACKER_EDIT_ANY);
              con_putc(data + ((data < 16) ? (data < 10 ? 48 : 55) : 0));
              break;
            case CDT_ASCII: hit = addHit(rectRel(1), TRACKER_EDIT_ANY);
              con_putc(data);
              break;
          }
          hit->trackerState = TRACKER_EDIT_PATTERN;
          hit->patternY = row;
          hit->patternX = k;
          hit->selectedChannel = i;
        }
      }
    }
    for (size_t j = 1; j <= _height; j++) {
      con_setAttrib(j == halfHeightOfRows + 2 ? 0x48 : 0x08);
      con_putcXY(_x + i * (patternWidth + 1) + 2, _y + j, 0xb3);
    }
  }
  width--;
  con_setAttrib(0x8);
  for (int i = 0; i <= _height; i++) {
    con_putcXY(_x + width, _y + i, 0xba);
  }
  return width;
} /* tracker_drawPatternEditorCentered */

int tracker_drawPatternEditor(int _x, int _y, int _height) {
  Hit *hit;
  if (sTrackerState == TRACKER_EDIT_PATTERN) {
    con_setAttrib(0x0F);
  } else {
    con_setAttrib(0x07);
  }
  con_printXY(_x, _y, "PATTERN");

  static int patternOffset = 0;
  int numChannels = sChip->getNumChannels();
  int width = 3;
  if (sPatternY < patternOffset) {
    patternOffset = sPatternY;
  }
  if (sPatternY >= patternOffset + _height) {
    patternOffset = sPatternY - _height + 1;
  }
  u8 maxPatternLen = 0;
  for (size_t i = 0; i < numChannels; i++) {
    int patternNum = sChip->getPatternNum(sSongY, i);
    int patternLen = sChip->getPatternLen(patternNum);
    maxPatternLen = (patternLen > maxPatternLen) ? patternLen : maxPatternLen;
  }
  u8 quarter = maxPatternLen >> 2;
  // Draw pattern row numbers
  for (size_t j = 0; j < maxPatternLen; j++) {
    if (j >= patternOffset && j - patternOffset < _height - 1) {
      if (j == sPatternY) {
        if (j % quarter == 0) {
          con_setAttrib(0x7F);
        } else {
          con_setAttrib(0x70);
        }
      } else {
        if (j % quarter == 0) {
          con_setAttrib(0x0F);
        } else {
          con_setAttrib(0x08);
        }
      }
      hit = addHit(rectLine(_x, _y + j + 2, 2), TRACKER_EDIT_ANY);
      hit->trackerState = TRACKER_EDIT_PATTERN;
      hit->patternY = j;
      con_printfXY(_x, _y + j + 2, "%02X", j);
    }
  }
  for (size_t i = 0; i < numChannels; i++) {
    int patternNum = sChip->getPatternNum(sSongY, i);
    int patternLen = sChip->getPatternLen(patternNum);

    // Compute pattern width
    int patternWidth = 0;
    for (size_t j = 0; j < patternLen; j++) {
      int w = sChip->getNumPatternDataColumns(i, patternNum, j);
      int tw = 0;
      for (size_t k = 0; k < w; k++) {
        ChipDataType type = sChip->getPatternDataType(i, patternNum, j, k);
        switch (type) {
          case CDT_LABEL:
          case CDT_HEX:
          case CDT_ASCII: tw++;
            break;
          case CDT_NOTE: tw += 3;
            break;
        }
      }
      patternWidth = (patternWidth > tw) ? patternWidth : tw;
    }
    width += patternWidth + 1;

    // Draw pattern data
    con_setAttrib(0x07);
    con_printfXY(_x + i * (patternWidth + 1) + 3, _y + 1, "%02X", patternNum);
    con_setAttrib(0x08);
    con_printXY(_x + i * (patternWidth + 1) + 6, _y + 1, sChip->getChannelName(i, patternWidth - 2));
    for (size_t j = 0; j < patternLen; j++) {
      if (sSelectedChannel == i && sPatternY == j) {
        con_setAttrib(0x70);
      } else {
        con_setAttrib(0x07);
      }
      if (j >= patternOffset && j - patternOffset < _height - 1) {
        con_gotoXY(_x + i * (patternWidth + 1) + 3, _y + j + 2);
        int w = sChip->getNumPatternDataColumns(i, patternNum, j);
        for (size_t k = 0; k < w; k++) {
          if (sPatternY == j && sSelectedChannel == i) {
            con_setAttrib(0x70);
            if (k == sPatternX && sTrackerState == TRACKER_EDIT_PATTERN) {
              con_setAttrib(0x170);
            }
          }
          ChipDataType type = sChip->getPatternDataType(i, patternNum, j, k);
          u8 data = sChip->getPatternData(i, patternNum, j, k);
          switch (type) {
            case CDT_LABEL: hit = addHit(rectRel(1), TRACKER_EDIT_ANY);
              con_putc(data);
              break;
            case CDT_NOTE: hit = addHit(rectRel(3), TRACKER_EDIT_ANY);
              tracker_drawNote(data);
              break;
            case CDT_HEX: hit = addHit(rectRel(1), TRACKER_EDIT_ANY);
              con_putc(data + ((data < 16) ? (data < 10 ? 48 : 55) : 0));
              break;
            case CDT_ASCII: hit = addHit(rectRel(1), TRACKER_EDIT_ANY);
              con_putc(data);
              break;
          }
          hit->trackerState = TRACKER_EDIT_PATTERN;
          hit->patternY = j;
          hit->patternX = k;
          hit->selectedChannel = i;
        }
      }
    }
    con_setAttrib(0x08);
    for (size_t j = 1; j <= _height; j++) {
      con_putcXY(_x + i * (patternWidth + 1) + 2, _y + j, 0xb3);
    }
  }
  width--;
  con_setAttrib(0x8);
  for (int i = 0; i <= _height; i++) {
    con_putcXY(_x + width, _y + i, 0xba);
  }
  return width;
} /* tracker_drawPatternEditor */

int tracker_drawInstrumentEditor(int _x, int _y, int _height) {
  Hit *hit;
  static int instOffset = 0;
  if (sInstrumentY < instOffset) {
    instOffset = sInstrumentY;
  }
  if (sInstrumentY >= instOffset + _height) {
    instOffset = sInstrumentY - _height + 1;
  }
  int instLen = sChip->getInstrumentLen(sSelectedInstrument);
  int numParams = sChip->getNumInstrumentParams(sSelectedInstrument);
  // Draw line numbers
  for (size_t instRow = 0; instRow < instLen; instRow++) {
    if (instRow >= instOffset && instRow - instOffset < _height - 1) {
      con_gotoXY(_x, _y + 3 + instRow - instOffset);
      if (instRow == sInstrumentY) {
        con_setAttrib(0x4F);
      } else {
        con_setAttrib(0x08);
      }
      hit = addHit(rectRel(3), TRACKER_EDIT_ANY);
      hit->instrumentY = instRow;
      con_printf(sChip->getInstrumentLabel(sSelectedInstrument, instRow));
      con_putc(':');
    }
  }
  // Params
  int instWidth = 2;
  for (size_t param = 0; param < numParams; param++) {
    // Get param max width
    int paramWidth = 0;
    for (size_t instRow = 0; instRow < instLen; instRow++) {
      int width = 0;
      int numData = sChip->getNumInstrumentData(sSelectedInstrument, param, instRow);
      for (size_t dataColumn = 0; dataColumn < numData; dataColumn++) {
        ChipDataType type = sChip->getInstrumentDataType(sSelectedInstrument, param, instRow, dataColumn);
        switch (type) {
          case CDT_LABEL: width++;
            break;
          case CDT_NOTE: width += 3;
            break;
          case CDT_HEX: width++;
            break;
          case CDT_ASCII: width++;
            break;
        }
      }
      width++;
      paramWidth = (width > paramWidth) ? width : paramWidth;
    }

    // Draw params
    con_setAttrib(0x08);
    con_printXY(_x + 1 + instWidth, _y + 2,
                sChip->getInstrumentParamName(sSelectedInstrument, param, paramWidth));
    for (size_t instRow = 0; instRow < instLen; instRow++) {
      // Highlight param
      for (int i = 0; i < paramWidth; ++i) {
        if (instRow == sInstrumentY) {
          u16 attrib = con_getAttribXY(_x + 1 + instWidth + i, _y + 3 + instRow - instOffset);
          attrib = (attrib & 0xF0F) | 0x40;
          con_setAttribXY(_x + 1 + instWidth + i, _y + 3 + instRow - instOffset, attrib);
        }
      }

      if (instRow >= instOffset && instRow - instOffset < _height - 2) {
        con_gotoXY(_x + 1 + instWidth, _y + 3 + instRow - instOffset);
        int numData = sChip->getNumInstrumentData(sSelectedInstrument, param, instRow);
        for (size_t dataColumn = 0; dataColumn < numData; dataColumn++) {
          ChipDataType type = sChip->getInstrumentDataType(sSelectedInstrument, param, instRow, dataColumn);
          u8 data = sChip->getInstrumentData(sSelectedInstrument, param, instRow, dataColumn);
          if (instRow == sInstrumentY) {
            if (dataColumn == sInstrumentX &&
                param == sInstrumentParam &&
                !sTEInstrumentName->isActive) {
              if (sTrackerState == TRACKER_EDIT_INSTRUMENT) {
                if (sbEditing) {
                  con_setAttrib(0x14F);
                } else {
                  con_setAttrib(0xF4);
                }
              } else {
                con_setAttrib(0x4F);
              }
            } else {
              con_setAttrib(0x47);
            }
          } else {
            con_setAttrib(0x07);
          }
          switch (type) {
            case CDT_LABEL: con_putc(data);
              break;
            case CDT_NOTE: hit = addHit(rectRel(3), TRACKER_EDIT_ANY);
              hit->trackerState = TRACKER_EDIT_INSTRUMENT;
              hit->instrumentY = instRow;
              hit->instrumentX = dataColumn;
              hit->instrumentParam = param;
              tracker_drawNote(data);
              break;
            case CDT_HEX: hit = addHit(rectRel(1), TRACKER_EDIT_ANY);
              hit->trackerState = TRACKER_EDIT_INSTRUMENT;
              hit->instrumentY = instRow;
              hit->instrumentX = dataColumn;
              hit->instrumentParam = param;
              con_putc(data + ((data < 16) ? (data < 10 ? 48 : 55) : 0));
              break;
            case CDT_ASCII: hit = addHit(rectRel(1), TRACKER_EDIT_ANY);
              hit->trackerState = TRACKER_EDIT_INSTRUMENT;
              hit->instrumentY = instRow;
              hit->instrumentX = dataColumn;
              hit->instrumentParam = param;
              con_putc(data);
              break;
          }
        }
        if (instRow == sInstrumentY) {
          con_setAttrib(0x4F);
        } else {
          con_setAttrib(0x07);
        }
        if (param < (numParams - 1)) {
          con_putc(' ');
        }
      }
    }
    instWidth += paramWidth;
  }
  if (instWidth < 16) {
    instWidth = 16;
  }

  // Draw title
  if (sTrackerState == TRACKER_EDIT_INSTRUMENT) {
    con_setAttrib(0x0F);
  } else {
    con_setAttrib(0x07);
  }
  con_printXY(_x, _y, "INSTR");
  con_setAttrib(0x07);

  TextEdit_pos(sTEInstrumentName, _x, _y + 1);
  if (!sTEInstrumentName->isActive) {
    TextEdit_width(sTEInstrumentName, instWidth - 2);
    TextEdit_set(sTEInstrumentName,
                 sChip->getInstrumentName(sSelectedInstrument,
                                          sTEInstrumentName->maxWidth));
  }
  con_printfXY(_x, _y + 2, "%02X", sSelectedInstrument);

  // Highlight playing instrument
  for (int i = 0; i < sChip->getNumChannels(); i++) {
    if (sChip->getPlayerInstrument(i) == sSelectedInstrument) {
      for (int j = 0; j < instWidth; j++) {
        u16 attrib = con_getAttribXY(_x + j, _y + 2);
        attrib = (attrib & 0xFF0) | 0xB;
        con_setAttribXY(_x + j, _y + 3 + sChip->getPlayerInstrumentRow(i), attrib);
      }
      break;
    }
  }

  // Draw double line
  con_setAttrib(0x8);
  for (int i = 0; i <= _height; i++) {
    con_putcXY(_x + instWidth, _y + i, 0xba);
  }
  return instWidth;
} /* tracker_drawInstrumentEditor */

int tracker_drawSongEditor(int _x, int _y, int _height) {
  Hit *hit;
  if (sTrackerState == TRACKER_EDIT_SONG) {
    con_setAttrib(0x0F);
  } else {
    con_setAttrib(0x07);
  }
  con_printXY(_x, _y, "SONG");
  static int songOffset = 0;
  int songLength = sChip->getNumSongRows();
  int songWidth = 0;
  if (sSongY < songOffset) {
    songOffset = sSongY;
  }
  if (sSongY >= songOffset + _height) {
    songOffset = sSongY - _height + 1;
  }
  int numChannels = sChip->getNumChannels();
  for (int songRow = 0; songRow < songLength; songRow++) {
    int width = 0;
    if (songRow >= songOffset && songRow - songOffset < _height) {
      con_gotoXY(_x, _y + 1 + songRow - songOffset);

      hit = addHit(rectRel(3), TRACKER_EDIT_ANY);
      hit->trackerState = TRACKER_EDIT_SONG;
      hit->songY = songRow;
      if (songRow == sSongY) {
        con_setAttrib(0x4F);
      } else {
        con_setAttrib(0x08);
      }
      con_printf("%02X:", songRow);

      con_setAttrib(0x07);
      for (int channel = 0; channel < numChannels; channel++) {
        int numCols = sChip->getNumSongDataColumns(channel);
        for (size_t dataColumn = 0; dataColumn < numCols; dataColumn++) {
          if (songRow == sSongY) {
            con_setAttrib(0x47);
            if (channel == sSelectedChannel) {
              if (dataColumn == sSongX && sTrackerState == TRACKER_EDIT_SONG) {
                if (sbEditing) {
                  con_setAttrib(0x14F);
                } else {
                  con_setAttrib(0xF4);
                }
              } else {
                con_setAttrib(0x4F);
              }
            }
          }
          ChipDataType type = sChip->getSongDataType(songRow, channel, dataColumn);
          u8 data = sChip->getSongData(songRow, channel, dataColumn);
          switch (type) {
            case CDT_HEX: hit = addHit(rectRel(1), TRACKER_EDIT_ANY);
              hit->trackerState = TRACKER_EDIT_SONG;
              hit->songY = songRow;
              hit->songX = dataColumn;
              hit->selectedChannel = channel;
              con_putc(data + ((data < 16) ? (data < 10 ? 48 : 55) : 0));
              width++;
              break;
            case CDT_LABEL: con_putc(data);
              width++;
              break;
            default: break;
          }
        }
        if (channel < (numChannels - 1)) {
          if (songRow == sSongY) {
            con_setAttrib(0x47);
          }
          con_putc(' ');
          width++;
        }
      }
      songWidth = (songWidth > width) ? songWidth : width;
    }
  }
  songWidth += 3;
  con_setAttrib(0x08);
  for (int i = 0; i <= _height; i++) {
    con_putcXY(_x + songWidth, _y + i, 0xba);
  }
  return songWidth;
} /* tracker_drawSongEditor */

void tracker_drawMessages(int _x, int _y, int _height) {
  con_setAttrib(0x08);
  con_printXY(_x, _y, "MESSAGES");

  con_setAttrib(0x07);
  con_printXY(_x, _y + _height, con_getMostRecentMessage());
  for (int i = _height - 1; i > 0; i--) {
    con_printXY(_x, _y + i, con_getNextMessage());
  }
}

void tracker_onChangeMetaData(TextEdit *_te, TrackerTextEditKey _exitKey) {

}

int tracker_drawMetaData(int _x, int _y, int _width) {
  Hit *hit;
  if (sTrackerState == TRACKER_EDIT_META_DATA) {
    con_setAttrib(0x0F);
  } else {
    con_setAttrib(0x07);
  }
  con_printXY(_x, _y, "METADATA");
  con_setAttrib(0x07);

  size_t titleWidth = 0;
  for (int i = 0; i < sChip->getNumMetaData(); ++i) {
    ChipMetaDataEntry *metaData = sChip->getMetaData(i);
    if (strlen(metaData->name) > titleWidth) {
      titleWidth = strlen(metaData->name);
    }
  }
  titleWidth++;

  // Highlight row
  con_setAttrib(0x4F);
  for (int i = 0; i < _width; ++i) {
    con_setAttribXY(_x + i, _y + sMetaDataY + 1, 0x4F);
  }

  for (int i = 0; i < sChip->getNumMetaData(); ++i) {
    ChipMetaDataEntry *metaData = sChip->getMetaData(i);
    if (sMetaDataY == i) {
      con_setAttrib(0x4F);
    } else {
      con_setAttrib(0x07);
    }
    int len = strlen(metaData->name);
    con_printXY(_x + titleWidth - len - 1, _y + i + 1, metaData->name);
    con_putc(':');
    switch (metaData->type) {
      case CMDT_STRING: {
        if (metaData->textEdit == NULL) {
          metaData->textEdit = TextEdit_new(
              &sTextEditRoot_Editor,
              _x + titleWidth, _y + i + 1,
              40, _width,
              true, false, "",
              tracker_onChangeMetaData,
              metaData
          );
        }
      }
      case CMDT_OPTIONS: {
        con_putc('[');
        if (sMetaDataY == i && sTrackerState == TRACKER_EDIT_META_DATA
            ) {
          if (sbEditing) {
            con_setAttrib(0x14F);
          } else {
            con_setAttrib(0xF4);
          }
        }
        con_print(metaData->options[metaData->value]);
        if (sMetaDataY == i && sTrackerState == TRACKER_EDIT_META_DATA
            ) {
          con_setAttrib(0x4F);
        }
        con_putc(']');
      }
      case CMDT_HEX:break; // TODO: Implement
      case CMDT_DECIMAL:break; // TODO: Implement
    }
  }
  return sChip->getNumMetaData();
}

void tracker_drawKeys(int _x, int _y, int _height) {
  static char *kModNames[] = {
      "", "SHIFT+", "CTRL+", "CMD+", "ALT+"
  };
  con_setAttrib(0x0F);
  con_printXY(_x, _y, "KEYS");
  con_setAttrib(0x07);
  int c = 0;
  for (int i = 0; i < actionsCount; ++i) {
    ActionTableEntry action = actions[i];
    if (sTrackerState & action.trackerState) {
      int mod = 0;
      if (action.modifiers & KMOD_SHIFT) mod = 1;
      if (action.modifiers & KMOD_CTRL) mod = 2;
      if (action.modifiers & KMOD_GUI) mod = 3;
      if (action.modifiers & KMOD_ALT) mod = 4;
      con_printfXY(_x, _y + 1 + c, "%s%s", kModNames[mod], SDL_GetScancodeName(action.scancode));
      con_printfXY(_x + 12, _y + 1 + c, actionNames[action.action]);
      c++;
    }
  }
}

void tracker_drawTableBar(int _x, int _y, int _offset, int _val, bool _marked) {
  if (sChip->getTableStyle(sSelectedTableKind) == CTS_BOTTOM) {
    if (_marked) {
      con_setAttrib(0x0f);
    } else {
      con_setAttrib(0x08);
    }
    if (_val < 0) {
      con_vline(_x + _offset, _y, _y + 15, 0xB0);
      return;
    }
    if (_val < 15) {
      con_vline(_x + _offset, _y, _y + (14 - _val), 0xB0);
    }
    if (_marked) {
      con_setAttrib(0x0f);
    } else {
      con_setAttrib(0x07);
    }
    con_vline(_x + _offset, _y + (15 - _val), _y + 15, 0xdb);
  } else {
    if (_marked) {
      con_setAttrib(0x0f);
    } else {
      con_setAttrib(0x08);
    }
    if (_val < 0) {
      con_vline(_x + _offset, _y, _y + 15, 0xB0);
      return;
    }
    if (_val < 8) {
      con_vline(_x + _offset, _y, _y + 7, 0xB0);
      if (_val > 0) {
        con_vline(_x + _offset, _y + 16 - _val, _y + 15, 0xB0);
      }
    } else {
      if (_val < 15) {
        con_vline(_x + _offset, _y, _y + 14 - _val, 0xB0);
      }
      con_vline(_x + _offset, _y + 8, _y + 15, 0xB0);
    }
    if (_marked) {
      con_setAttrib(0x0f);
    } else {
      con_setAttrib(0x07);
    }
    if (_val < 8) {
      con_vline(_x + _offset, _y + 8, _y + 15 - _val, 0xdb);
    } else {
      con_vline(_x + _offset, _y + 15 - _val, _y + 7, 0xdb);
    }
  }
  if (sTrackerState == TRACKER_EDIT_TABLE && _offset == sTableX) {
    if (_marked) {
      con_setAttrib(0x10F);
    } else {
      con_setAttrib(0x107);
    }
  } else {
    if (_marked) {
      con_setAttrib(0x0F);
    } else {
      con_setAttrib(0x07);
    }
  }
  con_printfXY(_x + _offset, _y + 16, "%X", _val & 0xf);
} /* tracker_drawTableBar */

void tracker_drawTables(int _x, int _y) {
  Hit *hit;
  if (sTrackerState == TRACKER_EDIT_TABLE) {
    con_setAttrib(0x0F);
  } else {
    con_setAttrib(0x07);
  }
  // Titles
  con_printXY(_x, _y, "TABLES");
  con_gotoXY(_x, _y + 1);
  for (size_t i = 0; i < sChip->getNumTableKinds(); i++) {
    if (sSelectedTableKind == i) {
      con_setAttrib(0x70);
    } else {
      con_setAttrib(0x07);
    }
    const char *name = sChip->getTableKindName(i);
    hit = addHit(rectRel(strlen(name) + 6), TRACKER_EDIT_ANY);
    hit->trackerState = TRACKER_EDIT_TABLE;
    hit->selectedTableKind = i;
    con_printf(" %s (%X) ", name, sSelectedTable[i]);
  }
  // Y Axis
  con_setAttrib(0x08);
  for (size_t i = 0; i < 16; i++) {
    con_printfXY(_x, _y + 2 + i, "%X", 15 - i);
  }
  // Data
  int cols = con_columns() - _x;

  int dataLen = sChip->getTableDataLen(sSelectedTableKind, sSelectedTable[sSelectedTableKind]);
  for (size_t i = 0; i < cols; i++) {
    u8 data = sChip->getTableData(sSelectedTableKind,
                                  sSelectedTable[sSelectedTableKind],
                                  i);
    tracker_drawTableBar(_x + 1, _y + 2, i,
                         (i < dataLen ? data : -1),
                         (i < dataLen ? (data & 0x10) != 0 : false));
  }
  sTableRect = rectFromPoints(_x + 1, _y + 2, con_columns(), _y + 17);
} /* tracker_drawTables */

void tracker_drawScreen() {
  clearHits();
  if (sChip->isPlaying()) {
    // TODO: add follow flag
    sSongY = sChip->getPlayerSongRow(sSelectedChannel);
    sPatternY = sChip->getPlayerPatternRow(sSelectedChannel);
    sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
  }
  con_cls();
  con_setAttrib(0x06);
  int x = (strlen(sChipName) + 13);
  con_printfXY(0, 0, "ESC v%d.%d (%s)", MAJOR_VERSION, MINOR_VERSION, sChipName);
  con_setAttrib(0x08);
  con_printfXY(x, 0, "OCT:%d", sOctave);
  con_printfXY(x + 7, 0, "FILE:%s", sFilename);
  con_setAttrib(0x07);
  con_printfXY(con_columns() - 18, 0, "Press '?' for Help");
  int width;
  width = tracker_drawSongEditor(0, 1, con_rows() - 3);
  if (width < con_columns()) {
    width += tracker_drawPatternEditorCentered(width + 1, 1, con_rows() - 3) + 1;
    if (width < con_columns()) {
      width += tracker_drawInstrumentEditor(width + 1, 1, con_rows() - 3) + 1;
      if (width < con_columns()) {
        if (sChip->useTables()) {
          if (con_rows() > 20) {
            tracker_drawTables(width + 1, 1);
            con_setAttrib(0x08);
            con_putcXY(width, 20, 0xcc);
            con_hline(width + 1, con_columns() - 1, 20, 0xcd);
            if (sbShowKeys) {
              tracker_drawKeys(width + 1, 21, con_rows() - 23);
            } else {
              tracker_drawMessages(width + 1, 21, con_rows() - 23);
            }
          } else {
            if (sbShowKeys) {
              tracker_drawKeys(width + 1, 1, con_rows() - 3);
            } else {
              tracker_drawTables(width + 1, 1);
            }
          }
        } else {
          if (sbShowKeys) {
            tracker_drawKeys(width + 1, 1, con_rows() - 3);
          } else {
            if (sChip->getNumMetaData() > 0) {
              int teX = width + 1;
              int teWidth = con_columns() - teX;
              int split = tracker_drawMetaData(width + 1, 1, teWidth) + 2;
              con_setAttrib(0x08);
              con_hline(width + 1, con_columns() - 1, split, 0xcd);
              tracker_drawMessages(width + 1, split + 1, con_rows() - split - 3);
            } else {
              tracker_drawMessages(width + 1, 1, con_rows() - 3);
            }
          }
        }
      }
    }
  }
  // con_fill(7,0xb0);
  // Draw Status Bar
  int w = con_columns();
  int y = con_rows() - 1;
  Hit *h = addHit(rectLine(1, y, 7), TRACKER_EDIT_ANY);
  h->action = ACTION_TOGGLE_EDIT;
  h->trackerState = TRACKER_EDIT_ANY;

  const char *statusBarText = "";
  switch (sTrackerState) {
    case TRACKER_EDIT_SONG: {
      statusBarText = sChip->getSongHelp(sSongY, sSelectedChannel, sSongX);
      break;
    }
    case TRACKER_EDIT_PATTERN: {
      statusBarText = sChip->getPatternHelp(sSelectedChannel, sSelectedPattern, sPatternY, sPatternX);
      break;
    }
    case TRACKER_EDIT_INSTRUMENT: {
      statusBarText = sChip->getInstrumentHelp(sSelectedInstrument, sInstrumentParam, sInstrumentY, sInstrumentX);
      break;
    }
    default: statusBarText = "";
  }
  size_t len = strlen(statusBarText);
  con_printXY(w - 1 - len, y, statusBarText);
  if (sbEditing) {
    con_printXY(1, y, "EDITING");
    for (size_t i = 0; i < w; i++) {
      con_setAttribXY(i, y, 0x1f);
    }
  } else {
    con_printXY(1, y, "JAMMING:");
    if (sPlonkNote == 0) {
      con_print("---");
    } else if (sPlonkNote == 255) {
      con_print("***");
    } else {
      con_printf("%s%d", sNotenames[(sPlonkNote - 1) % 12], (sPlonkNote - 1) / 12);
    }
    for (size_t i = 0; i < w; i++) {
      con_setAttribXY(i, y, 0x70);
    }
  }
  TextEdit_drawAll(sTextEditRoot_Editor);
} /* tracker_drawScreen */

void tracker_getSamples(ChipSample *_buf, int _len) {
  sChip->getSamples(_buf, _len);
}

void tracker_songMoveLeft() {
  do {
    if (sSongX == 0) {
      if (sSelectedChannel == 0) {
        sSelectedChannel = sChip->getNumChannels() - 1;
      } else {
        sSelectedChannel--;
      }
      sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
      sSongX = sChip->getNumSongDataColumns(sSelectedChannel) - 1;
    } else {
      sSongX--;
    }
  } while (sChip->getSongDataType(sSongY, sSelectedChannel, sSongX) == CDT_LABEL);
}

void tracker_songMoveRight() {
  do {
    sSongX++;
    if (sSongX == sChip->getNumSongDataColumns(sSelectedChannel)) {
      sSongX = 0;
      sSelectedChannel++;
      if (sSelectedChannel == sChip->getNumChannels()) {
        sSelectedChannel = 0;
      }
      sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
    }
  } while (sChip->getSongDataType(sSongY, sSelectedChannel, sSongX) == CDT_LABEL);
}

void tracker_songMoveDown() {
  sSongY++;
  if (sSongY == sChip->getNumSongRows()) {
    sSongY = 0;
  }
}

void tracker_patternMoveLeft() {
  do {
    if (sPatternX == 0) {
      if (sSelectedChannel == 0) {
        sSelectedChannel = sChip->getNumChannels() - 1;
      } else {
        sSelectedChannel--;
      }
      sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
      sPatternX = sChip->getNumPatternDataColumns(sSelectedChannel, sSelectedPattern, sPatternY) - 1;
    } else {
      sPatternX--;
    }
  } while (sChip->getPatternDataType(sSelectedChannel, sSelectedPattern, sPatternY, sPatternX) == CDT_LABEL);
}

void tracker_patternMoveRight() {
  do {
    sPatternX++;
    if (sPatternX == sChip->getNumPatternDataColumns(sSelectedChannel, sSelectedPattern, sPatternY)) {
      sPatternX = 0;
      sSelectedChannel++;
      if (sSelectedChannel == sChip->getNumChannels()) {
        sSelectedChannel = 0;
      }
      sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
    }
  } while (sChip->getPatternDataType(sSelectedChannel, sSelectedPattern, sPatternY, sPatternX) == CDT_LABEL);
}

void tracker_instrumentMoveLeft() {
  do {
    if (sInstrumentX == 0) {
      if (sInstrumentParam == 0) {
        sInstrumentParam = sChip->getNumInstrumentParams(sSelectedInstrument) - 1;
      } else {
        sInstrumentParam--;
      }
      sInstrumentX = sChip->getNumInstrumentData(sSelectedInstrument, sInstrumentParam, sInstrumentY) - 1;
    } else {
      sInstrumentX--;
    }
  } while (sChip->getInstrumentDataType(sSelectedInstrument,
                                        sInstrumentParam,
                                        sInstrumentY,
                                        sInstrumentX) == CDT_LABEL);
}

void tracker_instrumentMoveRight() {
  do {
    sInstrumentX++;
    if (sInstrumentX == sChip->getNumInstrumentData(sSelectedInstrument, sInstrumentParam, sInstrumentY)) {
      sInstrumentX = 0;
      sInstrumentParam++;
      if (sInstrumentParam == sChip->getNumInstrumentParams(sSelectedInstrument)) {
        sInstrumentParam = 0;
      }
    }
  } while (sChip->getInstrumentDataType(sSelectedInstrument, sInstrumentParam, sInstrumentY,
                                        sInstrumentX) == CDT_LABEL);
}

void tracker_instrumentMoveUp() {
  int c = sChip->getInstrumentLen(sSelectedInstrument);
  sInstrumentY = (sInstrumentY + c - 1) % c;
  int numDataColumns = sChip->getNumInstrumentData(sSelectedInstrument, sInstrumentParam, sInstrumentY);
  if (sInstrumentX >= numDataColumns) {
    sInstrumentX = numDataColumns - 1;
  }
}

bool tracker_textEditKey(TrackerTextEditKey _key) {
  if ((sTrackerState & TRACKER_EDIT_ANY) != 0) {
    return TextEdit_handleEditKey(&sTextEditRoot_Editor, _key);
  }
  return false;
}

bool tracker_asciiKey(int _key) {
  if ((sTrackerState & TRACKER_EDIT_ANY) != 0) {
    if (TextEdit_handleAsciiKey(&sTextEditRoot_Editor, _key)) {
      return true;
    }
  }
  switch (sTrackerState) {
    case TRACKER_EDIT_SONG:
      if (!sbEditing) {
        return false;
      }
      if (sChip->getSongDataType(sSongY, sSelectedChannel, sSongX) == CDT_ASCII) {
        sChip->setSongData(sSongY, sSelectedChannel, sSongX, _key);
        sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
        tracker_songMoveRight();
        return true;
      }
      break;
    case TRACKER_EDIT_PATTERN:
      if (!sbEditing) {
        return false;
      }
      if (sChip->getPatternDataType(sSelectedChannel, sSelectedPattern, sPatternY, sPatternX) == CDT_ASCII) {
        sChip->setPatternData(sSelectedChannel, sSelectedPattern, sPatternY, sPatternX, sSelectedInstrument,
                              _key);

        // tracker_patternMoveRight();
        sPatternY++;
        if (sPatternY == sChip->getPatternLen(sSelectedPattern)) {
          sPatternY = 0;
        }
        return true;
      }
      break;
    case TRACKER_EDIT_INSTRUMENT:
      if (!sbEditing) {
        return false;
      }
      if (sChip->getInstrumentDataType(sSelectedInstrument,
                                       sInstrumentParam,
                                       sInstrumentY,
                                       sInstrumentX) == CDT_ASCII) {
        if (sChip->setInstrumentData(sSelectedInstrument,
                                     sInstrumentParam,
                                     sInstrumentY,
                                     sInstrumentX,
                                     _key)) {
          // tracker_instrumentMoveRight();
          return true;
        }
      }
      break;
    default: break;
  } /* switch */
  return false;
}   /* tracker_asciiKey */

bool tracker_pianoKey(int _key, bool _isRepeat, bool _isDown) {
  // Adjust for keyboard octaves
  if (_key > 16) {
    _key -= 5;
  }
  int note;
  if (_key == 0) {
    note = 255;
  } else {
    note = sOctave * 12 + _key - 1;
  }
  if (!_isRepeat) {
    sChip->plonk(note, sSelectedChannel, sSelectedInstrument, _isDown);
    sPlonkNote = note;
    if (!_isDown) {
      return true;
    }
  }
  switch (sTrackerState) {
    case TRACKER_EDIT_SONG:
      if (!sbEditing) {
        return false;
      }
      if (sChip->getSongDataType(sSongY, sSelectedChannel, sSongX) == CDT_NOTE) {
        sChip->setSongData(sSongY, sSelectedChannel, sSongX, note);
        sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
        return true;
      }
      break;
    case TRACKER_EDIT_PATTERN:
      if (!sbEditing) {
        return false;
      }
      if (sChip->getPatternDataType(sSelectedChannel, sSelectedPattern, sPatternY, sPatternX) == CDT_NOTE) {
        sChip->setPatternData(sSelectedChannel, sSelectedPattern, sPatternY, sPatternX, sSelectedInstrument,
                              note);
        sPatternY++;
        if (sPatternY == sChip->getPatternLen(sSelectedPattern)) {
          sPatternY = 0;
        }
        return true;
      }
      break;
    case TRACKER_EDIT_INSTRUMENT:
      if (!sbEditing) {
        return false;
      }
      if (sChip->getInstrumentDataType(sSelectedInstrument,
                                       sInstrumentParam,
                                       sInstrumentY,
                                       sInstrumentX) == CDT_NOTE) {
        sChip->setInstrumentData(sSelectedInstrument, sInstrumentParam, sInstrumentY, sInstrumentX, note);

        // tracker_instrumentMoveRight();
        return true;
      }
      break;
    default: break;
  } /* switch */
  return false;
}   /* tracker_pianoKey */

bool tracker_hexKey(int _hex) {
  switch (sTrackerState) {
    case TRACKER_EDIT_SONG:
      if (!sbEditing) {
        return false;
      }
      if (sChip->getSongDataType(sSongY, sSelectedChannel, sSongX) == CDT_HEX) {
        sChip->setSongData(sSongY, sSelectedChannel, sSongX, _hex);
        sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
        // TODO: Make this an option
        // tracker_songMoveRight();
        tracker_songMoveDown();
        return true;
      }
      break;
    case TRACKER_EDIT_PATTERN:
      if (!sbEditing) {
        return false;
      }
      if (sChip->getPatternDataType(sSelectedChannel,
                                    sSelectedPattern,
                                    sPatternY,
                                    sPatternX) == CDT_HEX) {
        sChip->setPatternData(sSelectedChannel,
                              sSelectedPattern,
                              sPatternY,
                              sPatternX,
                              sSelectedInstrument,
                              _hex);

        // tracker_patternMoveRight();
        sPatternY++;
        if (sPatternY == sChip->getPatternLen(sSelectedPattern)) {
          sPatternY = 0;
        }
        return true;
      }
      break;
    case TRACKER_EDIT_INSTRUMENT:
      if (!sbEditing) {
        return false;
      }
      if (sChip->getInstrumentDataType(sSelectedInstrument,
                                       sInstrumentParam,
                                       sInstrumentY,
                                       sInstrumentX) == CDT_HEX) {
        sChip->setInstrumentData(sSelectedInstrument, sInstrumentParam, sInstrumentY, sInstrumentX, _hex);
        if ((sInstrumentX < sChip->getNumInstrumentData(sSelectedInstrument,
                                                        sInstrumentParam,
                                                        sInstrumentY) - 1) &&
            (sChip->getInstrumentDataType(sSelectedInstrument,
                                          sInstrumentParam,
                                          sInstrumentY,
                                          sInstrumentX + 1) == CDT_HEX)) {
          tracker_instrumentMoveRight();
        }
        return true;
      }
      break;

    case TRACKER_EDIT_TABLE:
      if (!sbEditing) {
        return false;
      }
      sChip->setTableData(sSelectedTableKind,
                          sSelectedTable[sSelectedTableKind],
                          sTableX,
                          _hex);
      if (sTableX < sChip->getTableDataLen(sSelectedTableKind, sSelectedTable[sSelectedTableKind]) - 1) {
        sTableX++;
      } else {
        sTableX = 0;
      }
    default: break;
  } /* switch */
  return false;
}   /* tracker_hexKey */

void tracker_mouseDownAt(int _x, int _y) {
  // Handle special cases
  if (rectHit(sTableRect, _x, _y)) {
    int x = _x - sTableRect.x1;
    int y = sTableRect.y2 - _y;
    if (sbEditing) {
      sTrackerState = TRACKER_EDIT_TABLE;
      if (x < sChip->getTableDataLen(sSelectedTableKind, sSelectedTable[sSelectedTableKind])) {
        sTableX = x;
        sChip->setTableData(sSelectedTableKind,
                            sSelectedTable[sSelectedTableKind],
                            x,
                            y);
      }
    }
    return;
  }
  // Handle Hits
  for (size_t i = 0; i < sHitPos; i++) {
    Hit hit = sHits[i];
    if (rectHit(hit.hitRect, _x, _y)) {
      if (hit.textEdit != NULL) {
        TextEdit_handleHit(hit.textEdit, _x);
      }
      if (hit.action != ACTION_NONE) {
        tracker_action(hit.action, hit.actionTrackerState);
      }
      if (hit.trackerState != TRACKER_STATE_NONE) {
        sTrackerState = hit.trackerState;
      }
      if (hit.songX >= 0) {
        sSongX = hit.songX;
      }
      if (hit.songY >= 0) {
        sSongY = hit.songY;
      }
      if (hit.selectedChannel >= 0) {
        sSelectedChannel = hit.selectedChannel;
      }
      if (hit.patternX >= 0) {
        sPatternX = hit.patternX;
      }
      if (hit.patternY >= 0) {
        sPatternY = hit.patternY;
      }
      if (hit.selectedPattern >= 0) {
        sSelectedPattern = hit.selectedPattern;
      }
      if (hit.instrumentX >= 0) {
        sInstrumentX = hit.instrumentX;
      }
      if (hit.instrumentY >= 0) {
        sInstrumentY = hit.instrumentY;
      }
      if (hit.instrumentParam >= 0) {
        sInstrumentParam = hit.instrumentParam;
      }
      if (hit.selectedInstrument >= 0) {
        sSelectedInstrument = hit.selectedInstrument;
      }
      if (hit.selectedTableKind >= 0) {
        sSelectedTableKind = hit.selectedTableKind;
      }
      if (hit.tableX >= 0) {
        sTableX = hit.tableX;
      }
    }
  }
} /* tracker_mouseDownAt */

u32 tracker_preferredScreenWidth() {
  u32 w = 0;
  u32 h = 0;
  sChip->preferredWindowSize(&w, &h);
  return w;
}

u32 tracker_preferredScreenHeight() {
  u32 w = 0;
  u32 h = 0;
  sChip->preferredWindowSize(&w, &h);
  return h;
}

// ACTIONS START HERE

ACTION(ACTION_MOVE_RIGHT, TRACKER_EDIT_SONG) {
  tracker_songMoveRight();
}

ACTION(ACTION_MOVE_LEFT, TRACKER_EDIT_SONG) {
  tracker_songMoveLeft();
}

ACTION(ACTION_MOVE_DOWN, TRACKER_EDIT_SONG) {
  sSongY = (sSongY + 1) % sChip->getNumSongRows();
  sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
}

ACTION(ACTION_MOVE_UP, TRACKER_EDIT_SONG) {
  int c = sChip->getNumSongRows();
  sSongY = (sSongY + c - 1) % c;
  sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
}

ACTION(ACTION_NEXT_CHANNEL, TRACKER_EDIT_SONG) {
  sSelectedChannel++;
  if (sSelectedChannel == sChip->getNumChannels()) {
    sSelectedChannel = 0;
  }
  sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
}

ACTION(ACTION_PREV_CHANNEL, TRACKER_EDIT_SONG) {
  if (sSelectedChannel == 0) {
    sSelectedChannel = sChip->getNumChannels() - 1;
  } else {
    sSelectedChannel--;
  }
  sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
}

ACTION(ACTION_MOVE_RIGHT, TRACKER_EDIT_PATTERN) {
  tracker_patternMoveRight();
}

ACTION(ACTION_MOVE_LEFT, TRACKER_EDIT_PATTERN) {
  tracker_patternMoveLeft();
}

ACTION(ACTION_MOVE_DOWN, TRACKER_EDIT_PATTERN) {
  sPatternY = (sPatternY + 1) % sChip->getPatternLen(sSelectedPattern);
}

ACTION(ACTION_MOVE_UP, TRACKER_EDIT_PATTERN) {
  int c = sChip->getPatternLen(sSelectedPattern);
  sPatternY = (sPatternY + c - 1) % c;
}

ACTION(ACTION_NEXT_SECTION, TRACKER_EDIT_ANY) {
  switch (sTrackerState) {
    case TRACKER_EDIT_SONG: {
      sTrackerState = TRACKER_EDIT_PATTERN;
      break;
    }
    case TRACKER_EDIT_PATTERN: {
      sTrackerState = TRACKER_EDIT_INSTRUMENT;
      break;
    }
    case TRACKER_EDIT_INSTRUMENT: {
      if (sChip->useTables()) {
        sTrackerState = TRACKER_EDIT_TABLE;
      } else {
        sTrackerState = TRACKER_EDIT_META_DATA;
      }
      break;
    }
    case TRACKER_EDIT_TABLE: {
      sTrackerState = TRACKER_EDIT_META_DATA;
      break;
    }
    case TRACKER_EDIT_META_DATA: {
      sTrackerState = TRACKER_EDIT_SONG;
      break;
    }
    default: break;
  }
}

ACTION(ACTION_PREV_SECTION, TRACKER_EDIT_ANY) {
  switch (sTrackerState) {
    case TRACKER_EDIT_SONG:sTrackerState = TRACKER_EDIT_META_DATA;
      break;
    case TRACKER_EDIT_META_DATA:
      if (sChip->useTables()) {
        sTrackerState = TRACKER_EDIT_TABLE;
      } else {
        sTrackerState = TRACKER_EDIT_INSTRUMENT;
      }
      break;
    case TRACKER_EDIT_PATTERN: sTrackerState = TRACKER_EDIT_SONG;
      break;
    case TRACKER_EDIT_INSTRUMENT: sTrackerState = TRACKER_EDIT_PATTERN;
      break;
    case TRACKER_EDIT_TABLE: sTrackerState = TRACKER_EDIT_INSTRUMENT;
      break;
    default: break;
  }
}

ACTION(ACTION_NEXT_CHANNEL, TRACKER_EDIT_PATTERN) {
  sSelectedChannel++;
  if (sSelectedChannel == sChip->getNumChannels()) {
    sSelectedChannel = 0;
  }
  sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
}

ACTION(ACTION_PREV_CHANNEL, TRACKER_EDIT_PATTERN) {
  if (sSelectedChannel == 0) {
    sSelectedChannel = sChip->getNumChannels() - 1;
  } else {
    sSelectedChannel--;
  }
  sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
}

ACTION(ACTION_PLAY_STOP_SONG, TRACKER_EDIT_ANY) {
  if (sChip->isPlaying()) {
    sChip->stop();
  } else {
    sChip->stop();
    sChip->playSongFrom(sSongY, sSongX, sPatternY, sPatternX);
  }
}

ACTION(ACTION_PLAY_STOP_PATTERN, TRACKER_EDIT_ANY) {
  if (sChip->isPlaying()) {
    sChip->stop();
  } else {
    sChip->stop();
    sChip->playPatternFrom(sSongY, sSongX, sPatternY, sPatternX);
  }
}

ACTION(ACTION_MOVE_LEFT, TRACKER_EDIT_INSTRUMENT) {
  tracker_instrumentMoveLeft();
}

ACTION(ACTION_MOVE_RIGHT, TRACKER_EDIT_INSTRUMENT) {
  tracker_instrumentMoveRight();
}

ACTION(ACTION_MOVE_UP, TRACKER_EDIT_INSTRUMENT) {
  if (sInstrumentY == 0) {
    TextEdit_activate(sTEInstrumentName);
    return;
  }
  tracker_instrumentMoveUp();
}

ACTION(ACTION_MOVE_DOWN, TRACKER_EDIT_INSTRUMENT) {
  sInstrumentY = (sInstrumentY + 1) % sChip->getInstrumentLen(sSelectedInstrument);
  int numDataColumns = sChip->getNumInstrumentData(sSelectedInstrument, sInstrumentParam, sInstrumentY);
  if (sInstrumentX >= numDataColumns) {
    sInstrumentX = numDataColumns - 1;
  }
}

ACTION(ACTION_NEXT_INSTRUMENT_PARAM, TRACKER_EDIT_INSTRUMENT) {
  bool isLabel = true;
  do {
    sInstrumentParam++;
    if (sInstrumentParam == sChip->getNumInstrumentParams(sSelectedInstrument)) {
      sInstrumentParam = 0;
    }
    int numDataColumns = sChip->getNumInstrumentData(sSelectedInstrument, sInstrumentParam, sInstrumentY);
    if (sInstrumentX >= numDataColumns) {
      sInstrumentX = numDataColumns - 1;
    }
    for (int i = 0; i < numDataColumns; i++) {
      if (sChip->getInstrumentDataType(sSelectedInstrument, sInstrumentParam, sInstrumentY, i) != CDT_LABEL) {
        isLabel = false;
        break;
      }
    }
  } while (isLabel);
}

ACTION(ACTION_PREV_INSTRUMENT_PARAM, TRACKER_EDIT_INSTRUMENT) {
  bool isLabel = true;
  do {
    if (sInstrumentParam == 0) {
      sInstrumentParam = sChip->getNumInstrumentParams(sSelectedInstrument) - 1;
    } else {
      sInstrumentParam--;
    }
    int numDataColumns = sChip->getNumInstrumentData(sSelectedInstrument, sInstrumentParam, sInstrumentY);
    if (sInstrumentX >= numDataColumns) {
      sInstrumentX = numDataColumns - 1;
    }
    for (int i = 0; i < numDataColumns; i++) {
      if (sChip->getInstrumentDataType(sSelectedInstrument, sInstrumentParam, sInstrumentY, i) != CDT_LABEL) {
        isLabel = false;
        break;
      }
    }
  } while (isLabel);
}

void writeLEu32(FILE *f, u32 val) {
  fputc(GETBYTE(val, 0), f);
  fputc(GETBYTE(val, 1), f);
  fputc(GETBYTE(val, 2), f);
  fputc(GETBYTE(val, 3), f);
}

void writeLEu16(FILE *f, u32 val) {
  fputc(GETBYTE(val, 0), f);
  fputc(GETBYTE(val, 1), f);
}

ACTION(ACTION_WAV_EXPORT, TRACKER_EDIT_ANY) {
  // .wav export
  // TODO: This currently does not handle endless songs!!
  char filename[1024];
  snprintf(filename, sizeof(filename), "%s.wav", sFilename);
  con_msgf("EXPORTING .WAV TO: %s...\n", filename);
  con_pauseAudio();
  // Flush the audio channel
  for (size_t i = 0; i < (2 * 44100); i++) {
    ChipSample samp;
    sChip->getSamples(&samp, 1);
  }
  // Measure song length;
  u32 length = 0;
  sChip->playSongFrom(0, 0, 0, 0);
  while (sChip->isPlaying()) {
    ChipSample samp;
    sChip->getSamples(&samp, 1);
    length += sizeof(ChipSample);
  }
/*
  // 2 Second "silence" after song finishes
  for(size_t i = 0; i < (2 * 44100); i++) {
    ChipSample samp;
    sChip->getSamples(&samp, 1);
    length += sizeof(ChipSample);
  }
*/

  // Write wave header
  FILE *f;
  f = fopen(filename, "w");
  if (!f) {
    con_msgf("EXPORT ERROR!\n");
    return;
  }
  fprintf(f, "RIFF");
  writeLEu32(f, 36 + length);
  fprintf(f, "WAVE");
  fprintf(f, "fmt ");
  writeLEu32(f, 16);
  writeLEu16(f, 1);             // PCM
  writeLEu16(f, 2);             // Stereo
  writeLEu32(f, 44100);         // Sample Rate
  writeLEu32(f, 44100 * 2 * 2); // Byte rate
  writeLEu16(f, 2 * 2);         // Block Align
  writeLEu16(f, 16);            // Bits per sample
  fprintf(f, "data");
  writeLEu32(f, length);

  // Write wav data
  sChip->playSongFrom(0, 0, 0, 0);
  for (size_t i = 0; i < length / 4; i++) {
    ChipSample s;
    sChip->getSamples(&s, 1);
    writeLEu16(f, s.left);
    writeLEu16(f, s.right);
  }
  // Finished
  fclose(f);
  sChip->silence();
  // Flush the audio channel
  for (size_t i = 0; i < (2 * 44100); i++) {
    ChipSample samp;
    sChip->getSamples(&samp, 1);
  }
  con_resumeAudio();
  con_msg("DONE.");
}

ACTION(ACTION_SAVE, TRACKER_EDIT_ANY) {
  con_msgf("SAVING TO %s...\n", sFilename);
  sChip->saveSong(sFilename);
  con_msg("DONE.");
}

ACTION(ACTION_PREV_INSTRUMENT, TRACKER_EDIT_ANY) {
  sSelectedInstrument--;
  if (sSelectedInstrument < 0) {
    sSelectedInstrument = sChip->getNumInstruments() - 1;
  }
  sInstrumentY = 0;
}

ACTION(ACTION_NEXT_INSTRUMENT, TRACKER_EDIT_ANY) {
  sSelectedInstrument++;
  if (sSelectedInstrument == sChip->getNumInstruments()) {
    sSelectedInstrument = 0;
  }
  sInstrumentY = 0;
}

ACTION(ACTION_INSERT, TRACKER_EDIT_ANY) {
  if (!sbEditing) {
    return;
  }
  switch (sTrackerState) {
    case TRACKER_EDIT_SONG: con_error(sChip->insertSongRow(sSelectedChannel, sSongY));
      break;
    case TRACKER_EDIT_PATTERN: con_error(sChip->insertPatternRow(sSelectedChannel, sSelectedPattern, sPatternY));
      break;
    case TRACKER_EDIT_INSTRUMENT: con_error(sChip->insertInstrumentRow(sSelectedInstrument, sInstrumentY));
      break;
    case TRACKER_EDIT_TABLE:
      con_error(sChip->insertTableColumn(sSelectedTableKind,
                                         sSelectedTable[sSelectedTableKind],
                                         sTableX));
      break;
    default: break;
  }
}

ACTION(ACTION_ADD, TRACKER_EDIT_ANY) {
  if (!sbEditing) {
    return;
  }
  switch (sTrackerState) {
    case TRACKER_EDIT_SONG: con_error(sChip->addSongRow());
      break;
    case TRACKER_EDIT_PATTERN: con_error(sChip->addPatternRow(sSelectedChannel, sSelectedPattern));
      break;
    case TRACKER_EDIT_INSTRUMENT: con_error(sChip->addInstrumentRow(sSelectedInstrument));
      break;
    case TRACKER_EDIT_TABLE:
      con_error(sChip->addTableColumn(sSelectedTableKind,
                                      sSelectedTable[sSelectedTableKind]));
      break;
    default: break;
  }
}

ACTION(ACTION_DELETE, TRACKER_EDIT_ANY) {
  if (!sbEditing) {
    return;
  }
  switch (sTrackerState) {
    case TRACKER_EDIT_SONG: con_error(sChip->deleteSongRow(sSelectedChannel, sSongY));
      if (sSongY >= sChip->getNumSongRows()) {
        sSongY = sChip->getNumSongRows() - 1;
      }
      break;
    case TRACKER_EDIT_PATTERN: con_error(sChip->deletePatternRow(sSelectedChannel, sSelectedPattern, sPatternY));
      break;
    case TRACKER_EDIT_INSTRUMENT: con_error(sChip->deleteInstrumentRow(sSelectedInstrument, sInstrumentY));
      if (sInstrumentY >= sChip->getInstrumentLen(sSelectedInstrument)) {
        sInstrumentY = sChip->getInstrumentLen(sSelectedInstrument) - 1;
      }
      break;
    case TRACKER_EDIT_TABLE: {
      con_error(sChip->deleteTableColumn(sSelectedTableKind,
                                         sSelectedTable[sSelectedTableKind],
                                         sTableX));
      int len = sChip->getTableDataLen(sSelectedTableKind,
                                       sSelectedTable[sSelectedTableKind]);
      if (sTableX >= len) {
        sTableX = len - 1;
        if (sTableX < 0) {
          sTableX = 0;
        }
      }
      break;
    }
    default: break;
  }
}

ACTION(ACTION_CLEAR, TRACKER_EDIT_ANY) {
  if (!sbEditing) {
    return;
  }
  switch (sTrackerState) {
    case TRACKER_EDIT_SONG:
      sChip->clearSongData(sSongY,
                           sSelectedChannel,
                           sSongX);
      sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
      break;
    case TRACKER_EDIT_PATTERN:
      sChip->clearPatternData(sSelectedChannel,
                              sSelectedPattern,
                              sPatternY,
                              sPatternX);
      sPatternY++;
      if (sPatternY == sChip->getPatternLen(sSelectedPattern)) {
        sPatternY = 0;
      }
      break;
    case TRACKER_EDIT_INSTRUMENT:
      sChip->clearInstrumentData(sSelectedInstrument,
                                 sInstrumentParam,
                                 sInstrumentY,
                                 sInstrumentX);
      break;
    default: break;
  }
}

ACTION(ACTION_PREV_OCTAVE, TRACKER_EDIT_ANY) {
  u8 min = sChip->getMinOctave();
  if (sOctave > min) {
    sOctave--;
  }
}

ACTION(ACTION_NEXT_OCTAVE, TRACKER_EDIT_ANY) {
  u8 max = sChip->getMaxOctave();
  if (sOctave < max) {
    sOctave++;
  }
}

ACTION(ACTION_TOGGLE_EDIT, TRACKER_EDIT_ANY) {
  sChip->silence();
  sPlonkNote = 0;
  sbEditing = !sbEditing;
}

ACTION(ACTION_NEXT_PATTERN, TRACKER_EDIT_ANY) {
  if (!sbEditing) {
    return;
  }
  u16 max = sChip->getNumPatterns();
  if (sSelectedPattern < max - 1) {
    sSelectedPattern++;
    sChip->setSongPattern(sSongY, sSelectedChannel, sSelectedPattern);
  }
}

ACTION(ACTION_PREV_PATTERN, TRACKER_EDIT_ANY) {
  if (!sbEditing) {
    return;
  }
  if (sSelectedPattern > 0) {
    sSelectedPattern--;
    sChip->setSongPattern(sSongY, sSelectedChannel, sSelectedPattern);
  }
}

ACTION(ACTION_SWAP_UP, TRACKER_EDIT_INSTRUMENT) {
  if (!sbEditing || sInstrumentY == 0) {
    return;
  }
  sChip->swapInstrumentRow(sSelectedInstrument, sInstrumentY, sInstrumentY - 1);
  sInstrumentY--;
}

ACTION(ACTION_SWAP_DOWN, TRACKER_EDIT_INSTRUMENT) {
  if (!sbEditing || sInstrumentY == (sChip->getInstrumentLen(sSelectedInstrument) - 1)) {
    return;
  }
  sChip->swapInstrumentRow(sSelectedInstrument, sInstrumentY, sInstrumentY + 1);
  sInstrumentY++;
}

ACTION(ACTION_NEXT_TABLE_KIND, TRACKER_EDIT_TABLE) {
  if (sSelectedTableKind < sChip->getNumTableKinds() - 1) {
    sSelectedTableKind++;
  } else {
    sSelectedTableKind = 0;
  }
  sTableX = 0;
}

ACTION(ACTION_PREV_TABLE_KIND, TRACKER_EDIT_TABLE) {
  if (sSelectedTableKind > 0) {
    sSelectedTableKind--;
  } else {
    sSelectedTableKind = sChip->getNumTableKinds() - 1;
  }
  sTableX = 0;
}

ACTION(ACTION_NEXT_TABLE, TRACKER_EDIT_TABLE) {
  if (sSelectedTable[sSelectedTableKind] > sChip->getMinTable(sSelectedTableKind)) {
    sSelectedTable[sSelectedTableKind]--;
  } else {
    sSelectedTable[sSelectedTableKind] = sChip->getNumTables(sSelectedTableKind) - 1;
  }
  sTableX = 0;
}

ACTION(ACTION_PREV_TABLE, TRACKER_EDIT_TABLE) {
  if (sSelectedTable[sSelectedTableKind] < sChip->getNumTables(sSelectedTableKind) - 1) {
    sSelectedTable[sSelectedTableKind]++;
  } else {
    sSelectedTable[sSelectedTableKind] = sChip->getMinTable(sSelectedTableKind);
  }
  sTableX = 0;
}

ACTION(ACTION_NEXT_TABLE_COLUMN, TRACKER_EDIT_TABLE) {
  if (sTableX < sChip->getTableDataLen(sSelectedTableKind, sSelectedTable[sSelectedTableKind]) - 1) {
    sTableX++;
  } else {
    sTableX = 0;
  }
}

ACTION(ACTION_PREV_TABLE_COLUMN, TRACKER_EDIT_TABLE) {
  if (sTableX > 0) {
    sTableX--;
  } else {
    sTableX = sChip->getTableDataLen(sSelectedTableKind, sSelectedTable[sSelectedTableKind]) - 1;
    if (sTableX < 0) {
      sTableX = 0;
    }
  }
}

ACTION(ACTION_SHOW_KEYS, TRACKER_EDIT_ANY) {
  sbShowKeys = !sbShowKeys;
  con_msgf("sbShowKeys : %s", (sbShowKeys ? "true" : "false"));
}

ACTION(ACTION_MOVE_LEFT, TRACKER_EDIT_META_DATA) {
  ChipMetaDataEntry *metaData = sChip->getMetaData(sMetaDataY);
  switch (metaData->type) {
    case CMDT_OPTIONS: {
      if (metaData->value > 0) {
        metaData->value--;
      } else {
        metaData->value = metaData->max;
      }
      sChip->setMetaData(sMetaDataY, metaData);
    }
    case CMDT_STRING:break; // TODO: Implement
    case CMDT_HEX:break; // TODO: Implement
    case CMDT_DECIMAL:break; // TODO: Implement
  }
}

ACTION(ACTION_MOVE_RIGHT, TRACKER_EDIT_META_DATA) {
  ChipMetaDataEntry *metaData = sChip->getMetaData(sMetaDataY);
  switch (metaData->type) {
    case CMDT_OPTIONS: {
      if (metaData->value < metaData->max) {
        metaData->value++;
      } else {
        metaData->value = 0;
      }
      sChip->setMetaData(sMetaDataY, metaData);
    }
    case CMDT_STRING:break; // TODO: Implement
    case CMDT_HEX:break; // TODO: Implement
    case CMDT_DECIMAL:break; // TODO: Implement
  }
}

ACTION(ACTION_MOVE_UP, TRACKER_EDIT_META_DATA) {
  if (sMetaDataY > 0) {
    sMetaDataY--;
  } else {
    sMetaDataY = sChip->getNumMetaData() - 1;
  }
}

ACTION(ACTION_MOVE_DOWN, TRACKER_EDIT_META_DATA) {
  if (sMetaDataY < sChip->getNumMetaData() - 1) {
    sMetaDataY++;
  } else {
    sMetaDataY = 0;
  }
}

void tracker_action(Action _action, TrackerState _state) {
  HANDLE_ACTION(ACTION_MOVE_RIGHT, TRACKER_EDIT_SONG);
  HANDLE_ACTION(ACTION_MOVE_LEFT, TRACKER_EDIT_SONG);
  HANDLE_ACTION(ACTION_MOVE_DOWN, TRACKER_EDIT_SONG);
  HANDLE_ACTION(ACTION_MOVE_UP, TRACKER_EDIT_SONG);
  HANDLE_ACTION(ACTION_NEXT_CHANNEL, TRACKER_EDIT_SONG);
  HANDLE_ACTION(ACTION_PREV_CHANNEL, TRACKER_EDIT_SONG);
  HANDLE_ACTION(ACTION_MOVE_RIGHT, TRACKER_EDIT_PATTERN);
  HANDLE_ACTION(ACTION_MOVE_LEFT, TRACKER_EDIT_PATTERN);
  HANDLE_ACTION(ACTION_MOVE_DOWN, TRACKER_EDIT_PATTERN);
  HANDLE_ACTION(ACTION_MOVE_UP, TRACKER_EDIT_PATTERN);
  HANDLE_ACTION(ACTION_NEXT_CHANNEL, TRACKER_EDIT_PATTERN);
  HANDLE_ACTION(ACTION_PREV_CHANNEL, TRACKER_EDIT_PATTERN);
  HANDLE_ACTION(ACTION_NEXT_SECTION, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_PREV_SECTION, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_PLAY_STOP_SONG, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_PLAY_STOP_PATTERN, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_MOVE_LEFT, TRACKER_EDIT_INSTRUMENT);
  HANDLE_ACTION(ACTION_MOVE_RIGHT, TRACKER_EDIT_INSTRUMENT);
  HANDLE_ACTION(ACTION_MOVE_UP, TRACKER_EDIT_INSTRUMENT);
  HANDLE_ACTION(ACTION_MOVE_DOWN, TRACKER_EDIT_INSTRUMENT);
  HANDLE_ACTION(ACTION_NEXT_INSTRUMENT_PARAM, TRACKER_EDIT_INSTRUMENT);
  HANDLE_ACTION(ACTION_PREV_INSTRUMENT_PARAM, TRACKER_EDIT_INSTRUMENT);
  HANDLE_ACTION(ACTION_WAV_EXPORT, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_SAVE, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_PREV_INSTRUMENT, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_NEXT_INSTRUMENT, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_INSERT, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_DELETE, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_ADD, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_CLEAR, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_PREV_OCTAVE, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_NEXT_OCTAVE, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_TOGGLE_EDIT, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_NEXT_PATTERN, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_PREV_PATTERN, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_SWAP_UP, TRACKER_EDIT_INSTRUMENT);
  HANDLE_ACTION(ACTION_SWAP_DOWN, TRACKER_EDIT_INSTRUMENT);
  HANDLE_ACTION(ACTION_NEXT_TABLE_KIND, TRACKER_EDIT_TABLE);
  HANDLE_ACTION(ACTION_PREV_TABLE_KIND, TRACKER_EDIT_TABLE);
  HANDLE_ACTION(ACTION_NEXT_TABLE, TRACKER_EDIT_TABLE);
  HANDLE_ACTION(ACTION_PREV_TABLE, TRACKER_EDIT_TABLE);
  HANDLE_ACTION(ACTION_NEXT_TABLE_COLUMN, TRACKER_EDIT_TABLE);
  HANDLE_ACTION(ACTION_PREV_TABLE_COLUMN, TRACKER_EDIT_TABLE);
  HANDLE_ACTION(ACTION_SHOW_KEYS, TRACKER_EDIT_ANY);
  HANDLE_ACTION(ACTION_MOVE_LEFT, TRACKER_EDIT_META_DATA);
  HANDLE_ACTION(ACTION_MOVE_RIGHT, TRACKER_EDIT_META_DATA);
  HANDLE_ACTION(ACTION_MOVE_UP, TRACKER_EDIT_META_DATA);
  HANDLE_ACTION(ACTION_MOVE_DOWN, TRACKER_EDIT_META_DATA);
} /* tracker_action */

