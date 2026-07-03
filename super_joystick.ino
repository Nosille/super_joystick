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
#define JOYSTICK_ID   0X02
#define KEYBOARD_ID   0X03

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

static const uint8_t axes_source[] = { 3,  3,  3,  3,  3,  3,  3,  3,  1,  6,  7,  9,  9,  9};
static const uint8_t axes_pin[]    = { 2,  3,  0,  1,  6,  7,  4,  5,  8,  0,  0,  0,  1,  2};
static const uint8_t axes_size = sizeof(axes_pin) / sizeof(axes_pin[0]);
static const uint8_t buttons_source[] = { 0,  0,  0,  2,  2,  4,  4,  4,  4,  5,  5,  5,  5,  4,  5,  4,  5,  6,  7};
static const uint8_t buttons_pin[]    = { 9,  6,  5, 14, 11,  0,  1,  2,  3,  0,  1,  2,  3,  4,  4,  5,  5, 24, 24};
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
    MY_HID_REPORT_DESC_JOYSTICK(HID_REPORT_ID(JOYSTICK_ID)),
    MY_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(KEYBOARD_ID))
};

Adafruit_USBD_HID usb_hid;
TaskHandle_t taskInputsHandle = NULL;

// Queues
QueueHandle_t queueAxes = NULL;
QueueHandle_t queueButtons = NULL;
QueueHandle_t queueMode = NULL;

// Global variables
int32_t axis_values[axes_size] = {0}; 
bool button_values[buttons_size] = {false};
int8_t leds_values[led_size]  = {32}; // current brightness of leds, 0 to 255
bool keyboard_mode = false;
int8_t has_keys = 0;  // If keys were sent in last hid report

int8_t key_x = 0;      // Current keyboard position
int8_t key_y = 0;      // Current keyboard position
uint8_t key_i = 0;     // Current key row
uint8_t key_j = 0;     // Current key col

