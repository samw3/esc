
#include <strings.h>
#include "tracker.h"
#include "console.h"
#include "chip.h"
#include "err.h"

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
    Rect mHitRect;
    TrackerState mValidStates;
    Action mAction;
    TrackerState mActionTrackerState;
    TrackerState mTrackerState;
    int mSongX;
    int mSongY;
    int mSelectedChannel;
    int mPatternX;
    int mPatternY;
    int mSelectedPattern;
    int mInstrumentX;
    int mInstrumentY;
    int mInstrumentParam;
    int mSelectedInstrument;
    int mSelectedTableKind;
    int mTableX;
    TextEdit *mTextEdit;
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

    // TODO: assert(sHitPos == NUM_HITS)
    return &sHits[sHitPos - 1];
}

typedef void (*TextEditChangeCallback)(TextEdit *_te, TrackerTextEditKey _exitKey);

struct TextEdit {
    u8 mXPos;
    u8 mYPos;
    u8 mMaxWidth;
    u8 mVisibleWidth;
    int mCursorPos;
    u8 mVisibleOffset;
    char *mString;
    char *mLastString;
    bool mbEnabled;
    bool mbEllipsis;
    bool mbActive;
    TextEditChangeCallback mChangeCallback;
    TextEdit *mNext;
    TextEdit *mPrev;
};

TextEdit *TextEdit_new(TextEdit **_root, u8 _x, u8 _y, u8 _maxWidth, u8 _visibleWidth, bool _enabled,
                       bool _ellipsis, char _initialString[], TextEditChangeCallback _changeCallback) {
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
            _x, _y, _maxWidth, _visibleWidth, len, visibleOffset, buffer, lastBuffer, _enabled, _ellipsis, false,
            _changeCallback, *_root, NULL
    };
    *_root = te;
    return te;
}

void TextEdit_delete(TextEdit **_root, TextEdit *_te) {
    // Unlink
    if (_te->mPrev == NULL) {
        *_root = _te->mNext;
    } else {
        _te->mPrev->mNext = _te->mNext;
    }
    if (_te->mNext != NULL) {
        _te->mNext->mPrev = _te->mPrev;
    }
    // Dealloc
    free(_te->mString);
    free(_te);
}

void TextEdit_deleteAll(TextEdit **_root) {
    while (_root != NULL) {
        TextEdit_delete(_root, *_root);
    }
}

