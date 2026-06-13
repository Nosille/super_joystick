#include "HardwareSerial.h"
#include <U8g2lib.h>
#include <Adafruit_TinyUSB.h>

#include "key_matrix.h"

// Draws a box containing a circle representing the joystick position.
// U8g2 breaks the display up into 8x8 pixel tiles for quick updates.
// Therefore the box is sized based on number of tiles it takes up. 
class DisplayJoy {
  private:
    U8G2* m_display;
    uint8_t m_xt, m_yt, m_wt, m_ht;  
    int32_t m_x, m_y, m_w, m_h, m_r;
    int32_t last_x, last_y;

  public:
    DisplayJoy(U8G2 *display, const uint8_t xt, const uint8_t yt, const uint8_t wt, const uint8_t ht, const int16_t r = 2) {
      m_display = display;
      m_xt = xt;
      m_yt = yt;
      m_wt = wt;
      m_ht = ht;      
      m_x = xt * 8;
      m_y = yt * 8;
      m_w = wt * 8;
      m_h = ht * 8;
      m_r = r;
    }
    void draw() {
      m_display->setDrawColor(0);
      m_display->drawBox(m_x, m_y, m_w, m_h); 
      m_display->setDrawColor(1);
      m_display->drawFrame(m_x + 1, m_y + 1, m_w - 2, m_h - 2);
      last_x = 0;
      last_y = 0;
      int32_t point_x = m_x + m_w / 2;
      int32_t point_y = m_y + m_h / 2;
      m_display->drawCircle(point_x, point_y, 2);
      m_display->updateDisplayArea(m_xt, m_yt, m_wt, m_ht);      
    }

    void update(const int32_t &value_x, const int32_t &value_y, const bool &value_b) {
      // remove last circle
      m_display->setDrawColor(0);      
      m_display->drawDisc(last_x, last_y, m_r);
      // draw new circle
      int32_t scaled_x = value_x * (m_w - 2*m_r - 4) / 2 / 32768;
      int32_t scaled_y = value_y * (m_h - 2*m_r - 4) / 2 / 32768;
      int32_t point_x = m_x + m_w / 2 + scaled_x;
      int32_t point_y = m_y + m_h / 2 + scaled_y;
      m_display->setDrawColor(1);
      if(value_b) {
        m_display->drawDisc(point_x, point_y, 2);
      } else {
        m_display->drawCircle(point_x, point_y, 2);
      }
      int32_t min_x = std::min(last_x - m_r - 1, point_x - m_r - 1);
      int32_t max_x = std::max(last_x + m_r + 1, point_x + m_r + 1);
      int32_t min_y = std::min(last_y - m_r - 1, point_y - m_r - 1);
      int32_t max_y = std::max(last_y + m_r + 1, point_y + m_r + 1); 
      min_x = min_x / 8;
      max_x = max_x / 8;
      min_y = min_y / 8;
      max_y = max_y / 8;
      m_display->updateDisplayArea(min_x, min_y, max_x - min_x + 1, max_y - min_y + 1);
      last_x = point_x;
      last_y = point_y;
    }
};

// Draws a bar representing the axis position.
// U8g2 breaks the display up into 8x8 pixel tiles for quick updates.
// Therefore the bar is sized based on number of tiles it takes up. 
class DisplayAxis {
  private:
    U8G2* m_display;
    uint8_t m_xt, m_yt, m_wt, m_ht;
    int32_t m_x, m_y, m_w, m_h;
    bool m_is_vert;

  public:
    DisplayAxis(U8G2 *display, const uint8_t xt, const uint8_t yt, const uint8_t wt, const uint8_t ht, bool is_vert = false) {
      m_display = display;
      m_xt = xt;
      m_yt = yt;
      m_wt = wt;
      m_ht = ht;      
      m_x = xt * 8;
      m_y = yt * 8;
      m_w = wt * 8;
      m_h = ht * 8;
      m_is_vert = is_vert;
    }

    void draw() {
      m_display->setDrawColor(0);      
      m_display->drawBox(m_x, m_y, m_w, m_h);
      m_display->setDrawColor(1);      
      if(m_is_vert) {
        m_display->drawBox(m_x + 1, m_y + m_h / 2, m_w - 2, 2);
      } else {
        m_display->drawBox(m_x + m_w / 2, m_y + 1, 2, m_h - 2);
      }
      m_display->updateDisplayArea(m_xt, m_yt, m_wt, m_ht);      
    }
    
