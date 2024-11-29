#include <stdarg.h>

#include "language.h"
#include "swkbd.h"
#include "timer.h"
#include "utf.h"


static inline char to_uppercase(char c) {
    if ((c >= 'a') && (c <= 'z')) c += ('A'-'a');
    return c;
}

bool BuildKeyboard(TouchBox* swkbd, const char* keys, const u8* layout, bool multi_line) {
    // count # of rows
    u32 n_rows = 0;
    for (u32 i = 0;; i++) {
        if (layout[i] == 0) {
            n_rows++;
            if (layout[i+1] == 0) break;
        }
    }

    // set p_y start position
    u32 height = (n_rows) ? (n_rows * SWKBD_STDKEY_HEIGHT) + ((n_rows-1) * SWKDB_KEY_SPACING) : 0;
    u32 p_y = SCREEN_HEIGHT - height - SWKBD_STDKEY_HEIGHT - SWKDB_KEY_SPACING;

    // set button positions
    TouchBox* tb = swkbd;
    if (!multi_line) {
        // set up the textbox
        TouchBox* txtbox = tb++;
        txtbox->x = (SCREEN_WIDTH_BOT - SWKBD_TEXTBOX_WIDTH) / 2;
        txtbox->y = p_y - 30;
        txtbox->w = SWKBD_TEXTBOX_WIDTH;
        txtbox->h = 30;
        txtbox->id = KEY_TXTBOX;
    }

    for (u32 l = 0, k = 0; layout[l] != 0; ) {
        // calculate width of current row
        u32 n_keys = layout[l++];
        u32 width = (n_keys * SWKBD_STDKEY_WIDTH) + ((n_keys-1) * SWKDB_KEY_SPACING);
        for (u32 i = 0; layout[l+i] != 0; i++)
            width = width - SWKBD_STDKEY_WIDTH + layout[l+i];

        // set p_x start position
        if (width > SCREEN_WIDTH_BOT) return false;
        u32 p_x = (SCREEN_WIDTH_BOT - width) / 2;

        // set up touchboxes
        for (u32 i = 0; i < n_keys; i++) {
            tb->id = keys[k++];
            tb->x = p_x;
            tb->y = p_y;
            tb->w = ((tb->id >= 0x80) || (tb->id == (u32) ' ')) ? layout[l++] : SWKBD_STDKEY_WIDTH;
            tb->h = SWKBD_STDKEY_HEIGHT;

            p_x += tb->w + SWKDB_KEY_SPACING;
            tb++;
        }

        // next row
        if (layout[l++] != 0) return false;
        p_y += SWKBD_STDKEY_HEIGHT + SWKDB_KEY_SPACING;
    }

    // set last touchbox zero (so the end can be detected)
    memset(tb, 0, sizeof(TouchBox));

    return true;
}

static void DrawKey(const TouchBox* key, const bool pressed, const u32 uppercase, const bool multi_line) {
    const char* keystrs[] = { SWKBD_KEYSTR };
    const char* ml_keystrs[] = { SWKBD_ML_KEYSTR };
    const u32 color = (pressed) ? COLOR_SWKBD_PRESSED :
        (key->id == KEY_ENTER) ? COLOR_SWKBD_ENTER :
        ((key->id == KEY_CAPS) && (uppercase > 1)) ? COLOR_SWKBD_CAPS :
        COLOR_SWKBD_NORMAL;

    // don't even try to draw the textbox
    if (key->id == KEY_TXTBOX) return;

    char keystr[16];
    if (key->id >= 0x80) snprintf(keystr, sizeof(keystr), "%s", (multi_line ? ml_keystrs : keystrs)[key->id - 0x80]);
    else {
        keystr[0] = (uppercase) ? to_uppercase(key->id) : key->id;
        keystr[1] = 0;
    }

    const u32 width = GetDrawStringWidth(keystr);
    const u32 f_offs_x = (key->w - width) / 2;
    const u32 f_offs_y = (key->h - FONT_HEIGHT_EXT) / 2;

    DrawRectangle(BOT_SCREEN, key->x, key->y, key->w, key->h, color);
    DrawString(BOT_SCREEN, keystr, key->x + f_offs_x, key->y + f_offs_y, COLOR_SWKBD_CHARS, color);
}

