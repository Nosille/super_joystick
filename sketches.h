#include "HardwareSerial.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

class DisplayJoy {
  private:
    Adafruit_SH1107* m_display;
    int32_t m_x, m_y, m_w, m_h, m_r;
    int32_t last_x, last_y;

  public:
    DisplayJoy(Adafruit_SH1107 *display, const int16_t x, const int16_t y, const int16_t w, const int16_t h, const int16_t r = 2) {
      m_display = display;
      m_x = x;
      m_y = y;
      m_w = w;
      m_h = h;
      m_r = r;
    }
    void draw() {
      m_display->fillRect(m_x, m_y, m_w, m_h, SH110X_BLACK); 
      m_display->drawRect(m_x, m_y, m_w, m_h, SH110X_WHITE);
      last_x = 0;
      last_y = 0;
      int32_t point_x = m_x + m_w / 2;
      int32_t point_y = m_y + m_h / 2;
      m_display->drawCircle(point_x, point_y, 2, SH110X_WHITE);
    }

    void update(const int32_t &value_x, const int32_t &value_y, const bool &value_b) {
      // remove last circle
      m_display->fillCircle(last_x, last_y, m_r, SH110X_BLACK);
      // draw new circle
      int32_t scaled_x = value_x * (m_w - 2*m_r - 2) / 2 / 32768;
      int32_t scaled_y = value_y * (m_h - 2*m_r - 2) / 2 / 32768;
      int32_t point_x = m_x + m_w / 2 + scaled_x;
      int32_t point_y = m_y + m_h / 2 + scaled_y;
      if(value_b) {
        m_display->fillCircle(point_x, point_y, 2, SH110X_WHITE);
      } else {
        m_display->drawCircle(point_x, point_y, 2, SH110X_WHITE);
      }
      last_x = point_x;
      last_y = point_y;
    }
};

class DisplayAxis {
  private:
    Adafruit_SH1107* m_display;
    int32_t m_x, m_y, m_w, m_h;
    bool m_is_vert;

  public:
    DisplayAxis(Adafruit_SH1107 *display, const int16_t x, const int16_t y, const int16_t w, const int16_t h, bool is_vert = false) {
      m_display = display;
      m_x = x;
      m_y = y;
      m_w = w;
      m_h = h;
      m_is_vert = is_vert;
    }

    void draw() {
      m_display->fillRect(m_x, m_y, m_w, m_h, SH110X_BLACK);
      if(m_is_vert) {
        m_display->fillRect(m_x, m_y + m_h / 2, m_w, 2, SH110X_WHITE);
      } else {
        m_display->fillRect(m_x + m_w / 2, m_y, 2, m_h, SH110X_WHITE);
      } 
    }
    
    void update(const int32_t &value) {
      m_display->fillRect(m_x, m_y, m_w, m_h, SH110X_BLACK);
      if(m_is_vert) {
        int32_t scaled = value * m_h / 2 / 32768;
        int32_t top    = std::min(m_y + m_h / 2 - 1, m_y + m_h / 2 + scaled);
        int32_t bottom = std::max(m_y + m_h / 2 + 1, m_y + m_h / 2 + scaled); 
        m_display->fillRect(m_x, top, m_w, bottom-top, SH110X_WHITE);
      } else {
        int32_t scaled = value * m_w / 2 / 32768;
        int32_t left  = std::min(m_x + m_w / 2 - 1, m_x + m_w / 2 + scaled);
        int32_t right = std::max(m_x + m_w / 2 + 1, m_x + m_w / 2 + scaled); 
        m_display->fillRect(left, m_y, right-left, m_h, SH110X_WHITE);
      }
    }
};

class DisplayButton {
  private:
    Adafruit_SH1107* m_display;  
    int32_t m_x, m_y, m_r;
    bool last_value;

  public:
    DisplayButton(Adafruit_SH1107 *display, const int16_t x, const int16_t y, const int16_t r) {
      m_display = display;
      m_x = x + r / 2;
      m_y = y + r / 2;
      m_r = r;
      last_value = 0;
    }
    
    void draw() {
      m_display->fillRect(m_x, m_y, m_r, m_r, SH110X_BLACK);
      m_display->drawCircle(m_x, m_y, m_r, SH110X_WHITE);
    }

    void update(const int32_t &value) {
      if(value) {
        if(!last_value) {
          m_display->fillCircle(m_x, m_y, m_r, SH110X_WHITE);
        }
      } else {
        if (last_value) {
          m_display->fillCircle(m_x, m_y, m_r, SH110X_BLACK);
          m_display->drawCircle(m_x, m_y, m_r, SH110X_WHITE);
        }
        
      }
      last_value = value;
    }
};