    void update(const int32_t &value) {
      m_display->setDrawColor(0);      
      m_display->drawBox(m_x, m_y, m_w, m_h);
      m_display->setDrawColor(1);
      if(m_is_vert) {
        int32_t scaled = value * m_h / 2 / 32768;
        int32_t top    = std::min(m_y + m_h / 2 - 1, m_y + m_h / 2 + scaled);
        int32_t bottom = std::max(m_y + m_h / 2 + 1, m_y + m_h / 2 + scaled); 
        m_display->drawBox(m_x + 1, top, m_w - 2, bottom-top);
      } else {
        int32_t scaled = value * m_w / 2 / 32768;
        int32_t left  = std::min(m_x + m_w / 2 - 1, m_x + m_w / 2 + scaled);
        int32_t right = std::max(m_x + m_w / 2 + 1, m_x + m_w / 2 + scaled); 
        m_display->drawBox(left, m_y + 1, right-left, m_h - 2);
      }
      m_display->updateDisplayArea(m_xt, m_yt, m_wt, m_ht);
    }
};

// Draws a circle representing the button status.
// U8g2 breaks the display up into 8x8 pixel tiles for quick updates.
// Therefore the button is sized based on number of tiles it takes up. 
class DisplayButton {
  private:
    U8G2* m_display;
    uint8_t m_xt, m_yt, m_wt, m_ht; 
    int32_t m_x, m_y, m_w, m_h;
    int32_t m_xc, m_yc, m_r;
    bool last_value;

  public:
    DisplayButton(U8G2 *display, const uint8_t xt, const uint8_t yt, const uint8_t wt, const uint8_t ht) {
      m_display = display;
      m_xt = xt;
      m_yt = yt;
      m_wt = wt;
      m_ht = ht;      
      m_x = xt * 8;
      m_y = yt * 8;
      m_w = wt * 8;
      m_h = ht * 8;
      m_xc = m_x + m_w / 2;
      m_yc = m_y + m_h / 2;
      m_r = std::min(m_w / 2 - 1, m_h / 2 - 1);
      last_value = 0;
    }
    
    void draw() {
      m_display->setDrawColor(0);      
      m_display->drawBox(m_x , m_y, m_w, m_h);
      m_display->setDrawColor(1);      
      m_display->drawCircle(m_xc, m_yc, m_r);
      m_display->updateDisplayArea(m_xt, m_yt, m_wt, m_ht);      
    }

    void update(const int32_t &value) {
      if(value) {
        if(!last_value) {
          m_display->setDrawColor(1);          
          m_display->drawDisc(m_xc, m_yc, m_r);
        }
      } else {
        if (last_value) {
          m_display->setDrawColor(0);          
          m_display->drawDisc(m_xc, m_yc, m_r);
          m_display->setDrawColor(1);
          m_display->drawCircle(m_xc, m_yc, m_r);
        }
        
      }
      last_value = value;
      m_display->updateDisplayArea(m_xt, m_yt, m_wt, m_ht);   
    }
};

// Draws a grid of text boxes.
// U8g2 breaks the display up into 8x8 pixel tiles for quick updates.
// Therefore the bar is sized based on number of tiles it takes up. 
class DisplayKeyMatrix {
  private:
    U8G2* m_display;
    char const (*m_matrix)[k_keyCols]; 
    uint8_t m_xt, m_yt, m_wt, m_ht;
    int32_t m_x, m_y, m_w, m_h;
    uint8_t last_n, last_i, last_j;

  public:
    DisplayKeyMatrix(U8G2 *display, const uint8_t xt, const uint8_t yt, const uint8_t wt, const uint8_t ht)
    {
      m_display = display;
      m_xt = xt;
      m_yt = yt;
      m_wt = wt;
      m_ht = ht; 
      m_x = xt * 8;
      m_y = yt * 8;
      m_w = wt * 8;
      m_h = ht * 8;
      
      last_n = 0;
      last_i = k_keyRows / 2;
      last_j = k_keyCols / 2;              
    }

    void draw() {
      updateMatrix(0);
    }