static void DrawKeyBoardBox(TouchBox* swkbd, u32 color) {
    // we need to make sure to skip the textbox here(first entry)

    // calculate / draw keyboard box
    u16 x0 = SCREEN_WIDTH_BOT, x1 = 0;
    u16 y0 = SCREEN_HEIGHT, y1 = 0;
    for (TouchBox* tb = swkbd + 1; tb->id != 0; tb++) {
        if (tb->x < x0) x0 = tb->x;
        if (tb->y < y0) y0 = tb->y;
        if ((tb->x + tb->w) > x1) x1 = tb->x + tb->w;
        if ((tb->y + tb->h) > y1) y1 = tb->y + tb->h;
    }
    DrawRectangle(BOT_SCREEN, 0, y0-1, SCREEN_WIDTH_BOT, y1-y0+2, COLOR_STD_BG);
    DrawRectangle(BOT_SCREEN, x0-1, y0-1, x1-x0+2, y1-y0+2, color);
}

static void DrawKeyBoard(TouchBox* swkbd, const u32 uppercase, const bool multi_line) {
    // we need to make sure to skip the textbox here(first entry)

    // draw keyboard
    for (TouchBox* tb = swkbd + (multi_line ? 0 : 1); tb->id != 0; tb++) {
        DrawKey(tb, false, uppercase, multi_line);
    }
}

static void DrawTextBox(const TouchBox* txtbox, const char* inputstr, const u32 cursor, u32* scroll) {
    const u32 input_shown_length = (txtbox->w / FONT_WIDTH_EXT) - 2;
    const u32 inputstr_size = strlen(inputstr); // we rely on a zero terminated string
    const u16 x = txtbox->x;
    const u16 y = txtbox->y;

    // fix scroll
    if (cursor < *scroll) {
        *scroll = cursor;
    } else {
        int scroll_adjust = -input_shown_length;
        for (u32 i = *scroll; i < cursor; i++) {
            if (i >= inputstr_size || (inputstr[i] & 0xC0) != 0x80) scroll_adjust++;
        }

        for (int i = 0; i < scroll_adjust; i++)
            *scroll += *scroll >= inputstr_size ? 1 : GetCharSize(inputstr + *scroll);
    }

    u32 input_shown_size = 0;
    for (u32 i = 0; i < input_shown_length || (*scroll + input_shown_size < inputstr_size && (inputstr[*scroll + input_shown_size] & 0xC0) == 0x80); input_shown_size++) {
        if (*scroll + input_shown_size >= inputstr_size || (inputstr[*scroll + input_shown_size] & 0xC0) != 0x80) i++;
    }

    // draw input string
    DrawStringF(BOT_SCREEN, x, y, COLOR_STD_FONT, COLOR_STD_BG, "%c%-*.*s%c",
        (*scroll) ? '<' : '|',
        (int) input_shown_size,
        (int) input_shown_size,
        (*scroll > inputstr_size) ? "" : inputstr + *scroll,
        (inputstr_size - (s32) *scroll > input_shown_size) ? '>' : '|'
    );

    // draw cursor
    u16 cpos = 0;
    for (u16 i = *scroll; i < cursor; i++) {
        if (i >= inputstr_size || (inputstr[i] & 0xC0) != 0x80) cpos++;
    }

    DrawStringF(BOT_SCREEN, x-(FONT_WIDTH_EXT/2), y+10, COLOR_STD_FONT, COLOR_STD_BG, "%-*.*s^%-*.*s",
        (int) (1 + cpos),
        (int) (1 + cpos),
        "",
        (int) (input_shown_length - cpos),
        (int) (input_shown_length - cpos),
        ""
    );
}

