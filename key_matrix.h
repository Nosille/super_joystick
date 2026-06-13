
// Keyboard character list
// uint8_t const k_keyRows = 6;
// uint8_t const k_keyCols = 10;
// char const k_keyMatrix_L1[k_keyRows][k_keyCols] = {
//   // bksp  lf   cr  esc  tab  home end ^home ^end del
//     {0x08,0x0A,0x0D,0x1B,0x09,0x02,0x03,0x01,0x04,0x7F},
//     { '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'}, 
//     { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'},
//     { 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't'},
//     { 'u', 'v', 'w', 'x', 'y', 'z', '-', '=', '[', ']'},
//     {0x0B,0X0C,'\`','\'', ';', ',', '.', '/','\\', ' '}
//   // Undo Redo
// };
// char const k_keyMatrix_L2[k_keyRows][k_keyCols] = {
//   //  F1   F2   F3   F4   F5   F6   F7   F8   F9   F10
//     {0x11,0X12,0X13,0X14,0X15,0X16,0X17,0X18,0X19,0X1A},
//     { '!', '@', '#', '$', '%', '^', '&', '*', '(', ')'},
//     { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'},
//     { 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T'},
//     { 'U', 'V', 'W', 'X', 'Y', 'Z', '_', '+','\{','\}'},
//     {0x1C,0x1D, '~','\"', ':', '<', '>', '?', '|', ' '}
//   // F11  F12   
// };
uint8_t const k_keyRows = 6;
uint8_t const k_keyCols = 8;
char const k_keyMatrix_L1[k_keyRows][k_keyCols] = {
  // bksp  lf   cr  esc  tab  ^home ^end del  
    {0x08,0x0A,0x0D,0x1B,0x09,0x01,0x04,0x7F},
    { '1', '2', '3', '4', '5', '6', '7', '8'},
    { '9', '0', 'a', 'b', 'c', 'd', 'e', 'f'},
    { 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n'},
    { 'o', 'p', 'q', 'r', 's', 't', 'u', 'v'},
    { 'w', 'x', 'y', 'z','\\', '.', ',', ' '}
};
char const k_keyMatrix_L2[k_keyRows][k_keyCols] = {
  //  F1   F2   F3   F4   F5   F6   F7   F8   
    {0x11,0X12,0X13,0X14,0X15,0X16,0X17,0X18},
    { '!', '@', '#', '$', '%', '^', '&', '*'},
    { '(', ')', 'A', 'B', 'C', 'D', 'E', 'F'},
    { 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N'},
    { 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V'},
    { 'W', 'X', 'Y', 'Z', '|', '<', '>', '_'}
};
char const k_keyMatrix_L3[k_keyRows][k_keyCols] = {
  //  F9  F10  F11  F12  pg_u
    {0x19,0X1A,0X1C,0X1D,0x05, ' ', ' ', ' '},
    { ' ', ' ', ' ', ' ', ' ', '~', '?', '='},
    { '[', ']','\{','\}', ':', ';', '+', '-'},
    { '/','\\', ' ', ' ', ' ','\`','\'','\"'},
    { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
    {0x0B,0x01,0x04,0x07,0x06,0x0E,0x0F,0x0C}
  // Undo Top Bottom ps  pg_d pausebreak Redo    
};


/*--------------------------------------------------------------------
 * ASCII to KEYCODE Conversion
 *  Expand to array of [128][2] (shift, keycode)
 *
 * Usage: example to convert input chr into keyboard report (modifier + keycode)
 *
 *  uint8_t const conv_table[128][2] =  { HID_ASCII_TO_KEYCODE };
 *
 *  uint8_t keycode[6] = { 0 };
 *  uint8_t modifier   = 0;
 *
 *  modifier   = conv_table[chr][0];
 *  keycode[0] = conv_table[chr][1];
 *  tud_hid_keyboard_report(report_id, modifier, keycode);
 *
 *--------------------------------------------------------------------*/
uint8_t const k_ascii2hid[128][2] = {
    {0, 0                     }, /* 0x00 Null      */ \
    {1, HID_KEY_HOME          }, /* 0x01           */ \
    {0, HID_KEY_HOME          }, /* 0x02           */ \
    {0, HID_KEY_END           }, /* 0x03           */ \
    {1, HID_KEY_END           }, /* 0x04           */ \
    {0, HID_KEY_PAGE_UP       }, /* 0x05           */ \
    {0, HID_KEY_PAGE_DOWN     }, /* 0x06           */ \
    {0, HID_KEY_PRINT_SCREEN  }, /* 0x07           */ \
    {0, HID_KEY_BACKSPACE     }, /* 0x08 Backspace */ \
    {0, HID_KEY_TAB           }, /* 0x09 Tab       */ \
    {2, HID_KEY_ENTER         }, /* 0x0A Line Feed */ \
    {1, HID_KEY_Z             }, /* 0x0B           */ \
    {1, HID_KEY_Y             }, /* 0x0C           */ \
    {0, HID_KEY_ENTER         }, /* 0x0D CR        */ \
    {0, HID_KEY_PAUSE         }, /* 0x0E           */ \
    {1, HID_KEY_PAUSE         }, /* 0x0F           */ \
    {0, 0                     }, /* 0x10           */ \
    {0, HID_KEY_F1            }, /* 0x11           */ \
    {0, HID_KEY_F2            }, /* 0x12           */ \
    {0, HID_KEY_F3            }, /* 0x13           */ \
    {0, HID_KEY_F4            }, /* 0x14           */ \
    {0, HID_KEY_F5            }, /* 0x15           */ \
    {0, HID_KEY_F6            }, /* 0x16           */ \
    {0, HID_KEY_F7            }, /* 0x17           */ \
    {0, HID_KEY_F8            }, /* 0x18           */ \
    {0, HID_KEY_F9            }, /* 0x19           */ \
    {0, HID_KEY_F10           }, /* 0x1A           */ \
    {0, HID_KEY_ESCAPE        }, /* 0x1B Escape    */ \
    {0, HID_KEY_F11           }, /* 0x1C           */ \
    {0, HID_KEY_F12           }, /* 0x1D           */ \
    {0, 0                     }, /* 0x1E           */ \
    {0, 0                     }, /* 0x1F           */ \
                                                      \
    {0, HID_KEY_SPACE         }, /* 0x20           */ \
    {2, HID_KEY_1             }, /* 0x21 !         */ \
    {2, HID_KEY_APOSTROPHE    }, /* 0x22 "         */ \
    {2, HID_KEY_3             }, /* 0x23 #         */ \
    {2, HID_KEY_4             }, /* 0x24 $         */ \
    {2, HID_KEY_5             }, /* 0x25 %         */ \
    {2, HID_KEY_7             }, /* 0x26 &         */ \
    {0, HID_KEY_APOSTROPHE    }, /* 0x27 '         */ \
    {2, HID_KEY_9             }, /* 0x28 (         */ \
    {2, HID_KEY_0             }, /* 0x29 )         */ \
    {2, HID_KEY_8             }, /* 0x2A *         */ \
    {2, HID_KEY_EQUAL         }, /* 0x2B +         */ \
    {0, HID_KEY_COMMA         }, /* 0x2C ,         */ \
    {0, HID_KEY_MINUS         }, /* 0x2D -         */ \
    {0, HID_KEY_PERIOD        }, /* 0x2E .         */ \
    {0, HID_KEY_SLASH         }, /* 0x2F /         */ \
    {0, HID_KEY_0             }, /* 0x30 0         */ \
    {0, HID_KEY_1             }, /* 0x31 1         */ \
    {0, HID_KEY_2             }, /* 0x32 2         */ \
    {0, HID_KEY_3             }, /* 0x33 3         */ \
    {0, HID_KEY_4             }, /* 0x34 4         */ \
    {0, HID_KEY_5             }, /* 0x35 5         */ \
    {0, HID_KEY_6             }, /* 0x36 6         */ \
    {0, HID_KEY_7             }, /* 0x37 7         */ \
    {0, HID_KEY_8             }, /* 0x38 8         */ \
    {0, HID_KEY_9             }, /* 0x39 9         */ \
    {2, HID_KEY_SEMICOLON     }, /* 0x3A :         */ \
    {0, HID_KEY_SEMICOLON     }, /* 0x3B ;         */ \
    {2, HID_KEY_COMMA         }, /* 0x3C <         */ \
    {0, HID_KEY_EQUAL         }, /* 0x3D =         */ \
    {2, HID_KEY_PERIOD        }, /* 0x3E >         */ \
    {2, HID_KEY_SLASH         }, /* 0x3F ?         */ \
                                                      \
    {2, HID_KEY_2             }, /* 0x40 @         */ \
    {2, HID_KEY_A             }, /* 0x41 A         */ \
    {2, HID_KEY_B             }, /* 0x42 B         */ \
    {2, HID_KEY_C             }, /* 0x43 C         */ \
    {2, HID_KEY_D             }, /* 0x44 D         */ \
    {2, HID_KEY_E             }, /* 0x45 E         */ \
    {2, HID_KEY_F             }, /* 0x46 F         */ \
    {2, HID_KEY_G             }, /* 0x47 G         */ \
    {2, HID_KEY_H             }, /* 0x48 H         */ \
    {2, HID_KEY_I             }, /* 0x49 I         */ \
    {2, HID_KEY_J             }, /* 0x4A J         */ \
    {2, HID_KEY_K             }, /* 0x4B K         */ \
    {2, HID_KEY_L             }, /* 0x4C L         */ \
    {2, HID_KEY_M             }, /* 0x4D M         */ \
    {2, HID_KEY_N             }, /* 0x4E N         */ \
    {2, HID_KEY_O             }, /* 0x4F O         */ \
    {2, HID_KEY_P             }, /* 0x50 P         */ \
    {2, HID_KEY_Q             }, /* 0x51 Q         */ \
    {2, HID_KEY_R             }, /* 0x52 R         */ \
    {2, HID_KEY_S             }, /* 0x53 S         */ \
    {2, HID_KEY_T             }, /* 0x55 T         */ \
    {2, HID_KEY_U             }, /* 0x55 U         */ \
    {2, HID_KEY_V             }, /* 0x56 V         */ \
    {2, HID_KEY_W             }, /* 0x57 W         */ \
    {2, HID_KEY_X             }, /* 0x58 X         */ \
    {2, HID_KEY_Y             }, /* 0x59 Y         */ \
    {2, HID_KEY_Z             }, /* 0x5A Z         */ \
    {0, HID_KEY_BRACKET_LEFT  }, /* 0x5B [         */ \
    {0, HID_KEY_BACKSLASH     }, /* 0x5C '\'       */ \
    {0, HID_KEY_BRACKET_RIGHT }, /* 0x5D ]         */ \
    {2, HID_KEY_6             }, /* 0x5E ^         */ \
    {2, HID_KEY_MINUS         }, /* 0x5F _         */ \
                                                      \
    {0, HID_KEY_GRAVE         }, /* 0x60 `         */ \
    {0, HID_KEY_A             }, /* 0x61 a         */ \
    {0, HID_KEY_B             }, /* 0x62 b         */ \
    {0, HID_KEY_C             }, /* 0x63 c         */ \
    {0, HID_KEY_D             }, /* 0x66 d         */ \
    {0, HID_KEY_E             }, /* 0x65 e         */ \
    {0, HID_KEY_F             }, /* 0x66 f         */ \
    {0, HID_KEY_G             }, /* 0x67 g         */ \
    {0, HID_KEY_H             }, /* 0x68 h         */ \
    {0, HID_KEY_I             }, /* 0x69 i         */ \
    {0, HID_KEY_J             }, /* 0x6A j         */ \
    {0, HID_KEY_K             }, /* 0x6B k         */ \
    {0, HID_KEY_L             }, /* 0x6C l         */ \
    {0, HID_KEY_M             }, /* 0x6D m         */ \
    {0, HID_KEY_N             }, /* 0x6E n         */ \
    {0, HID_KEY_O             }, /* 0x6F o         */ \
    {0, HID_KEY_P             }, /* 0x70 p         */ \
    {0, HID_KEY_Q             }, /* 0x71 q         */ \
    {0, HID_KEY_R             }, /* 0x72 r         */ \
    {0, HID_KEY_S             }, /* 0x73 s         */ \
    {0, HID_KEY_T             }, /* 0x75 t         */ \
    {0, HID_KEY_U             }, /* 0x75 u         */ \
    {0, HID_KEY_V             }, /* 0x76 v         */ \
    {0, HID_KEY_W             }, /* 0x77 w         */ \
    {0, HID_KEY_X             }, /* 0x78 x         */ \
    {0, HID_KEY_Y             }, /* 0x79 y         */ \
    {0, HID_KEY_Z             }, /* 0x7A z         */ \
    {2, HID_KEY_BRACKET_LEFT  }, /* 0x7B {         */ \
    {2, HID_KEY_BACKSLASH     }, /* 0x7C |         */ \
    {2, HID_KEY_BRACKET_RIGHT }, /* 0x7D }         */ \
    {2, HID_KEY_GRAVE         }, /* 0x7E ~         */ \
    {0, HID_KEY_DELETE        }  /* 0x7F Delete    */ \           
};
