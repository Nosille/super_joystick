// Source List
enum class Source : int16_t {
    Local=0,        // 0
    MCP3208,        // 1
    ArcadeLeft,     // 2
    ArcadeRight,    // 3
    EncoderLeft,    // 4
    EncoderRight,   // 5
    Imu,            // 6
    Count
};
static const uint8_t interrupt_pins[(uint8_t)Source::Count] = { 0, 0, 17, 11, 44, 43, 0};

// Axes Config
static const int16_t axes[][3] = 
{              //source,           pin,   scale
    {(int16_t)Source::MCP3208,      2,     16},
    {(int16_t)Source::MCP3208,      3,     16},
    {(int16_t)Source::MCP3208,      0,    -16},
    {(int16_t)Source::MCP3208,      1,    -16},
    {(int16_t)Source::MCP3208,      6,     16},
    {(int16_t)Source::MCP3208,      7,     16},
    {(int16_t)Source::MCP3208,      4,    -16},
    {(int16_t)Source::MCP3208,      5,    -16},
    {(int16_t)Source::Local,        5,    -16},
    {(int16_t)Source::EncoderLeft,  0,      1},
    {(int16_t)Source::EncoderRight, 0,      1},
    {(int16_t)Source::Imu,          0,   -100},
    {(int16_t)Source::Imu,          1,    100},
    {(int16_t)Source::Imu,          2,    100},
    {(int16_t)Source::Imu,          3,  -1000},
    {(int16_t)Source::Imu,          4,   1000},
    {(int16_t)Source::Imu,          5,   1000}                      
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
{             //source,             pin,          type
    {(int16_t)Source::Local,         1,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::Local,        38,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::Local,        33,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::Local,         6,    (int16_t)ButtonType::Touch},
    {(int16_t)Source::Local,         7,    (int16_t)ButtonType::Touch},
    {(int16_t)Source::ArcadeLeft,    0,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::ArcadeLeft,    1,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::ArcadeLeft,    2,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::ArcadeLeft,    3,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::ArcadeRight,   0,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::ArcadeRight,   1,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::ArcadeRight,   2,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::ArcadeRight,   3,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::ArcadeLeft,    4,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::ArcadeRight,   4,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::ArcadeLeft,    5,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::ArcadeRight,   5,    (int16_t)ButtonType::Digital}, 
    {(int16_t)Source::ArcadeLeft,   14,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::ArcadeRight,  14,    (int16_t)ButtonType::Digital}, 
    {(int16_t)Source::ArcadeLeft,   15,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::ArcadeRight,  15,    (int16_t)ButtonType::Digital}, 
    {(int16_t)Source::EncoderLeft,  24,    (int16_t)ButtonType::Digital},
    {(int16_t)Source::EncoderRight, 24,    (int16_t)ButtonType::Digital}                                                  
};
static const uint8_t buttons_size = sizeof(buttons) / sizeof(buttons[0]);

// LED Config
static const int16_t leds[][3] = 
{                  //source,        pin, type
    {(int16_t)Source::ArcadeLeft,    7,    0},
    {(int16_t)Source::ArcadeLeft,   11,    0},
    {(int16_t)Source::ArcadeLeft,   16,    0},
    {(int16_t)Source::ArcadeRight,   7,    0},
    {(int16_t)Source::ArcadeRight,  11,    0},
    {(int16_t)Source::ArcadeRight,  16,    0}                                                 
};
static const uint8_t led_size = sizeof(leds) / sizeof(leds[0]);