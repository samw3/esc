
#ifndef TRACKER_H
#define TRACKER_H value

#define MAJOR_VERSION 0
#define MINOR_VERSION 1

#include "chip.h"
#include "types.h"
#include "actions.h"

typedef enum {
  TEK_NONE,
  TEK_HOME,
  TEK_END,
  TEK_BACKSPACE,
  TEK_DELETE,
  TEK_CLEAR,
  TEK_INSERT,
  TEK_WORD_LEFT,
  TEK_WORD_RIGHT,
  TEK_LEFT,
  TEK_RIGHT,
  TEK_SPACE,
  TEK_ENTER,
  TEK_ESCAPE,
  TEK_UP,
  TEK_DOWN
} TrackerTextEditKey;

void tracker_init();

void tracker_destroy();

void tracker_setFilename(char *filename);

void *tracker_setChipName(char *chipName);

void tracker_drawScreen();

void tracker_getSamples(ChipSample *_buf, int _len);

bool tracker_textEditKey(TrackerTextEditKey _key);

bool tracker_asciiKey(int _key);

bool tracker_pianoKey(int _key, bool _isRepeat, bool _isDown);

bool tracker_hexKey(int _hex);

void tracker_action(Action _action, TrackerState _state);

void tracker_mouseDownAt(int _x, int _y);

void tracker_instrumentMoveUp();

u32 tracker_preferredScreenWidth();

u32 tracker_preferredScreenHeight();

#endif /* ifndef TRACKER_H */

