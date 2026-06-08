#include <USB.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>

#include "hid_mouse_description.h"
#include "hid_joystick_description.h"
#include "hid_keyboard_description.h"

// I2C_ADDR's
#define SH1107_ADDR   0x3C // Address 0x3C default
#define ARCADE_ADDR_L 0x3E // Address 0x3A default
#define ARCADE_ADDR_R 0x3B // Address 0x3A default
#define ENCODER_ADDR  0x36 // Address 0x36 default

// Report ID's
#define MOUSE_ID      0X01
#define JOYSTICK_ID   0X02
#define KEYBOARD_ID   0X03

// Pins
static const uint8_t axes[] = {18, 17, 16, 15, 13, 12, 11, 10, 14};
static const uint8_t buttons[] = {9, 6, 8, 5, 35, 37, 38, 39};
static const uint8_t axes_size = sizeof(axes) / sizeof(axes[0]);
static const uint8_t buttons_size = sizeof(buttons) / sizeof(buttons[0]);
static const uint8_t arcade_switchs[] = {18, 19, 20, 2};
static const uint8_t arcade_leds[] = {12, 13, 0, 1};
static const uint8_t encoder_switch = 24;
static const uint8_t encoder_led = 6;

// Devices
Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);
Adafruit_seesaw arcade_left;
Adafruit_seesaw arcade_right;
Adafruit_seesaw encoder;
seesaw_NeoPixel encoder_pixel = seesaw_NeoPixel(1, encoder_led, NEO_GRB + NEO_KHZ800);

bool display_installed = false;
bool arcade_left_installed = false;
bool arcade_right_installed = false;
bool encoder_installed = false;

// HID report descriptor using TinyUSB's templates
uint8_t const desc_hid_report[] = {
    MY_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(MOUSE_ID)),
    MY_HID_REPORT_DESC_JOYSTICK(HID_REPORT_ID(JOYSTICK_ID)),
    MY_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(KEYBOARD_ID))
};

// USB HID object
Adafruit_USBD_HID usb_hid;

