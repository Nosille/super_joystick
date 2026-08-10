#include "hid_mouse_description.h"
#include "hid_joystick_description.h"
#include "hid_keyboard_description.h"

// Report ID's
#define MOUSE_ID      0X01
#define KEYBOARD_ID   0X02
static const int16_t joystick_ids[2] = {0x03, 0x04};

// HID report descriptor
uint8_t const desc_hid_report[] = {
    MY_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(MOUSE_ID)),
    MY_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(KEYBOARD_ID)),
    MY_HID_REPORT_DESC_JOYSTICK(HID_REPORT_ID(joystick_ids[0])),
    MY_HID_REPORT_DESC_JOYSTICK(HID_REPORT_ID(joystick_ids[1])),
};

// Axis index for each value in the joystick reports
static const int16_t joystick_reports_size = sizeof(joystick_ids) / sizeof(joystick_ids[0]);
static const int16_t joystick_axes[][joystick_reports_size] = 
{ // joystick1  joystick2
    { 0,         11},   // 0
    { 1,         12},   // 1
    { 2,         13},   // 2
    { 3,         14},   // 3
    { 4,         15},   // 4
    { 5,         16},   // 5
    { 6,          9},   // 6
    { 7,         10},   // 7
    { 8,         -1},   // 8
};
static const uint8_t joystick_axes_size = sizeof(joystick_axes) / sizeof(joystick_axes[0]);

static const int16_t joystick_buttons[][joystick_reports_size] = 
{ // joystick1  joystick2
    { 0,         25},   // 0
    { 1,         26},   // 1
    { 2,         -1},   // 2
    { 3,         -1},   // 3
    { 4,         -1},   // 4
    { 5,         -1},   // 5
    { 6,         -1},   // 6
    { 7,         -1},   // 7
    { 8,         -1},   // 8
    { 9,         -1},   // 9
    {10,         -1},   // 10
    {11,         -1},   // 11
    {12,         -1},   // 12
    {13,         -1},   // 13
    {14,         -1},   // 14
    {15,         -1},   // 15
    {16,         -1},   // 16
    {17,         -1},   // 17
    {18,         -1},   // 18
    {19,         -1},   // 19
    {20,         -1},   // 20
    {21,         -1},   // 21
    {22,         -1},   // 22
    {23,         -1},   // 23
    {24,         -1},   // 24
};
static const uint8_t joystick_buttons_size = sizeof(joystick_buttons) / sizeof(joystick_buttons[0]);

// Axis index, scale factor, min, and max for each value in mouse report 
static const int16_t mouse_axes[][4] = 
{ //index scale   min    max
    { 2,   4096,  -127,   127}, // 0, horizontal motion
    { 3,   4096,  -127,   127}, // 1, vertical motion
    { 4,   8192,  -127,   127}, // 2, horizontal scroll
    { 5,   8192,  -127,   127}, // 3, vertical scroll
};
static const uint8_t mouse_axes_size = sizeof(mouse_axes) / sizeof(mouse_axes[0]);

// Button index for each value in mouse report 
static const int16_t mouse_buttons[] = 
{ //index
     14,                         // 0, left mouse button
     13,                         // 1, right mouse button
     16,                         // 2, middle mouse button
     -1,                         // 3, backward mouse button
     -1,                         // 4, forward mouse button
}; 
static const uint8_t mouse_buttons_size = sizeof(mouse_buttons) / sizeof(mouse_buttons[0]);

// Direct mapping of axes to keys in keyboard report.
// The first two axes are special.  Used to move in key matrix.
// An general axis can represent 2 keys.  One when value exceeds a min.  One when value exceeds a max.
static const int16_t keyboard_axes[][5] = 
{ //index  min_threshold    ASCII Character    max_threshold    ASCII Character
    { 0,          0,                       0,          0,                       0 },  // Special, horizontal axis for matrix
    { 1,          0,                       0,          0,                       0 },  // Special, vertical axis for matrix
    { 7,     -15000,      HID_KEY_ARROW_DOWN,      15000,      HID_KEY_ARROW_UP   },
    { 6,     -15000,      HID_KEY_ARROW_LEFT,      15000,      HID_KEY_ARROW_RIGHT},
};
static const uint8_t keyboard_axes_size = sizeof(keyboard_axes) / sizeof(keyboard_axes[0]);

// Direct mapping of buttons to keys in keyboard report.  One key per button.
// The first button is special. Used to select in key matrix.
static const int16_t keyboard_buttons[][2] = 
{
    {12,    /*key*/              },  // Special, Used to select highlighted key in matrix
    { 9,    HID_KEY_SHIFT_LEFT   },
    {10,    HID_KEY_CONTROL_LEFT },
    {11,    HID_KEY_ALT_LEFT     },
    { 5,    HID_KEY_HOME         },
    { 6,    HID_KEY_ESCAPE       },
    { 7,    HID_KEY_END          },
    { 8,    HID_KEY_ENTER        },
    {15,    HID_KEY_GUI_LEFT     },
}; 
static const uint8_t keyboard_buttons_size = sizeof(keyboard_buttons) / sizeof(keyboard_buttons[0]);