static void MoveTextBoxCursor(const TouchBox* txtbox, const char* inputstr, const u32 max_size, u32* cursor, u32* scroll) {
    const u32 input_shown = (txtbox->w / FONT_WIDTH_EXT) - 2;
    const u64 scroll_cooldown = 144;
    u64 timer = timer_start();
    u32 id = 0;
    u16 x, y;

    // process touch input
    while(HID_ReadTouchState(&x, &y)) {
        const TouchBox* tb = TouchBoxGet(&id, x, y, txtbox, 0);
        if (id == KEY_TXTBOX) {
            u16 x_tb = x - tb->x;

            const u32 inputstr_size = strlen(inputstr);
            const u16 cpos_x = (x_tb < (FONT_WIDTH_EXT/2)) ? 0 : (x_tb - (FONT_WIDTH_EXT/2)) / FONT_WIDTH_EXT;
            u16 cpos_length = 0;
            u16 cpos_size = 0;
            while ((cpos_length < cpos_x && cpos_length < input_shown) || (*scroll + cpos_size < inputstr_size && (inputstr[*scroll + cpos_size] & 0xC0) == 0x80)) {
                if (*scroll + cpos_size >= inputstr_size || (inputstr[*scroll + cpos_size] & 0xC0) != 0x80) cpos_length++;
                cpos_size++;
            }

            u32 cursor_next = *scroll + cpos_size;
            // move cursor to position pointed to
            if (*cursor != cursor_next) {
                if (cursor_next < max_size) *cursor = cursor_next;
                DrawTextBox(txtbox, inputstr, *cursor, scroll);
                timer = timer_start();
            }
            // move beyound visible field
            if (timer_msec(timer) >= scroll_cooldown) {
                if ((cpos_length == 0) && (*scroll > 0))
                    *scroll -= GetPrevCharSize(inputstr + *scroll);
                else if ((cpos_length >= input_shown) && (*cursor < (max_size-1)))
                    *scroll += GetCharSize(inputstr + *scroll);
            }
        }
    }
}

static char KeyboardWait(TouchBox* swkbd, bool uppercase, const bool multi_line) {
    u32 id = 0;
    u16 x, y;

    // wait for touch input (handle key input, too)
    while (true) {
        u32 pressed = InputWait(0);
        if (multi_line && pressed & TIMEOUT_HID) return 0;
        else if (pressed & BUTTON_B) return KEY_ESCAPE;
        else if (pressed & BUTTON_A) return KEY_ENTER;
        else if (pressed & BUTTON_X) return KEY_BKSPC;
        else if (pressed & BUTTON_Y) return multi_line ? KEY_CLIP : KEY_INSERT;
        else if (!multi_line && pressed & BUTTON_R1) return KEY_CAPS;
        else if (pressed & BUTTON_RIGHT) return KEY_RIGHT;
        else if (pressed & BUTTON_LEFT) return KEY_LEFT;
        else if (multi_line && pressed & BUTTON_UP) return KEY_UP;
        else if (multi_line && pressed & BUTTON_DOWN) return KEY_DOWN;
        else if (!multi_line && pressed & BUTTON_SELECT) return KEY_SWITCH;
        else if (pressed & BUTTON_TOUCH) break;
    }

    // process touch input
    while(HID_ReadTouchState(&x, &y)) {
        const TouchBox* tb = TouchBoxGet(&id, x, y, swkbd, 0);
        if (tb) {
            if (id == KEY_TXTBOX) break; // immediately break on textbox
            DrawKey(tb, true, uppercase, multi_line);
            while(HID_ReadTouchState(&x, &y) && (tb == TouchBoxGet(NULL, x, y, swkbd, 0)));
            DrawKey(tb, false, uppercase, multi_line);
        }
    }

    return (uppercase) ? to_uppercase((char) id) : (char) id;
}

