#include "crescent/input.h"

// clang-format off

static const char* mod_none_table[SCAN_MAX] = {
	[SCAN_A]                        = "a",
	[SCAN_B]                        = "b",
	[SCAN_C]                        = "c",
	[SCAN_D]                        = "d",
	[SCAN_E]                        = "e",
	[SCAN_F]                        = "f",
	[SCAN_G]                        = "g",
	[SCAN_H]                        = "h",
	[SCAN_I]                        = "i",
	[SCAN_J]                        = "j",
	[SCAN_K]                        = "k",
	[SCAN_L]                        = "l",
	[SCAN_M]                        = "m",
	[SCAN_N]                        = "n",
	[SCAN_O]                        = "o",
	[SCAN_P]                        = "p",
	[SCAN_Q]                        = "q",
	[SCAN_R]                        = "r",
	[SCAN_S]                        = "s",
	[SCAN_T]                        = "t",
	[SCAN_U]                        = "u",
	[SCAN_V]                        = "v",
	[SCAN_W]                        = "w",
	[SCAN_X]                        = "x",
	[SCAN_Y]                        = "y",
	[SCAN_Z]                        = "z",
	[SCAN_1]                        = "1",
	[SCAN_2]                        = "2",
	[SCAN_3]                        = "3",
	[SCAN_4]                        = "4",
	[SCAN_5]                        = "5",
	[SCAN_6]                        = "6",
	[SCAN_7]                        = "7",
	[SCAN_8]                        = "8",
	[SCAN_9]                        = "9",
	[SCAN_0]                        = "0",
	[SCAN_ENTER]                    = "\n",
	[SCAN_ESC]                      = "",
	[SCAN_BACKSPACE]                = "\b",
	[SCAN_TAB]                      = "\t",
	[SCAN_SPACE]                    = " ",
	[SCAN_MINUS]                    = "+",
	[SCAN_EQUAL]                    = "´",
	[SCAN_LBRACKET]                 = "å",
	[SCAN_RBRACKET]                 = "¨",
	[SCAN_BACKSLASH]                = "'",
	[SCAN_NON_US_HASH]              = "#",
	[SCAN_SEMICOLON]                = "ä",
	[SCAN_APOSTROPHE]               = "ö",
	[SCAN_GRAVE]                    = "§",
	[SCAN_COMMA]                    = ",",
	[SCAN_DOT]                      = ".",
	[SCAN_SLASH]                    = "-",
	[SCAN_KEYPAD_SLASH]             = "/",
	[SCAN_KEYPAD_STAR]              = "*",
	[SCAN_KEYPAD_MINUS]             = "-",
	[SCAN_KEYPAD_PLUS]              = "+",
	[SCAN_KEYPAD_ENTER]             = "\n",
	[SCAN_KEYPAD_1]                 = "1",
	[SCAN_KEYPAD_2]                 = "2",
	[SCAN_KEYPAD_3]                 = "3",
	[SCAN_KEYPAD_4]                 = "4",
	[SCAN_KEYPAD_5]                 = "5",
	[SCAN_KEYPAD_6]                 = "6",
	[SCAN_KEYPAD_7]                 = "7",
	[SCAN_KEYPAD_8]                 = "8",
	[SCAN_KEYPAD_9]                 = "9",
	[SCAN_KEYPAD_0]                 = "0",
	[SCAN_KEYPAD_DOT]               = ".",
	[SCAN_NON_US_BACKSLASH_WALL]    = "<",
};

static const char* mod_shift_table[SCAN_MAX] = {
	[SCAN_A]                        = "A",
	[SCAN_B]                        = "B",
	[SCAN_C]                        = "C",
	[SCAN_D]                        = "D",
	[SCAN_E]                        = "E",
	[SCAN_F]                        = "F",
	[SCAN_G]                        = "G",
	[SCAN_H]                        = "H",
	[SCAN_I]                        = "I",
	[SCAN_J]                        = "J",
	[SCAN_K]                        = "K",
	[SCAN_L]                        = "L",
	[SCAN_M]                        = "M",
	[SCAN_N]                        = "N",
	[SCAN_O]                        = "O",
	[SCAN_P]                        = "P",
	[SCAN_Q]                        = "Q",
	[SCAN_R]                        = "R",
	[SCAN_S]                        = "S",
	[SCAN_T]                        = "T",
	[SCAN_U]                        = "U",
	[SCAN_V]                        = "V",
	[SCAN_W]                        = "W",
	[SCAN_X]                        = "X",
	[SCAN_Y]                        = "Y",
	[SCAN_Z]                        = "Z",
	[SCAN_1]                        = "!",
	[SCAN_2]                        = "\"",
	[SCAN_3]                        = "#",
	[SCAN_4]                        = "¤",
	[SCAN_5]                        = "%",
	[SCAN_6]                        = "&",
	[SCAN_7]                        = "/",
	[SCAN_8]                        = "(",
	[SCAN_9]                        = ")",
	[SCAN_0]                        = "=",
	[SCAN_ENTER]                    = "\n",
	[SCAN_ESC]                      = "",
	[SCAN_BACKSPACE]                = "\b",
	[SCAN_TAB]                      = "\t",
	[SCAN_SPACE]                    = " ",
	[SCAN_MINUS]                    = "?",
	[SCAN_EQUAL]                    = "`",
	[SCAN_LBRACKET]                 = "Å",
	[SCAN_RBRACKET]                 = "^",
	[SCAN_BACKSLASH]                = "*",
	[SCAN_NON_US_HASH]              = "#",
	[SCAN_SEMICOLON]                = "Ä",
	[SCAN_APOSTROPHE]               = "Ö",
	[SCAN_GRAVE]                    = "½",
	[SCAN_COMMA]                    = ";",
	[SCAN_DOT]                      = ":",
	[SCAN_SLASH]                    = "_",
	[SCAN_KEYPAD_SLASH]             = "/",
	[SCAN_KEYPAD_STAR]              = "*",
	[SCAN_KEYPAD_MINUS]             = "-",
	[SCAN_KEYPAD_PLUS]              = "+",
	[SCAN_KEYPAD_ENTER]             = "\n",
	[SCAN_KEYPAD_1]                 = "1",
	[SCAN_KEYPAD_2]                 = "2",
	[SCAN_KEYPAD_3]                 = "3",
	[SCAN_KEYPAD_4]                 = "4",
	[SCAN_KEYPAD_5]                 = "5",
	[SCAN_KEYPAD_6]                 = "6",
	[SCAN_KEYPAD_7]                 = "7",
	[SCAN_KEYPAD_8]                 = "8",
	[SCAN_KEYPAD_9]                 = "9",
	[SCAN_KEYPAD_0]                 = "0",
	[SCAN_KEYPAD_DOT]               = ".",
	[SCAN_NON_US_BACKSLASH_WALL]    = ">"
};

