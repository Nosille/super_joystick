#include <string>
#include <cstdint>
#include <algorithm>
#include <USB.h>
#include <SPI.h>
#include <Wire.h>
#include <MCP3208.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

#include "display.h"
#include "hid_mouse_description.h"
#include "hid_joystick_description.h"
#include "hid_keyboard_description.h"

#define QUEUE_SIZE 1

// I2C_ADDR's
#define SH1107_ADDR    0x3C // Address 0x3C default
#define ARCADE_ADDR_L  0x49 // Address 0x49 default
#define ARCADE_ADDR_R  0x4A // Address 0x49 default
#define ENCODER_ADDR_L 0x36 // Address 0x36 default
#define ENCODER_ADDR_R 0x37 // Address 0x36 default
#define IMU_ADDR       0x28 // Address 0x28 default

// SPI_CS's
#define MCP3208_CS    15    // Chip Select (CS) pin

// Report ID's
#define MOUSE_ID      0X01
#define KEYBOARD_ID   0X02
#define JOYSTICK1_ID  0X03
#define JOYSTICK2_ID  0X04

// Definition
enum class Source : uint8_t {
    Digital,     // 0
    ADC,         // 1
    Touch,       // 2
    MCP3208,     // 3
    ArcadeLeft,  // 4
    ArcadeRight, // 5
    EncoderLeft, // 6
    EncoderRight,// 7
    Display,     // 8
    Imu,         // 9
    Count
};
static const uint8_t interrupt_pins[(uint8_t)Source::Count] = { 0, 0, 0, 0, 18, 13, 38, 39, 0, 0};

static const uint8_t axes_source[] = { 3,  3,  3,  3,  3,  3,  3,  3,  1,  6,  7,   9,  9,  9,    9,   9,   9};
static const int16_t axes_scale[]  = {16, 16,-16,-16, 16, 16,-16,-16,-16,  1,  1,-100,100,100,-1000,1000,1000};
static const uint8_t axes_pin[]    = { 2,  3,  0,  1,  6,  7,  4,  5,  8,  0,  0,   0,  1,  2,    3,   4,   5};
static const uint8_t axes_size = sizeof(axes_pin) / sizeof(axes_pin[0]);
static const uint8_t buttons_source[] = { 0,  0,  0,  2,  2,  4,  4,  4,  4,  5,  5,  5,  5,  4,  5,  4,  5,  4,  5,  4,  5,  6,  7};
static const uint8_t buttons_pin[]    = { 9,  6,  5, 14, 11,  0,  1,  2,  3,  0,  1,  2,  3,  4,  4,  5,  5, 14, 14, 15, 15, 24, 24};
static const uint8_t buttons_size = sizeof(buttons_pin) / sizeof(buttons_pin[0]);
static const uint8_t leds_source[] = { 1,  1,  2,  2};
static const uint8_t leds_pins[]   = {10,  1, 10,  1};
static const uint8_t led_size = sizeof(leds_pins) / sizeof(leds_pins[0]);

// Devices
MCP3208 mcp3208;
Adafruit_seesaw arcade_left;
Adafruit_seesaw arcade_right;
Adafruit_seesaw encoder_left;
Adafruit_seesaw encoder_right;
Display display(SH1107_ADDR);
seesaw_NeoPixel encoder_pixel_left  = seesaw_NeoPixel(1, 6, NEO_GRB + NEO_KHZ800);
seesaw_NeoPixel encoder_pixel_right = seesaw_NeoPixel(1, 6, NEO_GRB + NEO_KHZ800);
Adafruit_BNO055 bno = Adafruit_BNO055(55, IMU_ADDR);

int touch_threshold = 0;  // if 0 is used, benchmark value is used. Its by default 1,5% change, can be changed by touchSetDefaultThreshold(float percentage)
bool device_installed[(uint8_t)Source::Count] = {false};
bool interrupt_triggered[(uint8_t)Source::Count] = {false};

// HID report descriptor using TinyUSB's templates
uint8_t const desc_hid_report[] = {
    MY_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(MOUSE_ID)),
    MY_HID_REPORT_DESC_JOYSTICK(HID_REPORT_ID(JOYSTICK1_ID)),
    MY_HID_REPORT_DESC_JOYSTICK(HID_REPORT_ID(JOYSTICK2_ID)),
    MY_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(KEYBOARD_ID))
};

Adafruit_USBD_HID usb_hid;
TaskHandle_t taskInputsHandle = NULL;
TaskHandle_t taskReportHandle = NULL;

// Queues
QueueHandle_t queueAxes2Report = NULL;
QueueHandle_t queueButtons2Report = NULL;
QueueHandle_t queueAxes2Display = NULL;
QueueHandle_t queueButtons2Display = NULL;
QueueHandle_t queueMode = NULL;

// Global variables
int32_t axis_values[axes_size] = {0}; 
bool button_values[buttons_size] = {false};
int8_t leds_values[led_size]  = {32}; // current brightness of leds, 0 to 255
bool keyboard_mode = false;

// HID report pending mechanism
// Mouse report
uint8_t mouse_buttons = 0;
int8_t mouse_x = 0, mouse_y = 0, mouse_v = 0, mouse_h = 0;
bool mouse_needs_send = false;