bool ShowKeyboard(char* inputstr, const u32 max_size, const char *format, ...) {
    static const char keys_alphabet[] = { SWKBD_KEYS_ALPHABET };
    static const char keys_special[] = { SWKBD_KEYS_SPECIAL };
    static const char keys_numpad[] = { SWKBD_KEYS_NUMPAD };
    static const u8 layout_alphabet[] = { SWKBD_LAYOUT_ALPHABET };
    static const u8 layout_special[] = { SWKBD_LAYOUT_SPECIAL };
    static const u8 layout_numpad[] = { SWKBD_LAYOUT_NUMPAD };
    TouchBox swkbd_alphabet[64];
    TouchBox swkbd_special[32];
    TouchBox swkbd_numpad[32];
    TouchBox* textbox = swkbd_alphabet; // always use this textbox

    static bool show_instr = true;
    const char* instr = STR_KEYBOARD_CONTROLS_DETAILS;
    if (show_instr) {
        ShowPrompt(false, "%s", instr);
        show_instr = false;
    }

    // generate keyboards
    if (!BuildKeyboard(swkbd_alphabet, keys_alphabet, layout_alphabet, false)) return false;
    if (!BuildKeyboard(swkbd_special, keys_special, layout_special, false)) return false;
    if (!BuildKeyboard(swkbd_numpad, keys_numpad, layout_numpad, false)) return false;

    // (instructional) text
    char str[512]; // arbitrary limit, should be more than enough
    va_list va;
    va_start(va, format);
    vsnprintf(str, sizeof(str), format, va);
    va_end(va);
    u32 str_width = GetDrawStringWidth(str);
    if (str_width < (24 * FONT_WIDTH_EXT)) str_width = 24 * FONT_WIDTH_EXT;
    u32 str_x = (str_width >= SCREEN_WIDTH_BOT) ? 0 : (SCREEN_WIDTH_BOT - str_width) / 2;
    ClearScreen(BOT_SCREEN, COLOR_STD_BG);
    DrawStringF(BOT_SCREEN, str_x, 20, COLOR_STD_FONT, COLOR_STD_BG, "%s", str);

    // wait for all keys released
    while (HID_ReadState() & BUTTON_ANY);

    // handle keyboard
    u32 uppercase = 0; // 1 -> uppercase once, 2 -> uppercase always
    u32 scroll = 0;
    u32 cursor = 0;
    u32 inputstr_size = strnlen(inputstr, max_size);
    TouchBox* swkbd_prev = NULL;
    TouchBox* swkbd = swkbd_alphabet;
    bool ret = false;
    while (true) {
        // draw keyboard if required
        if (swkbd != swkbd_prev) {
            DrawKeyBoardBox(swkbd, COLOR_SWKBD_BOX);
            DrawKeyBoard(swkbd, uppercase, false);
            DrawTextBox(textbox, inputstr, cursor, &scroll);
            swkbd_prev = swkbd;
        }

        // handle user input
        char key = KeyboardWait(swkbd, uppercase, false);
        if (key == KEY_INSERT) key = ' '; // impromptu replacement
        if (key == KEY_TXTBOX) {
            MoveTextBoxCursor(textbox, inputstr, max_size, &cursor, &scroll);
        } else if (key == KEY_CAPS) {
            uppercase = (uppercase + 1) % 3;
            DrawKeyBoard(swkbd, uppercase, false);
            continue;
        } else if (key == KEY_ENTER) {
            ret = true;
            break;
        } else if (key == KEY_ESCAPE) {
            break;
        } else if (key == KEY_BKSPC) {
            if (cursor) {
                int size = GetPrevCharSize(inputstr + cursor);
                if (cursor <= inputstr_size) {
                    memmove(inputstr + cursor - size, inputstr + cursor, inputstr_size - cursor + size);
                    inputstr_size -= size;
                }
                cursor -= size;
            }
        } else if (key == KEY_LEFT) {
            if (cursor) cursor -= GetPrevCharSize(inputstr + cursor);
        } else if (key == KEY_RIGHT) {
            int size = cursor > inputstr_size ? 1 : GetCharSize(inputstr + cursor);
            if (cursor + size < max_size) cursor += size;
        } else if (key == KEY_ALPHA) {
            swkbd = swkbd_alphabet;
        } else if (key == KEY_SPECIAL) {
            swkbd = swkbd_special;
        } else if (key == KEY_NUMPAD) {
            swkbd = swkbd_numpad;
        } else if (key == KEY_SWITCH) {
            ClearScreen(BOT_SCREEN, COLOR_STD_BG);
            return ShowStringPrompt(inputstr, max_size, "%s", str);
        } else if (key == KEY_UNICODE) {
            if (cursor > 3 && cursor <= inputstr_size) {
                u16 codepoint = 0;
                for (char *c = inputstr + cursor - 4; c < inputstr + cursor; c++) {
                    if ((*c >= '0' && *c <= '9') || (*c >= 'A' && *c <= 'F') || (*c >= 'a' && *c <= 'f')) {
                        codepoint <<= 4;
                        codepoint |= *c - (*c <= '9' ? '0' : ((*c <= 'F' ? 'A' : 'a') - 10));
                    } else {
                        codepoint = 0;
                        break;
                    }
                }

                if(codepoint != 0) {
                    char character[5] = {0};
                    u16 input[2] = {codepoint, 0};
                    utf16_to_utf8((u8*)character, input, 4, 1);

                    u32 char_size = GetCharSize(character);
                    memmove(inputstr + cursor - 4 + char_size, inputstr + cursor, max_size - cursor + 4 - char_size);
                    memcpy(inputstr + cursor - 4, character, char_size);
                    cursor -= 4 - char_size;
                }
            }
        } else if (key && (key < 0x80)) {
            if ((cursor < (max_size-1)) && (inputstr_size < max_size)) {
                // pad string (if cursor beyound string size)
                while (inputstr_size < cursor) {
                    inputstr[inputstr_size++] = ' ';
                    inputstr[inputstr_size] = '\0';
                }
                // make room
                if (inputstr_size < (max_size-1)) { // only if there is still room
                    memmove(inputstr + cursor + 1, inputstr + cursor, max_size - cursor - 1);
                    inputstr_size++;
                }
                // insert char
                inputstr[cursor++] = key;
            }
            if (uppercase == 1) {
                uppercase = 0;
                DrawKeyBoard(swkbd, uppercase, false);
            }
        }

        // update text
        DrawTextBox(textbox, inputstr, cursor, &scroll);
    }

    ClearScreen(BOT_SCREEN, COLOR_STD_BG);
    return ret;
}

