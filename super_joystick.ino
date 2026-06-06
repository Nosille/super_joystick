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

// Global variables
my_hid_report_t joystick;
int mouse_mode = false;
int32_t lastEnc = 0;
int8_t lastLeds[] = {32, 32, 32, 32, 32, 32, 32, 32}; 
bool has_key = false;
int8_t key_x = 28;
int8_t key_y = 20;

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
  bool lbutton1 = false; // up
  bool lbutton2 = false; // right
  bool lbutton3 = false; // down
  bool lbutton4 = false; // left

  bool rbutton1 = false; // up
  bool rbutton2 = false; // right
  bool rbutton3 = false; // down
  bool rbutton4 = false; // left
  
  bool  b1  = !digitalRead(buttons[0]);
  bool  b2  = !digitalRead(buttons[1]);
  bool  b3  = !digitalRead(buttons[2]);
  bool  b4  = !digitalRead(buttons[3]);
  bool  b5  = !digitalRead(buttons[4]);
  bool  b6  = !digitalRead(buttons[5]);
  bool  b7  = !digitalRead(buttons[6]);
  bool  b8  = !digitalRead(buttons[7]);

  if(arcade_left_installed) {
    lbutton1 = !arcade_left.digitalRead(arcade_switchs[0]); // up
    lbutton2 = !arcade_left.digitalRead(arcade_switchs[1]); // right
    lbutton3 = !arcade_left.digitalRead(arcade_switchs[2]); // down
    lbutton4 = !arcade_left.digitalRead(arcade_switchs[3]); // left
  }
  if(arcade_right_installed) {
    rbutton1 = !arcade_right.digitalRead(arcade_switchs[0]); // up
    rbutton2 = !arcade_right.digitalRead(arcade_switchs[1]); // right
    rbutton3 = !arcade_right.digitalRead(arcade_switchs[2]); // down
    rbutton4 = !arcade_right.digitalRead(arcade_switchs[3]); // left
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
    
      if(has_key) {
        usb_hid.keyboardRelease(KEYBOARD_ID);
        has_key = false;
      }
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
    
    int8_t mouse_x =  static_cast<int8_t>(R1X/1028);
    int8_t mouse_y = -static_cast<int8_t>(R1Y/1028);
    int8_t mouse_h =  static_cast<int8_t>(L1X/4096);
    int8_t mouse_v = -static_cast<int8_t>(L1Y/4096);

    if (mouse_x < 2 && mouse_x > -2) mouse_x = 0;
    if (mouse_y < 2 && mouse_y > -2) mouse_y = 0;   
    if (mouse_h < 1 && mouse_h > -1) mouse_h = 0;
    if (mouse_v < 1 && mouse_v > -1) mouse_v = 0;       

    if (mouse_x < -127) mouse_x = -127; if (mouse_x > 127) mouse_x = 127;
    if (mouse_y < -127) mouse_y = -127; if (mouse_y > 127) mouse_y = 127;    
    if (mouse_h < -127) mouse_h = -127; if (mouse_h > 127) mouse_h = 127;
    if (mouse_v < -127) mouse_v = -127; if (mouse_v > 127) mouse_v = 127;    
    
    uint8_t buttons  =  ( lbutton4 << 0) 
                      | ( lbutton2 << 1) 
                      | ( lbutton1 << 2) 
                      // | ( b3       << 3)
                      // | ( b4       << 4)
                      ;

    if (usb_hid.ready()) {
      usb_hid.mouseReport(MOUSE_ID, buttons, mouse_x, mouse_y, mouse_v, mouse_h);
      delay(10); // delay before trying keyboard report
    }  else {
      Serial.println("Mouse not ready!");
    }

    uint8_t key_modifier = 0;
    if(b2) {
      key_modifier = key_modifier | HID_KEY_SHIFT_LEFT;
    } else if (b1) {
      key_modifier = key_modifier | HID_KEY_CONTROL_LEFT;
    }

    if (usb_hid.ready()) {
      if (R2Y > 15000) {
        Serial.println("HID_KEY_ARROW_UP");
        uint8_t keys[6] = {HID_KEY_ARROW_UP};
        usb_hid.keyboardReport(KEYBOARD_ID, key_modifier, keys);
        has_key = true;
      } else if (R2X >  15000) {
        Serial.println("HID_KEY_ARROW_RIGHT");
        uint8_t keys[6] = {HID_KEY_ARROW_RIGHT};
        usb_hid.keyboardReport(KEYBOARD_ID, key_modifier, keys);
        has_key = true;
      } else if (R2Y < -15000) {
        Serial.println("HID_KEY_ARROW_DOWN");
        uint8_t keys[6] = {HID_KEY_ARROW_DOWN};
        usb_hid.keyboardReport(KEYBOARD_ID, key_modifier, keys);
        has_key = true;        
      } else if (R2X < -15000) {
        Serial.println("HID_KEY_ARROW_LEFT"); 
        uint8_t keys[6] = {HID_KEY_ARROW_LEFT};
        usb_hid.keyboardReport(KEYBOARD_ID, key_modifier, keys);
        has_key = true;
      } else {
        // send empty key report if previously has key pressed
        if (has_key) usb_hid.keyboardRelease(KEYBOARD_ID);
        has_key = false;
      }
    } else {
      Serial.println("Keyboard not ready!");
    }

    lastEnc = Enc;

    if(display_installed) {
      display.fillRect(0, 0, 64, 128, SH110X_BLACK);
      display.setCursor( 0,  0); display.print("Mouse:");    
      display.setCursor( 0,  8); display.print("MX: "); display.setCursor( 24, 8); display.print(mouse_x, DEC);
      display.setCursor( 0, 16); display.print("MY: "); display.setCursor( 24, 16); display.print(mouse_y, DEC);   
      display.setCursor( 0, 24); display.print("MV: "); display.setCursor( 24, 24); display.print(mouse_v, DEC);
      display.setCursor( 0, 32); display.print("MH: "); display.setCursor( 24, 32); display.print(mouse_h, DEC); 
      
      display.setCursor( 0, 48); display.print("Keyboard:"); 
      display.setCursor( 0, 64); display.print("~!@#$%^&*."); 
      display.setCursor( 0, 72); display.print("1234567890"); 
      display.setCursor( 0, 80); display.print("ABCDEFGHIJ");
      display.setCursor( 0, 88); display.print("KLMNOPQRST");
      display.setCursor( 0, 96); display.print("UVWXYZ-+=_");
      display.setCursor( 0, 104); display.print("{}[]()<>/\\");
      display.setCursor( 0, 112); display.print(",.;:\`\'\""); 
    // display.setCursor( 0, 120); display.print("EN: "); display.setCursor( 24, 88); display.print(Enc, DEC);
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
      display.fillRect(0, 0, 64, 128, SH110X_BLACK);
      display.setCursor(0,0); display.print("Joystick:");        
      display.setCursor( 0, 16); display.print("LX: "); display.setCursor( 24, 16); display.print(joystick.l1x, DEC);
      display.setCursor( 0, 24); display.print("LY: "); display.setCursor( 24, 24); display.print(joystick.l1y, DEC);
      display.setCursor( 0, 32); display.print("RX: "); display.setCursor( 24, 32); display.print(joystick.r1x, DEC);
      display.setCursor( 0, 40); display.print("RY: "); display.setCursor( 24, 40); display.print(joystick.r1y, DEC);
      display.setCursor( 0, 48); display.print("LZ: "); display.setCursor( 24, 48); display.print(joystick.l2x, DEC);
      display.setCursor( 0, 56); display.print("LW: "); display.setCursor( 24, 56); display.print(joystick.l2y, DEC);
      display.setCursor( 0, 64); display.print("RZ: "); display.setCursor( 24, 64); display.print(joystick.r2x, DEC);
      display.setCursor( 0, 72); display.print("RW: "); display.setCursor( 24, 72); display.print(joystick.r2y, DEC);
      display.setCursor( 0, 80); display.print("PO: "); display.setCursor( 24, 80); display.print(joystick.pot, DEC);
      display.setCursor( 0, 88); display.print("EN: "); display.setCursor( 24, 88); display.print(Enc, DEC);     
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
        buffer[1] = lastLeds[0];
        buffer[2] = lastLeds[1];
        buffer[3] = lastLeds[2];
        buffer[4] = lastLeds[3];
        buffer[5] = lastLeds[4];
        buffer[6] = lastLeds[5];
        buffer[7] = lastLeds[6];
        buffer[8] = lastLeds[7];
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
      if(bufsize > 0) lastLeds[0] = buffer[0];
      if(bufsize > 1) lastLeds[1] = buffer[1];
      if(bufsize > 2) lastLeds[2] = buffer[2];
      if(bufsize > 3) lastLeds[3] = buffer[3]; 
      if(bufsize > 4) lastLeds[4] = buffer[4];
      if(bufsize > 5) lastLeds[5] = buffer[5];
      if(bufsize > 6) lastLeds[6] = buffer[6];
      if(bufsize > 7) lastLeds[7] = buffer[7]; 
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
    arcade_left.analogWrite(arcade_leds[0], lastLeds[0]);
    arcade_left.analogWrite(arcade_leds[1], lastLeds[1]);
    arcade_left.analogWrite(arcade_leds[2], lastLeds[2]);
    arcade_left.analogWrite(arcade_leds[3], lastLeds[3]);
  }

  if(arcade_right_installed) {
    arcade_right.analogWrite(arcade_leds[0], lastLeds[4]);
    arcade_right.analogWrite(arcade_leds[1], lastLeds[5]);
    arcade_right.analogWrite(arcade_leds[2], lastLeds[6]);
    arcade_right.analogWrite(arcade_leds[3], lastLeds[7]);
  }
}