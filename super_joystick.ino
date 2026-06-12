#include <USB.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>

#include "sketches.h"
#include "hid_mouse_description.h"
#include "hid_joystick_description.h"
#include "hid_keyboard_description.h"

#define QUEUE_SIZE 1

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
static const uint8_t axes[] = {18, 17, 13, 12, 16, 15, 11, 10, 14};
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
uint8_t const keyRows = 6;
uint8_t const keyCols = 10;
char const keyMatrix_L1[keyRows][keyCols] = {
  // bksp  lf   cr  esc  tab  home end ^home ^end del
    {0x08,0x0A,0x0D,0x1B,0x09,0x02,0x03,0x01,0x04,0x7F},
    { '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'}, 
    { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'},
    { 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't'},
    { 'u', 'v', 'w', 'x', 'y', 'z', '-', '=', '[', ']'},
    {0x0B,0X0C,'\`','\'', ';', ',', '.', '/','\\', ' '}
  // Undo Redo
};
char const keyMatrix_L2[keyRows][keyCols] = {
  //  F1   F2   F3   F4   F5   F6   F7   F8   F9   F10
    {0x11,0X12,0X13,0X14,0X15,0X16,0X17,0X18,0X19,0X1A},
    { '!', '@', '#', '$', '%', '^', '&', '*', '(', ')'},
    { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'},
    { 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T'},
    { 'U', 'V', 'W', 'X', 'Y', 'Z', '_', '+','\{','\}'},
    {0x1C,0x1D, '~','\"', ':', '<', '>', '?', '|', ' '}
  // F11  F12   
};
// uint8_t const keyRows = 6;
// uint8_t const keyCols = 5;
// char const keyMatrix_L1[keyRows][keyCols] = {
//     { 'a', 'b', 'c', 'd', 'e'},
//     { 'f', 'g', 'h', 'i', 'j'},
//     { 'k', 'l', 'm', 'n', 'o'},
//     { 'p', 'q', 'r', 's', 't'},
//     { 'u', 'v', 'w', 'x', 'y'},
//     { 'z', ',', '.','\\', ' '}
// };
// char const keyMatrix_L2[keyRows][keyCols] = {
//     { 'A', 'B', 'C', 'D', 'E'},
//     { 'F', 'G', 'H', 'I', 'J'},
//     { 'K', 'L', 'M', 'N', 'O'},
//     { 'P', 'Q', 'R', 'S', 'T'},
//     { 'U', 'V', 'W', 'X', 'Y'},
//     { 'Z', '<', '>', '|', '_'},
// };
// char const keyMatrix_L3[keyRows][keyCols] = {
//     { '1', '2', '3', '4', '5'},
//     { '6', '7', '8', '9', '0'},
//     { '!', '@', '#', '$', '%'},
//     { '^', '&', '*', '(', ')'},
//     { '/', '~', '?', '[', ']'},
//     { ' ', ':', ';','\{','\}'}
// };
// char const keyMatrix_L3[keyRows][keyCols] = {
//     {0x11,0X12,0X13,0X14,0X15},
//     {0X16,0X17,0X18,0X19,0X1A},
//     {0x1C,0x1D, '+', '-', '='},
//     {0x0B,0x0C,'\`','\'','\"'},
//     {0x09,0x02,0x03,0x01,0x04},
//     {0x08,0x0A,0x0D,0x1B,0x7F}
// }; 
uint8_t const ascii2hid[128][2] = { ASCII_TO_KEYCODE };

// Queues
QueueHandle_t queueAxes = NULL;
QueueHandle_t queueButtons = NULL;

// Global variables
my_hid_report_t joystick;
DisplayJoystick displayJoystick(&display);
DisplayKeyboard displayKeyboard(&display);
int keyboard_mode = false;
int32_t lastEnc = 0;
int8_t currentLeds[] = {32, 32, 32, 32, 32, 32, 32, 32}; // current brightness of leds, 0 to 255
int8_t has_keys = 0;  // If keys were sent in last hid report
int8_t key_x = 0;      // Current keyboard position
int8_t key_y = 0;      // Current keyboard position
uint8_t key_i = 0;     // Current key row
uint8_t key_j = 0;     // Current key col
// int8_t mouse_x = 0;    // Mouse delta x
// int8_t mouse_y = 0;    // Mouse delta y
// int8_t mouse_h = 0;    // Mouse scroll horizontal
// int8_t mouse_v = 0;    // Mouse scroll vertical
unsigned long timestamp_last; 

// Tasks
void taskReadAxes(void *parameter) {
  for (;;) {
    // unsigned long begin = millis();

    int32_t a[10] = {0};
    a[0] = analogRead(axes[0]);  // a[0]
    a[1] = analogRead(axes[1]);  // a[1]
    a[2] = analogRead(axes[2]);  // R1X
    a[3] = analogRead(axes[3]);  // R1Y
    a[4] = analogRead(axes[4]);  // a[4]
    a[5] = analogRead(axes[5]);  // a[5]
    a[6] = analogRead(axes[6]);  // R2X
    a[7] = analogRead(axes[7]);  // R2Y
    a[8] = analogRead(axes[8]);  // Pot

    a[0]  = (a[0] - 2048) * 16.0;
    a[1]  = (a[1] - 2048) * 16.0;
    a[2]  = (a[2] - 2048) * 16.0;
    a[3]  = (a[3] - 2048) * 16.0;
    a[4]  = (a[4] - 2048) * 16.0;
    a[5]  = (a[5] - 2048) * 16.0;
    a[6]  = (a[6] - 2048) * 16.0;
    a[7]  = (a[7] - 2048) * 16.0;
    a[8]  = (a[8] - 2048) * 16.0;

    if (a[0] < -32767) a[0] = -32767; if (a[0] > 32767) a[0] = 32767;
    if (a[1] < -32767) a[1] = -32767; if (a[1] > 32767) a[1] = 32767;
    if (a[2] < -32767) a[2] = -32767; if (a[2] > 32767) a[2] = 32767;
    if (a[3] < -32767) a[3] = -32767; if (a[3] > 32767) a[3] = 32767;
    if (a[4] < -32767) a[4] = -32767; if (a[4] > 32767) a[4] = 32767;
    if (a[5] < -32767) a[5] = -32767; if (a[5] > 32767) a[5] = 32767;
    if (a[6] < -32767) a[6] = -32767; if (a[6] > 32767) a[6] = 32767;
    if (a[7] < -32767) a[7] = -32767; if (a[7] > 32767) a[7] = 32767;
    if (a[8] < -32767) a[8] = -32767; if (a[8] > 32767) a[8] = 32767;

    // UBaseType_t free_space = uxQueueSpacesAvailable(queueAxes);
    // UBaseType_t stack_high_water_mark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print("taskAxes: Free space in queue: "); Serial.print(free_space); 
    // Serial.print(", Stack high water mark free: "); Serial.println(stack_high_water_mark);

    xQueueOverwrite(queueAxes, &a);

    // unsigned long end = millis();
    // Serial.print("  Time: "); Serial.println(end - begin);
    vTaskDelay(10 / portTICK_PERIOD_MS);  // 10ms

  }
}

void taskReadButtons(void *parameter) {
  for (;;) {
    unsigned long begin = millis();
    
    bool b[17] = {false};
    if(arcade_left_installed) {
      b[0] = !arcade_left.digitalRead(arcade_switchs[3]); // left
      b[1] = !arcade_left.digitalRead(arcade_switchs[0]); // up
      b[2] = !arcade_left.digitalRead(arcade_switchs[1]); // right
      b[3] = !arcade_left.digitalRead(arcade_switchs[2]); // down
    }
    if(arcade_right_installed) {
      b[4] = !arcade_right.digitalRead(arcade_switchs[3]); // left
      b[5] = !arcade_right.digitalRead(arcade_switchs[0]); // up
      b[6] = !arcade_right.digitalRead(arcade_switchs[1]); // right
      b[7] = !arcade_right.digitalRead(arcade_switchs[2]); // down
    }
    b[8]  = !digitalRead(buttons[0]);
    b[9]  = !digitalRead(buttons[1]);
    b[10] = !digitalRead(buttons[2]);
    b[11] = !digitalRead(buttons[3]);
    b[12] = !digitalRead(buttons[4]);
    b[13] = !digitalRead(buttons[5]);
    b[14] = !digitalRead(buttons[6]);
    b[15] = !digitalRead(buttons[7]);

    if (encoder_installed) {
      b[16] = !encoder.digitalRead(encoder_switch);   
    }

    UBaseType_t free_space = uxQueueSpacesAvailable(queueButtons);
    UBaseType_t stack_high_water_mark = uxTaskGetStackHighWaterMark(NULL);
    Serial.print("taskButtons: Free space in queue: "); Serial.print(free_space); 
    Serial.print(", Stack high water mark free: "); Serial.println(stack_high_water_mark);

    xQueueOverwrite(queueButtons, &b);
    unsigned long end = millis();
    Serial.print("  Time: "); Serial.println(end - begin);
    vTaskDelay(10 / portTICK_PERIOD_MS);  // 10ms
  }
}

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
  usb_hid.setPollInterval(10); // Poll every 10ms
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

  // Turnup i2c speeds
  Wire.setClock(400000L); // Increase I2C speed to 400kHz

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
    display.cp437(true);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0,0);
    display.print("Starting!");
    display.display(); // actually display all of the above

    displayJoystick.draw();
  }

  setLeds();

  // Create queues
  queueAxes = xQueueCreate(QUEUE_SIZE, 10 * sizeof(int32_t));
  if (queueAxes == NULL) {
    Serial.println("Failed to create axes queue!");
    while (1);
  }

  queueButtons = xQueueCreate(QUEUE_SIZE, 17 * sizeof(bool));
  if (queueButtons == NULL) {
    Serial.println("Failed to create button queue!");
    while (1);
  }

  // Create tasks
  xTaskCreatePinnedToCore(
    taskReadAxes,
    "axesTask",
    2000,  // Task stack
    NULL,
    1,
    NULL,
    1  // Core
  );
  xTaskCreatePinnedToCore(
    taskReadButtons,
    "buttonsTask",
    2000,  // Task stack
    NULL,
    1,
    NULL,
    1  // Core
  );

  timestamp_last = millis();
  Serial.println("Setup complete!");
}

