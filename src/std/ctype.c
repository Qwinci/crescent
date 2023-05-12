#include "ctype.h"

int tolower(int ch) {
	return ch | 1 << 5;
}

int toupper(int ch) {
	return ch & ~(1 << 5);
}

int isprint(int ch) {
	return ch >= ' ' && ch <= '~';
}

int isdigit(int ch) {
	return ch >= '0' && ch <= '9';
}

int isspace(int ch) {
	return ch == ' ' || ch == '\f' || ch == '\n' || ch == '\r' || ch == 9 || ch == 0xB;
}

int isxdigit(int ch) {
	return (ch >= '0' && ch <= '9') || (tolower(ch) >= 'a' && tolower(ch) <= 'f');
}