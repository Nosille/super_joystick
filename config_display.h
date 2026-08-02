#include "display.h"

class DisplayFront {
  private:
    U8G2* m_display;
    DisplayJoy m_joy1, m_joy2, m_joy3, m_joy4;
    DisplayAxis m_axis1;
    DisplayButton m_button1, m_button2, m_button3, m_button4;
    DisplayButton m_button5, m_button6, m_button7, m_button8;
    DisplayButton m_button9, m_button10;
    DisplayButton m_button11, m_button12, m_button13, m_button14, m_button15, m_button16;

  public:
    DisplayFront(U8G2 *display)
      : m_joy1(display,  0,  1,  4,  4)      // Upper Left
      , m_joy2(display,  4,  1,  4,  4)      // Upper Right
      , m_joy3(display,  0, 10,  4,  4)      // Lower Left
      , m_joy4(display,  4, 10,  4,  4)      // Lower Right
      , m_axis1(display,  0, 14,  8,  1)     // Bar at Bottom
      , m_button1(display,   0,  7,  1,  1)  // Left D-Pad
      , m_button2(display,   1,  6,  1,  1)  // Left D-Pad
      , m_button3(display,   2,  7,  1,  1)  // Left D-Pad
      , m_button4(display,   1,  8,  1,  1)  // Left D-Pad
      , m_button5(display,   5,  7,  1,  1)  // Right D-Pad
      , m_button6(display,   6,  6,  1,  1)  // Right D-Pad
      , m_button7(display,   7,  7,  1,  1)  // Right D-Pad
      , m_button8(display,   6,  8,  1,  1)  // Right D-Pad
      , m_button9(display,   0,  0,  1,  1)  // Left Touch
      , m_button10(display,  7,  0,  1,  1)  // Right Touch
      , m_button11(display,  2,  0,  1,  1)  // Upper Left Switch 
      , m_button12(display,  5,  0,  1,  1)  // Upper Right Switch     
      , m_button13(display,  2,  5,  1,  1)  // Lower Left Switch
      , m_button14(display,  5,  5,  1,  1)  // Lower Right Switch       
      , m_button15(display,  3,  6,  1,  1)  // Left Encoder
      , m_button16(display,  4,  6,  1,  1)  // Right Encoder
    {
      m_display = display;
    }

    void draw() {
      m_display->clearDisplay();      
      m_display->setDrawColor(1);
      // m_display->drawStr(8, 8, "Joystick:");
      // delay(0); m_display->updateDisplayArea(0, 0, 8, 1);        
      m_joy1.draw();
      m_joy2.draw();
      m_joy3.draw();
      m_joy4.draw();
      m_axis1.draw();
      m_button1.draw();
      m_button2.draw();
      m_button3.draw();
      m_button4.draw();
      m_button5.draw();
      m_button6.draw();
      m_button7.draw();
      m_button8.draw();
      m_button9.draw();
      m_button10.draw(); 
      m_button11.draw();
      m_button12.draw(); 
      m_button13.draw();
      m_button14.draw(); 
      m_button15.draw();
      m_button16.draw();                        
    }

    void update(const int32_t *a, const bool *b) {
      delay(0); m_joy1.update(a[0], -a[1], b[13]); 
      delay(0); m_joy2.update(a[2], -a[3], b[14]); 
      delay(0); m_joy3.update(a[4], -a[5], b[15]); 
      delay(0); m_joy4.update(a[6], -a[7], b[16]);
      delay(0); m_button1.update(b[5]);
      delay(0); m_button2.update(b[6]);
      delay(0); m_button3.update(b[7]);
      delay(0); m_button4.update(b[8]);
      delay(0); m_button5.update(b[9]);
      delay(0); m_button6.update(b[10]);
      delay(0); m_button7.update(b[11]);
      delay(0); m_button8.update(b[12]);
      delay(0); m_button9.update(b[3]);
      delay(0); m_button10.update(b[4]);
      delay(0); m_button11.update(b[17]);
      delay(0); m_button12.update(b[18]);  
      delay(0); m_button13.update(b[19]);
      delay(0); m_button14.update(b[20]); 
      delay(0); m_button15.update(b[25]);
      delay(0); m_button16.update(b[26]);                  

      delay(0); m_axis1.update(a[8]);

      m_display->setDrawColor(0);
      m_display->drawBox(0, 120, 64, 8);
      m_display->setDrawColor(1);
      std::string encposl = std::to_string(a[9]);      
      std::string encposr = std::to_string(a[10]);      
      m_display->drawStr(0, 128, "Enc: "); m_display->drawStr(24, 128, encposl.c_str()); m_display->drawStr(48, 128, encposr.c_str());
      delay(0); m_display->updateDisplayArea(0, 15, 8, 1);
    }
};

