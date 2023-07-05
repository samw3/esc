
#include "console.h"

#define GL_GLEXT_PROTOTYPES

#include <SDL.h>
#include <SDL_opengl.h>
#include <stdarg.h>
#include <string.h>
#include <err.h>
#include "types.h"
#include "font.h"
#include "console.h"
#include "tracker.h"
#include "actions.h"

SDL_Keycode asciiKeys[] = {
    SDL_SCANCODE_0, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5,
    SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_A, SDL_SCANCODE_B,
    SDL_SCANCODE_C, SDL_SCANCODE_D, SDL_SCANCODE_E, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H,
    SDL_SCANCODE_I, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_M, SDL_SCANCODE_N,
    SDL_SCANCODE_O, SDL_SCANCODE_P, SDL_SCANCODE_Q, SDL_SCANCODE_R, SDL_SCANCODE_S, SDL_SCANCODE_T,
    SDL_SCANCODE_U, SDL_SCANCODE_V, SDL_SCANCODE_W, SDL_SCANCODE_X, SDL_SCANCODE_Y, SDL_SCANCODE_Z,
    SDL_SCANCODE_SLASH
};
size_t asciiKeysCount = sizeof(asciiKeys) / sizeof(SDL_Keycode);

SDL_Keycode pianoKeys[] = {
    SDL_SCANCODE_1, SDL_SCANCODE_Z, SDL_SCANCODE_S, SDL_SCANCODE_X, SDL_SCANCODE_D, SDL_SCANCODE_C,
    SDL_SCANCODE_V,
    SDL_SCANCODE_G, SDL_SCANCODE_B, SDL_SCANCODE_H, SDL_SCANCODE_N, SDL_SCANCODE_J, SDL_SCANCODE_M,
    SDL_SCANCODE_COMMA, SDL_SCANCODE_L, SDL_SCANCODE_PERIOD, SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_SLASH,
    SDL_SCANCODE_Q, SDL_SCANCODE_2, SDL_SCANCODE_W, SDL_SCANCODE_3, SDL_SCANCODE_E, SDL_SCANCODE_R,
    SDL_SCANCODE_5, SDL_SCANCODE_T, SDL_SCANCODE_6, SDL_SCANCODE_Y, SDL_SCANCODE_7, SDL_SCANCODE_U,
    SDL_SCANCODE_I, SDL_SCANCODE_9, SDL_SCANCODE_O, SDL_SCANCODE_0, SDL_SCANCODE_P
};
size_t pianoKeysCount = sizeof(pianoKeys) / sizeof(SDL_Keycode);

SDL_Keycode hexKeys[] = {
    SDL_SCANCODE_0, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5,
    SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_A, SDL_SCANCODE_B,
    SDL_SCANCODE_C, SDL_SCANCODE_D, SDL_SCANCODE_E, SDL_SCANCODE_F
};

// Main loop flag
bool quit = false;

// The window we'll be rendering to
SDL_Window *gWindow = NULL;

// OpenGL context
SDL_GLContext gContext;
static const GLubyte sPalette[] = {
    0x16, 0x16, 0x16, // 0 Black
    0x99, 0x16, 0x16, // 1 Red
    0x16, 0x99, 0x16, // 2 Green
    0x99, 0x55, 0x16, // 3 Yellow
    0x16, 0x16, 0x99, // 4 Blue
    0x99, 0x16, 0x99, // 5 Magenta
    0x16, 0x99, 0x99, // 6 Cyan
    0x99, 0x99, 0x99, // 7 White
    0x55, 0x55, 0x55, // 8 Br. Black
    0xff, 0x55, 0x55, // 9 Br. Red
    0x55, 0xff, 0x55, // A Br. Green
    0xff, 0xff, 0x55, // B Br. Yellow
    0x55, 0x55, 0xff, // C Br. Blue
    0xff, 0x55, 0xff, // D Br. Magenta
    0x55, 0xff, 0xff, // E Br. Cyan
    0xff, 0xff, 0xff, // F Br. White
};

#define BLINK_SPEED (15)