class DisplayKeyMatrix {
  private:
    Adafruit_SH1107* m_display;
    static uint8_t const k_keyRows = 6;
    static uint8_t const k_keyCols = 10;
    int16_t m_x, m_y;

  public:
    DisplayKeyMatrix(Adafruit_SH1107 *display, const int16_t x, const int16_t y)
    {
      m_x = x;
      m_y = y;
    }

    void draw() {

    }
    
    void update(const char (*matrix)[k_keyCols], const uint8_t &key_i, const uint8_t &key_j) {
      for(uint8_t i = 0; i < k_keyRows; i++){
        m_display->setCursor( m_x, m_y + 8*i); 
        for(uint8_t j = 0; j < k_keyCols; j++) {
          // Highlight the appropriate key
          if ((i == key_i) && (j ==  key_j)) m_display->setTextColor(SH110X_BLACK, SH110X_WHITE); 
            if (     matrix[i][j] == 0x08) m_display->write(0x11); // backspace
            else if (matrix[i][j] == 0x09) m_display->write(0x09); // tab
            else if (matrix[i][j] == 0x0A) m_display->write(0xD9); // line feed
            else if (matrix[i][j] == 0x0D) m_display->write(0x14); // carriage return
            else if (matrix[i][j] == 0x1B) m_display->write(0x13); // escape
            else if (matrix[i][j] == 0x01) m_display->write(0x18); // start of file
            else if (matrix[i][j] == 0x02) m_display->write(0x1B); // home
            else if (matrix[i][j] == 0x03) m_display->write(0x1A); // end
            else if (matrix[i][j] == 0x04) m_display->write(0x19); // end of file
            else if (matrix[i][j] == 0x7F) m_display->write(0x10); // delete
            else if (matrix[i][j] == 0x11) m_display->write('1'); // F1
            else if (matrix[i][j] == 0x12) m_display->write('2'); // F2
            else if (matrix[i][j] == 0x13) m_display->write('3'); // F3
            else if (matrix[i][j] == 0x14) m_display->write('4'); // F4
            else if (matrix[i][j] == 0x15) m_display->write('5'); // F5
            else if (matrix[i][j] == 0x16) m_display->write('6'); // F6
            else if (matrix[i][j] == 0x17) m_display->write('7'); // F7
            else if (matrix[i][j] == 0x18) m_display->write('8'); // F8
            else if (matrix[i][j] == 0x19) m_display->write('9'); // F9
            else if (matrix[i][j] == 0x1A) m_display->write('0'); // F10
            else if (matrix[i][j] == 0x1C) m_display->write('1'); // F11
            else if (matrix[i][j] == 0x1D) m_display->write('2'); // F12
            else if (matrix[i][j] == 0x0B) m_display->write(0xAE); // undo
            else if (matrix[i][j] == 0x0C) m_display->write(0xAF); // redo

            else m_display->write(matrix[i][j]);
          if ((i == key_i) && (j ==  key_j)) m_display->setTextColor(SH110X_WHITE, SH110X_BLACK);          
        }
      }
    }
};

class DisplayJoystick {
  private:
    Adafruit_SH1107* m_display;
    DisplayJoy m_joy1, m_joy2, m_joy3, m_joy4;
    DisplayAxis m_axis1;
    DisplayButton m_button1, m_button2, m_button3, m_button4;
    DisplayButton m_button5, m_button6, m_button7, m_button8;
  
  public:
    DisplayJoystick(Adafruit_SH1107 *display)
      : m_joy1(display,  1, 17, 30, 30)
      , m_joy2(display, 33, 17, 30, 30)
      , m_joy3(display,  1, 73, 30, 30)
      , m_joy4(display, 33, 73, 30, 30)
      , m_axis1(display, 0, 105, 64,  6)
      , m_button1(display, 4, 57, 4)
      , m_button2(display, 12, 50, 4)
      , m_button3(display, 12, 64, 4)
      , m_button4(display, 20, 57, 4)
      , m_button5(display, 36, 57, 4)
      , m_button6(display, 44, 50, 4)
      , m_button7(display, 44, 64, 4)
      , m_button8(display, 52, 57, 4) 
    {
      m_display = display;
    }

    void draw() {
      m_display->fillRect(0, 0, 64, 128, SH110X_BLACK);
      m_display->setTextColor(SH110X_WHITE, SH110X_BLACK);
      m_display->setCursor(0,0); m_display->print("Joystick:");
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
      m_display->display();
    }