class DisplayBack {
  private:
    U8G2* m_display;
    DisplayButton m_button1, m_button2, m_button3, m_button4;

  public:
    DisplayBack(U8G2 *display)
      : m_button1(display,   0,  1,  3,  3)  // Left D-Pad
      , m_button2(display,   5,  1,  3,  3)  // Left D-Pad
      , m_button3(display,   1,  5,  2,  2)  // Left D-Pad
      , m_button4(display,   5,  5,  2,  2)  // Left D-Pad
    {
      m_display = display;
    }

    void draw() {
      m_display->clearDisplay();      
      m_display->setDrawColor(1);
      // m_display->drawStr(8, 8, "Joystick:");
      // delay(0); m_display->updateDisplayArea(0, 0, 8, 1);        
      m_button1.draw();
      m_button2.draw();
      m_button3.draw();
      m_button4.draw();
    }

    void update(const int32_t *a, const bool *b) {
      delay(0); m_button1.update(b[21]);
      delay(0); m_button2.update(b[22]);
      delay(0); m_button3.update(b[23]);
      delay(0); m_button4.update(b[24]);
    }
};

class DisplayKeyboard {
  private:
    U8G2* m_display;
    DisplayJoy m_joy1;
    DisplayAxis m_axis1, m_axis2;
    DisplayButton m_button1, m_button2, m_button3;
    DisplayKeyMatrix m_matrix;

  public:
    DisplayKeyboard(U8G2 *display)
      : m_joy1(display, 2, 2, 5, 4)
      , m_axis1(display, 2, 6, 5, 1, false)
      , m_axis2(display, 7, 2, 1, 4, true)
      , m_button1(display, 3, 1, 1, 1)
      , m_button2(display, 4, 1, 1, 1)
      , m_button3(display, 5, 1, 1, 1)
      , m_matrix(display, 0, 9, 8, 6)
    {
      m_display = display;
    }

    void draw() {
      m_display->clearDisplay();      
      m_display->setDrawColor(1);      
      m_display->drawStr(0, 8, "Keyboard:");
      delay(0); m_display->updateDisplayArea(0, 0, 8, 1);        
      m_joy1.draw();
      m_axis1.draw();
      m_axis2.draw();
      m_button1.draw();
      m_button2.draw();
      m_button3.draw();
      m_display->drawStr(0, 16, "Hom");
      m_display->drawStr(0, 24, "Esc");
      m_display->drawStr(0, 32, "End");
      m_display->drawStr(0, 40, "Ent");
      m_display->drawStr(0, 48, "Sft");
      m_display->drawStr(0, 56, "Ctr");
      m_display->drawStr(0, 64, "Alt");
      delay(0); m_display->updateDisplayArea(0, 1, 3, 7);      
      m_matrix.draw();
    }