    void updateMatrix(uint8_t n) {
      // Serial.println("updateMatrix");
      if(n == 1) {
        m_matrix = k_keyMatrix_L2;
      } else if(n == 2) {
        m_matrix = k_keyMatrix_L3;
      } else {
        m_matrix = k_keyMatrix_L1;
      }
      m_display->setDrawColor(0);
      m_display->drawBox(m_x, m_y, m_w, m_h);
      m_display->setDrawColor(1);             
      for(uint8_t i = 0; i < k_keyRows; i++){
        for(uint8_t j = 0; j < k_keyCols; j++) {
          drawKey(i, j);
        }
      }      
      m_display->updateDisplayArea(m_xt, m_yt, m_wt, m_ht);
      last_n = n;
    }
    
    void update(const uint8_t &n, const uint8_t &key_i, const uint8_t &key_j) {
      // if index changed updateMatrix
      if (n != last_n) {
        updateMatrix(n);
      }
      // if same key as last pass exit
      if(last_i == key_i && last_j == key_j) return;
      // Reset last key
      m_display->setDrawColor(0);
      m_display->drawBox(m_x + 8 * last_j, m_y + 8 * last_i, 8, 8);
      m_display->setDrawColor(1);
      drawKey(last_i, last_j);
      m_display->updateDisplayArea(m_xt + last_j, m_yt + last_i, 1, 1);
      // Highlight new key
      m_display->setDrawColor(1);
      m_display->drawBox(m_x + 8 * key_j, m_y + 8 * key_i, 8, 8);      
      m_display->setDrawColor(0);
      drawKey(key_i, key_j);
      m_display->updateDisplayArea(m_xt + key_j, m_yt + key_i, 1, 1);
      // Store last key
      last_i = key_i;
      last_j = key_j;
      // m_display->updateDisplay();      
    }

    void drawKey(uint8_t i, uint8_t j) {
      m_display->setCursor(m_x + 8 * j, m_y + 8 + 8 * i);          
      if (     m_matrix[i][j] == 0x08) {m_display->setFont(u8g2_font_micro_tr); m_display->print("bs");} // backspace
      else if (m_matrix[i][j] == 0x0A) {m_display->setFont(u8g2_font_micro_tr); m_display->print("lf");} // line feed
      else if (m_matrix[i][j] == 0x0D) {m_display->setFont(u8g2_font_micro_tr); m_display->print("cr");} // carriage return
      else if (m_matrix[i][j] == 0x1B) {m_display->setFont(u8g2_font_micro_tr); m_display->print("ec");} // escape
      else if (m_matrix[i][j] == 0x09) {m_display->setFont(u8g2_font_micro_tr); m_display->print("tb");} // tab
      else if (m_matrix[i][j] == 0x02) {m_display->setFont(u8g2_font_micro_tr); m_display->print("hm");} // home
      else if (m_matrix[i][j] == 0x03) {m_display->setFont(u8g2_font_micro_tr); m_display->print("ed");} // end
      else if (m_matrix[i][j] == 0x7F) {m_display->setFont(u8g2_font_micro_tr); m_display->print("dl");} // delete
      else if (m_matrix[i][j] == 0x01) {m_display->setFont(u8g2_font_micro_tr); m_display->print("tp");} // start of file
      else if (m_matrix[i][j] == 0x04) {m_display->setFont(u8g2_font_micro_tr); m_display->print("bt");} // end of file
      else if (m_matrix[i][j] == 0x0B) {m_display->setFont(u8g2_font_micro_tr); m_display->print("ud");} // Undo
      else if (m_matrix[i][j] == 0x0C) {m_display->setFont(u8g2_font_micro_tr); m_display->print("rd");} // Redo
      else if (m_matrix[i][j] == 0x05) {m_display->setFont(u8g2_font_micro_tr); m_display->print("pu");} // page up
      else if (m_matrix[i][j] == 0x06) {m_display->setFont(u8g2_font_micro_tr); m_display->print("pd");} // page down
      else if (m_matrix[i][j] == 0x07) {m_display->setFont(u8g2_font_micro_tr); m_display->print("ps");} // print screen
      else if (m_matrix[i][j] == 0x0E) {m_display->setFont(u8g2_font_micro_tr); m_display->print("pa");} // pause
      else if (m_matrix[i][j] == 0x0F) {m_display->setFont(u8g2_font_micro_tr); m_display->print("br");} // break

      else if (m_matrix[i][j] == 0x11) {m_display->setFont(u8g2_font_micro_tr); m_display->print("F1");} // F1
      else if (m_matrix[i][j] == 0x12) {m_display->setFont(u8g2_font_micro_tr); m_display->print("F2");} // F2
      else if (m_matrix[i][j] == 0x13) {m_display->setFont(u8g2_font_micro_tr); m_display->print("F3");} // F3
      else if (m_matrix[i][j] == 0x14) {m_display->setFont(u8g2_font_micro_tr); m_display->print("F4");} // F4
      else if (m_matrix[i][j] == 0x15) {m_display->setFont(u8g2_font_micro_tr); m_display->print("F5");} // F5
      else if (m_matrix[i][j] == 0x16) {m_display->setFont(u8g2_font_micro_tr); m_display->print("F6");} // F6
      else if (m_matrix[i][j] == 0x17) {m_display->setFont(u8g2_font_micro_tr); m_display->print("F7");} // F7
      else if (m_matrix[i][j] == 0x18) {m_display->setFont(u8g2_font_micro_tr); m_display->print("F8");} // F8
      else if (m_matrix[i][j] == 0x19) {m_display->setFont(u8g2_font_micro_tr); m_display->print("F9");} // F9
      else if (m_matrix[i][j] == 0x1A) {m_display->setFont(u8g2_font_micro_tr); m_display->print("F0");} // F10
      else if (m_matrix[i][j] == 0x1C) {m_display->setFont(u8g2_font_micro_tr); m_display->print("F1");} // F11
      else if (m_matrix[i][j] == 0x1D) {m_display->setFont(u8g2_font_micro_tr); m_display->print("F2");} // F12

      else {
        m_display->setCursor(m_x + 2 + 8 * j, m_y + 8 + 8 * i);   
        m_display->setFont(u8g2_font_5x8_mf); m_display->write(m_matrix[i][j]);
      }
      m_display->setFont(u8g2_font_5x8_mf);
    }
};