unsigned long last_read;
unsigned long last_display; 

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
    sensors_event_t imu_event;
    if(device_installed[(uint8_t)Source::Imu]) {
      bno.getEvent(&imu_event);
    }

    // Read axes values from sources
    for (uint8_t i = 0; i < axes_size; ++i) {
      if (axes_source[i] == (uint8_t)Source::ADC) {
        axis_values[i] = analogRead(axes_pin[i]);
      } else if (axes_source[i] == (uint8_t)Source::MCP3208) {
        axis_values[i] = mcp3208.readADC(axes_pin[i]);
      } else if (axes_source[i] == (uint8_t)Source::ArcadeLeft  && device_installed[(uint8_t)Source::ArcadeLeft]   && interrupt[(uint8_t)Source::ArcadeLeft]) {
        axis_values[i] = arcade_left.analogRead(axes_pin[i]);
      } else if (axes_source[i] == (uint8_t)Source::ArcadeRight && device_installed[(uint8_t)Source::ArcadeRight]  && interrupt[(uint8_t)Source::ArcadeLeft]) {
        axis_values[i] = arcade_right.analogRead(axes_pin[i]);
      } else if (axes_source[i] == (uint8_t)Source::EncoderLeft && device_installed[(uint8_t)Source::EncoderLeft]  && interrupt[(uint8_t)Source::EncoderLeft]) {
        axis_values[i] = encoder_left.getEncoderPosition();
      } else if (axes_source[i] == (uint8_t)Source::EncoderRight && device_installed[(uint8_t)Source::EncoderRight] && interrupt[(uint8_t)Source::EncoderRight]) {
        axis_values[i] = encoder_right.getEncoderPosition();
      } else if (axes_source[i] == (uint8_t)Source::Imu         && device_installed[(uint8_t)Source::Imu]) {
        if(axes_pin[i] == 0) axis_values[i] = imu_event.orientation.x;
        if(axes_pin[i] == 1) axis_values[i] = imu_event.orientation.y;
        if(axes_pin[i] == 2) axis_values[i] = imu_event.orientation.z;
      }
    }

    // Normalize axes values to -32767 to 32767 with center at 0 (16 bit) 
    for (uint8_t i = 0; i < axes_size; ++i) {
      if(axes_source[i] == (uint8_t)Source::ADC ||
         axes_source[i] == (uint8_t)Source::MCP3208) {
          axis_values[i] = (axis_values[i] - 2048) * 16.0;
          axis_values[i] = std::clamp(axis_values[i], -32767L, 32767L);
      } else if(axes_source[i] == (uint8_t)Source::ArcadeLeft ||
                axes_source[i] == (uint8_t)Source::ArcadeRight) {
          axis_values[i] = (axis_values[i] - 512) * 64.0;
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

    xQueueOverwrite(queueAxes, &axis_values);
    xQueueOverwrite(queueButtons, &button_values);

    // #ifdef TINYUSB_NEED_POLLING_TASK
    // // Manual call tud_task since it isn't called by Core's background
    // TinyUSBDevice.task();
    // #endif

    // not enumerated()/mounted() yet: nothing to do
    if (!TinyUSBDevice.mounted()) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    // Remote wakeup
    if ( TinyUSBDevice.suspended() && (button_values[3] || button_values[4] || button_values[5] || button_values[6] || button_values[7] || button_values[8]) )
    {
      // Wake up host if we are in suspend mode
      // and REMOTE_WAKEUP feature is enabled by host
      TinyUSBDevice.remoteWakeup();
    }

    // Update HID reports
    updateHidReports(axis_values, button_values);

    // switch modes with debounce
    if (button_values[0] && keyboard_mode) {
      Serial.println("Switching to Joystick");
      keyboard_mode = false;
      if(device_installed[(uint8_t)Source::Display]) display.switchMode(0);

      if(usb_hid.ready()) {
        usb_hid.mouseReport(MOUSE_ID, 0, 0, 0, 0, 0);
      }
      delay(10);

      if(usb_hid.ready() && has_keys) {    
        usb_hid.keyboardRelease(KEYBOARD_ID);
        has_keys = false;
      }
      delay(490);
      
    } else if (button_values[0]) {
      Serial.println("Switching to Keyboard");
      keyboard_mode = true;
      if(device_installed[(uint8_t)Source::Display]) display.switchMode(1);

      my_hid_report_t joystick;
      joystick.l1x = 0.0;
      joystick.l1y = 0.0;
      joystick.r1x = 0.0;
      joystick.r1y = 0.0;
      joystick.l2x = 0.0;
      joystick.l2y = 0.0;
      joystick.r2x = 0.0;
      joystick.r2y = 0.0;
      joystick.pot = 0.0;
      joystick.buttons = 0; 
      
      if(usb_hid.ready()) {  
        usb_hid.sendReport(JOYSTICK_ID, &joystick, sizeof(joystick));  
      }
      delay(500);
    }

    xQueueOverwrite(queueMode, &keyboard_mode);
    

    unsigned long end = millis();
    // Serial.print("  Process Time: "); Serial.println(end - begin);
    // Serial.print("     Loop Time: "); Serial.println(end - last_read);
    last_read = end;
    vTaskDelay(10 / portTICK_PERIOD_MS);  // 10ms
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
  if(report_id == JOYSTICK_ID) {
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
  if(report_id == JOYSTICK_ID) {
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
  // Update active report
  // Keyboard Mode
  if (keyboard_mode) {
    // Serial.println("Keyboard mode");
    // Mouse report
    if (usb_hid.ready()) {
      int8_t mouse_x =  static_cast<int8_t>(a[2]/1028);
      int8_t mouse_y = -static_cast<int8_t>(a[3]/1028);
      int8_t mouse_h =  static_cast<int8_t>(a[6]/4096);
      int8_t mouse_v =  static_cast<int8_t>(a[7]/4096);

      if (mouse_x < 2 && mouse_x > -2) mouse_x = 0;
      if (mouse_y < 2 && mouse_y > -2) mouse_y = 0;   
      if (mouse_h < 1 && mouse_h > -1) mouse_h = 0;
      if (mouse_v < 1 && mouse_v > -1) mouse_v = 0;       

      if (mouse_x < -127) mouse_x = -127; if (mouse_x > 127) mouse_x = 127;
      if (mouse_y < -127) mouse_y = -127; if (mouse_y > 127) mouse_y = 127;    
      if (mouse_h < -127) mouse_h = -127; if (mouse_h > 127) mouse_h = 127;
      if (mouse_v < -127) mouse_v = -127; if (mouse_v > 127) mouse_v = 127;    
      
      uint8_t buttons  =  ( b[4] << 0) 
                        | ( b[6] << 1) 
                        | ( b[5] << 2) 
                        // | ( b[10]       << 3)
                        // | ( b[11]       << 4)
                        ;
      // send mouse report
      usb_hid.mouseReport(MOUSE_ID, buttons, mouse_x, mouse_y, mouse_v, mouse_h);
      delay(10); // delay before trying keyboard report
    }  else {
      Serial.println("Mouse not ready!");
    }

    // Determine keyboard matrix mode
    char const (*matrix)[k_keyCols];
    // Serial.print("lastEnc: "); Serial.println(lastEnc);   
    if(abs(a[axes_size - 1]) % 3 == 1) {
      matrix = k_keyMatrix_L2;
    }else if(abs(a[axes_size - 1]) % 3 == 2) {
      matrix = k_keyMatrix_L3;
    } else {
      matrix = k_keyMatrix_L1;
    }
    
    // Keyboard report
    if (usb_hid.ready()) {
      // Caculate joystick position on keyMatrix
      key_x = static_cast<int8_t>( a[0] * (k_keyCols - 1) * 8 / 2 / 32767);  // characters are 5 pixels wide with a 1 pixel gap for 6 center to center
      key_y = static_cast<int8_t>(-a[1] * (k_keyRows - 1) * 8 / 2 / 32767);  // characters are 7 pixels tall with a 1 pixel gap for 8 center to center
      key_i = (key_y + (k_keyRows) * 8 / 2 ) / 8;
      key_j = (key_x + (k_keyCols) * 8 / 2 ) / 8;

      int count = 0;
      uint8_t modifier = 0;
      uint8_t keys[6] = { 0 }; // Be careful not to exceed the report limit of 6 keys
      
      // Capture matrix key if button pressed 
      if (b[18]) {
        modifier      = k_ascii2hid[(uint8_t)matrix[key_i][key_j]][0];
        keys[count++] = k_ascii2hid[(uint8_t)matrix[key_i][key_j]][1];
      }

      // direct button keys
      if (b[15]) {
        if(count < 6) keys[count++] = HID_KEY_SHIFT_LEFT;
      }
      if (b[16]) {
        if(count < 6) keys[count++] = HID_KEY_CONTROL_LEFT;
      }
      if (b[17]) {
        if(count < 6) keys[count++] = HID_KEY_ALT_LEFT;
      }
      if (b[9]) {
        if(count < 6) keys[count++] = HID_KEY_HOME;
      }
      if (b[10]) {
        if(count < 6) keys[count++] = HID_KEY_ESCAPE;
      }
      if (b[11]) {
        if(count < 6) keys[count++] = HID_KEY_END;
      }
      if (b[12]) {
        if(count < 6) keys[count++] = HID_KEY_ENTER;
      }
      if (b[3]) {
        if(count < 6) keys[count++] = HID_KEY_GUI_LEFT;
      }
      if (a[5] > 15000) {
        if(count < 6) keys[count++] = HID_KEY_ARROW_UP;
      } else if (a[4] >  15000) {
        if(count < 6) keys[count++] = HID_KEY_ARROW_RIGHT;
      } else if (a[5] < -15000) {
        if(count < 6) keys[count++] = HID_KEY_ARROW_DOWN;
      } else if (a[4] < -15000) {
        if(count < 6) keys[count++] = HID_KEY_ARROW_LEFT;
      }

      // Send keyboard report
      if(count > 0) {
        usb_hid.keyboardReport(KEYBOARD_ID, modifier, keys);
        has_keys = true;
      } else if (has_keys) {
        // send empty key report if previously has key pressed
        usb_hid.keyboardRelease(KEYBOARD_ID);
        has_keys = false;
      }
    } else {
      Serial.println("Keyboard not ready!");
    }


  // Joystick mode
  } else { 
    // Serial.println("Joystick Mode");
    my_hid_report_t joystick;
    joystick.l1x =  static_cast<int16_t>(a[0]);
    joystick.l1y = -static_cast<int16_t>(a[1]);
    joystick.r1x =  static_cast<int16_t>(a[2]);
    joystick.r1y = -static_cast<int16_t>(a[3]);
    joystick.l2x =  static_cast<int16_t>(a[4]);
    joystick.l2y = -static_cast<int16_t>(a[5]);
    joystick.r2x =  static_cast<int16_t>(a[6]);
    joystick.r2y = -static_cast<int16_t>(a[7]);
    joystick.pot =  static_cast<int16_t>(a[8]);

    // store buttons
    joystick.buttons  = ( b[0] << 0) 
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

    // Send joystick report
    if (usb_hid.ready()) {
      usb_hid.sendReport(JOYSTICK_ID, &joystick, sizeof(joystick));  
    } else {
      Serial.println("Joystick not ready!");
    }
  }    
}

void setup() {
  // Start serial
  Serial.begin(115200);

  // Set USB Description
  TinyUSBDevice.setID(0xFFFF, 0x0000);
  TinyUSBDevice.setManufacturerDescriptor("Nosille's Stuff");
  TinyUSBDevice.setProductDescriptor("Super Joystick");
  TinyUSBDevice.setVersion(0.0);
  TinyUSBDevice.setSerialDescriptor("0001");

  // Manual begin() is required on core without built-in support e.g. mbed rp2040
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }

  // Set up HID
  usb_hid.setPollInterval(10); // Poll every 20ms
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor("TinyUSB HID Composite");
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
    if(buttons_source[i] == 0) {
      pinMode(buttons_pin[i], INPUT_PULLUP);
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
    pinMode(interrupt_pins[(uint8_t)Source::EncoderRight], INPUT_PULLUP);    
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
    encoder_pixel_left.setBrightness(20);
    encoder_pixel_left.show();
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
  queueAxes = xQueueCreate(QUEUE_SIZE, axes_size * sizeof(int32_t));
  if (queueAxes == NULL) {
    Serial.println("Failed to create axes queue!");
    while (1);
  }

  queueButtons = xQueueCreate(QUEUE_SIZE, buttons_size * sizeof(bool));
  if (queueButtons == NULL) {
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

  last_display = millis();
  Serial.println("Setup complete!");
}

void loop() {
  // get axes
  // Serial.println("Get axes");
  int32_t a[axes_size] = {0};
  if (!xQueueReceive(queueAxes, &a, portMAX_DELAY)) {
    Serial.println("Failed to get Axes from queue.");
  }
  // get buttons
  // Serial.println("Get buttons");
  bool b[buttons_size] = {false};
  if (!xQueueReceive(queueButtons, &b, portMAX_DELAY)) {
    Serial.println("Failed to get Buttons from queue.");
  }

  // get mode
  bool mode = false;
  if (!xQueueReceive(queueMode, &mode, portMAX_DELAY)) {
    Serial.println("Failed to get Mode from queue.");
  }  

  if(device_installed[(uint8_t)Source::Display]) {
    if (mode) {
      display.updateKeyboard(a, b);
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

  if(device_installed[(uint8_t)Source::EncoderLeft]) {
    encoder_pixel_left.setPixelColor(0, ColorWheel(encoder_pixel_left, a[axes_size - 5] & 0xFF));
    encoder_pixel_left.show();
  }

  if(device_installed[(uint8_t)Source::EncoderRight]) {
    encoder_pixel_right.setPixelColor(0, ColorWheel(encoder_pixel_right, a[axes_size - 4] & 0xFF));
    encoder_pixel_right.show();
  }
   
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
