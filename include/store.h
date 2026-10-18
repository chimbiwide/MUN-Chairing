#ifndef STORE_H
#define STORE_H

#include <stdbool.h>
#include <session.h>

#define SESSION_FILE "mun_session.txt"

// Writes the whole session as UTF-8 text, one tab-separated record per line, first line "MUNTIMER<TAB>1".
// User text has tabs and line breaks flattened. Running clocks are written paused at their elapsed time.
// Writes path.tmp first, keeps the previous file as path.bak, then renames, so a crash mid-write
// never leaves a torn file. false when the file can't be written
bool save_session(const Session *session, const char *path, double now);
// Starts from init_session and applies the records it understands, ignoring unknown ones.
// Every number is clamped to its valid range and every index checked, so a damaged file loads
// what it can. When path is missing or not a session, path.bak is tried. false (with session
// left as init_session) when neither is a session
bool load_session(Session *session, const char *path);
// the log as text: committee, agenda and date, then "14:05  <text>" per line
bool export_log(const Session *session, const char *path);
// CSV for Excel, one row per delegate: Delegation,Attendance,GSL speeches,Caucus speeches,Speeches,Seconds spoken,Time spoken
// with RFC 4180 quoting, a UTF-8 byte order mark first, and "Not taken" as attendance before any roll call
bool export_stats(const Session *session, const char *path);
// prefix plus local date and time: "mun_log_20261017_1432.txt" from ("mun_log", "txt")
void stamped_name(const char *prefix, const char *extension, char *buffer, int size);

#endif