class DisplayJoystick {
  private:
    U8G2* m_display;
    DisplayJoy m_joy1, m_joy2, m_joy3, m_joy4;
    DisplayAxis m_axis1;
    DisplayButton m_button1, m_button2, m_button3, m_button4;
    DisplayButton m_button5, m_button6, m_button7, m_button8;
    uint8_t count;

  public:
    DisplayJoystick(U8G2 *display)
      : m_joy1(display, 0, 2, 4, 4)
      , m_joy2(display, 4, 2, 4, 4)
      , m_joy3(display, 0, 9, 4, 4)
      , m_joy4(display, 4, 9, 4, 4)
      , m_axis1(display, 0, 13, 8, 1)
      , m_button1(display, 0, 7, 1, 1)
      , m_button2(display, 1, 6, 1, 1)
      , m_button3(display, 2, 7, 1, 1)
      , m_button4(display, 1, 8, 1, 1)
      , m_button5(display, 5, 7, 1, 1)
      , m_button6(display, 6, 6, 1, 1)
      , m_button7(display, 7, 7, 1, 1)
      , m_button8(display, 6, 8, 1, 1) 
    {
      m_display = display;
      count = 0;
    }

    void draw() {
      m_display->clearDisplay();      
      m_display->setDrawColor(1);
      m_display->drawStr(0, 8, "Joystick:");
      m_display->updateDisplayArea(0, 0, 8, 1);        
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
    }

    void update(const int32_t *a, const bool *b) {
      count++;
      if (count == 1) m_joy1.update(a[0], -a[1], b[8]); 
      if (count == 2) m_joy2.update(a[2], -a[3], b[9]); 
      if (count == 3) m_joy3.update(a[4], -a[5], b[10]); 
      if (count == 4) m_joy4.update(a[6], -a[7], b[11]);
      if (count == 1) m_button1.update(b[0]);
      if (count == 2) m_button2.update(b[1]);
      if (count == 3) m_button3.update(b[2]);
      if (count == 4) m_button4.update(b[3]);
      if (count == 1) m_button5.update(b[4]);
      if (count == 2) m_button6.update(b[5]);
      if (count == 3) m_button7.update(b[6]);
      if (count == 4) m_button8.update(b[7]);

      if (count == 1) m_axis1.update(a[8]);

      if (count == 3) { 
        m_display->setDrawColor(0);
        m_display->drawBox(0, 112, 64, 8);
        m_display->setDrawColor(1);
        std::string encpos = std::to_string(a[9]);      
        m_display->drawStr(0, 120, "Enc: "); m_display->drawStr(24, 120, encpos.c_str());
        m_display->updateDisplayArea(0, 14, 8, 1);
      }
      if(count > 3) count = 0;
    }
};