void TextEdit_draw(TextEdit *_te) {
    if (_te->mbActive) {
        con_gotoXY(_te->mXPos, _te->mYPos);
        Hit *hit = addHit(rectRel(_te->mVisibleWidth + 1), TRACKER_EDIT_ANY);
        hit->mTextEdit = _te;
        con_setAttrib(0x0F);
        con_putc(0xDE);

        const int len = strlen(_te->mString);
        const int start = _te->mVisibleOffset;
        const int end = _te->mVisibleOffset + _te->mVisibleWidth;
        for (size_t i = start; i < end; i++) {
            if (i < len) {
                con_setAttrib(0x07);
                con_putc(_te->mString[i]);
            } else {
                con_setAttrib(0x80);
                con_putc(0xB2);
            }
        }
        con_setAttrib(0x0F);
        con_putc(0xDD);
        const int x = 1 + _te->mXPos + _te->mCursorPos - _te->mVisibleOffset;
        const int y = _te->mYPos;
        con_setAttribXY(x, y, 0x100 | con_getAttribXY(x, y));
    } else {
        con_gotoXY(_te->mXPos, _te->mYPos);
        Hit *hit = addHit(rectRel(_te->mVisibleWidth + 1), TRACKER_EDIT_ANY);
        hit->mTextEdit = _te;
        con_setAttrib(0x08);
        con_putc(0xDE);

        const int len = strlen(_te->mString);
        const int start = 0;
        const int end = _te->mVisibleWidth;
        for (size_t i = start; i < end; i++) {
            if (i < len) {
                con_setAttrib(0x07);
                con_putc((_te->mString)[i]);
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
        _root = _root->mNext;
    }
}

void TextEdit_pos(TextEdit *_te, int _x, int _y) {
    _te->mXPos = _x;
    _te->mYPos = _y;
}

void TextEdit_width(TextEdit *_te, int _visibleWidth) {
    _te->mVisibleWidth = _visibleWidth;
    int len = strlen(_te->mLastString);
    _te->mVisibleOffset = (len < _te->mVisibleWidth) ? 0 : len - _te->mVisibleWidth + 1;
    _te->mCursorPos = len;
}

void TextEdit_set(TextEdit *_te, const char *_string) {
    strncpy(_te->mString, _string, _te->mMaxWidth);
    strncpy(_te->mLastString, _string, _te->mMaxWidth);
    int len = strlen(_te->mLastString);
    _te->mVisibleOffset = (len < _te->mVisibleWidth) ? 0 : len - _te->mVisibleWidth + 1;
    _te->mCursorPos = len;
}

const char *TextEdit_get(TextEdit *_te) {
    return _te->mLastString;
}

void TextEdit_activate(TextEdit *_te) {
    _te->mbActive = true;
    strncpy(_te->mString, _te->mLastString, _te->mMaxWidth);
}

void TextEdit_deactivate(TextEdit *_te) {
    _te->mbActive = false;
}

void TextEdit_deactivateAll(TextEdit *_someNode) {
    if (_someNode == NULL) {
        return;
    }
    // Find root
    TextEdit *node = _someNode;
    while (node->mPrev != NULL) {
        node = node->mPrev;
    }
    // Deactive all
    while (node->mNext != NULL) {
        TextEdit_deactivate(node);
        node = node->mNext;
    }
}

bool TextEdit_handleEditKey(TextEdit **_root, TrackerTextEditKey _key) {
    // Find active TextEdit else return
    TextEdit *te = *_root;
    while (te != NULL) {
        if (te->mbActive) {
            break;
        }
        te = te->mNext;
    }
    if (te == NULL) {
        return false;
    }
    // Handle key
    switch (_key) {
        case TEK_ENTER:
        case TEK_DOWN:
        case TEK_UP:

            // Save the edit back to the reference string
            strncpy(te->mLastString, te->mString, te->mMaxWidth);
            te->mChangeCallback(te, _key);
            TextEdit_deactivate(te);
            return true;

        case TEK_ESCAPE: strncpy(te->mString, te->mLastString, te->mMaxWidth);
            int len = strlen(te->mString);
            te->mVisibleOffset = (len < te->mVisibleWidth) ? 0 : len - te->mVisibleWidth + 1;
            te->mCursorPos = len;
            te->mChangeCallback(te, _key);
            TextEdit_deactivate(te);
            return true;

        case TEK_HOME: te->mCursorPos = 0;
            te->mVisibleOffset = 0;
            return true;

        case TEK_END: te->mCursorPos = strlen(te->mString);
            if (te->mCursorPos - te->mVisibleOffset > te->mVisibleWidth - 1) {
                te->mVisibleOffset = te->mCursorPos - te->mVisibleWidth;
            }
            return true;

        case TEK_RIGHT: {
            int len = strlen(te->mString);
            if (te->mCursorPos < len) {
                te->mCursorPos++;
                if (te->mCursorPos - te->mVisibleOffset > te->mVisibleWidth - 1) {
                    te->mVisibleOffset++;
                }
            }
            return true;
        }

        case TEK_LEFT:
            if (te->mCursorPos > 0) {
                te->mCursorPos--;
                if (te->mCursorPos - te->mVisibleOffset < 0) {
                    te->mVisibleOffset--;
                }
            }
            return true;

        case TEK_WORD_RIGHT: {
            int len = strlen(te->mString);
            while ((te->mString[te->mCursorPos] != ' ') && te->mCursorPos < len) {
                te->mCursorPos++;
                if (te->mCursorPos - te->mVisibleOffset > te->mVisibleWidth - 1) {
                    te->mVisibleOffset++;
                }
            }
            while ((te->mString[te->mCursorPos] == ' ') && te->mCursorPos < len) {
                te->mCursorPos++;
                if (te->mCursorPos - te->mVisibleOffset > te->mVisibleWidth - 1) {
                    te->mVisibleOffset++;
                }
            }
            return true;
        }

        case TEK_WORD_LEFT: {
            int len = strlen(te->mString);
            bool first = true;
            while ((te->mString[te->mCursorPos] == ' ' || first || te->mCursorPos == len) &&
                   te->mCursorPos > 0) {
                te->mCursorPos--;
                if (te->mCursorPos - te->mVisibleOffset < 0) {
                    te->mVisibleOffset--;
                }
                first = false;
            }
            while ((te->mString[te->mCursorPos] != ' ') &&
                   te->mCursorPos > 0) {
                te->mCursorPos--;
                if (te->mCursorPos - te->mVisibleOffset < 0) {
                    te->mVisibleOffset--;
                }
            }
            while ((te->mString[te->mCursorPos] == ' ') &&
                   te->mCursorPos < len &&
                   te->mCursorPos != 0) {
                te->mCursorPos++;
                if (te->mCursorPos - te->mVisibleOffset > te->mVisibleWidth - 1) {
                    te->mVisibleOffset++;
                }
            }
            return true;
        }

        case TEK_CLEAR: te->mString[0] = '\0';
            te->mCursorPos = 0;
            te->mVisibleOffset = 0;
            return true;

        case TEK_DELETE: {
            int len = strlen(te->mString);
            if (te->mCursorPos < len) {
                memmove(&te->mString[te->mCursorPos],
                        &te->mString[te->mCursorPos + 1],
                        len - te->mCursorPos);
            }
            return true;
        }

        case TEK_BACKSPACE: {
            int len = strlen(te->mString);
            if (te->mCursorPos > 0) {
                memmove(&te->mString[te->mCursorPos - 1],
                        &te->mString[te->mCursorPos],
                        len - te->mCursorPos + 1);
                te->mCursorPos--;
                if (te->mCursorPos - te->mVisibleOffset < 0) {
                    te->mVisibleOffset--;
                }
            }
            return true;
        }

        case TEK_SPACE: {
            int len = strlen(te->mString);
            if (len < te->mMaxWidth) {
                memmove(&te->mString[te->mCursorPos + 1],
                        &te->mString[te->mCursorPos],
                        len - te->mCursorPos + 1);
                te->mString[te->mCursorPos] = ' ';
            }
            te->mCursorPos++;
            if (te->mCursorPos - te->mVisibleOffset > te->mVisibleWidth - 1) {
                te->mVisibleOffset++;
            }
            return true;
        }

        case TEK_INSERT: {
            int len = strlen(te->mString);
            if (len < te->mMaxWidth) {
                memmove(&te->mString[te->mCursorPos + 1],
                        &te->mString[te->mCursorPos],
                        len - te->mCursorPos + 1);
                te->mString[te->mCursorPos] = ' ';
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
        if (te->mbActive) {
            break;
        }
        te = te->mNext;
    }
    if (te == NULL) {
        return false;
    }
    // Handle key
    int len = strlen(te->mString);
    if (len < te->mMaxWidth) {
        memmove(&te->mString[te->mCursorPos + 1],
                &te->mString[te->mCursorPos],
                len - te->mCursorPos + 1);
        te->mString[te->mCursorPos] = _key;
        te->mCursorPos++;
        if (te->mCursorPos - te->mVisibleOffset > te->mVisibleWidth - 1) {
            te->mVisibleOffset++;
        }
    }
    return true;
}

void TextEdit_handleHit(TextEdit *_te, int _x) {
    if (_te->mbActive) {
        _te->mCursorPos = _x - _te->mXPos + _te->mVisibleOffset - 1;
        int len = strlen(_te->mString);
        if (_te->mCursorPos > len) {
            _te->mCursorPos = len;
        }
        if (_te->mCursorPos < 0) {
            _te->mCursorPos = 0;
        }
        if (_te->mCursorPos < _te->mVisibleOffset) {
            _te->mVisibleOffset = _te->mCursorPos;
        }
        if (_te->mCursorPos - _te->mVisibleOffset > _te->mVisibleWidth) {
            _te->mVisibleOffset = _te->mCursorPos - _te->mVisibleWidth + 1;
        }
    } else {
        TextEdit_deactivateAll(_te);
        _te->mbActive = true;
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
int sSelectedInstrument = 1;
int sSelectedTableKind = 0;
u16 *sSelectedTable = NULL;
int sTableX = 0;
Rect sTableRect = {0};
TrackerState sTrackerState = TRACKER_EDIT_SONG;
bool sbEditing = true;
bool sbShowKeys = false;

void tracker_onChangeInstrumentName(TextEdit *_te, TrackerTextEditKey _exitKey) {
    sChip->setInstrumentName(sSelectedInstrument,
                             sTEInstrumentName->mLastString);
    if (_exitKey == TEK_UP) {
        tracker_instrumentMoveUp();
    }
}

void tracker_init() {
    sChip->init();
    sChip->loadSong(sFilename);
    sSelectedPattern = sChip->getPatternNum(sSongY, sSelectedChannel);
    con_msgf("ESC v0.1 (%s)", sChipName);

    int count = sChip->getNumTableKinds();
    sSelectedTable = malloc(sizeof(u16) * count);
    for (size_t i = 0; i < count; i++) {
        sSelectedTable[i] = sChip->getMinTable(i);
    }
    sTextEditRoot_Editor = NULL;

    sTEInstrumentName = TextEdit_new(&sTextEditRoot_Editor, 20, 2, 40, 16, true, false, "",
                                     tracker_onChangeInstrumentName);

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
    u8 patternLen = sChip->getPatternLen();
    u8 quarter = patternLen >> 2;

    // Draw pattern row numbers
    int heightOfRows = _height - 1;
    int halfHeightOfRows = heightOfRows / 2;
    for (int j = 0; j < heightOfRows; j++) {
        int row = sPatternY - halfHeightOfRows + j;
        if (row >= 0 && row < patternLen) {
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
            hit->mTrackerState = TRACKER_EDIT_PATTERN;
            hit->mPatternY = row;
            con_printfXY(_x, _y + 2 + j, "%02X", row);
        }
    }
    for (size_t i = 0; i < numChannels; i++) {
        int patternNum = sChip->getPatternNum(sSongY, i);

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
                    hit->mTrackerState = TRACKER_EDIT_PATTERN;
                    hit->mPatternY = row;
                    hit->mPatternX = k;
                    hit->mSelectedChannel = i;
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
    u8 patternLen = sChip->getPatternLen();
    u8 quarter = patternLen >> 2;
    // Draw pattern row numbers
    for (size_t j = 0; j < patternLen; j++) {
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
            hit->mTrackerState = TRACKER_EDIT_PATTERN;
            hit->mPatternY = j;
            con_printfXY(_x, _y + j + 2, "%02X", j);
        }
    }
    for (size_t i = 0; i < numChannels; i++) {
        int patternNum = sChip->getPatternNum(sSongY, i);

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
                    hit->mTrackerState = TRACKER_EDIT_PATTERN;
                    hit->mPatternY = j;
                    hit->mPatternX = k;
                    hit->mSelectedChannel = i;
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
                con_setAttrib(0x70);
            } else {
                con_setAttrib(0x08);
            }
            hit = addHit(rectRel(3), TRACKER_EDIT_ANY);
            hit->mInstrumentY = instRow;
            con_printf(sChip->getInstrumentLabel(sSelectedInstrument, instRow));
            con_setAttrib(0x08);
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
            if (instRow >= instOffset && instRow - instOffset < _height - 2) {
                con_gotoXY(_x + 1 + instWidth, _y + 3 + instRow - instOffset);
                int numData = sChip->getNumInstrumentData(sSelectedInstrument, param, instRow);
                for (size_t dataColumn = 0; dataColumn < numData; dataColumn++) {
                    ChipDataType type = sChip->getInstrumentDataType(sSelectedInstrument, param, instRow, dataColumn);
                    u8 data = sChip->getInstrumentData(sSelectedInstrument, param, instRow, dataColumn);
                    if (instRow == sInstrumentY) {
                        if (dataColumn == sInstrumentX &&
                            param == sInstrumentParam &&
                            sTrackerState == TRACKER_EDIT_INSTRUMENT &&
                            !sTEInstrumentName->mbActive) {
                            con_setAttrib(0x170);
                        } else {
                            con_setAttrib(0x70);
                        }
                    } else {
                        con_setAttrib(0x07);
                    }
                    switch (type) {
                        case CDT_LABEL: con_putc(data);
                            break;
                        case CDT_NOTE: hit = addHit(rectRel(3), TRACKER_EDIT_ANY);
                            hit->mTrackerState = TRACKER_EDIT_INSTRUMENT;
                            hit->mInstrumentY = instRow;
                            hit->mInstrumentX = dataColumn;
                            hit->mInstrumentParam = param;
                            tracker_drawNote(data);
                            break;
                        case CDT_HEX: hit = addHit(rectRel(1), TRACKER_EDIT_ANY);
                            hit->mTrackerState = TRACKER_EDIT_INSTRUMENT;
                            hit->mInstrumentY = instRow;
                            hit->mInstrumentX = dataColumn;
                            hit->mInstrumentParam = param;
                            con_putc(data + ((data < 16) ? (data < 10 ? 48 : 55) : 0));
                            break;
                        case CDT_ASCII: hit = addHit(rectRel(1), TRACKER_EDIT_ANY);
                            hit->mTrackerState = TRACKER_EDIT_INSTRUMENT;
                            hit->mInstrumentY = instRow;
                            hit->mInstrumentX = dataColumn;
                            hit->mInstrumentParam = param;
                            con_putc(data);
                            break;
                    }
                }
                con_setAttrib(0x07);
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
    if (!sTEInstrumentName->mbActive) {
        TextEdit_width(sTEInstrumentName, instWidth - 2);
        TextEdit_set(sTEInstrumentName,
                     sChip->getInstrumentName(sSelectedInstrument,
                                              sTEInstrumentName->mMaxWidth));
    }
    con_printfXY(_x, _y + 2, "%02X", sSelectedInstrument);

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

            // attron(A_DIM);
            hit = addHit(rectRel(3), TRACKER_EDIT_ANY);
            hit->mTrackerState = TRACKER_EDIT_SONG;
            hit->mSongY = songRow;
            if (songRow == sSongY) {
                con_setAttrib(0x4F);
            } else {
                con_setAttrib(0x08);
            }
            con_printf("%02X:", songRow);

            con_setAttrib(0x07);
            // attroff(A_DIM);
            for (int channel = 0; channel < numChannels; channel++) {
                int numCols = sChip->getNumSongDataColumns(channel);
                for (size_t dataColumn = 0; dataColumn < numCols; dataColumn++) {
                    if (songRow == sSongY) {
                        con_setAttrib(0x4F);
                        if (channel == sSelectedChannel) {
                            if (dataColumn == sSongX && sTrackerState == TRACKER_EDIT_SONG) {
                                if (sbEditing) {
                                    con_setAttrib(0x14F);
                                } else {
                                    con_setAttrib(0xF4);
                                }
                            } else {
                                con_setAttrib(0xF4);
                            }
                        }
                    }
                    ChipDataType type = sChip->getSongDataType(songRow, channel, dataColumn);
                    u8 data = sChip->getSongData(songRow, channel, dataColumn);
                    switch (type) {
                        case CDT_HEX: hit = addHit(rectRel(1), TRACKER_EDIT_ANY);
                            hit->mTrackerState = TRACKER_EDIT_SONG;
                            hit->mSongY = songRow;
                            hit->mSongX = dataColumn;
                            hit->mSelectedChannel = channel;
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
        hit->mTrackerState = TRACKER_EDIT_TABLE;
        hit->mSelectedTableKind = i;
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
        sSongY = sChip->getPlayerSongRow();
        sPatternY = sChip->getPlayerPatternRow();
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
                        tracker_drawMessages(width + 1, 1, con_rows() - 3);
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
    h->mAction = ACTION_TOGGLE_EDIT;
    h->mTrackerState = TRACKER_EDIT_ANY;

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
        con_printXY(1, y, "JAMMING");
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

bool tracker_pianoKey(int _key, bool _isRepeat) {
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
        sChip->plonk(note, sSelectedChannel, sSelectedInstrument);
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
                tracker_songMoveRight();
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
        if (rectHit(hit.mHitRect, _x, _y)) {
            if (hit.mTextEdit != NULL) {
                TextEdit_handleHit(hit.mTextEdit, _x);
            }
            if (hit.mAction != ACTION_NONE) {
                tracker_action(hit.mAction, hit.mActionTrackerState);
            }
            if (hit.mTrackerState != TRACKER_STATE_NONE) {
                sTrackerState = hit.mTrackerState;
            }
            if (hit.mSongX >= 0) {
                sSongX = hit.mSongX;
            }
            if (hit.mSongY >= 0) {
                sSongY = hit.mSongY;
            }
            if (hit.mSelectedChannel >= 0) {
                sSelectedChannel = hit.mSelectedChannel;
            }
            if (hit.mPatternX >= 0) {
                sPatternX = hit.mPatternX;
            }
            if (hit.mPatternY >= 0) {
                sPatternY = hit.mPatternY;
            }
            if (hit.mSelectedPattern >= 0) {
                sSelectedPattern = hit.mSelectedPattern;
            }
            if (hit.mInstrumentX >= 0) {
                sInstrumentX = hit.mInstrumentX;
            }
            if (hit.mInstrumentY >= 0) {
                sInstrumentY = hit.mInstrumentY;
            }
            if (hit.mInstrumentParam >= 0) {
                sInstrumentParam = hit.mInstrumentParam;
            }
            if (hit.mSelectedInstrument >= 0) {
                sSelectedInstrument = hit.mSelectedInstrument;
            }
            if (hit.mSelectedTableKind >= 0) {
                sSelectedTableKind = hit.mSelectedTableKind;
            }
            if (hit.mTableX >= 0) {
                sTableX = hit.mTableX;
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
    sPatternY = (sPatternY + 1) % sChip->getPatternLen();
}

ACTION(ACTION_MOVE_UP, TRACKER_EDIT_PATTERN) {
    int c = sChip->getPatternLen();
    sPatternY = (sPatternY + c - 1) % c;
}

ACTION(ACTION_NEXT_SECTION, TRACKER_EDIT_ANY) {
    switch (sTrackerState) {
        case TRACKER_EDIT_SONG: sTrackerState = TRACKER_EDIT_PATTERN;
            break;
        case TRACKER_EDIT_PATTERN: sTrackerState = TRACKER_EDIT_INSTRUMENT;
            break;
        case TRACKER_EDIT_INSTRUMENT:
            if (sChip->useTables()) {
                sTrackerState = TRACKER_EDIT_TABLE;
            } else {
                sTrackerState = TRACKER_EDIT_SONG;
            }
            break;
        case TRACKER_EDIT_TABLE: sTrackerState = TRACKER_EDIT_SONG;
            break;
        default: break;
    }
}

ACTION(ACTION_PREV_SECTION, TRACKER_EDIT_ANY) {
    switch (sTrackerState) {
        case TRACKER_EDIT_SONG:
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
        sChip->playSongFrom(sSongY, 0, 0, 0);
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
    sInstrumentParam++;
    if (sInstrumentParam == sChip->getNumInstrumentParams(sSelectedInstrument)) {
        sInstrumentParam = 0;
    }
    int numDataColumns = sChip->getNumInstrumentData(sSelectedInstrument, sInstrumentParam, sInstrumentY);
    if (sInstrumentX >= numDataColumns) {
        sInstrumentX = numDataColumns - 1;
    }
}

ACTION(ACTION_PREV_INSTRUMENT_PARAM, TRACKER_EDIT_INSTRUMENT) {
    if (sInstrumentParam == 0) {
        sInstrumentParam = sChip->getNumInstrumentParams(sSelectedInstrument) - 1;
    } else {
        sInstrumentParam--;
    }
    int numDataColumns = sChip->getNumInstrumentData(sSelectedInstrument, sInstrumentParam, sInstrumentY);
    if (sInstrumentX >= numDataColumns) {
        sInstrumentX = numDataColumns - 1;
    }
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
    if (sSelectedInstrument == 0) {
        sSelectedInstrument = sChip->getNumInstruments() - 1;
    }
    sInstrumentY = 0;
}

ACTION(ACTION_NEXT_INSTRUMENT, TRACKER_EDIT_ANY) {
    sSelectedInstrument++;
    if (sSelectedInstrument == sChip->getNumInstruments()) {
        sSelectedInstrument = 1;
    }
    sInstrumentY = 0;
}

ACTION(ACTION_INSERT, TRACKER_EDIT_ANY) {
    if (!sbEditing) {
        return;
    }
    switch (sTrackerState) {
        case TRACKER_EDIT_SONG: con_error(sChip->insertSongRow(sSongY));
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
        case TRACKER_EDIT_SONG: con_error(sChip->deleteSongRow(sSongY));
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
} /* tracker_action */