static GLboolean sDirty = GL_TRUE;
static u32 *sChars = NULL;
static char *sPrintFBuffer = NULL;
static GLubyte *sTexture;
static int sCharsXPos = 0;
static int sCharsYPos = 0;
static u32 sLastAttrib = (128 + 31) << 8;
static GLuint *sIndexBuffer = NULL;
static GLfloat *sVertexBuffer = NULL;
static GLshort *sTextureCoordBuffer = NULL;
static GLubyte *sFgColorBuffer = NULL;
static GLubyte *sBgColorBuffer = NULL;
GLuint sIndexHandle;
GLuint sVertexHandle;
GLuint sTextureCoordHandle;
GLuint sFgColorHandle;
GLuint sBgColorHandle;
static int sBlinkTimer = BLINK_SPEED;
static GLboolean sBlinkState = GL_FALSE;
static int sScreenWidth = STARTING_SCREEN_WIDTH;
static int sScreenHeight = STARTING_SCREEN_HEIGHT;
static int sActualScreenWidth, sActualScreenHeight;
static int sScreenOffsetX, sScreenOffsetY;
char sMessages[NUM_MESSAGES][256];
int sMessagesBottom = NUM_MESSAGES - 1;
int sMessagesPos = NUM_MESSAGES - 1;

int _error(const char *_msg) {
  printf("%s\n", _msg);
  return -1;
}

/**
 *  Adds a message to the console buffer
 */
void con_msg(char *_message) {
  if (_message == NULL) {
    return;
  }
  strncpy(sMessages[sMessagesBottom], _message, 255);
  sMessages[sMessagesBottom][255] = '\0';
  sMessagesBottom = (sMessagesBottom - 1 + NUM_MESSAGES) % NUM_MESSAGES;
}

/**
 *  Adds a ChipError message to the console buffer
 */
void con_error(ChipError _error) {
  con_msg((char *) _error);
}

/**
 * Printf's a message to the console buffer
 */
void con_msgf(const char *_format, ...) {
  va_list args;
  va_start(args, _format);
  vsnprintf(sMessages[sMessagesBottom], 255, _format, args);
  va_end(args);
  sMessagesBottom = (sMessagesBottom - 1 + NUM_MESSAGES) % NUM_MESSAGES;
}

/**
 *  Gets the most recent message
 */
char *con_getMostRecentMessage() {
  sMessagesPos = sMessagesBottom + 1;
  return sMessages[sMessagesPos];
}

/**
 *  Get the next recent message
 */
char *con_getNextMessage() {
  sMessagesPos = (sMessagesPos + 1) % NUM_MESSAGES;
  return sMessages[sMessagesPos];
}

/**
 *  Gets the number of columns allocated for the screen
 */
int con_columns() {
  return sScreenWidth / 8;
}

/**
 *  Gets the number of rows allocated for the screen
 */
int con_rows() {
  return sScreenHeight / 16;
}

/**
 *  Gets the area of the screen in chars
 */
int con_area() {
  return con_columns() * con_rows();
}

/**
*  Sets the default color attribute used.
*  bits 0-3 = FG Color
*  bits 4-6 = BG Color
*  bit 7 = Blink
*/
void con_setAttrib(int _attrib) {
  sLastAttrib = _attrib << 8;
}

/**
* Gets the attribute at the specified location
*/
u32 con_getAttribXY(int _col, int _row) {
  if (_col >= con_columns()) {
    return 0;
  }
  if (_row >= con_rows()) {
    return 0;
  }
  int pos = _row * con_columns() + _col;
  if (pos >= con_area() || pos < 0) {
    return 0;
  }
  return sChars[pos] >> 8;
}

/**
* Sets the attribute at the specified location
*/
void con_setAttribXY(int _col, int _row, int _attrib) {
  if (_col >= con_columns()) {
    return;
  }
  if (_row >= con_rows()) {
    return;
  }
  int pos = _row * con_columns() + _col;
  if (pos >= con_area() || pos < 0) {
    return;
  }
  sChars[pos] = (sChars[pos] & 255) | (_attrib << 8);
  sDirty = GL_TRUE;
}

/**
* clears the screen
*/
void con_cls() {
  for (int i = 0; i < con_area(); i++) {
    sChars[i] = 7 << 8 | 32;
  }
  con_setAttrib(7);
}

/**
* Fills the screen with the attribute and char
*/
void con_fill(int _attrib, char _char) {
  for (int i = 0; i < con_area(); i++) {
    sChars[i] = _attrib << 8 | (_char & 0xff);
  }
}

