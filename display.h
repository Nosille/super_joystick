#include "HardwareSerial.h"
#include <string>
#include <Wire.h>
#include <algorithm>
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
      delay(0); m_display->updateDisplayArea(m_xt, m_yt, m_wt, m_ht);      
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
      delay(0); m_display->updateDisplayArea(min_x, min_y, max_x - min_x + 1, max_y - min_y + 1);
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
      delay(0); m_display->updateDisplayArea(m_xt, m_yt, m_wt, m_ht);      
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
      delay(0); m_display->updateDisplayArea(m_xt, m_yt, m_wt, m_ht);
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
      delay(0); m_display->updateDisplayArea(m_xt, m_yt, m_wt, m_ht);      
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
      delay(0); m_display->updateDisplayArea(m_xt, m_yt, m_wt, m_ht);   
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
      updateMatrix(0, k_keyRows / 2, k_keyCols / 2);
    }

    void updateMatrix(const uint8_t &n, const uint8_t &key_i, const uint8_t &key_j) {
      // Serial.println("updateMatrix");
      // load matrix
      m_matrix = k_keyMatrix[n];

      // draw matrix
      m_display->setDrawColor(0);
      m_display->drawBox(m_x, m_y, m_w, m_h);
      m_display->setDrawColor(1);             
      for(uint8_t i = 0; i < k_keyRows; i++){
        for(uint8_t j = 0; j < k_keyCols; j++) {
          drawKey(i, j);
        }
      }   
      // highlight key
      m_display->setDrawColor(1);
      m_display->drawBox(m_x + 8 * key_j, m_y + 8 * key_i, 8, 8);      
      m_display->setDrawColor(0);
      drawKey(key_i, key_j);   
      delay(0); m_display->updateDisplayArea(m_xt, m_yt, m_wt, m_ht);
      last_n = n;
    }
    
    void update(const uint8_t &n, const uint8_t &key_i, const uint8_t &key_j) {
      // if index changed updateMatrix
      if (n != last_n) {
        updateMatrix(n, key_i, key_j);
      }
      // if same key as last pass exit
      if(last_i == key_i && last_j == key_j) return;
      // Reset last key
      m_display->setDrawColor(0);
      m_display->drawBox(m_x + 8 * last_j, m_y + 8 * last_i, 8, 8);
      m_display->setDrawColor(1);
      drawKey(last_i, last_j);
      delay(0); m_display->updateDisplayArea(m_xt + last_j, m_yt + last_i, 1, 1);
      // Highlight new key
      m_display->setDrawColor(1);
      m_display->drawBox(m_x + 8 * key_j, m_y + 8 * key_i, 8, 8);      
      m_display->setDrawColor(0);
      drawKey(key_i, key_j);
      delay(0); m_display->updateDisplayArea(m_xt + key_j, m_yt + key_i, 1, 1);
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