static const char* mod_alt_gr_table[SCAN_MAX] = {
	[SCAN_A]                        = "ə",
	[SCAN_B]                        = "b",
	[SCAN_C]                        = "c",
	[SCAN_D]                        = "ð",
	[SCAN_E]                        = "€",
	[SCAN_F]                        = "f",
	[SCAN_G]                        = "g",
	[SCAN_H]                        = "h",
	[SCAN_I]                        = "ı",
	[SCAN_J]                        = "j",
	[SCAN_K]                        = "ĸ",
	[SCAN_L]                        = "/",
	[SCAN_M]                        = "µ",
	[SCAN_N]                        = "ŋ",
	[SCAN_O]                        = "œ",
	[SCAN_P]                        = "̛",
	[SCAN_Q]                        = "q",
	[SCAN_R]                        = "r",
	[SCAN_S]                        = "ß",
	[SCAN_T]                        = "þ",
	[SCAN_U]                        = "u",
	[SCAN_V]                        = "v",
	[SCAN_W]                        = "w",
	[SCAN_X]                        = "×",
	[SCAN_Y]                        = "y",
	[SCAN_Z]                        = "ʒ",
	[SCAN_1]                        = "",
	[SCAN_2]                        = "@",
	[SCAN_3]                        = "£",
	[SCAN_4]                        = "$",
	[SCAN_5]                        = "€",
	[SCAN_6]                        = "‚",
	[SCAN_7]                        = "{",
	[SCAN_8]                        = "[",
	[SCAN_9]                        = "]",
	[SCAN_0]                        = "}",
	[SCAN_ENTER]                    = "\n",
	[SCAN_ESC]                      = "",
	[SCAN_BACKSPACE]                = "\b",
	[SCAN_TAB]                      = "\t",
	[SCAN_SPACE]                    = " ",
	[SCAN_MINUS]                    = "\\",
	[SCAN_EQUAL]                    = "¸",
	[SCAN_LBRACKET]                 = "˝",
	[SCAN_RBRACKET]                 = "~",
	[SCAN_BACKSLASH]                = "ˇ",
	[SCAN_NON_US_HASH]              = "#",
	[SCAN_SEMICOLON]                = "æ",
	[SCAN_APOSTROPHE]               = "ø",
	[SCAN_GRAVE]                    = "/",
	[SCAN_COMMA]                    = "’",
	[SCAN_DOT]                      = "̣",
	[SCAN_SLASH]                    = "–",
	[SCAN_KEYPAD_SLASH]             = "/",
	[SCAN_KEYPAD_STAR]              = "*",
	[SCAN_KEYPAD_MINUS]             = "-",
	[SCAN_KEYPAD_PLUS]              = "+",
	[SCAN_KEYPAD_ENTER]             = "\n",
	[SCAN_KEYPAD_1]                 = "1",
	[SCAN_KEYPAD_2]                 = "2",
	[SCAN_KEYPAD_3]                 = "3",
	[SCAN_KEYPAD_4]                 = "4",
	[SCAN_KEYPAD_5]                 = "5",
	[SCAN_KEYPAD_6]                 = "6",
	[SCAN_KEYPAD_7]                 = "7",
	[SCAN_KEYPAD_8]                 = "8",
	[SCAN_KEYPAD_9]                 = "9",
	[SCAN_KEYPAD_0]                 = "0",
	[SCAN_KEYPAD_DOT]               = ".",
	[SCAN_NON_US_BACKSLASH_WALL]    = "|"
};

// clang-format on

const char* layout_fi_scan_to_key(Scancode code, Modifier modifiers) {
	if (modifiers & MOD_SHIFT) {
		return mod_shift_table[code];
	}
	else if (modifiers & MOD_ALT_GR) {
		return mod_alt_gr_table[code];
	}
	else {
		return mod_none_table[code];
	}
}