/**
* Moves the cursor to the specified location
*/
void con_gotoXY(int _col, int _row) {
  if (_col < 0) {
    return;
  }
  if (_row < 0) {
    return;
  }
  if (_col * _row > con_area()) {
    return;
  }
  sCharsXPos = _col;
  sCharsYPos = _row;
}

/**
* Reports the current X position of the cursor
*/
int con_x() {
  return sCharsXPos;
}

/**
* Reports the current Y position of the cursor
*/
int con_y() {
  return sCharsYPos;
}

/**
 * Print a formatted string at the specified location
 */
void con_printfXY(int _col, int _row, const char *_format, ...) {
  char buffer[con_area()];
  va_list args;

  va_start(args, _format);
  vsnprintf(buffer, con_area(), _format, args);
  con_gotoXY(_col, _row);
  con_print(buffer);
  va_end(args);
}

/**
* Print a formatted string
*/
void con_printf(const char *_format, ...) {
  char buffer[con_area()];
  va_list args;

  va_start(args, _format);
  vsnprintf(buffer, con_area(), _format, args);
  con_print(buffer);
  va_end(args);
}

void con_print(const char *_string) {
  int len = strlen(_string);
  for (int i = 0; i < len; ++i) {
    char c = _string[i];
    switch (c) {
      case '\n': sCharsXPos = 0;
        sCharsYPos++;
        break;
      default: con_putc(c);
        break;
    }
  }
}

void con_nprint(const char *_string, int _len) {
  int len = strlen(_string);
  len = len < _len ? len : _len;
  for (int i = 0; i < len; ++i) {
    char c = _string[i];
    switch (c) {
      case '\n': sCharsXPos = 0;
        sCharsYPos++;
        break;
      default: con_putc(c);
        break;
    }
  }
}

void con_printXY(int _col, int _row, const char *_string) {
  con_gotoXY(_col, _row);
  con_print(_string);
}

/**
* Prints a single char to the screen
*/
void con_putc(char _char) {
  if (sCharsXPos >= con_columns()) {
    return;
  }
  if (sCharsYPos >= con_rows()) {
    return;
  }
  int pos = sCharsYPos * con_columns() + sCharsXPos++;
  if (pos >= con_area() || pos < 0) {
    return;
  }
  sChars[pos] = (_char & 0xff) | sLastAttrib;
  sDirty = GL_TRUE;
}

/**
* Prints a single char to the screen
*/
void con_putcXY(int _col, int _row, char _char) {
  if (_col >= con_columns()) {
    return;
  }
  if (_row >= con_rows()) {
    return;
  }
  int pos = _row * con_columns() + _col;
  if (pos >= con_area() || pos < 0) {
    return;
  }
  sChars[pos] = (_char & 0xff) | sLastAttrib;
  sDirty = GL_TRUE;
}

void con_hline(int _x1, int _x2, int _y, u8 _char) {
  if (_x1 > _x2) {
    int t = _x1;
    _x1 = _x2;
    _x2 = t;
  }
  for (size_t i = _x1; i <= _x2; i++) {
    con_putcXY(i, _y, _char);
  }
}

void con_vline(int _x, int _y1, int _y2, u8 _char) {
  if (_y1 > _y2) {
    int t = _y1;
    _y1 = _y2;
    _y2 = t;
  }
  for (size_t i = _y1; i <= _y2; i++) {
    con_putcXY(_x, i, _char);
  }
}

void con_shutdown() {
  quit = true;
}

/**
 * Pauses audio playback
 */
void con_pauseAudio() {
  SDL_PauseAudio(1);
}

/**
 * Resumes audio playback
 */
void con_resumeAudio() {
  SDL_PauseAudio(0);
}

