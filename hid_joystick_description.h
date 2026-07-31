typedef struct TU_ATTR_PACKED
{
  int16_t x = 0.0;          ///< x position of upper left analog-stick
  int16_t y = 0.0;          ///< y position of upper left analog-stick
  int16_t z = 0.0;          ///< x position of upper right analog-stick
  int16_t rx = 0.0;         ///< y position of upper right analog-stick
  int16_t ry = 0.0;         ///< x position of lower left analog-stick
  int16_t rz = 0.0;         ///< y position of lower left analog-stick
  int16_t slider = 0.0;     ///< x position of lower right analog-stick
  int16_t dial = 0.0;       ///< y position of lower right analog-stick  
  int16_t wheel = 0.0;      ///< potentiometer
  uint32_t buttons = 0;     ///< Buttons mask for currently pressed buttons
}my_joystick_report_t;

struct joystick_report
{
 my_joystick_report_t joystick;
 bool needs_send = false;
};

// Gamepad Report Descriptor Template
// with 4 joysticks + dial (9 axes of 2 byte each)
// and 32 buttons of 1 bit each for a total of 2 bytes
// | L1x | L1y | R1x | R1y | L2X | L2Y | R2X | R2Y | Dial |Button Map |
#define MY_HID_REPORT_DESC_JOYSTICK(...) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     )                 ,\
  HID_USAGE      ( HID_USAGE_DESKTOP_JOYSTICK )                 ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION )                 ,\
    /* Report ID if any */\
    __VA_ARGS__ \
    /* 8 bit X, Y, Z, Rx, Ry, Rz, Slider, Dial, Wheel (min -32767, max 32767 ) */ \
    HID_USAGE_PAGE     ( HID_USAGE_PAGE_DESKTOP                 ) ,\
      HID_USAGE        ( HID_USAGE_DESKTOP_X                    ) ,\
      HID_USAGE        ( HID_USAGE_DESKTOP_Y                    ) ,\
      HID_USAGE        ( HID_USAGE_DESKTOP_Z                    ) ,\  
      HID_USAGE        ( HID_USAGE_DESKTOP_RX                   ) ,\
      HID_USAGE        ( HID_USAGE_DESKTOP_RY                   ) ,\
      HID_USAGE        ( HID_USAGE_DESKTOP_RZ                   ) ,\ 
      HID_USAGE        ( HID_USAGE_DESKTOP_SLIDER               ) ,\ 
      HID_USAGE        ( HID_USAGE_DESKTOP_DIAL                 ) ,\ 
      HID_USAGE        ( HID_USAGE_DESKTOP_WHEEL                ) ,\                                
      HID_LOGICAL_MIN_N( 0x8001, 2                              ) ,\
      HID_LOGICAL_MAX_N( 0x7fff, 2                              ) ,\
      HID_REPORT_COUNT ( 9                                      ) ,\
      HID_REPORT_SIZE  ( 16                                     ) ,\
      HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
    /* 16 bit Button Map */ \
    HID_USAGE_PAGE     ( HID_USAGE_PAGE_BUTTON                  ) ,\
      HID_USAGE_MIN    ( 1                                      ) ,\
      HID_USAGE_MAX    ( 32                                     ) ,\
      HID_LOGICAL_MIN  ( 0                                      ) ,\
      HID_LOGICAL_MAX  ( 1                                      ) ,\
      HID_REPORT_COUNT ( 32                                     ) ,\
      HID_REPORT_SIZE  ( 1                                      ) ,\
      HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
    /* 8 bit Led Controls*/ \
    HID_USAGE_PAGE     ( HID_USAGE_PAGE_LED                     ) ,\
      HID_USAGE        ( 0x01                                   ) ,\ 
      HID_LOGICAL_MIN  ( 0x00                                   ) ,\
      HID_LOGICAL_MAX  ( 0xff                                   ) ,\
      HID_REPORT_COUNT ( 8                                      ) ,\
      HID_REPORT_SIZE  ( 8                                      ) ,\
      HID_OUTPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\      
  HID_COLLECTION_END \
