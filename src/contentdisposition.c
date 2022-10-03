#include <stdio.h>
#include <string.h>
#include "quotedstringparser.h"
#include "dbg.h"

static int matches(char input, const char * matches) {
	while (*matches != '\0') {
		if (*matches == input) {
			return 1;
		}
		matches ++;
	}
	return 0;
}

static int my_isspace(char input) {
	return matches(input, "\t \r\n");
}

static char * find_str_after_semicolon(char * buf, char * match) {
	char * buf_pos;
	size_t len;
	buf_pos = buf;
	do {
		buf_pos = strchr(buf_pos, ';');
		if (buf_pos != NULL) {
			buf_pos ++;
			while (my_isspace(*buf_pos)) {
				buf_pos++;
			}
			len = strlen(match);
			if (strncmp(match, buf_pos, len) == 0) {
				return buf_pos + len;
			}
		}
	} while (buf_pos != NULL);
	return NULL;
}

static char * find_attribute_value(char * hdr, char * attrib) {
	char * pos;
	dynstring_context_t fld_name, fld_value;
	size_t consumed;
	pos = hdr;
	dynstring_initialise(&fld_name, 128);
	dynstring_initialise(&fld_value, 128);
try_again:
	while (my_isspace(*pos)) { pos++; }
	dynstring_empty(&fld_name);
	dynstring_empty(&fld_value);
	consumed = qsp_append_to_dynstring(&fld_name, pos, strlen(pos), ";=");
	if (consumed) {
		pos += consumed;
	}
	if (*pos == '=') {
		pos ++;
		consumed = qsp_append_to_dynstring(&fld_value, pos, strlen(pos), ";");
		pos += consumed;
	}
	if (strncmp(attrib, fld_name.buf, fld_name.pos) == 0) {
		// we found the requested attribute!
		dynstring_free(&fld_name);
		return dynstring_detachcstring(&fld_value);
	}
	if (*pos == '\0' && *pos != ';') {
		// end of the line boys!
		dynstring_free(&fld_name);
		dynstring_free(&fld_value);
		return NULL;
	}
	pos ++;
	goto try_again;
}

static char * getstringafterattributename(char * hdr, char * attrib) {
	char * field_value;
	dynstring_context_t value;
	field_value = find_str_after_semicolon(hdr, attrib);
	if (field_value == NULL) { return NULL; }
	dynstring_initialise(&value, 128);
	qsp_append_to_dynstring(&value, field_value, strlen(field_value), ";");
	// replace with dynstring_detachcstring()
	dynstring_appendchar(&value, '\0');
	dynstring_compact(&value);
	field_value = value.buf;
	value.buf = NULL;
	dynstring_free(&value);
	// --------------------------------------
	return field_value;
}

char * keyvalueheader_getbyname(char * hdr, char * name) {
	return find_attribute_value(hdr, name);
}

char * contentdisposition_getname(char * hdr) {
	return find_attribute_value(hdr, "name");
}

char * contentdisposition_getfilename(char * hdr) {
	return find_attribute_value(hdr, "filename");
}