char ShowMultiLineKeyboard(const char* instructions, TouchBox* swkbd_alphabet, TouchBox* swkbd_special, TouchBox* swkbd_numpad, TouchBox** swkbd, TouchBox** swkbd_prev, u32* uppercase) {
    if (instructions) {
        u32 str_width = GetDrawStringWidth(instructions);
        if (str_width < (24 * FONT_WIDTH_EXT)) str_width = 24 * FONT_WIDTH_EXT;
        u32 str_x = (str_width >= SCREEN_WIDTH_BOT) ? 0 : (SCREEN_WIDTH_BOT - str_width) / 2;
        ClearScreen(BOT_SCREEN, COLOR_STD_BG);
        DrawStringF(BOT_SCREEN, str_x, 20, COLOR_STD_FONT, COLOR_STD_BG, "%s", instructions);
        *swkbd_prev = NULL; // Force keyboard redraw
    }

    // handle keyboard
    while (true) {
        // draw keyboard if required
        if (*swkbd != *swkbd_prev) {
            DrawKeyBoardBox(*swkbd, COLOR_SWKBD_BOX);
            DrawKeyBoard(*swkbd, *uppercase, true);
            *swkbd_prev = *swkbd;
        }

        // handle user input
        char key = KeyboardWait(*swkbd, *uppercase, true);
        if (key == KEY_ALPHA) {
            *swkbd = swkbd_alphabet;
        } else if (key == KEY_SPECIAL) {
            *swkbd = swkbd_special;
        } else if (key == KEY_NUMPAD) {
            *swkbd = swkbd_numpad;
        } else if (key == KEY_CAPS) {
            *uppercase = (*uppercase + 1) % 3;
            DrawKeyBoard(*swkbd, *uppercase, true);
        } else return key;
    }
}