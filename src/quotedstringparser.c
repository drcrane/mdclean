#include "quotedstringparser.h"

#include <string.h>
#include <stdarg.h>

#include <stdlib.h>
#include <stdio.h>

#include "dbg.h"

int qsp_callbacker(char * input, void * arg, int(*cb)(void * arg, int idx, char * field)) {
	char * ptr;
	char * rover;
	char * temp;
	int count = 0;
	int keep_going = 1;

	rover = input;
	do {
		if (*rover == '\"') {
			ptr = rover + 1;
skip_escaped_quotes:
			rover++;
			temp = strchr(rover, '\"');
			if (temp != NULL) {
				if (*(temp - 1) == '\\') {
					rover = temp;
					goto skip_escaped_quotes;
				}
				*temp = '\0';
				keep_going = !cb(arg, count, ptr);
				count++;
				rover = temp + 1;
				if (*rover == ',') {
					// Skip a comma that is immediately after a closing "
					rover++;
				}
			} else {
				cb(arg, count, rover);
				count++;
				keep_going = 0;
			}
		} else if (*rover == ' ') {
			rover++;
		} else {
			temp = strchr(rover, ',');
			if (temp != NULL) {
				*temp = '\0';
				keep_going = !cb(arg, count, rover);
				count++;
				rover = temp + 1;
			} else {
				cb(arg, count, rover);
				count++;
				keep_going = 0;
			}
		}
	} while (*rover != '\0' && keep_going);
	return count;
}

int qsp_inplace(char * input, int max, ...) {
	va_list ap;
	char ** ptr;
	char * rover;
	char * temp;
	int count = 0;

	rover = input;
	va_start(ap, max);
	do {
		if (*rover == '\"') {
			ptr = va_arg(ap, char **);
			*rover = '\0';
			rover++;
			*ptr = rover;
			temp = rover;
			// TODO: perhaps allow for data escaped with two "'s ***NOT IMPLEMENTED***
			// for example 1,"Hello ""Ben""",9 -> [0] = 1, [1] = Hello "Ben", [2] = 9
			// To be clear, this does not do that now, it only supports the \" method of escaping quotes.
			do {
				temp = strchr(temp, '\"');
				if (temp != NULL) {
					if (*(temp - 1) != '\\') {
						break;
					}
					temp++;
				}
			} while (temp != NULL);
			// TODO: Unescape rover
			// Values are returned with their \'s at the moment.
			if (temp != NULL) {
				// callback here
				*temp = 0;
				rover = temp + 1;
			} else {
				rover++;
			}
			count++;
		} else if (*rover == ' ') {
			rover++;
		} else if (*rover == ',') {
			rover++;
		} else {
			ptr = va_arg(ap, char **);
			*ptr = rover;
			temp = strchr(rover, '\"');
			if (temp != NULL) {
				char * temp2 = strchr(rover, ',');
				if (temp2 != NULL && temp2 < temp) {
					goto go_to_next_comma;
				}
				rover = temp;
			} else {
go_to_next_comma:
				temp = strchr(rover, ',');
				if (temp != NULL) {
					*temp = 0;
					// callback here...
					rover = temp + 1;
				} else {
					rover++;
				}
			}
			count++;
		}
	} while (*rover != '\0' && count < max);
	// callback here...
	va_end(ap);
	return count;
}

int qsp_inplace_commandline(char * input, int max, ...) {
	va_list ap;
	char ** ptr = NULL;
	char * rover;
	char * temp;
	int count = 0;

	rover = input;
	va_start(ap, max);
	do {
		if (*rover == '\"') {
			ptr = va_arg(ap, char **);
			*rover = '\0';
			rover++;
			temp = rover;
			do {
				temp = strchr(temp, '\"');
				if (temp != NULL) {
					if (*(temp - 1) != '\\') {
						break;
					}
					temp++;
				}
			} while (temp != NULL);
			// TODO: Unescape rover
			// Values are returned with their \'s at the moment.
			if (temp != NULL) {
				// callback here
				*temp = 0;
				*ptr = rover;
				rover = temp + 1;
			} else {
				// In this case the string must have terminated before the closing "
				// Save this one and exit the loop.
				*ptr = rover;
				count++;
				break;
				//rover++;
			}
			count++;
		} else if (*rover == ' ') {
			rover++;
		} else {
			ptr = va_arg(ap, char **);
			temp = strchr(rover, '\"');
			if (temp != NULL) {
				char * temp2 = strchr(rover, ' ');
				if (temp2 != NULL && temp2 < temp) {
					goto go_to_next_space;
				}
				// Cater for the odd case where the string looks like this:
				// Java"Welcome"
				*ptr = rover;
				rover = temp;
			} else {
go_to_next_space:
				temp = strchr(rover, ' ');
				if (temp != NULL) {
					*temp = '\0';
					// callback here...
					*ptr = rover;
					rover = temp + 1;
				} else {
					// if we are here then we have reached the end of the string
					// and so can terminate the while loop.
					*ptr = rover;
					count++;
					break;
				}
			}
			count++;
		}
	} while (*rover != '\0' && count < max);
	va_end(ap);
	return count;
}

#define QSP_STATE_OUTSIDE_QUOTES 0
#define QSP_STATE_IN_QUOTES      1
#define QSP_STATE_ESCAPE         2

// Below from utilityfn... should have a C strings file for all this stuff.
//int strutil_matches(char input, char * canded);

/*
 * Does input match any character in matches
 */
static int matches(char input, const char * matches) {
	while (*matches != '\0') {
		if (*matches == input) {
			return 1;
		}
		matches++;
	}
	return 0;
}

/**
 * Look at the characters in buf until len bytes or one of the delimiters. When within quotes
 * append the delimiters to the dynstring. This results in a function that when called with
 * (string begins at [ and ends at ]: [abcde"fghij k"] and delimiters of [, ] the character
 * string [abcdefghij k] will be appended to dynstring.
 * @dynstring string to append characters
 * @buf input string
 * @len maximum length of the input string (will terminate at nul if encountered before len)
 * @delim string of characters to match which will cause the function to terminate
 * returns number of bytes consumed (does not include the matched delimiter or nul byte)
 * make a string until a delim is encountered outside of quotes
 * stop at a nul character.
 */
size_t qsp_append_to_dynstring(dynstring_context_t * dynstring, char * buf, size_t len, char * delim) {
	char * rover;
	int state = QSP_STATE_OUTSIDE_QUOTES;
	size_t count = 0;
	rover = buf;
	while (count < len) {
		int c = *rover;
		if (c == '"') {
			if (state == QSP_STATE_IN_QUOTES) {
				state = QSP_STATE_OUTSIDE_QUOTES;
			} else {
				state = QSP_STATE_IN_QUOTES;
			}
		} else
		if (state == QSP_STATE_IN_QUOTES) {
			if (c == '\\') {
				state = QSP_STATE_ESCAPE;
			} else {
				dynstring_appendchar(dynstring, c);
			}
		} else
		if (state == QSP_STATE_ESCAPE) {
			dynstring_appendchar(dynstring, c);
			state = QSP_STATE_IN_QUOTES;
		} else
		if (matches(c, delim) || c == '\0') {
			break;
		} else {
			dynstring_appendchar(dynstring, c);
		}
		rover++;
		count++;
	}
	return count;
}