void resize() {
  int w, h;
  SDL_GL_GetDrawableSize(gWindow, &w, &h);
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  float left = -(sScreenOffsetX / 2);
  float right = sScreenWidth + (sScreenOffsetX - (sScreenOffsetX / 2));
  float top = -(sScreenOffsetY / 4.0f);
  float bottom = (sScreenHeight / 2) + ((sScreenOffsetY / 2.0f) - (sScreenOffsetY / 4.0f));

  // glOrtho(-2.0f, con_columns() * 8.0f + 2, con_rows() * 8.0f + 1, -1.0f, 0.0f, 1.0f);
  glOrtho(left, right, bottom, top, 0.0f, 1.0f);
  glMatrixMode(GL_MODELVIEW);

  // Build VBOs
  size_t size;

  // Create Index Buffer
  size = con_columns() * con_rows() * 6 * sizeof(GLuint);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  if (sIndexHandle != -1) {
    glDeleteBuffers(1, &sIndexHandle);
  }
  if (sIndexBuffer != NULL) {
    free(sIndexBuffer);
  }
  sIndexBuffer = malloc(size);
  if (!sIndexBuffer) {
    err(1, "ERROR: Cannot allocate index vbo.");
  }
  int len = con_columns() * con_rows() * 6;
  for (int i = 0, v = 0; i < len; i += 6, v += 4) {
    sIndexBuffer[i + 0] = v + 0;
    sIndexBuffer[i + 1] = v + 1;
    sIndexBuffer[i + 2] = v + 2;
    sIndexBuffer[i + 3] = v + 2;
    sIndexBuffer[i + 4] = v + 3;
    sIndexBuffer[i + 5] = v + 1;
  }
  glGenBuffers(1, &sIndexHandle);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sIndexHandle);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, sIndexBuffer, GL_STATIC_DRAW);

  // Create Vertex Buffer
  size = con_columns() * con_rows() * 4 * 3 * sizeof(GLfloat);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  if (sVertexHandle != -1) {
    glDeleteBuffers(1, &sVertexHandle);
  }
  if (sVertexBuffer != NULL) {
    free(sVertexBuffer);
  }
  sVertexBuffer = malloc(size);
  memset(sVertexBuffer, 0, size);
  if (!sVertexBuffer) {
    err(1, "ERROR: Cannot allocate vertex vbo.");
  }
  for (int y = 0; y < con_rows(); y++) {
    for (int x = 0; x < con_columns(); x++) {
      static const float offsets[12] = {0, 0, 0, 8, 0, 0, 0, 8, 0, 8, 8, 0};
      const GLuint addr = (y * con_columns() + x) * (4 * 3);
      int i = 0;
      while (i < 12) {
        sVertexBuffer[addr + i] = (x * 8) + offsets[i];
        i++;
        sVertexBuffer[addr + i] = (y * 8) + offsets[i];
        i++;
        i++;
      }
    }
  }
  glGenBuffers(1, &sVertexHandle);
  glBindBuffer(GL_ARRAY_BUFFER, sVertexHandle);
  glBufferData(GL_ARRAY_BUFFER, size, sVertexBuffer, GL_STATIC_DRAW);

  // Create Texture Coord Buffer
  size = con_columns() * con_rows() * 4 * 2 * sizeof(GLshort);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  if (sTextureCoordHandle != -1) {
    glDeleteBuffers(1, &sTextureCoordHandle);
  }
  if (sTextureCoordBuffer != NULL) {
    free(sTextureCoordBuffer);
  }
  sTextureCoordBuffer = malloc(size);
  if (!sTextureCoordBuffer) {
    err(1, "ERROR: Cannot allocate texture coordinate vbo.");
  }
  memset(sTextureCoordBuffer, 0, size);
  glGenBuffers(1, &sTextureCoordHandle);
  glBindBuffer(GL_ARRAY_BUFFER, sTextureCoordHandle);
  glBufferData(GL_ARRAY_BUFFER, size, sTextureCoordBuffer, GL_DYNAMIC_DRAW);

  // Create Foreground Color Buffer
  size = con_columns() * con_rows() * 4 * 4 * sizeof(GLubyte);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  if (sFgColorHandle != -1) {
    glDeleteBuffers(1, &sFgColorHandle);
  }
  if (sFgColorBuffer != NULL) {
    free(sFgColorBuffer);
  }
  sFgColorBuffer = malloc(size);
  if (!sFgColorBuffer) {
    err(1, "ERROR: Cannot allocate foreground color vbo.");
  }
  memset(sFgColorBuffer, 255, size);
  glGenBuffers(1, &sFgColorHandle);
  glBindBuffer(GL_ARRAY_BUFFER, sFgColorHandle);
  glBufferData(GL_ARRAY_BUFFER, size, sFgColorBuffer, GL_DYNAMIC_DRAW);

  // Create Background Color Buffer
  size = con_columns() * con_rows() * 4 * 4 * sizeof(GLubyte);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  if (sBgColorHandle != -1) {
    glDeleteBuffers(1, &sBgColorHandle);
  }
  if (sBgColorBuffer != NULL) {
    free(sBgColorBuffer);
  }
  sBgColorBuffer = malloc(size);
  if (!sBgColorBuffer) {
    err(1, "ERROR: Cannot allocate background color vbo.");
  }
  memset(sBgColorBuffer, 0, size);
  glGenBuffers(1, &sBgColorHandle);
  glBindBuffer(GL_ARRAY_BUFFER, sBgColorHandle);
  glBufferData(GL_ARRAY_BUFFER, size, sBgColorBuffer, GL_DYNAMIC_DRAW);

  // Allocate PrintF Buffer
  if (sPrintFBuffer == NULL) {
    free(sPrintFBuffer);
  }
  sPrintFBuffer = malloc(sizeof(char) * con_area());
  if (!sPrintFBuffer) {
    err(1, "ERROR: Cannot allocate printf buffer.");
  }

  // Allocate screen buffer
  if (sChars == NULL) {
    free(sChars);
  }
  sChars = malloc(con_columns() * con_rows() * sizeof(u32));
  if (!sChars) {
    err(1, "ERROR: Cannot allocate screen buffer.");
  }
  memset(sChars, 0, con_area() * sizeof(u32));
} /* resize */

