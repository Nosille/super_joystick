#include <map>
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

#include "config_input.h"
#include "config_report.h"
#include "config_display.h"

#define QUEUE_SIZE 1

// Handles
TaskHandle_t taskInputsHandle = NULL;
TaskHandle_t taskReportHandle = NULL;
TaskHandle_t taskSendHandle = NULL;
TaskHandle_t taskDisplayHandle = NULL;

// Queues
QueueHandle_t queueAxes = NULL;
QueueHandle_t queueButtons = NULL;
QueueHandle_t queueMode = NULL;
QueueHandle_t queueMatrix = NULL;
QueueHandle_t queueMouse = NULL;
QueueHandle_t queueKeyboard = NULL;
QueueHandle_t queueJoystick1 = NULL;
QueueHandle_t queueJoystick2 = NULL;

// Global variables
int8_t leds_values[led_size] = { 32 };  // current brightness of leds, 0 to 255


//Interupt callbacks
void interruptSource(void* arg) {
  // Serial.println("interrupt: ");
  bool* interrupt = (bool*)arg;
  *interrupt = true;
}

// Tasks
void taskReadInputs(void* parameter) {
  // Devices
  std::map<int16_t, MCP3208> mcp3208;
  std::map<int16_t, Adafruit_seesaw> seesaw;
  std::map<int16_t, Adafruit_BNO055> bno055;
  // seesaw_NeoPixel encoder_pixel_left = seesaw_NeoPixel(1, 6, NEO_GRB + NEO_KHZ800, &Wire1);
  // seesaw_NeoPixel encoder_pixel_right = seesaw_NeoPixel(1, 6, NEO_GRB + NEO_KHZ800, &Wire1);

  // Global variables
  int32_t axis_values[axes_size] = { 0 };
  bool axis_changed[axes_size] = { false };
  bool button_values[buttons_size] = { false };

  int touch_threshold = 0;  // if 0 is used, benchmark value is used. Its by default 1,5% change, can be changed by touchSetDefaultThreshold(float percentage)
  std::map<int16_t, uint32_t> button_mask;
  std::map<int16_t, uint32_t> interrupt_mask;
  bool device_installed[devices_size] = { false };
  bool interrupt_triggered[devices_size] = { false };

  unsigned long last_read = millis();

  // I2C
  Wire1.begin();
  Wire1.setClock(400000L);

  // Connect to devices
  for (uint8_t i = 0; i < devices_size; i++) {
    // Connect to MCP3208
    if(devices[i][0] == (int16_t)Source::MCP3208) {
      MCP3208 device;
      if (!SPI.begin() || !device.begin((uint8_t)devices[i][1])) {
        Serial.print(i); Serial.print(") "); Serial.println("MCP3208 not found!");
      } else {
        mcp3208[i] = device;
        device_installed[i] = true;
        Serial.print(i); Serial.print(") "); Serial.println("MCP3208 configured.");
      }
    }

    // Connect to Seesaw
    if(devices[i][0] == (int16_t)Source::Ada1616 || devices[i][0] == (int16_t)Source::Encoder) {
      Adafruit_seesaw device(&Wire1);
      if (!device.begin((uint8_t)devices[i][1])) {
        Serial.print(i); Serial.print(") "); Serial.println("Seesaw not found!");
      } else {
        seesaw[i] = device;
        device_installed[i] = true;
        interrupt_mask[i] = 0;
        button_mask[i] = 0;
        Serial.print(i); Serial.print(") "); Serial.println("Seesaw configured.");
      }
    }

    // Connect to BNO055
    if(devices[i][0] == (int16_t)Source::BNO055) {  
      Adafruit_BNO055 device = Adafruit_BNO055(55, (uint8_t)devices[i][1], &Wire1);  
      if (!device.begin()) {
        Serial.print(i); Serial.print(") "); Serial.println("BNO055 not found!");
      } else {
        bno055[i] = device;
        device_installed[i] = true;
        device.setExtCrystalUse(true);
        Serial.print(i); Serial.print(") "); Serial.println("BNO055 configured.");
      }
    }
  }

  // Setup axes pins
  for (uint8_t i = 0; i < axes_size; i++) {
    if (devices[axes[i][0]][0] == (int16_t)Source::Local) {
      // Serial.print(i); Serial.print(") "); Serial.println("local axis configured.");
      pinMode(axes[i][1], INPUT);
    } else if (devices[axes[i][0]][0] == (int16_t)Source::Ada1616 && device_installed[axes[i][0]]) {
      // Serial.print(i); Serial.print(") "); Serial.println("ada1616 axis configured.");
      seesaw[axes[i][0]].pinMode(axes[i][1], INPUT);
    } else if (devices[axes[i][0]][0] == (int16_t)Source::Encoder && device_installed[axes[i][0]]) {
      // Serial.print(i); Serial.print(") "); Serial.println("encoder axis configured.");
      seesaw[axes[i][0]].pinMode(axes[i][1], INPUT);
    }
  }

  // Setup button pins
  touchSetDefaultThreshold(5);
  Serial.print("button size: "); Serial.println(buttons_size);
  for (uint8_t i = 0; i < buttons_size; i++) {
    if (devices[buttons[i][0]][0] == (int16_t)Source::Local) {
      if (buttons[i][2] == (int16_t)ButtonType::Digital) {
        Serial.print(i); Serial.print(") "); Serial.println("local digital configured.");
        pinMode(buttons[i][1], INPUT_PULLUP);
        attachInterruptArg(
          buttons[i][1], [](void* arg) {
            interruptSource(arg);
          },
          (void*)&interrupt_triggered[buttons[i][0]], CHANGE);
      } else if (buttons[i][2] == (int16_t)ButtonType::Touch) {
        Serial.print(i); Serial.print(") "); Serial.println("local touch configured.");
        touchAttachInterruptArg(
          buttons[i][1], [](void* arg) {
            interruptSource(arg);
          },
          (void*)&interrupt_triggered[buttons[i][0]], touch_threshold);
      }
    } else if (devices[buttons[i][0]][0] == (int16_t)Source::Ada1616 && device_installed[buttons[i][0]]) {
      Serial.print(i); Serial.print(") "); Serial.println("Ada1616 digital configured.");
      seesaw[buttons[i][0]].pinMode(buttons[i][1], INPUT_PULLUP);
      interrupt_mask[buttons[i][0]] |= (1UL << buttons[i][1]);
    } else if (devices[buttons[i][0]][0] == (int16_t)Source::Encoder && device_installed[buttons[i][0]]) {
      Serial.print(i); Serial.print(") "); Serial.println("Encoder digital configured.");
      seesaw[buttons[i][0]].pinMode(buttons[i][1], INPUT_PULLUP);
      interrupt_mask[buttons[i][0]] |= (1UL << buttons[i][1]);
    }
  }

  // Setup interrupts
  for (const auto& [index, mask] : interrupt_mask) {
    Serial.print(index); Serial.print(": "); Serial.println(mask);
    if (devices[index][0] == (int16_t)Source::Ada1616 && device_installed[index]) {
      pinMode(devices[index][2], INPUT_PULLUP);
      attachInterruptArg(
        digitalPinToInterrupt(devices[index][2]),
        [](void* arg) {
          interruptSource(arg);
        },
        (void*)&interrupt_triggered[index], FALLING);
      seesaw[index].setGPIOInterrupts(mask, 1);
    } else if (devices[index][0] == (int16_t)Source::Encoder && device_installed[index]) {
      pinMode(devices[index][2], INPUT_PULLUP);
      attachInterruptArg(
        digitalPinToInterrupt(devices[index][2]),
        [](void* arg) {
          interruptSource(arg);
        },
        (void*)&interrupt_triggered[index], FALLING);
      seesaw[index].setGPIOInterrupts(mask, 1);
      seesaw[index].enableEncoderInterrupt();
    }
  }

  // Main loop
  for (;;) {
    unsigned long begin = millis();

    // Make copy of interrupt state and reset
    bool interrupt[devices_size] = { false };
    for (uint8_t i = 0; i < devices_size; i++) {
      interrupt[i] = interrupt_triggered[i];
      interrupt_triggered[i] = false;
      if(interrupt[i]) {
        Serial.print("interrupt: "); Serial.println(i); 
      }
    }

    // Read Imu Data
    std::map<int16_t, double> x, y, z, rx, ry, rz;
    for (uint8_t i = 0; i < devices_size; i++) {
      if (devices[i][0] == (int16_t)Source::BNO055 && device_installed[i]) {
        sensors_event_t event_euler, event_gyro;
        bno055[i].getEvent(&event_euler, Adafruit_BNO055::VECTOR_EULER);
        bno055[i].getEvent(&event_gyro, Adafruit_BNO055::VECTOR_GYROSCOPE);
        x[i] = event_euler.orientation.y;
        y[i] = event_euler.orientation.z;
        z[i] = event_euler.orientation.x;
        if (z[i] > 180.0) z[i] -= 360.0;
        rx[i] = event_gyro.gyro.y;
        ry[i] = event_gyro.gyro.x;
        rz[i] = event_gyro.gyro.z;
        // Serial.print(x[i]); Serial.print(":"); Serial.print(y[i]); Serial.print(":"); Serial.println(z[i]);
      }
    }

    // Read axes values from sources
    for (uint8_t i = 0; i < axes_size; ++i) {
      if (devices[axes[i][0]][0] == (int16_t)Source::Local) {
        axis_values[i] = analogRead(axes[i][1]);
        axis_changed[i] = true;
      } else if (devices[axes[i][0]][0] == (int16_t)Source::MCP3208) {
        axis_values[i] = mcp3208[axes[i][0]].readADC(axes[i][1]);
        axis_changed[i] = true;
      } else if (devices[axes[i][0]][0] == (int16_t)Source::Ada1616 && device_installed[axes[i][0]] && interrupt[axes[i][0]]) {
        axis_values[i] = seesaw[axes[i][0]].analogRead(axes[i][1]);
        axis_changed[i] = true; 
      } else if (devices[axes[i][0]][0] == (int16_t)Source::Encoder && device_installed[axes[i][0]] && interrupt[axes[i][0]]) {
        axis_values[i] = seesaw[axes[i][0]].getEncoderPosition();
        axis_changed[i] = true;
      } else if (devices[axes[i][0]][0] == (int16_t)Source::BNO055 && device_installed[axes[i][0]]) {
        if (axes[i][1] == 0) { axis_values[i] = x[axes[i][0]]; }
        if (axes[i][1] == 1) { axis_values[i] = y[axes[i][0]]; }
        if (axes[i][1] == 2) { axis_values[i] = z[axes[i][0]]; }
        if (axes[i][1] == 3) { axis_values[i] = rx[axes[i][0]]; }
        if (axes[i][1] == 4) { axis_values[i] = ry[axes[i][0]]; }
        if (axes[i][1] == 5) { axis_values[i] = rz[axes[i][0]]; }
        axis_changed[i] = true;
      }
    }

    // scale and shift axes values
    for (uint8_t i = 0; i < axes_size; ++i) {
      if(axis_changed[i] == true) {
        axis_values[i] = (axis_values[i] - axes[i][2]) * axes[i][3];
        axis_values[i] = std::clamp(axis_values[i], (int32_t)axes[i][4], (int32_t)axes[i][5]);
      }
      axis_changed[i] = false;      
    }

    // Read buttons values from sources
    for (uint8_t i = 0; i < buttons_size; ++i) {
      if (devices[buttons[i][0]][0] == (int16_t)Source::Local && interrupt[buttons[i][0]]) {
        if (buttons[i][2] == (int16_t)ButtonType::Digital) {
          button_values[i] = !digitalRead(buttons[i][1]);
        } else if (buttons[i][2] == (int16_t)ButtonType::Touch) {
          if (touchInterruptGetLastStatus(buttons[i][1])) {
            button_values[i] = true;
          } else {
            button_values[i] = false;
          }
        }
      } else if (devices[buttons[i][0]][0] == (int16_t)Source::Ada1616 && device_installed[buttons[i][0]] && interrupt[buttons[i][0]]) {
        button_mask[buttons[i][0]] |= (1UL << buttons[i][1]);
      } else if (devices[buttons[i][0]][0] == (int16_t)Source::Encoder && device_installed[buttons[i][0]] && interrupt[buttons[i][0]]) {
        button_values[i] = !seesaw[buttons[i][0]].digitalRead(buttons[i][1]);
      }
    }

    for (const auto& [index, mask] : button_mask) {
      if (devices[index][0] == (int16_t)Source::Ada1616 && device_installed[index]) {
        uint32_t state = seesaw[index].digitalReadBulk(mask);
        for (uint8_t i = 0; i < buttons_size; ++i) {
          if (buttons[i][0] == index) {
            button_values[i] = !(state & (1UL << buttons[i][1]));
          }
        }
      }
    }

    // UBaseType_t stack_high_water_mark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print(" high water mark free: "); Serial.println(stack_high_water_mark);

    xQueueOverwrite(queueAxes, axis_values);
    xQueueOverwrite(queueButtons, button_values);

    unsigned long end = millis();
    // Serial.print("     Read Time: "); Serial.println(end - begin);
    // Serial.print("     Loop Time: "); Serial.println(end - last_read);
    last_read = end;
    vTaskDelay(5 / portTICK_PERIOD_MS);  // 5ms
  }
}

