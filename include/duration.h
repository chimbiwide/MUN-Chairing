#ifndef DURATION_H
#define DURATION_H

#include <stdbool.h>

#define MAX_CAUCUS_SECONDS (240 * 60)
#define MAX_SPEAKING_SECONDS (30 * 60)

// a caucus length as chairs write it: "9:45" is 9 minutes in total at 45 seconds per speaker
typedef struct {
    int total;      // seconds
    int speaker;    // seconds per speaker, 0 when only a total was given ("10" for an unmod)
} Duration;

// Reads a caucus length. Case and surrounding spaces don't matter.
//   "10"  "10m"  "10 min"  "7.5"          total only, minutes by default
//   "9:45"  "9/45"  "9 45"                 total in minutes, then seconds per speaker
//   "10:1"  "15:1.5"                       a bare speaker number under 10 means minutes,
//   "10:05"                                but one written with a leading zero means seconds
//   "10:1:30"                              speaker written as m:ss
//   "10m45s"  "10 min 45 sec"  "30:10m"    explicit units win over the defaults
//   "90s"  "100s:45"                       a total can be given in seconds
// Fails with a short message for the chair: empty or unreadable text, a zero length,
// a total over MAX_CAUCUS_SECONDS, a speaker time longer than the total, or a bare number
// of 100 or more ending in 01 to 59 that looks like a missing colon ("945" -> "Did you mean 9:45?").
bool parse_caucus(const char *text, Duration *out, char *error, int error_size);

// Reads one speaking time, seconds by default: "90", "1:30", "90s", "1m30s", "1.5 min".
// A bare number under 10 means minutes ("1" is 60 s, "1.5" is 90 s) unless it has a leading zero ("05").
// Fails on empty, zero or unreadable text and on anything over MAX_SPEAKING_SECONDS.
bool parse_speaking(const char *text, int *seconds, char *error, int error_size);

// how many full speeches fit: total / speaker, 0 when there is no speaker time
int speaker_slots(Duration duration);
// seconds left over after the full speeches: total % speaker, 0 when there is no speaker time
int slot_leftover(Duration duration);

// clock face for whole seconds: "0:45", "9:00", "59:59", "1:00:00"; negative values get a leading '-'
void format_clock(int seconds, char *buffer, int size);
// clock face for a countdown: ceil of what is left ("0:45" with 44.2 s left, "0:00" at zero),
// and once a full second over, '+' and the whole seconds over ("+0:03" at -3.7 s)
void format_left(double left, char *buffer, int size);
// spoken form: "45 s", "1 min", "1 min 30 s", "1 h 5 min"
void format_span(int seconds, char *buffer, int size);
// the short form chairs write, which parse_caucus reads back to the same Duration:
//   total: whole minutes "9", tenths of a minute "7.5", otherwise seconds "100s"
//   speaker (after ':', left out when 0): whole minutes under 10 "1", whole minutes from 10 "10m",
//   under 10 s "5s", under a minute "45", otherwise m:ss "1:30"
// so "9:45", "10:1", "10:1:30", "30:10m", "7.5:45", "10"
void format_duration(Duration duration, char *buffer, int size);
// one line of feedback under the motion form:
// "9 min total · 45 s each · 12 speakers", plus " · 15 s left over" when it doesn't divide,
// "1 speaker" when only one fits, and just "10 min" when there is no speaker time
void describe_duration(Duration duration, char *buffer, int size);

#endif