// Keyboard report
uint8_t keyboard_modifier = 0;
uint8_t keyboard_keys[6] = {0};
bool keyboard_release_pending = false;

// Joystick reports
my_joystick_report_t joystick1 = {0}, joystick2 = {0};
bool joystick1_needs_send= false, joystick2_needs_send = false;

int current_matrix = 0;
int8_t key_x = 0;      // Current keyboard position
int8_t key_y = 0;      // Current keyboard position
uint8_t key_i = 0;     // Current key row
uint8_t key_j = 0;     // Current key col

unsigned long last_read;
unsigned long last_display; 
const unsigned long kMatrixDebounceMs = 200;
bool last_matrix_inc = false;
bool last_matrix_dec = false;
unsigned long last_matrix_change_ms = 0;

//Interupt callbacks
void interruptSource(void* arg) {
  int sourceIndex = (int)arg;
  Serial.print("interrupt: "); Serial.println(sourceIndex);
  interrupt_triggered[sourceIndex] = true;
}

// Tasks
void taskReadInputs(void *parameter) {
  for (;;) {
    unsigned long begin = millis();

    // Make copy of interrupt state and reset
    bool interrupt[(uint8_t)Source::Count] = {false};
    for (uint8_t i = 0; i < (uint8_t)Source::Count; i++) {
      interrupt[i] = interrupt_triggered[i];
      interrupt_triggered[i] = false;
    }

    // Read Imu data
    double x, y, z, rx, ry, rz;
    if(device_installed[(uint8_t)Source::Imu]) {
      sensors_event_t event_euler, event_gyro;
      bno.getEvent(&event_euler, Adafruit_BNO055::VECTOR_EULER);
      bno.getEvent(&event_gyro, Adafruit_BNO055::VECTOR_GYROSCOPE);
      x = event_euler.orientation.y;
      y = event_euler.orientation.z;
      z = event_euler.orientation.x; if (z > 180.0) z -= 360.0;
      rx = event_gyro.gyro.y;
      ry = event_gyro.gyro.x;
      rz = event_gyro.gyro.z;
    }

    // Read axes values from sources
    for (uint8_t i = 0; i < axes_size; ++i) {
      if (axes_source[i] == (uint8_t)Source::ADC) {
        axis_values[i] = analogRead(axes_pin[i]);
      } else if (axes_source[i] == (uint8_t)Source::MCP3208) {
        axis_values[i] = mcp3208.readADC(axes_pin[i]);
      } else if (axes_source[i] == (uint8_t)Source::ArcadeLeft  && device_installed[(uint8_t)Source::ArcadeLeft]   && interrupt[(uint8_t)Source::ArcadeLeft]) {
        axis_values[i] = arcade_left.analogRead(axes_pin[i]);
      } else if (axes_source[i] == (uint8_t)Source::ArcadeRight && device_installed[(uint8_t)Source::ArcadeRight]  && interrupt[(uint8_t)Source::ArcadeRight]) {
        axis_values[i] = arcade_right.analogRead(axes_pin[i]);
      } else if (axes_source[i] == (uint8_t)Source::EncoderLeft && device_installed[(uint8_t)Source::EncoderLeft]  && interrupt[(uint8_t)Source::EncoderLeft]) {
        axis_values[i] = -encoder_left.getEncoderPosition();
      } else if (axes_source[i] == (uint8_t)Source::EncoderRight && device_installed[(uint8_t)Source::EncoderRight] && interrupt[(uint8_t)Source::EncoderRight]) {
        axis_values[i] = -encoder_right.getEncoderPosition();
      } else if (axes_source[i] == (uint8_t)Source::Imu         && device_installed[(uint8_t)Source::Imu]) {
        if(axes_pin[i] == 0) { axis_values[i] = x  * axes_scale[i]; }
        if(axes_pin[i] == 1) { axis_values[i] = y  * axes_scale[i]; }
        if(axes_pin[i] == 2) { axis_values[i] = z  * axes_scale[i]; }
        if(axes_pin[i] == 3) { axis_values[i] = rx * axes_scale[i]; }
        if(axes_pin[i] == 4) { axis_values[i] = ry * axes_scale[i]; }
        if(axes_pin[i] == 5) { axis_values[i] = rz * axes_scale[i]; }
      }
    }

    // Normalize axes values to -32767 to 32767 with center at 0 (16 bit) 
    for (uint8_t i = 0; i < axes_size; ++i) {
      if(axes_source[i] == (uint8_t)Source::ADC ||
         axes_source[i] == (uint8_t)Source::MCP3208) {
          axis_values[i] = (axis_values[i] - 2048) * axes_scale[i];
          axis_values[i] = std::clamp(axis_values[i], -32767L, 32767L);
      } else if(axes_source[i] == (uint8_t)Source::ArcadeLeft ||
                axes_source[i] == (uint8_t)Source::ArcadeRight) {
          axis_values[i] = (axis_values[i] - 512) * axes_scale[i];
          axis_values[i] = std::clamp(axis_values[i], -32767L, 32767L);
      }
    }

    // Read buttons values from sources
    uint32_t arcade_left_mask = 0;
    uint32_t arcade_right_mask = 0;
    for (uint8_t i = 0; i < buttons_size; ++i) {
      if (buttons_source[i] == 0) {
        button_values[i] = !digitalRead(buttons_pin[i]);
      } else if (buttons_source[i] == (uint8_t)Source::Touch  && interrupt[(uint8_t)Source::Touch]) {
        if (touchInterruptGetLastStatus(buttons_pin[i])) {
          button_values[i] = true;
        } else {
          button_values[i] = false;
        }
      } else if (buttons_source[i] == (uint8_t)Source::ArcadeLeft  && device_installed[(uint8_t)Source::ArcadeLeft]  && interrupt[(uint8_t)Source::ArcadeLeft]) {
        arcade_left_mask |= (1UL << buttons_pin[i]);
      } else if (buttons_source[i] == (uint8_t)Source::ArcadeRight && device_installed[(uint8_t)Source::ArcadeRight] && interrupt[(uint8_t)Source::ArcadeRight]) {
        arcade_right_mask |= (1UL << buttons_pin[i]);
      } else if (buttons_source[i] == (uint8_t)Source::EncoderLeft && device_installed[(uint8_t)Source::EncoderLeft] && interrupt[(uint8_t)Source::EncoderLeft]) {
        button_values[i] = !encoder_left.digitalRead(buttons_pin[i]);
      } else if (buttons_source[i] == (uint8_t)Source::EncoderRight && device_installed[(uint8_t)Source::EncoderRight] && interrupt[(uint8_t)Source::EncoderRight]) {
        button_values[i] = !encoder_right.digitalRead(buttons_pin[i]);
      }
    }
    
    if(arcade_left_mask != 0) {
      uint32_t state = arcade_left.digitalReadBulk(arcade_left_mask);
      for (uint8_t i = 0; i < buttons_size; ++i) {
        if (buttons_source[i] == (uint8_t)Source::ArcadeLeft) {
          button_values[i] = !(state & (1UL << buttons_pin[i]));
        }
      }
    }
    
    if(arcade_right_mask != 0) {
      uint32_t state = arcade_right.digitalReadBulk(arcade_right_mask);
      for (uint8_t i = 0; i < buttons_size; ++i) {
        if (buttons_source[i] == (uint8_t)Source::ArcadeRight) {
          button_values[i] = !(state & (1UL << buttons_pin[i]));
        }
      }
    }

    // UBaseType_t free_space = uxQueueSpacesAvailable(queueButtons);
    // Serial.print("taskButtons: Free space in queue: "); Serial.println(free_space); 
    // UBaseType_t stack_high_water_mark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print(" high water mark free: "); Serial.println(stack_high_water_mark)

    xQueueOverwrite(queueAxes2Report, axis_values);
    xQueueOverwrite(queueButtons2Report, button_values);
    xQueueOverwrite(queueAxes2Display, axis_values);
    xQueueOverwrite(queueButtons2Display, button_values);    

    // switch modes with debounce
    if (button_values[1] && keyboard_mode) {
      Serial.println("Switching to Joystick");
      keyboard_mode = false;
      delay(100);
      if(device_installed[(uint8_t)Source::Display]) display.switchMode(0);

      mouse_x = 0;
      mouse_y = 0;
      mouse_v = 0;
      mouse_h = 0;
      mouse_buttons = 0;
      mouse_needs_send = true;
      delay(400);
      
    } else if (button_values[1]) {
      Serial.println("Switching to Keyboard");
      keyboard_mode = true;
      delay(100);
      if(device_installed[(uint8_t)Source::Display]) display.switchMode(1);

      joystick1.x = 0.0;
      joystick1.y = 0.0;
      joystick1.z = 0.0;
      joystick1.rx = 0.0;
      joystick1.ry = 0.0;
      joystick1.rz = 0.0;
      joystick1.slider = 0.0;
      joystick1.dial = 0.0;
      joystick1.wheel = 0.0;
      joystick1.buttons = 0; 
      joystick1_needs_send = true;

      joystick2.x = 0.0;
      joystick2.y = 0.0;
      joystick2.z = 0.0;
      joystick2.rx = 0.0;
      joystick2.ry = 0.0;
      joystick2.rz = 0.0;
      joystick2.slider = 0.0;
      joystick2.dial = 0.0;
      joystick2.wheel = 0.0;
      joystick2.buttons = 0; 
      joystick2_needs_send = true;
      delay(400);
    }

    xQueueOverwrite(queueMode, &keyboard_mode);
    
    unsigned long end = millis();
    Serial.print("  Process Time: "); Serial.println(end - begin);
    Serial.print("     Loop Time: "); Serial.println(end - last_read);
    last_read = end;
    vTaskDelay(5 / portTICK_PERIOD_MS);  // 5ms
  }
}