void taskReportHid(void* parameter) {
  // Report
  mouse_report mouse;
  keyboard_report keyboard;
  joystick_report joystick1;
  joystick_report joystick2;

  xQueueOverwrite(queueMouse, &mouse);
  xQueueOverwrite(queueKeyboard, &keyboard);
  xQueueOverwrite(queueJoystick1, &joystick1);
  xQueueOverwrite(queueJoystick2, &joystick2);  

  unsigned long last_report = millis();
  
  for (;;) {
    unsigned long begin = millis();
    // get axes
    // Serial.println("Get axes");
    int32_t a[axes_size] = { 0 };
    if (!xQueuePeek(queueAxes, &a, pdMS_TO_TICKS(1))) {
      Serial.println("Failed to get Axes from queue.");
    }

    // get buttons
    // Serial.println("Get buttons");
    bool b[buttons_size] = { false };
    if (!xQueuePeek(queueButtons, &b, pdMS_TO_TICKS(1))) {
      Serial.println("Failed to get Buttons from queue.");
    }

    // get mode
    uint8_t mode = 0;
    if (!xQueuePeek(queueMode, &mode, pdMS_TO_TICKS(1))) {
      Serial.println("Failed to get Mode from queue.");
    }

    // get matrix
    int current_matrix = 0;
    if (!xQueuePeek(queueMatrix, &current_matrix, pdMS_TO_TICKS(1))) {
      Serial.println("Failed to get current_matrix from queue.");
    }

    // Update HID reports
    updateHidReports(a, b, mode, current_matrix, mouse, keyboard, joystick1, joystick2);

    if(mode == 2) {
      xQueueOverwrite(queueMouse, &mouse);
      xQueueOverwrite(queueKeyboard, &keyboard);
    } else {
      xQueueOverwrite(queueJoystick1, &joystick1);
      xQueueOverwrite(queueJoystick2, &joystick2);
    }

    // UBaseType_t stack_high_water_mark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print(" high water mark free: "); Serial.println(stack_high_water_mark);

    unsigned long end = millis();
    // Serial.print("   Report Time: "); Serial.println(end - begin);
    // Serial.print("     Loop Time: "); Serial.println(end - last_report);
    last_report = end;

    vTaskDelay(5 / portTICK_PERIOD_MS);  // 5ms
  }
}