void loop() {
  // Serial.println("Looping:"); 
  
  // get axes
  // Serial.println("Get axes");
  int32_t a[10] = {0};
  if (!xQueueReceive(queueAxes, &a, portMAX_DELAY)) {
    Serial.println("Failed to get Axes from queue.");
  }
  // get buttons
  // Serial.println("Get buttons");
  bool b[17] = {false};
  if (!xQueueReceive(queueButtons, &b, portMAX_DELAY)) {
    Serial.println("Failed to get Buttons from queue.");
  }

  if (encoder_installed) {
      a[9] = encoder.getEncoderPosition(); 
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
  if ( TinyUSBDevice.suspended() && (b[8] || b[9] || b[10] || b[11]) )
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    TinyUSBDevice.remoteWakeup();
  }

  // switch modes with debounce
  if (b[16] && keyboard_mode) {
    Serial.println("Switching to Joystick");
    keyboard_mode = false;
    if(display_installed) displayJoystick.draw();

    if(usb_hid.ready()) {
      usb_hid.mouseReport(MOUSE_ID, 0, 0, 0, 0, 0);
    }
    delay(10);

    if(usb_hid.ready() && has_keys) {    
      usb_hid.keyboardRelease(KEYBOARD_ID);
      has_keys = false;
    }
    delay(500);
    
  } else if (b[16]) {
    Serial.println("Switching to Keyboard");
    keyboard_mode = true;
    if(display_installed) displayKeyboard.draw();

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

  // Update active report
  // Keyboard Mode
  if (keyboard_mode) {
    Serial.println("Keyboard mode");
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
      
      uint8_t buttons  =  ( b[9] << 0) 
                        | ( b[11] << 1) 
                        | ( b[10] << 2) 
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
    char const (*matrix)[keyCols];
    if(abs(b[9] % 2) == 1) matrix = keyMatrix_L2;
    else matrix = keyMatrix_L1;
    
    // Keyboard report
    if (usb_hid.ready()) {
      // Caculate joystick position on keyMatrix
      key_x = static_cast<int8_t>( a[0] * (keyCols - 1) * 6 / 2 / 32767);  // characters are 5 pixels wide with a 1 pixel gap for 6 center to center
      key_y = static_cast<int8_t>(-a[1] * (keyRows - 1) * 8 / 2 / 32767);  // characters are 7 pixels tall with a 1 pixel gap for 8 center to center
      key_i = (key_y + (keyRows) * 8 / 2 ) / 8;
      key_j = (key_x + (keyCols) * 6 / 2 ) / 6;

      int count = 0;
      uint8_t modifier = 0;
      uint8_t keys[6] = { 0 }; // Be careful not to exceed the report limit of 6 keys
      
      // Capture matrix key if button pressed 
      if (b[7]) {
        modifier      = ascii2hid[(uint8_t)matrix[key_i][key_j]][0];
        keys[count++] = ascii2hid[(uint8_t)matrix[key_i][key_j]][1];
      }

      // direct button keys
      if (b[4]) {
        if(count < 6) keys[count++] = HID_KEY_SHIFT_LEFT;
      }
      if (b[5]) {
        if(count < 6) keys[count++] = HID_KEY_CONTROL_LEFT;
      }
      if (b[6]) {
        if(count < 6) keys[count++] = HID_KEY_ALT_LEFT;
      }
      if (b[0]) {
        if(count < 6) keys[count++] = HID_KEY_HOME;
      }
      if (b[1]) {
        if(count < 6) keys[count++] = HID_KEY_ESCAPE;
      }
      if (b[2]) {
        if(count < 6) keys[count++] = HID_KEY_END;
      }
      if (b[3]) {
        if(count < 6) keys[count++] = HID_KEY_ENTER;
      }
      // if (b[8]) {
      //   if(count < 6) keys[count++] = HID_KEY_GUI_LEFT;
      // }
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

    lastEnc = a[9];

    if(display_installed) {
      displayKeyboard.update(a[2], a[3], a[6], a[7], b[9], b[10], b[11], b[0], b[1], b[2], b[3], b[4], b[5], b[6], matrix, key_i, key_j);
    }
  // Joystick mode
  } else { 
    // Serial.println("Joystick Mode");
    joystick.l1x =  static_cast<int16_t>(a[0]);
    joystick.l1y = -static_cast<int16_t>(a[1]);
    joystick.r1x =  static_cast<int16_t>(a[2]);
    joystick.r1y = -static_cast<int16_t>(a[3]);
    joystick.r2x =  static_cast<int16_t>(a[4]);
    joystick.r2y = -static_cast<int16_t>(a[5]);
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
                      ;

    lastEnc = a[9];

    // Send joystick report
    if (usb_hid.ready()) {
      usb_hid.sendReport(JOYSTICK_ID, &joystick, sizeof(joystick));  
    } else {
      Serial.println("Joystick not ready!");
    }

    if(display_installed) {
      displayJoystick.update(a, b);
    }
  }    

  // display values
  if(display_installed) {
    unsigned long timestamp = millis(); 
    display.fillRect(0, 120, 64, 8, SH110X_BLACK);
    display.setCursor( 0, 120); display.print("DT: "); display.setCursor( 24, 120); display.print(timestamp-timestamp_last);
    timestamp_last = timestamp;  
    display.display();
  }

  if(encoder_installed) {
    encoder_pixel.setPixelColor(0, ColorWheel(a[9] & 0xFF));
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