void taskUpdateHid (void *parameter) {
  for (;;) { 
    unsigned long begin = millis();
    // get axes
    // Serial.println("Get axes");
    int32_t a[axes_size] = {0};
    if (!xQueueReceive(queueAxes2Report, &a, portMAX_DELAY)) {
      Serial.println("Failed to get Axes from queue.");
    }

    // get buttons
    // Serial.println("Get buttons");
    bool b[buttons_size] = {false};
    if (!xQueueReceive(queueButtons2Report, &b, portMAX_DELAY)) {
      Serial.println("Failed to get Buttons from queue.");
    }

    // Update HID reports
    updateHidReports(a, b);

    vTaskDelay(10 / portTICK_PERIOD_MS);  // 5ms    
  }
}

void taskProcessHid(void *parameter) {
  for (;;) {
    if (joystick1_needs_send && usb_hid.ready()) {
      usb_hid.sendReport(JOYSTICK1_ID, &joystick1, sizeof(joystick1));
      joystick1_needs_send = false;
    }
    
    if (joystick2_needs_send && usb_hid.ready()) {
      usb_hid.sendReport(JOYSTICK2_ID, &joystick2, sizeof(joystick2));
      joystick2_needs_send = false;
    }

    if (mouse_needs_send && usb_hid.ready()) {
      usb_hid.mouseReport(MOUSE_ID, mouse_buttons, mouse_x, mouse_y, mouse_v, mouse_h);
      mouse_needs_send = false;
    }
    
    if (keyboard_keys[0] > 0 && usb_hid.ready()) {
      usb_hid.keyboardReport(KEYBOARD_ID, keyboard_modifier, keyboard_keys);
      keyboard_release_pending = true;
    } else if (keyboard_release_pending && usb_hid.ready()) {
      usb_hid.keyboardRelease(KEYBOARD_ID);
      keyboard_release_pending = false;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);  // 5ms
  }
}