void taskSendHid(void* parameter) {
  // HID report descriptor
  uint8_t const desc_hid_report[] = {
      MY_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(MOUSE_ID)),
      MY_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(KEYBOARD_ID)),
      MY_HID_REPORT_DESC_JOYSTICK(HID_REPORT_ID(JOYSTICK1_ID)),
      MY_HID_REPORT_DESC_JOYSTICK(HID_REPORT_ID(JOYSTICK2_ID))
  };

  // Set up HID
  Adafruit_USBD_HID usb_hid;
  usb_hid.setPollInterval(2); // Poll every 2ms
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor("Nosille's Joystick");
  usb_hid.setReportCallback(get_report_callback, set_report_callback);
  usb_hid.begin();
  
  bool release_pending = false;
  unsigned long last_send = millis();

  for (;;) {
    unsigned long begin = millis();

    // #ifdef TINYUSB_NEED_POLLING_TASK
    // // Manual call tud_task since it isn't called by Core's background
    // TinyUSBDevice.task();
    // #endif

    // not enumerated()/mounted() yet: nothing to do
    if (!TinyUSBDevice.mounted()) {
      Serial.println("hid not mounted!");
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    // get mouse
    mouse_report mouse;
    if (!xQueuePeek(queueMouse, &mouse, pdMS_TO_TICKS(1))) {
      Serial.println("Failed to get Mouse from queue.");
    }
  
    // get keyboard
    keyboard_report keyboard;
    if (!xQueuePeek(queueKeyboard, &keyboard, pdMS_TO_TICKS(1))) {
      Serial.println("Failed to get Keyboard from queue.");
    }

    // get joystick
    joystick_report joystick1;
    if (!xQueuePeek(queueJoystick1, &joystick1, pdMS_TO_TICKS(1))) {
      Serial.println("Failed to get Joystick1 from queue.");
    }
    joystick_report joystick2;
    if (!xQueuePeek(queueJoystick2, &joystick2, pdMS_TO_TICKS(1))) {
      Serial.println("Failed to get Joystick2 from queue.");
    }

    // Remote wakeup
    if (TinyUSBDevice.suspended() && joystick1.joystick.buttons) {
      // Wake up host if we are in suspend mode
      // and REMOTE_WAKEUP feature is enabled by host
      TinyUSBDevice.remoteWakeup();
    }

    // if(!usb_hid.ready()) {
    //   Serial.println("hid not ready!");
    // }

    if (joystick1.needs_send && usb_hid.ready()) {
      usb_hid.sendReport(JOYSTICK1_ID, &joystick1.joystick, sizeof(joystick1.joystick));
      joystick1.needs_send = false;
      xQueueOverwrite(queueJoystick1, &joystick1);
    }

    if (joystick2.needs_send && usb_hid.ready()) {
      usb_hid.sendReport(JOYSTICK2_ID, &joystick2.joystick, sizeof(joystick2.joystick));
      joystick2.needs_send = false;
      xQueueOverwrite(queueJoystick2, &joystick2);
    }

    if (mouse.needs_send && usb_hid.ready()) {
      usb_hid.mouseReport(MOUSE_ID, mouse.buttons, mouse.x, mouse.y, mouse.v, mouse.h);
      mouse.needs_send = false;
      xQueueOverwrite(queueMouse, &mouse);
    }

    if (keyboard.needs_send && usb_hid.ready()) {
      usb_hid.keyboardReport(KEYBOARD_ID, keyboard.modifier, keyboard.keys);
      release_pending = true;
      xQueueOverwrite(queueKeyboard, &keyboard);
    } else if (release_pending && usb_hid.ready()) {
      usb_hid.keyboardRelease(KEYBOARD_ID);
      release_pending = false;
      xQueueOverwrite(queueKeyboard, &keyboard);      
    }

    // UBaseType_t stack_high_water_mark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print(" high water mark free: "); Serial.println(stack_high_water_mark);

    unsigned long end = millis();
    // Serial.print("     Send Time: "); Serial.println(end - begin);
    // Serial.print("     Loop Time: "); Serial.println(end - last_send);
    last_send = end;

    vTaskDelay(2 / portTICK_PERIOD_MS);  // 5ms
  }
}