class DisplayKeyboard {
  private:
    U8G2* m_display;
    DisplayJoy m_joy1;
    DisplayAxis m_axis1, m_axis2;
    DisplayButton m_button1, m_button2, m_button3;
    DisplayKeyMatrix m_matrix;
    uint8_t count;    

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
      count = 0;      
    }

    void draw() {
      m_display->clearDisplay();      
      m_display->setDrawColor(1);      
      m_display->drawStr(0, 8, "Keyboard:");
      m_display->updateDisplayArea(0, 0, 8, 1);        
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
      m_display->updateDisplayArea(0, 1, 3, 7);      
      m_matrix.draw();
    }

    void update(const int32_t *a, const bool *b) {
      // Serial.println("update Keyboard");
      count++;
      // Serial.println(count);      
      if (count == 1) m_joy1.update(a[2], -a[3], 0);
      if (count == 1) m_button1.update(b[9]);
      if (count == 1) m_button2.update(b[10]);
      if (count == 1) m_button3.update(b[11]);
      if (count == 2) m_axis1.update(a[6]);
      if (count == 2) m_axis2.update(a[7]);
      if (count == 3) {
        m_display->setDrawColor(b[0] ? 0 : 1); m_display->drawStr(0, 16, "Hom");
        m_display->setDrawColor(b[1] ? 0 : 1); m_display->drawStr(0, 24, "Esc");
        m_display->setDrawColor(b[2] ? 0 : 1); m_display->drawStr(0, 32, "End");
        m_display->setDrawColor(b[3] ? 0 : 1); m_display->drawStr(0, 40, "Ent");
        m_display->setDrawColor(b[4] ? 0 : 1); m_display->drawStr(0, 48, "Sft");
        m_display->setDrawColor(b[5] ? 0 : 1); m_display->drawStr(0, 56, "Ctr");
        m_display->setDrawColor(b[6] ? 0 : 1); m_display->drawStr(0, 64, "Alt");
        m_display->updateDisplayArea(0, 1, 3, 7);
      }
      
      // Determine current matrixf
      uint8_t n = 0;
      if(abs(a[9]) % 3 == 1) n = 1;
      else if(abs(a[9]) % 3 == 2) n = 2;
      else n = 0;
      // Serial.print(a[9]); Serial.print(" : "); Serial.println(n);   

      // Caculate joystick position on keyMatrix
      if (count == 4) {
        int8_t key_x = static_cast<int8_t>( a[0] * (k_keyCols - 1) * 8 / 2 / 32767);  // characters are 8 pixels wide
        int8_t key_y = static_cast<int8_t>(-a[1] * (k_keyRows - 1) * 8 / 2 / 32767);  // characters are 8 pixels tall
        uint8_t key_i = (key_y + (k_keyRows) * 8 / 2 ) / 8;
        uint8_t key_j = (key_x + (k_keyCols) * 8 / 2 ) / 8;
        m_matrix.update(n, key_i, key_j);
      }
      if(count > 3) count = 0;      
    }
};

class Display{
  private:
    U8G2 m_display;
    
    uint8_t display_mode;
    DisplayJoystick m_displayJoystick;
    DisplayKeyboard m_displayKeyboard; 

  public:  
    Display(int i2c_addr)
      : m_display(U8G2_SH1107_64X128_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE))
      , m_displayJoystick(&m_display) 
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
      m_display.updateDisplay();
      // TBD splash screen
      delay(1000);

      // Show joystick data at first
      switchMode(0);
    }

    void switchMode(uint8_t mode)
    {
      if(mode == 1) {
        // Setup display to show keyboard/mouse data
        m_displayKeyboard.draw();
      } else {
      // Setup display to show joystick data
      m_displayJoystick.draw();
      }
    }

    void updateJoystick(const int32_t *a, const bool *b) {
      m_displayJoystick.update(a, b);
    }

    void updateKeyboard(const int32_t *a, const bool *b)  {
      m_displayKeyboard.update(a, b);
    }

    void updateInfo(const unsigned long &time_current, const unsigned long &time_last) {
      m_display.setDrawColor(0);
      m_display.drawBox(0, 120, 64, 8);
      m_display.setDrawColor(1);
      std::string timestr = std::to_string(time_current-time_last);
      m_display.drawStr(0, 128, "DT: "); m_display.drawStr(24, 128, timestr.c_str());
      Serial.print("DT: "); Serial.println(time_current-time_last);
      m_display.updateDisplayArea(0, 15, 8, 1);
    }
};