void setLeds() {
  // Setup arcade controllers
  for (uint8_t i = 0; i < led_size; ++i) {
    if (leds_source[i] == 0) {
      analogWrite(leds_pins[i], leds_values[i]);
    } else if (device_installed[(uint8_t)Source::ArcadeLeft]  && leds_source[i] == (uint8_t)Source::ArcadeLeft) {
      arcade_left.analogWrite(leds_pins[i], leds_values[i]);
    } else if (device_installed[(uint8_t)Source::ArcadeRight] && leds_source[i] == (uint8_t)Source::ArcadeRight) {
      arcade_right.analogWrite(leds_pins[i], leds_values[i]);
    } else if (device_installed[(uint8_t)Source::EncoderLeft]  && leds_source[i] == (uint8_t)Source::EncoderLeft) {
      encoder_left.analogWrite(leds_pins[i], leds_values[i]);
    } else if (device_installed[(uint8_t)Source::EncoderRight] && leds_source[i] == (uint8_t)Source::EncoderRight) {
      encoder_right.analogWrite(leds_pins[i], leds_values[i]);
    }
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t get_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  // not used in this example
  (void) buffer;
  (void) reqlen;

  // Populate the buffer with led data
  if(report_id == JOYSTICK1_ID) {
    if (report_type == HID_REPORT_TYPE_FEATURE) {
      buffer[0] = report_id;
      for (uint8_t i = 0; i < led_size; ++i) {
        buffer[1 + i] = leds_values[i];
      }
      return 1 + led_size * sizeof(leds_values[0])  ; // Return the number of bytes written
    }
  }
  if(report_id == JOYSTICK2_ID) {
    if (report_type == HID_REPORT_TYPE_FEATURE) {
      buffer[0] = report_id;
      for (uint8_t i = 0; i < led_size; ++i) {
        buffer[1 + i] = leds_values[i];
      }
      return 1 + led_size * sizeof(leds_values[0])  ; // Return the number of bytes written
    }
  }
  return 0; // Unsupported report
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void set_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  // This example doesn't use report ID
  (void) report_id;
   
  // Check if it is the correct report and if SDL sent output data
  if(report_id == JOYSTICK1_ID) {
    if (report_type == HID_REPORT_TYPE_OUTPUT) {
      // Buffer contains the LED/Rumble data from SDL
      for (uint8_t i = 0; i < led_size; ++i) {
        if (bufsize > i) {
          leds_values[i] = buffer[i];
        }
      }
    }
  }

  if(report_id == JOYSTICK2_ID) {
    if (report_type == HID_REPORT_TYPE_OUTPUT) {
      // Buffer contains the LED/Rumble data from SDL
      for (uint8_t i = 0; i < led_size; ++i) {
        if (bufsize > i) {
          leds_values[i] = buffer[i];
        }
      }
    }
  }

  setLeds();
}

void updateHidReports(int32_t *a, bool *b) {
  // #ifdef TINYUSB_NEED_POLLING_TASK
  // // Manual call tud_task since it isn't called by Core's background
  // TinyUSBDevice.task();
  // #endif

  // not enumerated()/mounted() yet: nothing to do
  if (!TinyUSBDevice.mounted()) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    return;
  }

  // Remote wakeup
  if ( TinyUSBDevice.suspended() && (button_values[3] || button_values[4] || button_values[5] || button_values[6] || button_values[7] || button_values[8]) )
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    TinyUSBDevice.remoteWakeup();
  }
  
  // Update active report
  // Keyboard Mode
  if (keyboard_mode) {
    // Serial.println("Keyboard mode");
    // populate pending mousedata
    {
      int8_t mx =  static_cast<int8_t>(a[2]/2048);
      int8_t my = -static_cast<int8_t>(a[3]/2048);
      int8_t mh =  static_cast<int8_t>(a[4]/4096);
      int8_t mv =  static_cast<int8_t>(a[5]/4096);

      if (mx < 2 && mx > -2) mx = 0;
      if (my < 2 && my > -2) my = 0;   
      if (mh < 1 && mh > -1) mh = 0;
      if (mv < 1 && mv > -1) mv = 0;       

      if (mx < -127) mx = -127; if (mx > 127) mx = 127;
      if (my < -127) my = -127; if (my > 127) my = 127;    
      if (mh < -127) mh = -127; if (mh > 127) mh = 127;
      if (mv < -127) mv = -127; if (mv > 127) mv = 127;    
      
      mouse_x = mx;
      mouse_y = my;
      mouse_h = mh;
      mouse_v = mv;
      mouse_buttons = ( b[14] << 0) 
                    | ( b[13] << 1) 
                    | ( b[16] << 2) 
                    // | ( b[21]       << 3)
                    // | ( b[22]       << 4)
                    ;
      mouse_needs_send = true;
    }

    // Determine keyboard matrix mode
    char const (*matrix)[k_keyCols];
    const bool matrix_inc_pressed = b[0];
    const bool matrix_dec_pressed = b[2];
    const unsigned long now = millis();

    if (matrix_inc_pressed && !last_matrix_inc &&
        (now - last_matrix_change_ms) > kMatrixDebounceMs) {
      current_matrix++;
      last_matrix_change_ms = now;
      // Serial.print("current_matrix: "); Serial.println(current_matrix);
    } else if (matrix_dec_pressed && !last_matrix_dec &&
               (now - last_matrix_change_ms) > kMatrixDebounceMs) {
      current_matrix--;
      last_matrix_change_ms = now;
      // Serial.print("current_matrix: "); Serial.println(current_matrix);      
    }

    last_matrix_inc = matrix_inc_pressed;
    last_matrix_dec = matrix_dec_pressed;
    
    if(current_matrix % 3 == 1){
      matrix = k_keyMatrix_L2;
    }else if(current_matrix % 3 == 2) {
      matrix = k_keyMatrix_L3;
    } else {
      matrix = k_keyMatrix_L1;
    }
    
    // populate pending keyboard data
    // Calculate joystick position on keyMatrix
    key_x = static_cast<int8_t>( a[0] * (k_keyCols - 1) * 8 / 2 / 32767);  // characters are on an 8x8 pixel grid
    key_y = static_cast<int8_t>(-a[1] * (k_keyRows - 1) * 8 / 2 / 32767);  // characters are on an 8x8 pixel grid
    key_i = (key_y + (k_keyRows) * 8 / 2 ) / 8;
    key_j = (key_x + (k_keyCols) * 8 / 2 ) / 8;

    uint8_t modifier = 0;
    std::vector<uint8_t> keys;
    keys.reserve(6);  // Reserve space for up to 6 keys
    
    // Capture matrix key if button pressed 
    if (b[12]) {
      modifier     = k_ascii2hid[(uint8_t)matrix[key_i][key_j]][0];
      keys.push_back(k_ascii2hid[(uint8_t)matrix[key_i][key_j]][1]);
    }

    // direct button keys
    if (b[9]) {
      if(keys.size() < 6) keys.push_back(HID_KEY_SHIFT_LEFT);
    }
    if (b[10]) {
      if(keys.size() < 6) keys.push_back(HID_KEY_CONTROL_LEFT);
    }
    if (b[11]) {
      if(keys.size() < 6) keys.push_back(HID_KEY_ALT_LEFT);
    }
    if (b[5]) {
      if(keys.size() < 6) keys.push_back(HID_KEY_HOME);
    }
    if (b[6]) {
      if(keys.size() < 6) keys.push_back(HID_KEY_ESCAPE);
    }
    if (b[7]) {
      if(keys.size() < 6) keys.push_back(HID_KEY_END);
    }
    if (b[8]) {
      if(keys.size() < 6) keys.push_back(HID_KEY_ENTER);
    }
    if (b[15]) {
      if(keys.size() < 6) keys.push_back(HID_KEY_GUI_LEFT);
    }
    if (a[7] > 15000) {
      if(keys.size() < 6) keys.push_back(HID_KEY_ARROW_UP);
    } else if (a[6] >  15000) {
      if(keys.size() < 6) keys.push_back(HID_KEY_ARROW_RIGHT);
    } else if (a[7] < -15000) {
      if(keys.size() < 6) keys.push_back(HID_KEY_ARROW_DOWN);
    } else if (a[6] < -15000) {
      if(keys.size() < 6) keys.push_back(HID_KEY_ARROW_LEFT);
    }

    // reset keyboard report
    keyboard_modifier = 0;
    for (auto& key : keyboard_keys) {
      key = 0;
    }
    // update report with new keys
    if(keys.size() > 0) {
      keyboard_modifier = modifier;
      for (size_t i = 0; i < keys.size(); i++) {
        keyboard_keys[i] = keys[i];
      }
    }


  // Joystick mode
  } else { 
    // Store joystick1 data for later sending via callback
    joystick1.x      =  static_cast<int16_t>(a[0]);
    joystick1.y      = -static_cast<int16_t>(a[1]);
    joystick1.z      =  static_cast<int16_t>(a[2]);
    joystick1.rx     = -static_cast<int16_t>(a[3]);
    joystick1.ry     =  static_cast<int16_t>(a[4]);
    joystick1.rz     = -static_cast<int16_t>(a[5]);
    joystick1.slider =  static_cast<int16_t>(a[6]);
    joystick1.dial   = -static_cast<int16_t>(a[7]);
    joystick1.wheel  =  static_cast<int16_t>(a[8]);
    // store buttons
    joystick1.buttons = ( b[0] << 0) 
                      | ( b[1] << 1) 
                      | ( b[2] << 2) 
                      | ( b[3] << 3)
                      | ( b[4] << 4) 
                      | ( b[5] << 5) 
                      | ( b[6] << 6) 
                      | ( b[7] << 7)                    
                      | ( b[8] << 8) 
                      | ( b[9] << 9) 
                      | ( b[10] << 10) 
                      | ( b[11] << 11) 
                      | ( b[12] << 12) 
                      | ( b[13] << 13) 
                      | ( b[14] << 14) 
                      | ( b[15] << 15)
                      | ( b[16] << 16)
                      | ( b[17] << 17)
                      | ( b[18] << 18)
                      | ( b[19] << 19)
                      | ( b[20] << 20)                     
                      ;

    // Flag joystick1 for pending send
    joystick1_needs_send = true;

    // Store joystick2 data for later sending via callback
    joystick2.x      =  static_cast<int16_t>(a[11]);
    joystick2.y      =  static_cast<int16_t>(a[12]);
    joystick2.z      =  static_cast<int16_t>(a[13]);
    joystick2.rx     =  static_cast<int16_t>(a[14]);
    joystick2.ry     =  static_cast<int16_t>(a[15]);
    joystick2.rz     =  static_cast<int16_t>(a[16]);
    joystick2.slider =  static_cast<int16_t>(a[9]);
    joystick2.dial   =  static_cast<int16_t>(a[10]);
    joystick2.wheel  =  0;

    joystick2.buttons = ( b[21] << 21)
                      | ( b[22] << 22) 
                      ;

    joystick2_needs_send = true;  // Flag that joystick2 needs to be sent
  }    
}