    void update(const int *current_matrix, const int32_t *a, const bool *b) {
      // Serial.println("update Keyboard");
      delay(0); m_joy1.update(a[2], -a[3], 0);
      delay(0); m_button1.update(b[14]);
      delay(0); m_button2.update(b[16]);
      delay(0); m_button3.update(b[13]);
      delay(0); m_axis1.update(a[4]);
      delay(0); m_axis2.update(a[5]);
      m_display->setDrawColor(b[ 5] ? 0 : 1); m_display->drawStr(0, 16, "Hom");
      m_display->setDrawColor(b[ 6] ? 0 : 1); m_display->drawStr(0, 24, "Esc");
      m_display->setDrawColor(b[ 7] ? 0 : 1); m_display->drawStr(0, 32, "End");
      m_display->setDrawColor(b[ 8] ? 0 : 1); m_display->drawStr(0, 40, "Ent");
      m_display->setDrawColor(b[ 9] ? 0 : 1); m_display->drawStr(0, 48, "Sft");
      m_display->setDrawColor(b[10] ? 0 : 1); m_display->drawStr(0, 56, "Ctr");
      m_display->setDrawColor(b[11] ? 0 : 1); m_display->drawStr(0, 64, "Alt");
      delay(0); m_display->updateDisplayArea(0, 1, 3, 7);
      
      // Determine current matrixf
      uint8_t n = 0;
      if(abs(*current_matrix) % 3 == 1) n = 1;
      else if(abs(*current_matrix) % 3 == 2) n = 2;
      else n = 0;
      // Serial.print(a[9]); Serial.print(" : "); Serial.println(n);   

      // Caculate joystick position on keyMatrix
      int8_t key_x = static_cast<int8_t>( a[0] * (k_keyCols - 1) * 8 / 2 / 32767);  // characters are 8 pixels wide
      int8_t key_y = static_cast<int8_t>(-a[1] * (k_keyRows - 1) * 8 / 2 / 32767);  // characters are 8 pixels tall
      uint8_t key_i = (key_y + (k_keyRows) * 8 / 2 ) / 8;
      uint8_t key_j = (key_x + (k_keyCols) * 8 / 2 ) / 8;
      delay(0); m_matrix.update(n, key_i, key_j);
    }
};

class Display{
  private:
    U8G2 m_display;
    
    uint8_t display_mode;
    DisplayFront m_displayFront;
    DisplayBack m_displayBack;
    DisplayKeyboard m_displayKeyboard; 

  public:  
    Display(int i2c_addr, TwoWire *Wi = NULL)
      : m_display(U8G2_SH1107_64X128_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE,  Wi->getClock(), Wi->getBusNum()))
      , m_displayFront(&m_display) 
      , m_displayBack(&m_display) 
      , m_displayKeyboard(&m_display) 
    {
      display_mode = 0;
      m_display.setI2CAddress(i2c_addr << 1);
    };

    bool begin() {
      return m_display.begin();
    }

    void setup() {
      m_display.clearDisplay();      
      m_display.setDrawColor(1);
      m_display.setFont(u8g2_font_5x8_mf);
      m_display.drawStr(0, 8, "Starting!");
      delay(0); m_display.updateDisplay();
      // TBD splash screen
      delay(1000);

      // Show front joystick data at first
      switchMode(0);
    }

    void switchMode(uint8_t mode)
    {
      display_mode = mode;
      if(display_mode == 1) {
        // Setup display to show back joystick data
        m_displayBack.draw();
      } else if(display_mode == 2) {
        // Setup display to show keyboard/mouse data
        m_displayKeyboard.draw();
      } else {
        // Setup display to show front joystick data
        m_displayFront.draw();
      }
    }

    void update(const int *current_matrix, const int32_t *a, const bool *b) {
      if (display_mode == 2) {
        m_displayKeyboard.update(current_matrix, a, b);
      } else if (display_mode == 1) {
        m_displayBack.update(a, b);
      } else {
        m_displayFront.update(a, b);
      }
    }

    void updateInfo(const unsigned long &time_current, const unsigned long &time_last) {
      m_display.setDrawColor(0);
      m_display.drawBox(0, 120, 64, 8);
      m_display.setDrawColor(1);
      std::string timestr = std::to_string(time_current-time_last);
      m_display.drawStr(0, 128, "DT: "); m_display.drawStr(24, 128, timestr.c_str());
      // Serial.print("DT: "); Serial.println(time_current-time_last);
      delay(0); m_display.updateDisplayArea(0, 15, 8, 1);
    }
};