void audiocb(void *userdata, u8 *buf, int len) {
  ChipSample *bufCS = (ChipSample *) buf;
  size_t lenCS = len / sizeof(ChipSample);
  tracker_getSamples(bufCS, lenCS);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    err(1, "Usage: %s <chip> <filename>\n", argv[0]);
  }

  // Init messages array
  for (size_t i = 0; i < 256; i++) {
    sMessages[i][0] = 0;
  }
  if (!tracker_setChipName(argv[1])) {
    err(1, "Cannot find chip: %s", argv[1]);
  }
  tracker_setFilename(argv[2]);
  SDL_AudioSpec requested, obtained;

  // Initialize SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    err(1, "SDL could not initialize! SDL Error: %s\n", SDL_GetError());
  }
  atexit(SDL_Quit);
  requested.freq = 44100;
  requested.format = AUDIO_S16;
  requested.samples = 512;
  requested.callback = audiocb;
  requested.channels = 2;
  if (SDL_OpenAudio(&requested, &obtained) < 0) {
    err(1, "SDL_OpenAudio");
  }
  //fprintf(stderr, "freq %d\n", obtained.freq);
  //fprintf(stderr, "format %x\n", obtained.format);
  //fprintf(stderr, "samples %d\n", obtained.samples);

  // Use OpenGL 2.1
  // SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
  // SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 1 );

  sScreenWidth = tracker_preferredScreenWidth() == 0 ? STARTING_SCREEN_WIDTH : tracker_preferredScreenWidth();
  sScreenHeight = tracker_preferredScreenHeight() == 0 ? STARTING_SCREEN_HEIGHT : tracker_preferredScreenHeight();

  // Create window
  sActualScreenWidth = sScreenWidth + 4;
  sActualScreenHeight = sScreenHeight + 4;
  sScreenOffsetX = sActualScreenWidth - sScreenWidth;
  sScreenOffsetY = sActualScreenHeight - sScreenHeight;
  gWindow = SDL_CreateWindow("ESC", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, sActualScreenWidth,
                             sActualScreenHeight,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |
                             SDL_WINDOW_ALLOW_HIGHDPI);
  if (gWindow == NULL) {
    err(1, "Window could not be created! SDL Error: %s\n", SDL_GetError());
  }

  // Create context
  gContext = SDL_GL_CreateContext(gWindow);
  if (gContext == NULL) {
    err(1, "OpenGL context could not be created! SDL Error: %s\n", SDL_GetError());
  }

  // Use Vsync
  if (SDL_GL_SetSwapInterval(1) < 0) {
    err(1, "Warning: Unable to set VSync! SDL Error: %s\n", SDL_GetError());
  }

  // Allocate texture
  sTexture = malloc(128 * 128);
  if (!sTexture) {
    err(1, "ERROR: Cannot allocate texture.");
  }
  memset(sTexture, 0, 128 * 128);

  // Copy font to texture
  {
    for (int x = 0; x < 16; x++) {
      for (int y = 0; y < 16; y++) {
        for (int yy = 0; yy < 8; yy++) {
          GLubyte row = font_data[((y * 16 + x) * 8) + yy];
          for (int xx = 0; xx < 8; xx++) {
            GLubyte alpha = ((row >> (7 - xx)) & 1) * 255;
            sTexture[((y * 8 + yy) * 128) + (x * 8 + xx)] = alpha;
          }
        }
      }
    }
  }

  // Scale texture matrix to pixel size
  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();
  glScalef(1.0f / 128.0f, 1.0f / 128.0f, 1.0f);

  // Create textures
  GLuint texId;
  glGenTextures(1, &texId);
  glBindTexture(GL_TEXTURE_2D, texId);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 128, 128, 0, GL_ALPHA, GL_UNSIGNED_BYTE, sTexture);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  // Reset model view matrix
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Init the screen
  resize();
  con_cls();

  tracker_init();
  SDL_PauseAudio(0);
  con_msg("READY");

  // Event handler
  SDL_Event e;

  // While application is running
  while (!quit) {
    // Handle events on queue
    while (SDL_PollEvent(&e) != 0) {
      // User requests quit
      switch (e.type) {
        case SDL_QUIT: quit = true;
          break;
        case SDL_WINDOWEVENT:
          switch (e.window.event) {
            case SDL_WINDOWEVENT_RESIZED:printf("SDL_WINDOWEVENT_RESIZED: %d %d\n", e.window.data1, e.window.data2);
              sActualScreenWidth = (e.window.data1 < 128 ? 128 : e.window.data1);
              sActualScreenHeight = (e.window.data2 < 128 ? 128 : e.window.data2);
              if (sActualScreenWidth != e.window.data1 || sActualScreenHeight != e.window.data2) {
                SDL_SetWindowSize(gWindow, sActualScreenWidth, sActualScreenHeight);
              }
              sScreenWidth = (sActualScreenWidth / 8) * 8;
              sScreenHeight = (sActualScreenHeight / 16) * 16;
              sScreenOffsetX = (sActualScreenWidth - sScreenWidth);
              sScreenOffsetY = (sActualScreenHeight - sScreenHeight);
              resize();
              break;
          }
          break;
        case SDL_MOUSEBUTTONDOWN:
          if ((e.button.state && SDL_BUTTON_LMASK) != 0) {
            int x = (e.button.x - sScreenOffsetX) / 8;
            int y = (e.button.y - sScreenOffsetY) / 16;
            tracker_mouseDownAt(x, y);
          }
          break;
        case SDL_MOUSEMOTION:
          if ((e.motion.state && SDL_BUTTON_LMASK) != 0) {
            int x = (e.motion.x - sScreenOffsetX) / 8;
            int y = (e.motion.y - sScreenOffsetY) / 16;
            tracker_mouseDownAt(x, y);
          }
          break;
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
          bool keyFound = false;
          bool isDown = (e.type == SDL_KEYDOWN);

          {
            TrackerTextEditKey key = TEK_NONE;
            SDL_Scancode sc = e.key.keysym.scancode;

            // TODO: Mac only currently
            if (KMOD_GUI & e.key.keysym.mod) {
              switch (sc) {
                case SDL_SCANCODE_LEFT: key = TEK_HOME;
                  break;

                case SDL_SCANCODE_RIGHT: key = TEK_END;
                  break;

                case SDL_SCANCODE_BACKSPACE: key = TEK_CLEAR;
                  break;
                default: break;
              }
            } else if (KMOD_ALT & e.key.keysym.mod) {
              switch (sc) {
                case SDL_SCANCODE_LEFT: key = TEK_WORD_LEFT;
                  break;

                case SDL_SCANCODE_RIGHT: key = TEK_WORD_RIGHT;
                  break;
                default: break;
              }
            } else if (KMOD_SHIFT & e.key.keysym.mod) {
              switch (sc) {
                case SDL_SCANCODE_DELETE: key = TEK_INSERT;
                  break;
                default: break;
              }
            } else if (e.key.keysym.mod == 0) {
              switch (sc) {
                case SDL_SCANCODE_LEFT: key = TEK_LEFT;
                  break;

                case SDL_SCANCODE_RIGHT: key = TEK_RIGHT;
                  break;

                case SDL_SCANCODE_BACKSPACE: key = TEK_BACKSPACE;
                  break;

                case SDL_SCANCODE_DELETE: key = TEK_DELETE;
                  break;

                case SDL_SCANCODE_RETURN: key = TEK_ENTER;
                  break;

                case SDL_SCANCODE_ESCAPE: key = TEK_ESCAPE;
                  break;

                case SDL_SCANCODE_SPACE: key = TEK_SPACE;
                  break;

                case SDL_SCANCODE_UP: key = TEK_UP;
                  break;

                case SDL_SCANCODE_DOWN: key = TEK_DOWN;
                  break;

                default: break;
              }
            }
            if (key != TEK_NONE && isDown) {
              keyFound = tracker_textEditKey(key);
            }
          }

          // Check for ascii keys
          if (!keyFound && isDown) {
            u32 key = e.key.keysym.sym;
            if (key > 32 && key < 127) {
              if (KMOD_SHIFT & e.key.keysym.mod) {
                switch (key) {
                  case SDLK_1: key = '!';
                    break;
                  case SDLK_2: key = '@';
                    break;
                  case SDLK_3: key = '#';
                    break;
                  case SDLK_4: key = '$';
                    break;
                  case SDLK_5: key = '%';
                    break;
                  case SDLK_6: key = '^';
                    break;
                  case SDLK_7: key = '&';
                    break;
                  case SDLK_8: key = '*';
                    break;
                  case SDLK_9: key = '(';
                    break;
                  case SDLK_0: key = ')';
                    break;
                  case SDLK_MINUS: key = '_';
                    break;
                  case SDLK_EQUALS: key = '+';
                    break;
                  default:
                    if (key >= 'a' && key <= 'z') {
                      key -= ('a' - 'A');
                    } else {
                      key = 0;
                    }
                    break;
                } /* switch */
              }
              if (key != 0) {
                keyFound = tracker_asciiKey(key);
              }
            }
          }

          // Check for hex keys
          if (!keyFound && isDown) {
            for (size_t i = 0; i < 16; i++) {
              if ((hexKeys[i] == e.key.keysym.scancode) && (e.key.keysym.mod == KMOD_NONE)) {
                keyFound = tracker_hexKey(i);
                break;
              }
            }
          }

          // Check for piano keys
          if (!keyFound) {
            for (size_t i = 0; i < pianoKeysCount; i++) {
              if ((pianoKeys[i] == e.key.keysym.scancode) && (e.key.keysym.mod == KMOD_NONE)) {
                keyFound = tracker_pianoKey(i, e.key.repeat != 0, isDown);
                break;
              }
            }
          }

          // Handle action keys
          if (!keyFound && isDown) {
            for (size_t i = 0; i < actionsCount; i++) {
              ActionTableEntry a = actions[i];
              if ((a.scancode == e.key.keysym.scancode) &&
                  ((a.modifiers == 0 && e.key.keysym.mod == 0) ||
                   ((a.modifiers & e.key.keysym.mod) != 0))) {
                tracker_action(a.action, a.trackerState);
                sBlinkTimer = BLINK_SPEED;
                sBlinkState = false;
              }
            }
          }
          break;
        }
      } /* switch */
    }

    // Render tracker
    glClearColor(sPalette[0] / 255.0f, sPalette[1] / 255.0f, sPalette[2] / 255.0f, 1.0f);
    tracker_drawScreen();

    // Handle blink attrib
    sBlinkTimer--;
    if (sBlinkTimer < 0) {
      sBlinkState = !sBlinkState;
      sBlinkTimer = BLINK_SPEED;
      if (!sDirty) {
        for (int y = 0; y < con_rows(); y++) {
          for (int x = 0; x < con_columns(); x++) {
            const u32 d = sChars[y * con_columns() + x];
            const GLubyte blink = (d >> 16) & 1;
            if (blink == 1) {
              sDirty = GL_TRUE;
              break;
            }
          }
          if (sDirty) {
            break;
          }
        }
      }
    }
    if (sDirty) {
      size_t size;
      sDirty = GL_FALSE;
      static const float offsets[8] = {0, 0, 8, 0, 0, 8, 8, 8};
      for (int y = 0; y < con_rows(); y++) {
        for (int x = 0; x < con_columns(); x++) {
          const u32 d = sChars[y * con_columns() + x];
          const GLushort c = d & 255;
          const int row = c / 16;
          const int col = c % 16;
          const GLubyte blink = (d >> 16) & 1;
          const GLubyte fg = (d >> 8) & 15;
          const GLubyte bg = (d >> 12) & 15;
          const GLubyte bgr = sPalette[((blink == 1 && sBlinkState) ? fg : bg) * 3 + 0];
          const GLubyte bgg = sPalette[((blink == 1 && sBlinkState) ? fg : bg) * 3 + 1];
          const GLubyte bgb = sPalette[((blink == 1 && sBlinkState) ? fg : bg) * 3 + 2];
          const GLubyte fgr = sPalette[((blink == 1 && sBlinkState) ? bg : fg) * 3 + 0];
          const GLubyte fgg = sPalette[((blink == 1 && sBlinkState) ? bg : fg) * 3 + 1];
          const GLubyte fgb = sPalette[((blink == 1 && sBlinkState) ? bg : fg) * 3 + 2];
          const GLuint addr = (y * con_columns() + x) * (4 * 2);
          int i = 0;
          while (i < 8) {
            sTextureCoordBuffer[addr + i] = (col * 8) + offsets[i];
            i++;
            sTextureCoordBuffer[addr + i] = (row * 8) + offsets[i];
            i++;
          }
          const GLuint addr2 = (y * con_columns() + x) * (4 * 4);
          i = 0;
          while (i < 16) {
            sFgColorBuffer[addr2 + i] = fgr;
            sBgColorBuffer[addr2 + i++] = bgr;
            sFgColorBuffer[addr2 + i] = fgg;
            sBgColorBuffer[addr2 + i++] = bgg;
            sFgColorBuffer[addr2 + i] = fgb;
            sBgColorBuffer[addr2 + i++] = bgb;
            sFgColorBuffer[addr2 + i] = 255;
            sBgColorBuffer[addr2 + i++] = 255;
          }
        }
      }
      glBindBuffer(GL_ARRAY_BUFFER, sTextureCoordHandle);
      size = con_columns() * con_rows() * 4 * 2 * sizeof(GLshort);
      glBufferData(GL_ARRAY_BUFFER, size, sTextureCoordBuffer, GL_DYNAMIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER, sFgColorHandle);
      size = con_columns() * con_rows() * 4 * 4 * sizeof(GLubyte);
      glBufferData(GL_ARRAY_BUFFER, size, sFgColorBuffer, GL_DYNAMIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER, sBgColorHandle);
      size = con_columns() * con_rows() * 4 * 4 * sizeof(GLubyte);
      glBufferData(GL_ARRAY_BUFFER, size, sBgColorBuffer, GL_DYNAMIC_DRAW);
    }
    glClear(GL_COLOR_BUFFER_BIT);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    glBindBuffer(GL_ARRAY_BUFFER, sVertexHandle);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, sBgColorHandle);
    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sIndexHandle);
    glDrawElements(GL_TRIANGLES, con_columns() * con_rows() * 6, GL_UNSIGNED_INT, 0);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, texId);

    glBindBuffer(GL_ARRAY_BUFFER, sTextureCoordHandle);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_SHORT, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, sFgColorHandle);
    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sIndexHandle);
    glDrawElements(GL_TRIANGLES, con_columns() * con_rows() * 6, GL_UNSIGNED_INT, 0);

    // Update screen
    SDL_GL_SwapWindow(gWindow);
  }
  SDL_PauseAudio(1);

  // Free resources and close SDL
  tracker_destroy();

  free(sChars);
  sChars = NULL;

  free(sTexture);
  sTexture = NULL;

  // Destroy window
  SDL_DestroyWindow(gWindow);
  gWindow = NULL;

  // Quit SDL subsystems
  SDL_Quit();

  return 0;
} /* main */