void taskDisplay(void* parameter) {
  // Devices
  Display display(SH1107_ADDR, &Wire);
  bool display_installed = false;
  
  // Global variables
  uint8_t display_mode = 0;
  unsigned long last_display = millis();

  uint8_t current_matrix = 0;
  const unsigned long kMatrixDebounceMs = 200;
  bool last_matrix_inc = false;
  bool last_matrix_dec = false;
  unsigned long last_matrix_change_ms = 0;
  
  // I2C
  Wire.begin();
  Wire.setClock(400000L);

  // Connect to OLED Display
  if (!display.begin()) {
    Serial.println("Couldn't find display!");
  } else {
    display_installed = true;
    Serial.println("OLED configured.");
  }

  // Setup display
  if (display_installed) {
    display.setup();
  }

  for (;;) {
    unsigned long begin = millis();    
    // get axes
    // Serial.println("Get axes");
    int32_t a[axes_size] = { 0 };
    if (!xQueuePeek(queueAxes, &a, 0)) {
      Serial.println("Failed to get Axes from queue.");
    }

    // get buttons
    // Serial.println("Get buttons");
    bool b[buttons_size] = { false };
    if (!xQueuePeek(queueButtons, &b, 0)) {
      Serial.println("Failed to get Buttons from queue.");
    }

    // switch modes with debounce
    if (b[1] && display_mode == 2) {
      Serial.println("Switching to Joystick Front");
      display_mode = 0;
      delay(100);
      if (display_installed) display.switchMode(display_mode);

      mouse_report mouse;
      mouse.x = 0;
      mouse.y = 0;
      mouse.v = 0;
      mouse.h = 0;
      mouse.buttons = 0;
      mouse.needs_send = true;
      xQueueOverwrite(queueMouse, &mouse);
      delay(400);
    } else if (b[1] && display_mode == 0) {
      Serial.println("Switching to Joystick Back");
      display_mode = 1;
      delay(100);
      if (display_installed) display.switchMode(display_mode);
      delay(400);
    } else if (b[1] && display_mode == 1) {
      Serial.println("Switching to Keyboard");
      display_mode = 2;
      delay(100);
      if (display_installed) display.switchMode(display_mode);

      joystick_report joystick1;
      joystick1.joystick.x = 0.0;
      joystick1.joystick.y = 0.0;
      joystick1.joystick.z = 0.0;
      joystick1.joystick.rx = 0.0;
      joystick1.joystick.ry = 0.0;
      joystick1.joystick.rz = 0.0;
      joystick1.joystick.slider = 0.0;
      joystick1.joystick.dial = 0.0;
      joystick1.joystick.wheel = 0.0;
      joystick1.joystick.buttons = 0;
      joystick1.needs_send = true;
      xQueueOverwrite(queueJoystick1, &joystick1);

      joystick_report joystick2;
      joystick2.joystick.x = 0.0;
      joystick2.joystick.y = 0.0;
      joystick2.joystick.z = 0.0;
      joystick2.joystick.rx = 0.0;
      joystick2.joystick.ry = 0.0;
      joystick2.joystick.rz = 0.0;
      joystick2.joystick.slider = 0.0;
      joystick2.joystick.dial = 0.0;
      joystick2.joystick.wheel = 0.0;
      joystick2.joystick.buttons = 0;
      joystick2.needs_send = true;
      xQueueOverwrite(queueJoystick2, &joystick2);
      delay(400);
    }

    // Determine keyboard matrix mode
    char const(*matrix)[k_keyCols];
    const bool matrix_inc_pressed = b[0];
    const bool matrix_dec_pressed = b[2];
    const unsigned long now = millis();

    if(display_mode == 2) {
      if (matrix_inc_pressed && !last_matrix_inc && (now - last_matrix_change_ms) > kMatrixDebounceMs) {
        current_matrix++;
        last_matrix_change_ms = now;
        // Serial.print("current_matrix: "); Serial.println(current_matrix);
      } else if (matrix_dec_pressed && !last_matrix_dec && (now - last_matrix_change_ms) > kMatrixDebounceMs) {
        current_matrix--;
        last_matrix_change_ms = now;
        // Serial.print("current_matrix: "); Serial.println(current_matrix);
      }
      if (current_matrix < 0) {
        current_matrix = k_numMatrices - 1;
      } else if (current_matrix >= k_numMatrices) {
        current_matrix = 0;
      }
      last_matrix_inc = matrix_inc_pressed;
      last_matrix_dec = matrix_dec_pressed;
    }

    xQueueOverwrite(queueMode, &display_mode);
    xQueueOverwrite(queueMatrix, &current_matrix);

    // Update display
    if (display_installed) {
      display.update(&current_matrix, a, b);
    }

    // display info
    unsigned long end = millis();
    // if (display_installed) {
    //   display.updateInfo(end, last_display);
    // }

    // UBaseType_t stack_high_water_mark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print(" high water mark free: "); Serial.println(stack_high_water_mark);

    // Serial.print("  Display Time: "); Serial.println(end - begin);
    // Serial.print("     Loop Time: "); Serial.println(end - last_display);
    last_display = end;

    vTaskDelay(50 / portTICK_PERIOD_MS);  // 50ms
  }
}