void setup() {
  // Set USB Description
  TinyUSBDevice.setID(0xFFFF, 0x0002);
  TinyUSBDevice.setManufacturerDescriptor("Nosille's Stuff");
  TinyUSBDevice.setProductDescriptor("Super Joystick");
  TinyUSBDevice.setSerialDescriptor("0001");
  TinyUSBDevice.setVersion(0.0);

  // Start serial
  Serial.begin(115200);

  // Manual begin() is required on core without built-in support e.g. mbed rp2040
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }

  // Set up HID
  usb_hid.setPollInterval(10); // Poll every 20ms
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor("Nosille's Joystick");
  usb_hid.setReportCallback(get_report_callback, set_report_callback);
  usb_hid.begin();

  // If already enumerated, additional class driver begin() e.g msc, hid, midi won't take effect until re-enumeration
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }

  // Connect to MCP3208
  if (!SPI.begin() || !mcp3208.begin(MCP3208_CS)) {
    Serial.println(F("MCP3208 not found!"));
  } else {
    device_installed[(uint8_t)Source::MCP3208] = true;
    Serial.println("MCP3208 configured.");
  }

  // Connect to arcade controllers
  if (!arcade_left.begin(ARCADE_ADDR_L)) {
    Serial.println(F("Left arcade controller not found!"));
  } else {
    uint16_t pid;
    uint8_t year, mon, day;
    arcade_left.getProdDatecode(&pid, &year, &mon, &day);
    if (pid != 5690) {
      Serial.println("Wrong PID for left arcade controller!");
    } else {
       device_installed[(uint8_t)Source::ArcadeLeft] = true;
       Serial.println("Left arcade controller configured.");
    }
  }
  
  if (!arcade_right.begin(ARCADE_ADDR_R)) {
    Serial.println(F("Right arcade controller not found!"));
  } else {
    uint16_t pid;
    uint8_t year, mon, day;
    arcade_right.getProdDatecode(&pid, &year, &mon, &day);
    if (pid != 5690) {
      Serial.println("Wrong PID for right arcade controller!");
    } else {
      device_installed[(uint8_t)Source::ArcadeRight] = true;
      Serial.println("Right arcade controller configured.");
    }
  }

  // Connect to rotary encoder
  if (!encoder_left.begin(ENCODER_ADDR_L) || ! encoder_pixel_left.begin(ENCODER_ADDR_L)) {
    Serial.println("Couldn't find left rotary encoder");
  } else {
    uint16_t pid;
    uint8_t year, mon, day;
    encoder_left.getProdDatecode(&pid, &year, &mon, &day);
    if (pid != 4991) {
      Serial.println("Wrong PID for left rotary encoder!");
    } else {
      device_installed[(uint8_t)Source::EncoderLeft] = true;
      Serial.println("Left rotary encoder configured.");
    }
  }

  if (!encoder_right.begin(ENCODER_ADDR_R) || ! encoder_pixel_right.begin(ENCODER_ADDR_R)) {
    Serial.println("Couldn't find right rotary encoder");
  } else {
    uint16_t pid;
    uint8_t year, mon, day;
    encoder_right.getProdDatecode(&pid, &year, &mon, &day);
    if (pid != 4991) {
      Serial.println("Wrong PID for right rotary encoder!");
    } else {
      device_installed[(uint8_t)Source::EncoderRight] = true;
      Serial.println("Right rotary encoder configured.");
    }
  }

  // Connect to imu
  if (!bno.begin()) {
    Serial.println("Couldn't find imu");
  } else {
    device_installed[(uint8_t)Source::Imu] = true;
    Serial.println("Imu configured.");
  }

  // Connect to OLED Display
  if (!display.begin()) {
    Serial.println("Couldn't find display!");
  } else {
    device_installed[(uint8_t)Source::Display] = true;
    Serial.println("OLED configured.");
  }

  // Turn up i2c speeds
  Wire.setClock(400000L); // Increase I2C speed to 400kHz

  // Setup axes pins
  for (uint8_t i = 0; i < axes_size; i++) {
    if(axes_source[i] == 0) {
      pinMode(axes_pin[i], INPUT);
    } else if (axes_source[i] == (uint8_t)Source::ArcadeLeft && device_installed[(uint8_t)Source::ArcadeLeft]) {
      arcade_left.pinMode(axes_pin[i], INPUT);
    } else if (axes_source[i] == (uint8_t)Source::ArcadeRight && device_installed[(uint8_t)Source::ArcadeRight]) {
      arcade_right.pinMode(axes_pin[i], INPUT);
    } else if (axes_source[i] == (uint8_t)Source::EncoderLeft && device_installed[(uint8_t)Source::EncoderLeft]) {
      encoder_left.pinMode(axes_pin[i], INPUT);
    } else if (axes_source[i] == (uint8_t)Source::EncoderRight && device_installed[(uint8_t)Source::EncoderRight]) {
      encoder_right.pinMode(axes_pin[i], INPUT);
    }
  }

  // Setup button pins
  touchSetDefaultThreshold(5);
  uint32_t arcade_left_interrupt_mask = 0;
  uint32_t arcade_right_interrupt_mask = 0;
  uint32_t encoder_interrupt_mask = 0;
  for (uint8_t i = 0; i < buttons_size; i++) {
    if(buttons_source[i] == (uint8_t)Source::Digital) {
      pinMode(buttons_pin[i], INPUT_PULLUP);
    } else if (buttons_source[i] == (uint8_t)Source::ADC) {
      // No configuration required for ADC voltage readings
    } else if (buttons_source[i] == (uint8_t)Source::Touch) {
      touchAttachInterruptArg(buttons_pin[i], [](void* arg) { interruptSource(arg); }, (void*)buttons_source[i], touch_threshold); 
    } else if (buttons_source[i] == (uint8_t)Source::ArcadeLeft && device_installed[(uint8_t)Source::ArcadeLeft]) {
      arcade_left.pinMode(buttons_pin[i], INPUT_PULLUP);
      arcade_left_interrupt_mask |= (1UL << buttons_pin[i]);
    } else if (buttons_source[i] == (uint8_t)Source::ArcadeRight && device_installed[(uint8_t)Source::ArcadeRight]) {
      arcade_right.pinMode(buttons_pin[i], INPUT_PULLUP);
      arcade_right_interrupt_mask |= (1UL << buttons_pin[i]);
    } else if (buttons_source[i] == (uint8_t)Source::EncoderLeft && device_installed[(uint8_t)Source::EncoderLeft]) {
      encoder_left.pinMode(buttons_pin[i], INPUT_PULLUP);
      encoder_interrupt_mask |= (1UL << buttons_pin[i]);
    } else if (buttons_source[i] == (uint8_t)Source::EncoderRight && device_installed[(uint8_t)Source::EncoderRight]) {
      encoder_right.pinMode(buttons_pin[i], INPUT_PULLUP);
      encoder_interrupt_mask |= (1UL << buttons_pin[i]);
    }
  }

  // Setup arcade controllers
  if (device_installed[(uint8_t)Source::ArcadeLeft]) {
    pinMode(interrupt_pins[(uint8_t)Source::ArcadeLeft], INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(interrupt_pins[(uint8_t)Source::ArcadeLeft]), 
                      [](void* arg) { interruptSource(arg); }, (void*)(uint8_t)Source::ArcadeLeft, FALLING);
    arcade_left.setGPIOInterrupts(arcade_left_interrupt_mask, 1);
  }
  if (device_installed[(uint8_t)Source::ArcadeRight]) {
    pinMode(interrupt_pins[(uint8_t)Source::ArcadeRight], INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(interrupt_pins[(uint8_t)Source::ArcadeRight]), 
                      [](void* arg) { interruptSource(arg); }, (void*)(uint8_t)Source::ArcadeRight, FALLING);    
    arcade_right.setGPIOInterrupts(arcade_right_interrupt_mask, 1);
  }

  // Setup rotary encoder
  if (device_installed[(uint8_t)Source::EncoderLeft]) {
    pinMode(interrupt_pins[(uint8_t)Source::EncoderLeft], INPUT_PULLUP);    
    attachInterruptArg(digitalPinToInterrupt(interrupt_pins[(uint8_t)Source::EncoderLeft]), 
                      [](void* arg) { interruptSource(arg); }, (void*)(uint8_t)Source::EncoderLeft, FALLING);     
    encoder_left.setGPIOInterrupts(encoder_interrupt_mask, 1);
    encoder_left.enableEncoderInterrupt();
    encoder_pixel_left.setBrightness(20);
    encoder_pixel_left.show();
  }

  if (device_installed[(uint8_t)Source::EncoderRight]) {
    pinMode(interrupt_pins[(uint8_t)Source::EncoderRight], INPUT_PULLUP);    
    attachInterruptArg(digitalPinToInterrupt(interrupt_pins[(uint8_t)Source::EncoderRight]), 
                      [](void* arg) { interruptSource(arg); }, (void*)(uint8_t)Source::EncoderRight, FALLING);     
    encoder_right.setGPIOInterrupts(encoder_interrupt_mask, 1);
    encoder_right.enableEncoderInterrupt();
    encoder_pixel_right.setBrightness(20);
    encoder_pixel_right.show();
  }

  // Setup IMU
  if(device_installed[(uint8_t)Source::Imu]) {
    bno.setExtCrystalUse(true);
  }

  // Setup display
  if(device_installed[(uint8_t)Source::Display]) {
    display.setup();
  }

  setLeds();

  // Create queues
  queueAxes2Report = xQueueCreate(QUEUE_SIZE, axes_size * sizeof(int32_t));
  queueAxes2Display = xQueueCreate(QUEUE_SIZE, axes_size * sizeof(int32_t));  
  if (queueAxes2Report == NULL || queueAxes2Display == NULL) {
    Serial.println("Failed to create axes queue!");
    while (1);
  }

  queueButtons2Report = xQueueCreate(QUEUE_SIZE, buttons_size * sizeof(bool));
  queueButtons2Display = xQueueCreate(QUEUE_SIZE, buttons_size * sizeof(bool));  
  if (queueButtons2Report == NULL || queueButtons2Display == NULL) {
    Serial.println("Failed to create button queue!");
    while (1);
  }

  queueMode = xQueueCreate(QUEUE_SIZE, sizeof(bool));
  if (queueMode == NULL) {
    Serial.println("Failed to create mode queue!");
    while (1);
  }

  // Create tasks
  xTaskCreatePinnedToCore(
    taskReadInputs,
    "inputsTask",
    2000,  // Task stack
    NULL,
    1,
    &taskInputsHandle,
    1  // Core
  );

  xTaskCreatePinnedToCore(
    taskUpdateHid,
    "hidUpdateTask",
    2000,  // Task stack
    NULL,
    1,
    &taskReportHandle,
    1  // Core
  );
  
  xTaskCreatePinnedToCore(
    taskProcessHid,
    "hidProcessTask",
    2000,  // Task stack
    NULL,
    1,
    &taskReportHandle,
    1  // Core
  );

  last_display = millis();
  Serial.println("Setup complete!");
}

