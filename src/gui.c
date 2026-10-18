#include <raylib.h>

// raygui's defaults also read a gamepad, which would scroll lists and press buttons on its own
#define GUI_SCROLL_DELTA GetMouseWheelMove()
#define GUI_BUTTON_DOWN IsMouseButtonDown(MOUSE_BUTTON_LEFT)
#define GUI_BUTTON_PRESSED IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
#define GUI_BUTTON_RELEASED IsMouseButtonReleased(MOUSE_BUTTON_LEFT)
#define GUI_BUTTON_DOWN_ALT IsMouseButtonDown(MOUSE_BUTTON_RIGHT)
#define GUI_BUTTON_PRESSED_MID IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

// raygui keeps one cursor for every text box. a box focused from code should start with it at the end
void place_text_cursor(const char *text) {
    textBoxCursorIndex = text != NULL ? (int)strlen(text) : 0;
}