void setLeds() {
  // Setup arcade controllers
  for (uint8_t i = 0; i < led_size; ++i) {
    // if (leds[i][0] == (int16_t)Source::Local) {
    //   analogWrite(leds[i][1], leds_values[i]);
    // } else if (device_installed[(int16_t)Source::ArcadeLeft] && leds[i][0] == (int16_t)Source::ArcadeLeft) {
    //   arcade_left.analogWrite(leds[i][1], leds_values[i]);
    // } else if (device_installed[(int16_t)Source::ArcadeRight] && leds[i][0] == (int16_t)Source::ArcadeRight) {
    //   arcade_right.analogWrite(leds[i][1], leds_values[i]);
    // } else if (device_installed[(int16_t)Source::EncoderLeft] && leds[i][0] == (int16_t)Source::EncoderLeft) {
    //   encoder_left.analogWrite(leds[i][1], leds_values[i]);
    // } else if (device_installed[(int16_t)Source::EncoderRight] && leds[i][0] == (int16_t)Source::EncoderRight) {
    //   encoder_right.analogWrite(leds[i][1], leds_values[i]);
    // }
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t get_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  // not used in this example
  (void)buffer;
  (void)reqlen;

  // Populate the buffer with led data
  if (report_id == JOYSTICK1_ID) {
    if (report_type == HID_REPORT_TYPE_FEATURE) {
      buffer[0] = report_id;
      for (uint8_t i = 0; i < led_size; ++i) {
        buffer[1 + i] = leds_values[i];
      }
      return 1 + led_size * sizeof(leds_values[0]);  // Return the number of bytes written
    }
  }
  if (report_id == JOYSTICK2_ID) {
    if (report_type == HID_REPORT_TYPE_FEATURE) {
      buffer[0] = report_id;
      for (uint8_t i = 0; i < led_size; ++i) {
        buffer[1 + i] = leds_values[i];
      }
      return 1 + led_size * sizeof(leds_values[0]);  // Return the number of bytes written
    }
  }
  return 0;  // Unsupported report
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void set_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  // This example doesn't use report ID
  (void)report_id;

  // Check if it is the correct report and if SDL sent output data
  if (report_id == JOYSTICK1_ID) {
    if (report_type == HID_REPORT_TYPE_OUTPUT) {
      // Buffer contains the LED/Rumble data from SDL
      for (uint8_t i = 0; i < led_size; ++i) {
        if (bufsize > i) {
          leds_values[i] = buffer[i];
        }
      }
    }
  }

  if (report_id == JOYSTICK2_ID) {
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

void updateHidReports(const int32_t* a, const bool* b, const bool mode, const uint8_t& current_matrix,
              mouse_report& mouse, keyboard_report& keyboard,
              joystick_report& joystick1, joystick_report& joystick2) {
  // Update active report
  // Keyboard Mode
  if (mode) {
    // populate pending mouse data
    {
      // mouse axes
      int8_t mx =  static_cast<int8_t>(a[mouse_axes[0][0]] / mouse_axes[0][1]);
      int8_t my = -static_cast<int8_t>(a[mouse_axes[1][0]] / mouse_axes[1][1]);
      int8_t mh =  static_cast<int8_t>(a[mouse_axes[2][0]] / mouse_axes[2][1]);
      int8_t mv =  static_cast<int8_t>(a[mouse_axes[3][0]] / mouse_axes[3][1]);

      if (mx < mouse_axes[0][2]) mx = mouse_axes[0][2];
      if (mx > mouse_axes[0][3]) mx = mouse_axes[0][3];
      if (my < mouse_axes[1][2]) my = mouse_axes[1][2];
      if (my > mouse_axes[1][3]) my = mouse_axes[1][3];
      if (mh < mouse_axes[2][2]) mh = mouse_axes[2][2];
      if (mh > mouse_axes[2][3]) mh = mouse_axes[2][3];
      if (mv < mouse_axes[3][2]) mv = mouse_axes[3][2];
      if (mv > mouse_axes[3][3]) mv = mouse_axes[3][3];

      mouse.x = mx;
      mouse.y = my;
      mouse.h = mh;
      mouse.v = mv;

      // mouse buttons
      mouse.buttons = 0;
      for(int i = 0; i < std::min(mouse_buttons_size, (uint8_t)5); i++) {
        if(mouse_buttons[i] >= 0) {
          mouse.buttons |=  b[mouse_buttons[i]] << i;
        }
      }
      mouse.needs_send = true;
    }

    // populate pending keyboard data
    {
      // Get keymatrix
      char const (*matrix)[k_keyCols];
      matrix = k_keyMatrix[current_matrix];

      // Calculate joystick position on keyMatrix
      int8_t key_x = static_cast<int8_t>( a[keyboard_axes[0][0]] * (k_keyCols - 1) * 8 / 2 / 32767);   // characters are on an 8x8 pixel grid
      int8_t key_y = static_cast<int8_t>(-a[keyboard_axes[1][0]] * (k_keyRows - 1) * 8 / 2 / 32767);  // characters are on an 8x8 pixel grid
      uint8_t key_i = (key_y + (k_keyRows)*8 / 2) / 8;
      uint8_t key_j = (key_x + (k_keyCols)*8 / 2) / 8;

      uint8_t modifier = 0;
      std::vector<uint8_t> keys;
      keys.reserve(6);  // Reserve space for up to 6 keys

      // Capture matrix key if button pressed
      if (b[keyboard_buttons[0][0]]) {
        modifier = k_ascii2hid[(uint8_t)matrix[key_i][key_j]][0];
        keys.push_back(k_ascii2hid[(uint8_t)matrix[key_i][key_j]][1]);
      }

      // direct axis to keys
      for(int i = 2; i < keyboard_axes_size; i++) {
        if (a[keyboard_axes[i][0]] < keyboard_axes[i][1]) {
          if (keys.size() < 6) keys.push_back(keyboard_axes[i][2]);
        } else if (a[keyboard_axes[i][0]] > keyboard_axes[i][3]) {
          if (keys.size() < 6) keys.push_back(keyboard_axes[i][4]);
        }
      }

      // direct button to keys
      for(int i = 1; i < keyboard_buttons_size; i++) {
        if (b[keyboard_buttons[i][0]]) {
          if (keys.size() < 6) keys.push_back(keyboard_buttons[i][1]);
        }
      }

      // reset keyboard report
      keyboard.modifier = 0;
      for (auto& key : keyboard.keys) {
        key = 0;
      }
      // update report with new keys
      if (keys.size() > 0) {
        keyboard.modifier = modifier;
        for (size_t i = 0; i < keys.size(); i++) {
          keyboard.keys[i] = keys[i];
        }
        keyboard.needs_send = true;
      }
    }

  // Joystick mode
  } else {
    // Store joystick1 data for later sending via callback
    joystick1.joystick.x      =  static_cast<int16_t>(a[joystick_axes[0][0]]);
    joystick1.joystick.y      =  static_cast<int16_t>(a[joystick_axes[1][0]]);
    joystick1.joystick.z      =  static_cast<int16_t>(a[joystick_axes[2][0]]);
    joystick1.joystick.rx     =  static_cast<int16_t>(a[joystick_axes[3][0]]);
    joystick1.joystick.ry     =  static_cast<int16_t>(a[joystick_axes[4][0]]);
    joystick1.joystick.rz     =  static_cast<int16_t>(a[joystick_axes[5][0]]);
    joystick1.joystick.slider =  static_cast<int16_t>(a[joystick_axes[6][0]]);
    joystick1.joystick.dial   =  static_cast<int16_t>(a[joystick_axes[7][0]]);
    joystick1.joystick.wheel  =  static_cast<int16_t>(a[joystick_axes[8][0]]);
    joystick1.joystick.buttons = 0;
    for(int i = 0; i < std::min(joystick_buttons_size, (uint8_t)32); i++) {
      if(joystick_buttons[i][0] >= 0) joystick1.joystick.buttons |=  (b[joystick_buttons[i][0]] << i);
    }
    // Flag joystick1 for pending send
    joystick1.needs_send = true;

    // Store joystick2 data for later sending via callback
    joystick2.joystick.x      =  static_cast<int16_t>(a[joystick_axes[0][1]]);
    joystick2.joystick.y      =  static_cast<int16_t>(a[joystick_axes[1][1]]);
    joystick2.joystick.z      =  static_cast<int16_t>(a[joystick_axes[2][1]]);
    joystick2.joystick.rx     =  static_cast<int16_t>(a[joystick_axes[3][1]]);
    joystick2.joystick.ry     =  static_cast<int16_t>(a[joystick_axes[4][1]]);
    joystick2.joystick.rz     =  static_cast<int16_t>(a[joystick_axes[5][1]]);
    joystick2.joystick.slider =  static_cast<int16_t>(a[joystick_axes[6][1]]);
    joystick2.joystick.dial   =  static_cast<int16_t>(a[joystick_axes[7][1]]);
    joystick2.joystick.wheel  =  static_cast<int16_t>(a[joystick_axes[8][1]]);
    joystick2.joystick.buttons = 0;
    for(int i = 0; i < std::min(joystick_buttons_size, (uint8_t)32); i++) {
      if(joystick_buttons[i][1] >= 0) joystick2.joystick.buttons |= (b[joystick_buttons[i][1]] << i);
    }
    // Flag that joystick2 needs to be sent
    joystick2.needs_send = true;  
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

  // If already enumerated, additional class driver begin() e.g msc, hid, midi won't take effect until re-enumeration
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }

  // Pause execution until serial msg is recieved (helps with debugging)
  // while (Serial.available() == 0) {
  //   // Do nothing, just wait
  // }

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

  queueMode = xQueueCreate(QUEUE_SIZE, sizeof(uint8_t));
  if (queueMode == NULL) {
    Serial.println("Failed to create mode queue!");
    while (1);
  }

  queueMatrix = xQueueCreate(QUEUE_SIZE, sizeof(int));
  if (queueMatrix == NULL) {
    Serial.println("Failed to create matrix queue!");
    while (1);
  }

  queueMouse = xQueueCreate(QUEUE_SIZE, sizeof(mouse_report));
  if (queueMouse == NULL) {
    Serial.println("Failed to create mouse queue!");
    while (1);
  }

  queueKeyboard = xQueueCreate(QUEUE_SIZE, sizeof(keyboard_report));
  if (queueMouse == NULL) {
    Serial.println("Failed to create keyboard queue!");
    while (1);
  }

  queueJoystick1 = xQueueCreate(QUEUE_SIZE, sizeof(joystick_report));
  if (queueJoystick1 == NULL) {
    Serial.println("Failed to create joystick1 queue!");
    while (1);
  }

  queueJoystick2 = xQueueCreate(QUEUE_SIZE, sizeof(joystick_report));
  if (queueJoystick2 == NULL) {
    Serial.println("Failed to create joystick2 queue!");
    while (1);
  }

  // Create tasks
  xTaskCreatePinnedToCore(
    taskReadInputs,
    "readTask",
    4096,  // Task stack
    NULL,
    2,
    &taskInputsHandle,
    1  // Core
  );

  xTaskCreatePinnedToCore(
    taskReportHid,
    "hidReportTask",
    4096,  // Task stack
    NULL,
    2,
    &taskReportHandle,
    1  // Core
  );

  xTaskCreatePinnedToCore(
    taskSendHid,
    "hidSendTask",
    4096,  // Task stack
    NULL,
    2,
    &taskSendHandle,
    1  // Core
  );

  xTaskCreatePinnedToCore(
    taskDisplay,
    "displayTask",
    4096,  // Task stack
    NULL,
    1,
    &taskDisplayHandle,
    1  // Core
  );

  setLeds();
  Serial.println("Setup complete!");
}

void loop() {

  // if(device_installed[(int16_t)Source::EncoderLeft]) {
  //   encoder_pixel_left.setPixelColor(0, ColorWheel(encoder_pixel_left, a[axes_size - 5] & 0xFF));
  //   encoder_pixel_left.show();
  // }

  // if(device_installed[(int16_t)Source::EncoderRight]) {
  //   encoder_pixel_right.setPixelColor(0, ColorWheel(encoder_pixel_right, a[axes_size - 4] & 0xFF));
  //   encoder_pixel_right.show();
  // }

  yield();
}

uint32_t ColorWheel(seesaw_NeoPixel& pixel, byte WheelPos) {
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
