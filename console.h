
#ifndef CONSOLE_H
#define CONSOLE_H

#include "types.h"
#include <SDL_keycode.h>
#include "chip.h"

#define STARTING_SCREEN_WIDTH (1360)
#define STARTING_SCREEN_HEIGHT (720)
#define NUM_MESSAGES (256)

/**
 *  Gets the number of columns allocated for the screen
 */
int con_columns();

/**
 *  Adds a message to the console buffer
 */
void con_msg(char *_message);

/**
 *  Adds a ChipError message to the console buffer
 */
void con_error(ChipError _error);

/**
 * Printf's a message to the console buffer
 */
void con_msgf(const char *format, ...);

/**
 *  Gets the most recent message
 */
char *con_getMostRecentMessage();

/**
 *  Get the next recent message
 */
char *con_getNextMessage();

/**
 *  Gets the number of rows allocated for the screen
 */
int con_rows();

/**
 *  Sets the default color attribute used.
 *  bits 0-3 = FG Color
 *  bits 4-6 = BG Color
 *  bit 7 = Blink
 */
void con_setAttrib(int _attrib);

/**
 * Sets the attribute at the specified location
 */
void con_setAttribXY(int _col, int _row, int _attrib);

/**
* Gets the attribute at the specified location
*/
u32 con_getAttribXY(int _col, int _row);

/**
 * Moves the cursor to the specified location
 */
void con_gotoXY(int _col, int _row);

/**
* Reports the current X position of the cursor
*/
int con_x();

/**
* Reports the current Y position of the cursor
*/
int con_y();

/**
 * Print an unformatted string
 */
void con_print(const char *_string);


/**
 * Print an unformatted string with limited length
 */
void con_nprint(const char *_string, int _len);

/**
 * Print a string at the specified location
 */
void con_printXY(int _col, int _row, const char *_string);

/**
 * Print a formatted string at the specified location
 */
void con_printfXY(int _col, int _row, const char *_format, ...);

/**
 * Print a formatted string
 */
void con_printf(const char *_format, ...);

/**
 * Draws a horizontal line
 */
void con_hline(int _x1, int _x2, int _y, u8 _char);

/**
 * Draws a vertical line
 */
void con_vline(int _x, int _y1, int _y2, u8 _char);

/**
 * Prints a single char to the screen
 */
void con_putc(char _char);

/**
 * Prints a single char to the screen at col, row
 */
void con_putcXY(int _col, int _row, char _char);

/**
 * clears the screen
 */
void con_cls();

/**
 * Fills the screen with the attribute and char
 */
void con_fill(int _attrib, char _char);

/**
 * Closes the window and cleans up
 */
void con_shutdown();

/**
 * Pauses audio playback
 */
void con_pauseAudio();

/**
 * Resumes audio playback
 */
void con_resumeAudio();

#endif // ifndef CONSOLE_H