void loop() {
  // get axes
  // Serial.println("Get axes");
  int32_t a[axes_size] = {0};
  if (!xQueueReceive(queueAxes2Display, &a, portMAX_DELAY)) {
    Serial.println("Failed to get Axes from queue.");
  }

  // get buttons
  // Serial.println("Get buttons");
  bool b[buttons_size] = {false};
  if (!xQueueReceive(queueButtons2Display, &b, portMAX_DELAY)) {
    Serial.println("Failed to get Buttons from queue.");
  }

  // get mode
  bool mode = false;
  if (!xQueueReceive(queueMode, &mode, portMAX_DELAY)) {
    Serial.println("Failed to get Mode from queue.");
  }

  // Update display
  if(device_installed[(uint8_t)Source::Display]) {
    if (mode) {
      display.updateKeyboard(&current_matrix, a, b);
    } else { 
      display.updateJoystick(a, b);
    }
  }

  // display info
  if(device_installed[(uint8_t)Source::Display]) {
    unsigned long timestamp = millis(); 
    display.updateInfo(timestamp, last_display);
    last_display = timestamp;      
  }

  // if(device_installed[(uint8_t)Source::EncoderLeft]) {
  //   encoder_pixel_left.setPixelColor(0, ColorWheel(encoder_pixel_left, a[axes_size - 5] & 0xFF));
  //   encoder_pixel_left.show();
  // }

  // if(device_installed[(uint8_t)Source::EncoderRight]) {
  //   encoder_pixel_right.setPixelColor(0, ColorWheel(encoder_pixel_right, a[axes_size - 4] & 0xFF));
  //   encoder_pixel_right.show();
  // }
   
  yield();
}

uint32_t ColorWheel(seesaw_NeoPixel &pixel, byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return pixel.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return pixel.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return pixel.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
