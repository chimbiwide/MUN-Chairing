#ifndef TEXT_H
#define TEXT_H

#include <stdbool.h>

// buffer sizes in bytes, terminator included
#define NAME_SIZE 64
#define TOPIC_SIZE 128
#define TITLE_SIZE 96
#define NOTE_SIZE 48
#define ERROR_SIZE 96

// snprintf "%s" that accepts NULL as ""
void copy_text(char *target, int size, const char *source);
// strips leading and trailing ASCII whitespace in place
void trim_text(char *text);
// replaces tabs, CR and LF with spaces so the text fits on one line of a save file
void flatten_text(char *text);
// lowercase ASCII with Latin-1 / Latin Extended-A accents removed ("Côte d'Ivoire" -> "cote divoire").
// apostrophes and periods are dropped, other punctuation becomes a space, runs of spaces collapse
void fold_name(const char *text, char *out, int size);
// equal after fold_name
bool same_name(const char *a, const char *b);
// how well name matches what the chair typed, 0 when it doesn't:
// 100 equal, 80 prefix, 70 initials ("uae"), 60 word prefix ("korea"), 40 anywhere
int match_name(const char *name, const char *query);

#endif
