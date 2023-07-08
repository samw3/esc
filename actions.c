
#include "actions.h"

ActionTableEntry actions[] = {
    {ACTION_MOVE_LEFT,             TRACKER_EDIT_SONG,       SDL_SCANCODE_LEFT,         KMOD_NONE},
    {ACTION_MOVE_RIGHT,            TRACKER_EDIT_SONG,       SDL_SCANCODE_RIGHT,        KMOD_NONE},
    {ACTION_MOVE_UP,               TRACKER_EDIT_SONG,       SDL_SCANCODE_UP,           KMOD_NONE},
    {ACTION_MOVE_DOWN,             TRACKER_EDIT_SONG,       SDL_SCANCODE_DOWN,         KMOD_NONE},
    {ACTION_NEXT_CHANNEL,          TRACKER_EDIT_SONG,       SDL_SCANCODE_TAB,          KMOD_NONE},
    {ACTION_PREV_CHANNEL,          TRACKER_EDIT_SONG,       SDL_SCANCODE_TAB,          KMOD_SHIFT},
    {ACTION_MOVE_LEFT,             TRACKER_EDIT_PATTERN,    SDL_SCANCODE_LEFT,         KMOD_NONE},
    {ACTION_MOVE_RIGHT,            TRACKER_EDIT_PATTERN,    SDL_SCANCODE_RIGHT,        KMOD_NONE},
    {ACTION_MOVE_UP,               TRACKER_EDIT_PATTERN,    SDL_SCANCODE_UP,           KMOD_NONE},
    {ACTION_MOVE_DOWN,             TRACKER_EDIT_PATTERN,    SDL_SCANCODE_DOWN,         KMOD_NONE},

    {ACTION_MOVE_LEFT,             TRACKER_EDIT_INSTRUMENT, SDL_SCANCODE_LEFT,         KMOD_NONE},
    {ACTION_MOVE_RIGHT,            TRACKER_EDIT_INSTRUMENT, SDL_SCANCODE_RIGHT,        KMOD_NONE},
    {ACTION_MOVE_UP,               TRACKER_EDIT_INSTRUMENT, SDL_SCANCODE_UP,           KMOD_NONE},
    {ACTION_MOVE_DOWN,             TRACKER_EDIT_INSTRUMENT, SDL_SCANCODE_DOWN,         KMOD_NONE},

    {ACTION_MOVE_LEFT,             TRACKER_EDIT_META_DATA,  SDL_SCANCODE_LEFT,         KMOD_NONE},
    {ACTION_MOVE_RIGHT,            TRACKER_EDIT_META_DATA,  SDL_SCANCODE_RIGHT,        KMOD_NONE},
    {ACTION_MOVE_UP,               TRACKER_EDIT_META_DATA,  SDL_SCANCODE_UP,           KMOD_NONE},
    {ACTION_MOVE_DOWN,             TRACKER_EDIT_META_DATA,  SDL_SCANCODE_DOWN,         KMOD_NONE},

    {ACTION_NEXT_INSTRUMENT_PARAM, TRACKER_EDIT_INSTRUMENT, SDL_SCANCODE_TAB,          KMOD_NONE},
    {ACTION_PREV_INSTRUMENT_PARAM, TRACKER_EDIT_INSTRUMENT, SDL_SCANCODE_TAB,          KMOD_SHIFT},

    {ACTION_NEXT_CHANNEL,          TRACKER_EDIT_PATTERN,    SDL_SCANCODE_TAB,          KMOD_NONE},
    {ACTION_PREV_CHANNEL,          TRACKER_EDIT_PATTERN,    SDL_SCANCODE_TAB,          KMOD_SHIFT},
    {ACTION_NEXT_SECTION,          TRACKER_EDIT_ANY,        SDL_SCANCODE_GRAVE,        KMOD_NONE},
    {ACTION_PREV_SECTION,          TRACKER_EDIT_ANY,        SDL_SCANCODE_GRAVE,        KMOD_SHIFT},
    {ACTION_PLAY_STOP_SONG,        TRACKER_EDIT_ANY,        SDL_SCANCODE_RETURN,       KMOD_NONE},
    {ACTION_PLAY_STOP_PATTERN,     TRACKER_EDIT_ANY,        SDL_SCANCODE_RETURN,       KMOD_SHIFT},

    {ACTION_WAV_EXPORT,            TRACKER_EDIT_ANY,        SDL_SCANCODE_W,            KMOD_SHIFT},
    {ACTION_SAVE,                  TRACKER_EDIT_ANY,        SDL_SCANCODE_S,            KMOD_SHIFT},

    {ACTION_PREV_INSTRUMENT,       TRACKER_EDIT_ANY,        SDL_SCANCODE_LEFTBRACKET,  KMOD_NONE},
    {ACTION_NEXT_INSTRUMENT,       TRACKER_EDIT_ANY,        SDL_SCANCODE_RIGHTBRACKET, KMOD_NONE},

    {ACTION_PREV_PATTERN,          TRACKER_EDIT_ANY,        SDL_SCANCODE_LEFTBRACKET,  KMOD_SHIFT},
    {ACTION_NEXT_PATTERN,          TRACKER_EDIT_ANY,        SDL_SCANCODE_RIGHTBRACKET, KMOD_SHIFT},

    {ACTION_INSERT,                TRACKER_EDIT_ANY,        SDL_SCANCODE_I,            KMOD_SHIFT},
    {ACTION_ADD,                   TRACKER_EDIT_ANY,        SDL_SCANCODE_A,            KMOD_SHIFT},
    {ACTION_DELETE,                TRACKER_EDIT_ANY,        SDL_SCANCODE_D,            KMOD_SHIFT},

    {ACTION_CLEAR,                 TRACKER_EDIT_ANY,        SDL_SCANCODE_BACKSPACE,    KMOD_NONE},

    {ACTION_PREV_OCTAVE,           TRACKER_EDIT_ANY,        SDL_SCANCODE_COMMA,        KMOD_SHIFT},
    {ACTION_NEXT_OCTAVE,           TRACKER_EDIT_ANY,        SDL_SCANCODE_PERIOD,       KMOD_SHIFT},

    {ACTION_TOGGLE_EDIT,           TRACKER_EDIT_ANY,        SDL_SCANCODE_SPACE,        KMOD_NONE},

    {ACTION_SWAP_UP,               TRACKER_EDIT_INSTRUMENT, SDL_SCANCODE_UP,           KMOD_ALT},
    {ACTION_SWAP_DOWN,             TRACKER_EDIT_INSTRUMENT, SDL_SCANCODE_DOWN,         KMOD_ALT},

    {ACTION_JUMP,                  TRACKER_EDIT_ANY,        SDL_SCANCODE_SPACE,        KMOD_CTRL},

    {ACTION_NEXT_TABLE_KIND,       TRACKER_EDIT_TABLE,      SDL_SCANCODE_TAB,          KMOD_NONE},
    {ACTION_PREV_TABLE_KIND,       TRACKER_EDIT_TABLE,      SDL_SCANCODE_TAB,          KMOD_SHIFT},
    {ACTION_NEXT_TABLE,            TRACKER_EDIT_TABLE,      SDL_SCANCODE_UP,           KMOD_NONE},
    {ACTION_PREV_TABLE,            TRACKER_EDIT_TABLE,      SDL_SCANCODE_DOWN,         KMOD_NONE},
    {ACTION_NEXT_TABLE_COLUMN,     TRACKER_EDIT_TABLE,      SDL_SCANCODE_RIGHT,        KMOD_NONE},
    {ACTION_PREV_TABLE_COLUMN,     TRACKER_EDIT_TABLE,      SDL_SCANCODE_LEFT,         KMOD_NONE},

    {ACTION_SHOW_KEYS,             TRACKER_EDIT_ANY,        SDL_SCANCODE_SLASH,        KMOD_SHIFT}
};
size_t actionsCount = sizeof(actions) / sizeof(ActionTableEntry);

// Matches Action enum
char *actionNames[] = {
    "Move Left",
    "Move Right",
    "Move Up",
    "Move Down",
    "Next Section",
    "Prev Section",
    "Next Channel",
    "Prev Channel",
    "Play Stop Song",
    "Play Stop Pattern",
    "Next Instrument Param",
    "Prev Instrument Param",
    "Wav Export",
    "Save",
    "Next Instrument",
    "Prev Instrument",
    "Insert",
    "Add",
    "Delete",
    "Clear",
    "Next Octave",
    "Prev Octave",
    "Next Pattern",
    "Prev Pattern",
    "Toggle Edit",
    "Swap Up",
    "Swap Down",
    "Jump",
    "Next Table Kind",
    "Prev Table Kind",
    "Next Table",
    "Prev Table",
    "Next Table Column",
    "Prev Table Column",
    "Show Keys"
};
