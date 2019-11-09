#ifndef CHARUTILS_H
#define CHARUTILS_H

// Returns lowercase version of char
char cLower(char c) {
	if(c >= 65 && c <= 90)
		return c + 32;
	else
		return c;
}

// Returns uppercase version of char
char cUpper(char c) {
	if(c >= 97 && c <= 122)
		return c - 32;
	else
		return c;
}

// Converts char array to lowercase
void cToLower(char* c) {
	int i = 0;
	while(c[i] != 0x0) {
		c[i] = cLower(c[i]);
		i++;
	}
}

// Converts char array to uppercase
void cToUpper(char* c) {
	int i = 0;
	while(c[i] != 0x0) {
		c[i] = cUpper(c[i]);
		i++;
	}
}

// Debug char str
void debugCharStr(char* c) {
	int i = 0;
	while(c[i] != 0x0) {
    	Serial.print((int)c[i]);
		Serial.print("\t");
		Serial.print(c[i++]);
		Serial.print("\n");
	}
	Serial.print("\n");
}

// Check if two strings are equal
bool cEquals(char* c, char* c2) {
	int i = 0;
	while(c[i] != 0x0 && c2[i] != 0x0)
		if(c[i] != c2[i++])
			return false;
	return c[i] == c2[i]; // They should both be null terminators
}

// Checks if a command is equal to cmd. Returns -1 if the command is not cmd, or the index of the
// start of the parameters.
int checkCommand(char* c, char* cmd) {

}

// Get "fragment" from string. Basically string split at spaces
// Searches from start to end of char string
// Loads fragment into buf
// Returns the index of the next fragment, or -1 for the end of the string
int getFragment(char* c, int start, char* buf) {
	int i = start,
		bufi = 0;
	while(c[i] != ' ' && c[i] != 0x0)
		buf[bufi++] = c[i++];
	buf[bufi] = 0x0;
	if(c[i] == 0x0)
		return -1;
	return i + 1;
}

#endif
