// Source List
enum class Source : int16_t {
    Local=0,        // 0
    MCP3208,        // 1
    Ada1616,        // 2
    Encoder,        // 3    
    BNO055,         // 4
    Count
};
static const int16_t devices[][3] = 
{            //source,            addr,   interrupt
    {(int16_t)Source::Local,         0,      0},   // 0
    {(int16_t)Source::MCP3208,      12,      0},   // 1
    {(int16_t)Source::Ada1616,    0x49,     17},   // 2
    {(int16_t)Source::Ada1616,    0x4A,     11},   // 3
    {(int16_t)Source::Encoder,    0x36,     44},   // 4
    {(int16_t)Source::Encoder,    0x37,     43},   // 5 
    {(int16_t)Source::BNO055,     0x28,      0},   // 6
};
static const uint8_t devices_size = sizeof(devices) / sizeof(devices[0]);

// Axes Config
static const int16_t axes[][6] = 
{//device,  pin,   shift     scale     min       max
    {1,      2,     2048,      16,   -32767,    32767},
    {1,      3,     2048,      16,   -32767,    32767},
    {1,      0,     2048,     -16,   -32767,    32767},
    {1,      1,     2048,     -16,   -32767,    32767},
    {1,      6,     2048,      16,   -32767,    32767},
    {1,      7,     2048,      16,   -32767,    32767},
    {1,      4,     2048,     -16,   -32767,    32767},
    {1,      5,     2048,     -16,   -32767,    32767},
    {0,      5,     2048,     -16,   -32767,    32767},
    {4,      0,        0,      -1,   -32767,    32767},
    {5,      0,        0,      -1,   -32767,    32767},
    {6,      0,        0,    -100,   -32767,    32767},
    {6,      1,        0,     100,   -32767,    32767},
    {6,      2,        0,     100,   -32767,    32767},
    {6,      3,        0,   -1000,   -32767,    32767},
    {6,      4,        0,    1000,   -32767,    32767},
    {6,      5,        0,    1000,   -32767,    32767}                      
};
static const uint8_t axes_size = sizeof(axes) / sizeof(axes[0]);

// type List
enum class ButtonType : int16_t {
    Digital=0, // 0
    Touch,     // 1
    Count
};

// Button Config
static const int16_t buttons[][3] = 
{ //device, pin,          type
    {0,      1,    (int16_t)ButtonType::Digital},
    {0,     38,    (int16_t)ButtonType::Digital},
    {0,     33,    (int16_t)ButtonType::Digital},
    {0,      6,    (int16_t)ButtonType::Touch},
    {0,     10,    (int16_t)ButtonType::Touch},
    {2,      0,    (int16_t)ButtonType::Digital},
    {2,      1,    (int16_t)ButtonType::Digital},
    {2,      2,    (int16_t)ButtonType::Digital},
    {2,      3,    (int16_t)ButtonType::Digital},
    {3,      0,    (int16_t)ButtonType::Digital},
    {3,      1,    (int16_t)ButtonType::Digital},
    {3,      2,    (int16_t)ButtonType::Digital},
    {3,      3,    (int16_t)ButtonType::Digital},
    {2,      4,    (int16_t)ButtonType::Digital},
    {3,      4,    (int16_t)ButtonType::Digital},
    {2,      5,    (int16_t)ButtonType::Digital},
    {3,      5,    (int16_t)ButtonType::Digital}, 
    {2,     14,    (int16_t)ButtonType::Digital},
    {3,     14,    (int16_t)ButtonType::Digital}, 
    {2,     15,    (int16_t)ButtonType::Digital},
    {3,     15,    (int16_t)ButtonType::Digital}, 
    {0,     18,    (int16_t)ButtonType::Digital},
    {0,      7,    (int16_t)ButtonType::Digital},
    {0,     14,    (int16_t)ButtonType::Digital},
    {0,      3,    (int16_t)ButtonType::Digital},
    {4,     24,    (int16_t)ButtonType::Digital},
    {5,     24,    (int16_t)ButtonType::Digital},
};
static const uint8_t buttons_size = sizeof(buttons) / sizeof(buttons[0]);

// LED Config
static const int16_t leds[][3] = 
{ //device,vpin,  type
    {2,      7,    0},
    {2,     11,    0},
    {2,     16,    0},
    {3,      7,    0},
    {3,     11,    0},
    {3,     16,    0}                                                 
};
static const uint8_t led_size = sizeof(leds) / sizeof(leds[0]);