// Keyboard character list
char const keyMatrix[8][10] = {
  // bksp  lf   cr  esc  tab  home end ^home ^end del
    {0x08,0x0A,0x0D,0x1B,0x09,0x02,0x03,0x01,0x04,0x7F},
    { '~', '!', '@', '#', '$', '%', '^', '&', '*', '|'},
    { '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'}, 
    { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'},
    { 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T'},
    { 'U', 'V', 'W', 'X', 'Y', 'Z', '-', '+', '=', '_'},
    {'\{','\}', '[', ']', '(', ')', '<', '>', '/','\\'},
    { ' ', '.', ',', ':', ';','\`','\'','\"',0xF8, '?'},
};
uint8_t const keyRows = sizeof(keyMatrix) / sizeof(keyMatrix[0]);
uint8_t const keyCols = sizeof(keyMatrix[0]) / sizeof(keyMatrix[0][0]);
uint8_t const ascii2hid[128][2] = { ASCII_TO_KEYCODE };

// Global variables
my_hid_report_t joystick;
int mouse_mode = false;
int32_t lastEnc = 0;
int8_t currentLeds[] = {32, 32, 32, 32, 32, 32, 32, 32}; // current brightness of leds, 0 to 255
int8_t has_keys = 0;  // If keys were sent in last hid report
int8_t key_x = 0;      // Current keyboard position
int8_t key_y = 0;      // Current keyboard position
uint8_t key_i = 0;     // Current key row
uint8_t key_j = 0;     // Current key col
int8_t mouse_x = 0;    // Mouse delta x
int8_t mouse_y = 0;    // Mouse delta y
int8_t mouse_h = 0;    // Mouse scroll horizontal
int8_t mouse_v = 0;    // Mouse scroll vertical

void setup() {
  // Start serial
  Serial.begin(115200);
    
  // Allow time for the display to power up and communications to begin
  delay(1000);

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
  usb_hid.setPollInterval(2);
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

  // Connect to arcade controllers
  if (!arcade_left.begin(ARCADE_ADDR_L)) {
    Serial.println(F("Left arcade controller not found!"));
  } else {
    uint16_t pid;
    uint8_t year, mon, day;
    arcade_left.getProdDatecode(&pid, &year, &mon, &day);
    if (pid != 5296) {
      Serial.println("Wrong PID for left arcade controller!");
    } else {
       arcade_left_installed = true;
       Serial.println("Left arcade controller configured.");
    }
  }
  
  if (!arcade_right.begin(ARCADE_ADDR_R)) {
    Serial.println(F("Right arcade controller not found!"));
  } else {
    uint16_t pid;
    uint8_t year, mon, day;
    arcade_left.getProdDatecode(&pid, &year, &mon, &day);
    if (pid != 5296) {
      Serial.println("Wrong PID for right arcade controller!");
    } else {
      arcade_right_installed = true;
      Serial.println("Right arcade controller configured.");
    }
  }

  // Connect to rotary encoder
  if (! encoder.begin(ENCODER_ADDR) || ! encoder_pixel.begin(ENCODER_ADDR)) {
    Serial.println("Couldn't find rotary encoder");
  } else {
    uint16_t pid;
    uint8_t year, mon, day;
    encoder.getProdDatecode(&pid, &year, &mon, &day);
    if (pid != 4991) {
      Serial.println("Wrong PID for rotary encoder!");
    } else {
      encoder_installed = true;
      Serial.println("rotary encoder configured.");
    }
  }

  // Connect to OLED Display
  if (!display.begin(SH1107_ADDR, true)) {
    Serial.println("Couldn't find display!");
  } else {
    display_installed = true;
    Serial.println("OLED configured.");
  }

  // Setup axes pins
  for (uint8_t i; i < axes_size; i++) {
    pinMode(axes[i], INPUT);
  }

  // Setup button pins
  for (uint8_t i; i < buttons_size; i++) {
    pinMode(buttons[i], INPUT_PULLUP);
  }

  // Setup arcade controllers
  if(arcade_left_installed) {
    arcade_left.pinMode(arcade_switchs[0], INPUT_PULLUP);
    arcade_left.pinMode(arcade_switchs[1], INPUT_PULLUP);
    arcade_left.pinMode(arcade_switchs[2], INPUT_PULLUP);
    arcade_left.pinMode(arcade_switchs[3], INPUT_PULLUP);
  }

  if(arcade_right_installed) {
    arcade_right.pinMode(arcade_switchs[0], INPUT_PULLUP);
    arcade_right.pinMode(arcade_switchs[1], INPUT_PULLUP);
    arcade_right.pinMode(arcade_switchs[2], INPUT_PULLUP);
    arcade_right.pinMode(arcade_switchs[3], INPUT_PULLUP);
  }

  // Setup rotary encoder
  if (encoder_installed) {
    encoder.pinMode(encoder_switch, INPUT_PULLUP);
    encoder_pixel.setBrightness(20);
    encoder_pixel.show();

    delay(10);
    encoder.setGPIOInterrupts((uint32_t)1 << encoder_switch, 1);
    encoder.enableEncoderInterrupt();
  }

  // Setup display
  if(display_installed) {
    display.display();  // I believe this shows the adafruit splash
    delay(1000);

    // Clear the buffer.
    display.clearDisplay();
    display.display();

    // Setup display to show joystick data
    display.setRotation(0);
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0,0);
    display.print("Starting!");
    display.display(); // actually display all of the above
  }

  setLeds();

  Serial.println("Setup complete!");
}

void loop() {
  delay(20);

  // pol axes
  int32_t L1X = analogRead(axes[0]);
  int32_t L1Y = analogRead(axes[1]);
  int32_t L2X = analogRead(axes[2]);
  int32_t L2Y = analogRead(axes[3]);
  int32_t R1X = analogRead(axes[4]);
  int32_t R1Y = analogRead(axes[5]);  
  int32_t R2X = analogRead(axes[6]);
  int32_t R2Y = analogRead(axes[7]);
  int32_t Pot = analogRead(axes[8]);

  L1X  = (L1X - 2048) * 16.0;
  L1Y  = (L1Y - 2048) * 16.0;
  L2X  = (L2X - 2048) * 16.0;
  L2Y  = (L2Y - 2048) * 16.0;
  R1X  = (R1X - 2048) * 16.0;
  R1Y  = (R1Y - 2048) * 16.0;
  R2X  = (R2X - 2048) * 16.0;
  R2Y  = (R2Y - 2048) * 16.0;
  Pot  = (Pot - 2048) * 16.0;

  if (L1X < -32767) L1X = -32767; if (L1X > 32767) L1X = 32767;
  if (L1Y < -32767) L1Y = -32767; if (L1Y > 32767) L1Y = 32767;
  if (L2X < -32767) L2X = -32767; if (L2X > 32767) L2X = 32767;
  if (L2Y < -32767) L2Y = -32767; if (L2Y > 32767) L2Y = 32767;
  if (R1X < -32767) R1X = -32767; if (R1X > 32767) R1X = 32767;
  if (R1Y < -32767) R1Y = -32767; if (R1Y > 32767) R1Y = 32767;
  if (R2X < -32767) R2X = -32767; if (R2X > 32767) R2X = 32767;
  if (R2Y < -32767) R2Y = -32767; if (R2Y > 32767) R2Y = 32767;
  if (Pot < -32767) Pot = -32767; if (Pot > 32767) Pot = 32767;
  
  // pol buttons
  bool lbutton1 = false; // left
  bool lbutton2 = false; // up
  bool lbutton3 = false; // right
  bool lbutton4 = false; // down

  bool rbutton1 = false; // left
  bool rbutton2 = false; // up
  bool rbutton3 = false; // right
  bool rbutton4 = false; // down
  
  bool  b1  = !digitalRead(buttons[0]);
  bool  b2  = !digitalRead(buttons[1]);
  bool  b3  = !digitalRead(buttons[2]);
  bool  b4  = !digitalRead(buttons[3]);
  bool  b5  = !digitalRead(buttons[4]);
  bool  b6  = !digitalRead(buttons[5]);
  bool  b7  = !digitalRead(buttons[6]);
  bool  b8  = !digitalRead(buttons[7]);

  if(arcade_left_installed) {
    lbutton1 = !arcade_left.digitalRead(arcade_switchs[3]); // left
    lbutton2 = !arcade_left.digitalRead(arcade_switchs[0]); // up
    lbutton3 = !arcade_left.digitalRead(arcade_switchs[1]); // right
    lbutton4 = !arcade_left.digitalRead(arcade_switchs[2]); // down
  }
  if(arcade_right_installed) {
    rbutton1 = !arcade_right.digitalRead(arcade_switchs[3]); // left
    rbutton2 = !arcade_right.digitalRead(arcade_switchs[0]); // up
    rbutton3 = !arcade_right.digitalRead(arcade_switchs[1]); // right
    rbutton4 = !arcade_right.digitalRead(arcade_switchs[2]); // down
  }

  // pol encoder
  int32_t Enc = 0; 
  bool bEnc = false;
  if (encoder_installed) {
    Enc = encoder.getEncoderPosition(); 
    bEnc = !encoder.digitalRead(encoder_switch);   
  }

  // #ifdef TINYUSB_NEED_POLLING_TASK
  // // Manual call tud_task since it isn't called by Core's background
  // TinyUSBDevice.task();
  // #endif

  // not enumerated()/mounted() yet: nothing to do
  if (!TinyUSBDevice.mounted()) {
    return;
  }

  // Remote wakeup
  if ( TinyUSBDevice.suspended() && (b1 || b2 || b3 || b4) )
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    TinyUSBDevice.remoteWakeup();
  }

  // switch modes with debounce
  if (bEnc && mouse_mode) {
    Serial.println("Switching to Joystick");
    mouse_mode = false;

    if(usb_hid.ready()) {
      usb_hid.mouseReport(MOUSE_ID, 0, 0, 0, 0, 0);
    }
    delay(10);

    if(usb_hid.ready() && has_keys) {    
      usb_hid.keyboardRelease(KEYBOARD_ID);
      has_keys = false;
    }
    delay(500);
    
  } else if (bEnc) {
    Serial.println("Switching to Mouse");
    mouse_mode = true;

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
    
    usb_hid.sendReport(JOYSTICK_ID, &joystick, sizeof(joystick));  
    delay(500);
  }

  // Update active report
  if (mouse_mode) {
    
    mouse_x =  static_cast<int8_t>(R1X/1028);
    mouse_y = -static_cast<int8_t>(R1Y/1028);
    mouse_h =  static_cast<int8_t>(L1X/4096);
    mouse_v =  static_cast<int8_t>(L1Y/4096);

    if (mouse_x < 2 && mouse_x > -2) mouse_x = 0;
    if (mouse_y < 2 && mouse_y > -2) mouse_y = 0;   
    if (mouse_h < 1 && mouse_h > -1) mouse_h = 0;
    if (mouse_v < 1 && mouse_v > -1) mouse_v = 0;       

    if (mouse_x < -127) mouse_x = -127; if (mouse_x > 127) mouse_x = 127;
    if (mouse_y < -127) mouse_y = -127; if (mouse_y > 127) mouse_y = 127;    
    if (mouse_h < -127) mouse_h = -127; if (mouse_h > 127) mouse_h = 127;
    if (mouse_v < -127) mouse_v = -127; if (mouse_v > 127) mouse_v = 127;    
    
    uint8_t buttons  =  ( b2 << 0) 
                      | ( b4 << 1) 
                      | ( b3 << 2) 
                      // | ( b3       << 3)
                      // | ( b4       << 4)
                      ;

    if (usb_hid.ready()) {
      usb_hid.mouseReport(MOUSE_ID, buttons, mouse_x, mouse_y, mouse_v, mouse_h);
      delay(10); // delay before trying keyboard report
    }  else {
      Serial.println("Mouse not ready!");
    }

    // Caculate joystick position on keyboard and find key at that location
    key_x = static_cast<int8_t>( L2X * (keyCols - 1) * 6 / 2 / 32767);  // characters are 5 pixels wide with a 1 pixel gap for 6 center to center
    key_y = static_cast<int8_t>(-L2Y * (keyRows - 1) * 8 / 2 / 32767);  // characters are 7 pixels tall with a 1 pixel gap for 8 center to center
    key_i = (key_y + (keyRows) * 8 / 2 ) / 8;
    key_j = (key_x + (keyCols) * 6 / 2 ) / 6;

    uint8_t modifier = 0;
    if (usb_hid.ready()) {
      int count = 0;
      uint8_t keys[6] = { 0 }; // Be careful not to exceed the report limit of 6 keys
      if (rbutton1) {
        if(count < 6) keys[count++] = HID_KEY_SHIFT_LEFT;
      }
      if (rbutton2) {
        if(count < 6) keys[count++] = HID_KEY_CONTROL_LEFT;
      }
      if (rbutton3) {
        if(count < 6) keys[count++] = HID_KEY_ALT_LEFT;
      }
      if (rbutton4) {
        if(count < 6) {
          modifier      = ascii2hid[(uint8_t)keyMatrix[key_i][key_j]][0];
          keys[count++] = ascii2hid[(uint8_t)keyMatrix[key_i][key_j]][1];
        }
      }
      if (lbutton1) {
        if(count < 6) keys[count++] = HID_KEY_HOME;
      }
      if (lbutton2) {
        if(count < 6) keys[count++] = HID_KEY_ESCAPE;
      }
      if (lbutton3) {
        if(count < 6) keys[count++] = HID_KEY_END;
      }
      if (lbutton4) {
        if(count < 6) keys[count++] = HID_KEY_ENTER;
      }

      if (b1) {
        if(count < 6) keys[count++] = HID_KEY_GUI_LEFT;
      }
      
      if (R2Y > 15000) {
        if(count < 6) keys[count++] = HID_KEY_ARROW_UP;
      } else if (R2X >  15000) {
        if(count < 6) keys[count++] = HID_KEY_ARROW_RIGHT;
      } else if (R2Y < -15000) {
        if(count < 6) keys[count++] = HID_KEY_ARROW_DOWN;
      } else if (R2X < -15000) {
        if(count < 6) keys[count++] = HID_KEY_ARROW_LEFT;
      }

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

    lastEnc = Enc;

    if(display_installed) {
      display.fillRect(0, 0, 64, 128, SH110X_BLACK);  
      // Mouse feedback in upper part of display
      drawMouse(0, 0, R1X, R1Y, L1X, L1Y, b2, b3, b4, lbutton1, lbutton2, lbutton3, lbutton4, rbutton1, rbutton2, rbutton3);
      // Draw keyboard matrix in lower display
      drawKeyMatrix(2, 64);
    }
  } else { 
    joystick.l1x =  static_cast<int16_t>(L1X);
    joystick.l1y = -static_cast<int16_t>(L1Y);
    joystick.r1x =  static_cast<int16_t>(R1X);
    joystick.r1y = -static_cast<int16_t>(R1Y);
    joystick.l2x =  static_cast<int16_t>(L2X);
    joystick.l2y = -static_cast<int16_t>(L2Y);
    joystick.r2x =  static_cast<int16_t>(R2X);
    joystick.r2y = -static_cast<int16_t>(R2Y);
    joystick.pot =  static_cast<int16_t>(Pot);

    // store buttons
    joystick.buttons  = ( lbutton1 << 0) 
                      | ( lbutton2 << 1) 
                      | ( lbutton3 << 2) 
                      | ( lbutton4 << 3)
                      | ( rbutton1 << 4) 
                      | ( rbutton2 << 5) 
                      | ( rbutton3 << 6) 
                      | ( rbutton4 << 7)                    
                      | ( b1 << 8) 
                      | ( b2 << 9) 
                      | ( b3 << 10) 
                      | ( b4 << 11) 
                      | ( b5 << 12) 
                      | ( b6 << 13) 
                      | ( b7 << 14) 
                      | ( b8 << 15)
                      ;

    lastEnc = Enc;

    if (usb_hid.ready()) {
      usb_hid.sendReport(JOYSTICK_ID, &joystick, sizeof(joystick));  
    } else {
      Serial.println("Joystick not ready!");
    }

    if(display_installed) {
      display.setTextColor(SH110X_WHITE, SH110X_BLACK);
      display.fillRect(0, 0, 64, 128, SH110X_BLACK);
      display.setCursor(0,0); display.print("Joystick:");
      drawJoystick( 1, 17, 30, 30, L1X * 15 / 32768, -L1Y * 15 / 32768, b1); 
      drawJoystick(33, 17, 30, 30, R1X * 15 / 32768, -R1Y * 15 / 32768, b2); 
      drawButton( 4, 57, 4, lbutton1); drawButton(36, 57, 4, rbutton1);
      drawButton(12, 50, 4, lbutton2); drawButton(44, 50, 4, rbutton2);
      drawButton(20, 57, 4, lbutton3); drawButton(52, 57, 4, rbutton3);
      drawButton(12, 64, 4, lbutton4); drawButton(44, 64, 4, rbutton4);
      drawJoystick( 1, 73, 30, 30, L2X * 15 / 32768, -L2Y * 15 / 32768, b3); 
      drawJoystick(33, 73, 30, 30, R2X * 15 / 32768, -R2Y * 15 / 32768, b4); 
      drawBarCenter(32, 105, 6, Pot * 32 / 32768, false);
      display.setCursor( 0, 120); display.print("EN: "); display.setCursor( 24, 120); display.print(Enc, DEC);     
    }
  }    

  // display values
  if(display_installed) {
    display.display();
  }

  if(encoder_installed) {
    encoder_pixel.setPixelColor(0, ColorWheel(Enc & 0xFF));
    encoder_pixel.show();
  }

  yield();
}

void drawBarCenter(const int32_t &x, const int32_t &y, const int32_t &h, const int32_t &value, bool is_vert) {
  if(is_vert) {
    int32_t top    = std::min(y-1, y + value);
    int32_t bottom = std::max(y+1, y + value); 
    display.fillRect(x, top, h, bottom-top, SH110X_WHITE);
  } else {
    int32_t left  = std::min(x-1, x + value);
    int32_t right = std::max(x+1, x + value); 
    display.fillRect(left, y, right-left, h, SH110X_WHITE);
  }
}

void drawButton(const int32_t &x, const int32_t &y, const int32_t &r, const bool &value_b) {
  int32_t point_x = x + r / 2;
  int32_t point_y = y + r / 2; 
  if(value_b) {
    display.fillCircle(point_x, point_y, r, SH110X_WHITE);
  } else {
    display.drawCircle(point_x, point_y, r, SH110X_WHITE);
  }
}

void drawJoystick(const int32_t &x, const int32_t &y, const int32_t &w, const int32_t &h, 
                  const int32_t &value_x, const int32_t &value_y, const bool &value_b) {
  int32_t point_x = x + w / 2 + value_x;
  int32_t point_y = y + w / 2 + value_y; 
  display.drawRect(x, y, w, h, SH110X_WHITE);
  if(value_b) {
    display.fillCircle(point_x, point_y, 2, SH110X_WHITE);
  } else {
    display.drawCircle(point_x, point_y, 2, SH110X_WHITE);
  }
}

void drawMouse(const int32_t &x, const int32_t &y, const int32_t &value_x, const int32_t &value_y, const int32_t &value_h, const int32_t &value_v,
               const bool &bleft, const bool &bright, const bool &bcenter, const bool &bhome, const bool &besc, const bool &bend, 
               const bool &benter, const bool &bshift, const bool &bcontrol, const bool &balt) {

  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  display.setCursor( x,  y); display.print("Mouse:");
  drawJoystick( x+25, y+17, 30, 30, value_x * 15 / 32768, -value_y * 15 / 32768, false);
  drawBarCenter(x+40, y+48, 7,  value_h * 32 / 32768, false);
  drawBarCenter(x+56, y+32, 7, -value_v * 32 / 32768, true);
  drawButton(x+28, y+10, 3, bleft); 
  drawButton(x+38, y+10, 3, bright);
  drawButton(x+48, y+10, 3, bcenter);
  display.setCursor(x+0,y+ 8); display.setTextColor(bhome    ? SH110X_BLACK : SH110X_WHITE, bhome    ? SH110X_WHITE : SH110X_BLACK); display.print("Hom");
  display.setCursor(x+0,y+16); display.setTextColor(besc     ? SH110X_BLACK : SH110X_WHITE, besc     ? SH110X_WHITE : SH110X_BLACK); display.print("Esc");
  display.setCursor(x+0,y+24); display.setTextColor(bend     ? SH110X_BLACK : SH110X_WHITE, bend     ? SH110X_WHITE : SH110X_BLACK); display.print("End");
  display.setCursor(x+0,y+32); display.setTextColor(benter   ? SH110X_BLACK : SH110X_WHITE, benter   ? SH110X_WHITE : SH110X_BLACK); display.print("Ent");            
  display.setCursor(x+0,y+40); display.setTextColor(bshift   ? SH110X_BLACK : SH110X_WHITE, bshift   ? SH110X_WHITE : SH110X_BLACK); display.print("Sft");
  display.setCursor(x+0,y+48); display.setTextColor(bcontrol ? SH110X_BLACK : SH110X_WHITE, bcontrol ? SH110X_WHITE : SH110X_BLACK); display.print("Ctr");
  display.setCursor(x+0,y+56); display.setTextColor(balt     ? SH110X_BLACK : SH110X_WHITE, balt     ? SH110X_WHITE : SH110X_BLACK); display.print("Alt");
  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
}

void drawKeyMatrix(int32_t x, int32_t y) {
  for(uint8_t i = 0; i < keyRows; i++){
    display.setCursor( x, y + 8*i); 
    for(uint8_t j = 0; j < keyCols; j++) {
      // Highlight the appropriate key
      if ((i == key_i) && (j ==  key_j)) display.setTextColor(SH110X_BLACK, SH110X_WHITE); 
        if (keyMatrix[i][j] == 0x08) display.write('b');
        else if (keyMatrix[i][j] == 0x0A) display.write('n');
        else if (keyMatrix[i][j] == 0x0D) display.write('r');
        else if (keyMatrix[i][j] == 0x02) display.write('h');
        else if (keyMatrix[i][j] == 0x03) display.write('e');
        else if (keyMatrix[i][j] == 0x01) display.write('H');
        else if (keyMatrix[i][j] == 0x02) display.write('E');            
        else if (keyMatrix[i][j] == 0x7F) display.write('d');
        else display.write(keyMatrix[i][j]);
      if ((i == key_i) && (j ==  key_j)) display.setTextColor(SH110X_WHITE, SH110X_BLACK);          
    }
  }
}

uint32_t ColorWheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return encoder_pixel.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return encoder_pixel.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return encoder_pixel.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
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
        buffer[1] = currentLeds[0];
        buffer[2] = currentLeds[1];
        buffer[3] = currentLeds[2];
        buffer[4] = currentLeds[3];
        buffer[5] = currentLeds[4];
        buffer[6] = currentLeds[5];
        buffer[7] = currentLeds[6];
        buffer[8] = currentLeds[7];
        return 9; // Return the number of bytes written
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
      if(bufsize > 0) currentLeds[0] = buffer[0];
      if(bufsize > 1) currentLeds[1] = buffer[1];
      if(bufsize > 2) currentLeds[2] = buffer[2];
      if(bufsize > 3) currentLeds[3] = buffer[3]; 
      if(bufsize > 4) currentLeds[4] = buffer[4];
      if(bufsize > 5) currentLeds[5] = buffer[5];
      if(bufsize > 6) currentLeds[6] = buffer[6];
      if(bufsize > 7) currentLeds[7] = buffer[7]; 
    }
  }

  setLeds();
}

// // Output report callback for keyboard indicator such as Caplocks
// void set_keyboard_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
//   (void) report_id;
//   (void) bufsize;

//   // LED indicator is output report with only 1 byte length
//   if (report_type != HID_REPORT_TYPE_OUTPUT) return;

// // #ifdef LED_BUILTIN
// //   // turn on LED if capslock is set
// //   digitalWrite(LED_BUILTIN, ledIndicator & KEYBOARD_LED_CAPSLOCK);
// // #endif
// }

void setLeds() {
  // Setup arcade controllers
  if(arcade_left_installed) {
    arcade_left.analogWrite(arcade_leds[0], currentLeds[0]);
    arcade_left.analogWrite(arcade_leds[1], currentLeds[1]);
    arcade_left.analogWrite(arcade_leds[2], currentLeds[2]);
    arcade_left.analogWrite(arcade_leds[3], currentLeds[3]);
  }

  if(arcade_right_installed) {
    arcade_right.analogWrite(arcade_leds[0], currentLeds[4]);
    arcade_right.analogWrite(arcade_leds[1], currentLeds[5]);
    arcade_right.analogWrite(arcade_leds[2], currentLeds[6]);
    arcade_right.analogWrite(arcade_leds[3], currentLeds[7]);
  }
}