    void update(const int32_t *a, const bool *b) {
      // Serial.print("a[0] = "); Serial.println(a[0]);
      m_joy1.update(a[0], -a[1], b[8]); 
      m_joy2.update(a[2], -a[3], b[9]); 
      m_joy3.update(a[4], -a[5], b[10]); 
      m_joy4.update(a[6], -a[7], b[11]);
      m_axis1.update(a[8]);
      m_button1.update(b[0]);
      m_button2.update(b[1]);
      m_button3.update(b[3]);
      m_button4.update(b[2]);
      m_button5.update(b[4]);
      m_button6.update(b[5]);
      m_button7.update(b[7]);
      m_button8.update(b[6]);
      // m_display->display();
    }
};

class DisplayKeyboard {
  private:
    Adafruit_SH1107* m_display;
    DisplayJoy m_joy1;
    DisplayAxis m_axis1, m_axis2;
    DisplayButton m_button1, m_button2, m_button3;
    DisplayKeyMatrix m_matrix;

  public:
    DisplayKeyboard(Adafruit_SH1107 *display)
      : m_joy1(display, 25,  17, 30, 30)
      , m_axis1(display, 24, 48, 32,  6, false)
      , m_axis2(display, 56, 16,  6, 32, true)
      , m_button1(display, 28, 10, 3)
      , m_button2(display, 38, 10, 3)
      , m_button3(display, 48, 10, 3)
      , m_matrix(display,2, 72)
    {
      m_display = display;
    }

    void draw() {
      m_display->setTextColor(SH110X_WHITE, SH110X_BLACK);
      m_display->fillRect(0, 0, 64, 128, SH110X_BLACK);
      m_display->setCursor(0,0); m_display->print("Keyboard:");
      m_joy1.draw();
      m_axis1.draw();
      m_axis2.draw();
      m_button1.draw();
      m_button2.draw();
      m_button3.draw();
      m_display->setCursor(0, 8); m_display->print("Hom");
      m_display->setCursor(0,16); m_display->print("Esc");
      m_display->setCursor(0,24); m_display->print("End");
      m_display->setCursor(0,32); m_display->print("Ent");            
      m_display->setCursor(0,40); m_display->print("Sft");
      m_display->setCursor(0,48); m_display->print("Ctr");
      m_display->setCursor(0,56); m_display->print("Alt");
      m_matrix.draw();
      m_display->display();
    }

    void update(const int16_t &value_x, const int16_t &value_y, const int16_t &value_h, const int16_t &value_v,
               const bool &bleft, const bool &bcenter, const bool &bright, const bool &bhome, const bool &besc, 
               const bool &bend, const bool &benter, const bool &bshift, const bool &bcontrol, const bool &balt,
               const char (*matrix)[10], const uint8_t &key_i, const uint8_t &key_j) 
    {
      m_joy1.update(value_x, -value_y, 0);
      m_axis1.update(value_h);
      m_axis2.update(value_v);
      m_button1.update(bleft);
      m_button2.update(bcenter);
      m_button3.update(bright);
      m_display->setCursor(0, 8); m_display->setTextColor(bhome    ? SH110X_BLACK : SH110X_WHITE, bhome    ? SH110X_WHITE : SH110X_BLACK); m_display->print("Hom");
      m_display->setCursor(0,16); m_display->setTextColor(besc     ? SH110X_BLACK : SH110X_WHITE, besc     ? SH110X_WHITE : SH110X_BLACK); m_display->print("Esc");
      m_display->setCursor(0,24); m_display->setTextColor(bend     ? SH110X_BLACK : SH110X_WHITE, bend     ? SH110X_WHITE : SH110X_BLACK); m_display->print("End");
      m_display->setCursor(0,32); m_display->setTextColor(benter   ? SH110X_BLACK : SH110X_WHITE, benter   ? SH110X_WHITE : SH110X_BLACK); m_display->print("Ent");            
      m_display->setCursor(0,40); m_display->setTextColor(bshift   ? SH110X_BLACK : SH110X_WHITE, bshift   ? SH110X_WHITE : SH110X_BLACK); m_display->print("Sft");
      m_display->setCursor(0,48); m_display->setTextColor(bcontrol ? SH110X_BLACK : SH110X_WHITE, bcontrol ? SH110X_WHITE : SH110X_BLACK); m_display->print("Ctr");
      m_display->setCursor(0,56); m_display->setTextColor(balt     ? SH110X_BLACK : SH110X_WHITE, balt     ? SH110X_WHITE : SH110X_BLACK); m_display->print("Alt");
      m_display->setTextColor(SH110X_WHITE, SH110X_BLACK);
      m_matrix.update(matrix, key_i, key_j);
      // m_display->display();
    }
};