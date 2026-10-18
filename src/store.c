// fsync and fileno are POSIX, which a strict -std=c23 hides unless asked for
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include <store.h>
#include <duration.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <io.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

#define FILE_MAGIC "MUNTIMER"
#define FILE_VERSION 1
#define LINE_SIZE 4096
#define MAX_FIELDS 12
#define PATH_SIZE 1024
#define MAX_SECONDS 86400
#define MAX_COUNT 1000000
#define MAX_WARN_TIME 600

// put stops at the first failed write, so a writer is checked once, when it closes
typedef struct {
    FILE *file;
    bool ok;
} Writer;

// what a load can only apply once every line is read
typedef struct {
    int gsl_current;
    int caucus_current;
} Pending;

// the on/off settings, written in this order
typedef struct {
    const char *key;
    size_t offset;
} FlagSetting;

static const FlagSetting flag_settings[] = {
    {"auto_start", offsetof(Settings, auto_start)},
    {"proposer_first", offsetof(Settings, proposer_first)},
    {"drop_motions", offsetof(Settings, drop_motions)},
    {"auto_sort", offsetof(Settings, auto_sort)},
    {"security_council", offsetof(Settings, security_council)},
    {"quarter_quorum", offsetof(Settings, quarter_quorum)},
    {"dark", offsetof(Settings, dark)},
};

#define FLAG_COUNT ((int)(sizeof(flag_settings) / sizeof(flag_settings[0])))

static int clamp_int(long long value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return (int)value;
}

static void put(Writer *out, const char *format, ...) __attribute__((format(printf, 2, 3)));

static void put(Writer *out, const char *format, ...) {
    if (!out->ok) return;
    va_list args;
    va_start(args, format);
    if (vfprintf(out->file, format, args) < 0) out->ok = false;
    va_end(args);
}

// best effort: moves what was written from the system's cache onto the disk. without it a power cut
// soon after the renames can leave both the file and its .bak empty (ext4 only does this by itself
// when a rename replaces an existing file, and the save moves the old one away first)
static void sync_file(FILE *file) {
#if defined(_WIN32)
    _commit(_fileno(file));
#elif defined(__unix__) || defined(__APPLE__)
    fsync(fileno(file));
#else
    (void)file;
#endif
}

// flushes and closes, true when every write reached the file. sync also puts it on the disk
static bool close_writer(Writer *out, bool sync) {
    bool ok = out->ok && fflush(out->file) == 0 && ferror(out->file) == 0;
    if (ok && sync) sync_file(out->file);
    if (fclose(out->file) != 0) ok = false;
    return ok;
}

// a copy with tabs and line breaks made spaces, so the text stays one field of one line
static const char *flat_copy(const char *text, char *buffer, int size) {
    copy_text(buffer, size, text);
    flatten_text(buffer);
    return buffer;
}

static void put_clock(Writer *out, const char *record, const Clock *clock, double now) {
    put(out, "%s\t%.3f\t%.3f\n", record, clock->limit, clock_elapsed(clock, now));
}

static void put_speakers(Writer *out, const char *record, const SpeakerList *list) {
    char name[NAME_SIZE];
    char note[NOTE_SIZE];
    int count = clamp_int(list->count, 0, MAX_SPEAKERS);
    for (int i = 0; i < count; i++) {
        const Speaker *speaker = &list->items[i];
        put(out, "%s\t%d\t%d\t%.3f\t%s\t%s\n", record, speaker->done ? 1 : 0, speaker->limit, speaker->spoke,
            flat_copy(speaker->name, name, sizeof(name)), flat_copy(speaker->note, note, sizeof(note)));
    }
}

// the fields of a motion, each after a tab
static void put_motion(Writer *out, const Motion *motion) {
    char proposer[NAME_SIZE];
    char topic[TOPIC_SIZE];
    put(out, "\t%d\t%d\t%d\t%d\t%s\t%s", (int)motion->type, motion->total, motion->speaker, motion->serial,
        flat_copy(motion->proposer, proposer, sizeof(proposer)), flat_copy(motion->topic, topic, sizeof(topic)));
}

static void put_session(Writer *out, const Session *session, double now) {
    char committee[TITLE_SIZE];
    char agenda[TOPIC_SIZE];
    put(out, "%s\t%d\n", FILE_MAGIC, FILE_VERSION);
    put(out, "committee\t%s\n", flat_copy(session->committee, committee, sizeof(committee)));
    put(out, "agenda\t%s\n", flat_copy(session->agenda, agenda, sizeof(agenda)));

    const Settings *settings = &session->settings;
    put(out, "setting\tgsl_time\t%d\n", settings->gsl_time);
    put(out, "setting\twarn_time\t%d\n", settings->warn_time);
    put(out, "setting\tmod_clock\t%d\n", (int)settings->mod_clock);
    for (int i = 0; i < FLAG_COUNT; i++) {
        const bool *flag = (const bool *)((const char *)settings + flag_settings[i].offset);
        put(out, "setting\t%s\t%d\n", flag_settings[i].key, *flag ? 1 : 0);
    }
    put(out, "phase\t%d\n", (int)session->phase);

    const Roster *roster = &session->roster;
    char name[NAME_SIZE];
    put(out, "roll\t%d\n", roster->roll_taken ? 1 : 0);
    int delegates = clamp_int(roster->count, 0, MAX_DELEGATES);
    for (int i = 0; i < delegates; i++) {
        const Delegate *delegate = &roster->items[i];
        put(out, "delegate\t%d\t%d\t%d\t%d\t%d\t%.3f\t%s\n", (int)delegate->attendance, (int)delegate->ballot,
            delegate->veto ? 1 : 0, delegate->gsl_speeches, delegate->caucus_speeches, delegate->spoken,
            flat_copy(delegate->name, name, sizeof(name)));
    }

    put_clock(out, "gsl_clock", &session->gsl_clock, now);
    put_speakers(out, "gsl", &session->gsl);
    put(out, "gsl_current\t%d\n", session->gsl.current);

    const MotionList *motions = &session->motions;
    int motion_count = clamp_int(motions->count, 0, MAX_MOTIONS);
    for (int i = 0; i < motion_count; i++) {
        put(out, "motion");
        put_motion(out, &motions->items[i]);
        put(out, "\n");
    }
    put(out, "motion_serial\t%d\n", motions->serial);

    const Caucus *caucus = &session->caucus;
    put(out, "caucus\t%d\t%d", caucus->held ? 1 : 0, caucus->active ? 1 : 0);
    put_motion(out, &caucus->motion);
    put(out, "\n");
    put_clock(out, "caucus_total", &caucus->total, now);
    put_clock(out, "caucus_speaker", &caucus->speaker, now);
    put_speakers(out, "speaker", &caucus->speakers);
    put(out, "caucus_current\t%d\n", caucus->speakers.current);
    put(out, "alerts\t%u\n", session->alerts);

    const Log *log = &session->log;
    char at[sizeof(log->items[0].time)];
    char text[LOG_TEXT];
    int entries = clamp_int(log->count, 0, MAX_LOG);
    for (int i = 0; i < entries; i++) {
        put(out, "log\t%s\t%s\n", flat_copy(log->items[i].time, at, sizeof(at)),
            flat_copy(log->items[i].text, text, sizeof(text)));
    }
}

// path with suffix appended, false when it doesn't fit
static bool with_suffix(const char *path, const char *suffix, char *buffer, int size) {
    int written = snprintf(buffer, (size_t)size, "%s%s", path, suffix);
    return written >= 0 && written < size;
}

bool save_session(const Session *session, const char *path, double now) {
    if (session == NULL || path == NULL) return false;
    char temporary[PATH_SIZE];
    char backup[PATH_SIZE];
    if (!with_suffix(path, ".tmp", temporary, sizeof(temporary)) || !with_suffix(path, ".bak", backup, sizeof(backup))) {
        return false;
    }

    FILE *file = fopen(temporary, "wb");
    if (file == NULL) return false;
    Writer out = {.file = file, .ok = true};
    put_session(&out, session, now);
    if (!close_writer(&out, true)) {
        remove(temporary);
        return false;
    }

    // Windows can't rename onto an existing file, so the old one moves aside first
    remove(backup);
    bool moved = rename(path, backup) == 0;
    if (!moved && errno != ENOENT) {
        remove(temporary);
        return false;
    }
    if (rename(temporary, path) != 0) {
        if (moved) rename(backup, path);
        remove(temporary);
        return false;
    }
    return true;
}

// one line without its line break or a trailing \r (files edited on Windows).
// what doesn't fit in size is skipped. false at the end of the file
static bool read_line(FILE *file, char *line, int size) {
    int c = getc(file);
    if (c == EOF) return false;
    int length = 0;
    while (c != EOF && c != '\n') {
        if (length < size - 1) line[length++] = (char)c;
        c = getc(file);
    }
    if (length > 0 && line[length - 1] == '\r') length--;
    line[length] = '\0';
    return true;
}

// splits line on tabs in place, keeping empty fields. past max fields the last one keeps the rest
static int split_fields(char *line, char **field, int max) {
    int count = 0;
    field[count++] = line;
    for (char *c = line; *c != '\0' && count < max; c++) {
        if (*c == '\t') {
            *c = '\0';
            field[count++] = c + 1;
        }
    }
    return count;
}

// the whole number field starts with, clamped to low..high. fallback when it has none
static int read_int(const char *field, int low, int high, int fallback) {
    char *end = NULL;
    long long value = strtoll(field, &end, 10);
    return end == field ? fallback : clamp_int(value, low, high);
}

static bool read_flag(const char *field, bool fallback) {
    return read_int(field, 0, 1, fallback ? 1 : 0) == 1;
}

static double read_seconds(const char *field, double fallback) {
    char *end = NULL;
    double value = strtod(field, &end);
    if (end == field || isnan(value)) return fallback;
    if (value < 0.0) return 0.0;
    return value < MAX_SECONDS ? value : MAX_SECONDS;
}

// "MUNTIMER<TAB><version>" with a whole number version from 1 on. a UTF-8 byte order mark is skipped
static bool is_header(char *line) {
    if (strncmp(line, "\xEF\xBB\xBF", 3) == 0) line += 3;
    char *field[MAX_FIELDS];
    int count = split_fields(line, field, MAX_FIELDS);
    if (count < 2 || strcmp(field[0], FILE_MAGIC) != 0 || field[1][0] == '\0') return false;
    for (const char *c = field[1]; *c != '\0'; c++) {
        if (!isdigit((unsigned char)*c)) return false;
    }
    return strtol(field[1], NULL, 10) >= 1;
}

// field[0] is name and the line has all the fields the record needs, name included
static bool is_record(char **field, int count, const char *name, int needed) {
    return count >= needed && strcmp(field[0], name) == 0;
}

static void read_setting(Settings *settings, const char *key, const char *value) {
    if (strcmp(key, "gsl_time") == 0) {
        settings->gsl_time = read_int(value, 1, MAX_SPEAKING_SECONDS, settings->gsl_time);
    }
    else if (strcmp(key, "warn_time") == 0) {
        settings->warn_time = read_int(value, 0, MAX_WARN_TIME, settings->warn_time);
    }
    else if (strcmp(key, "mod_clock") == 0) {
        settings->mod_clock = (ModClock)read_int(value, MOD_CLOCK_SLOTS, MOD_CLOCK_CONTINUOUS, (int)settings->mod_clock);
    }
    else {
        for (int i = 0; i < FLAG_COUNT; i++) {
            if (strcmp(key, flag_settings[i].key) != 0) continue;
            bool *flag = (bool *)((char *)settings + flag_settings[i].offset);
            *flag = read_flag(value, *flag);
        }
    }
}

// attendance, ballot, veto, gsl speeches, caucus speeches, spoken, name
static void read_delegate(Roster *roster, char **field) {
    if (roster->count >= MAX_DELEGATES) return;
    Delegate *delegate = &roster->items[roster->count++];
    *delegate = (Delegate){0};
    delegate->attendance = (Attendance)read_int(field[1], ABSENT, PRESENT_VOTING, ABSENT);
    delegate->ballot = (Ballot)read_int(field[2], BALLOT_NONE, BALLOT_ABSTAIN, BALLOT_NONE);
    delegate->veto = read_flag(field[3], false);
    delegate->gsl_speeches = read_int(field[4], 0, MAX_COUNT, 0);
    delegate->caucus_speeches = read_int(field[5], 0, MAX_COUNT, 0);
    delegate->spoken = read_seconds(field[6], 0.0);
    copy_text(delegate->name, sizeof(delegate->name), field[7]);
}

// done, limit, spoke, name, note
static void read_speaker(SpeakerList *list, char **field) {
    if (list->count >= MAX_SPEAKERS) return;
    Speaker *speaker = &list->items[list->count++];
    *speaker = (Speaker){0};
    speaker->done = read_flag(field[1], false);
    speaker->limit = read_int(field[2], 0, MAX_SECONDS, 0);
    speaker->spoke = read_seconds(field[3], 0.0);
    copy_text(speaker->name, sizeof(speaker->name), field[4]);
    copy_text(speaker->note, sizeof(speaker->note), field[5]);
}

// type, total, speaker, serial, proposer, topic
static void read_motion(Motion *motion, char **field) {
    *motion = (Motion){0};
    motion->type = (MotionType)read_int(field[0], 0, MOTION_TYPE_COUNT - 1, MOTION_OTHER);
    motion->total = read_int(field[1], 0, MAX_SECONDS, 0);
    motion->speaker = read_int(field[2], 0, MAX_SECONDS, 0);
    motion->serial = read_int(field[3], 0, MAX_COUNT, 0);
    copy_text(motion->proposer, sizeof(motion->proposer), field[4]);
    copy_text(motion->topic, sizeof(motion->topic), field[5]);
}

// limit, elapsed. a loaded clock is paused where it was
static void read_clock(Clock *clock, char **field) {
    clock->limit = read_seconds(field[1], clock->limit);
    clock->banked = read_seconds(field[2], 0.0);
    clock->started = 0.0;
    clock->running = false;
}

static void read_log(Log *log, char **field) {
    if (log->count >= MAX_LOG) return;
    LogEntry *entry = &log->items[log->count++];
    copy_text(entry->time, sizeof(entry->time), field[1]);
    copy_text(entry->text, sizeof(entry->text), field[2]);
}

static void apply_record(Session *session, Pending *pending, char **field, int count) {
    Caucus *caucus = &session->caucus;
    MotionList *motions = &session->motions;
    if (is_record(field, count, "committee", 2)) {
        copy_text(session->committee, sizeof(session->committee), field[1]);
    }
    else if (is_record(field, count, "agenda", 2)) {
        copy_text(session->agenda, sizeof(session->agenda), field[1]);
    }
    else if (is_record(field, count, "setting", 3)) {
        read_setting(&session->settings, field[1], field[2]);
    }
    else if (is_record(field, count, "phase", 2)) {
        session->phase = (Phase)read_int(field[1], PHASE_DEBATE, PHASE_ADJOURNED, (int)session->phase);
    }
    else if (is_record(field, count, "roll", 2)) {
        session->roster.roll_taken = read_flag(field[1], session->roster.roll_taken);
    }
    else if (is_record(field, count, "delegate", 8)) {
        read_delegate(&session->roster, field);
    }
    else if (is_record(field, count, "gsl_clock", 3)) {
        read_clock(&session->gsl_clock, field);
    }
    else if (is_record(field, count, "gsl", 6)) {
        read_speaker(&session->gsl, field);
    }
    else if (is_record(field, count, "gsl_current", 2)) {
        pending->gsl_current = read_int(field[1], -1, MAX_COUNT, -1);
    }
    else if (is_record(field, count, "motion", 7)) {
        if (motions->count < MAX_MOTIONS) read_motion(&motions->items[motions->count++], field + 1);
    }
    else if (is_record(field, count, "motion_serial", 2)) {
        motions->serial = read_int(field[1], 0, MAX_COUNT, motions->serial);
    }
    else if (is_record(field, count, "caucus", 9)) {
        caucus->held = read_flag(field[1], false);
        caucus->active = read_flag(field[2], false);
        read_motion(&caucus->motion, field + 3);
    }
    else if (is_record(field, count, "caucus_total", 3)) {
        read_clock(&caucus->total, field);
    }
    else if (is_record(field, count, "caucus_speaker", 3)) {
        read_clock(&caucus->speaker, field);
    }
    else if (is_record(field, count, "speaker", 6)) {
        read_speaker(&caucus->speakers, field);
    }
    else if (is_record(field, count, "caucus_current", 2)) {
        pending->caucus_current = read_int(field[1], -1, MAX_COUNT, -1);
    }
    else if (is_record(field, count, "alerts", 2)) {
        session->alerts = (unsigned)read_int(field[1], 0, MAX_COUNT, 0);
    }
    else if (is_record(field, count, "log", 3)) {
        read_log(&session->log, field);
    }
}

// index when it names an entry of list that is not crossed out, otherwise -1
static int checked_current(const SpeakerList *list, int index) {
    if (index < 0 || index >= list->count || list->items[index].done) return -1;
    return index;
}

// false, with session untouched, when path can't be opened or is not a session
static bool load_file(Session *session, const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    char line[LINE_SIZE];
    if (!read_line(file, line, sizeof(line)) || !is_header(line)) {
        fclose(file);
        return false;
    }

    Pending pending = {.gsl_current = -1, .caucus_current = -1};
    char *field[MAX_FIELDS];
    while (read_line(file, line, sizeof(line))) {
        int count = split_fields(line, field, MAX_FIELDS);
        apply_record(session, &pending, field, count);
    }
    fclose(file);

    session->gsl.current = checked_current(&session->gsl, pending.gsl_current);
    session->caucus.speakers.current = checked_current(&session->caucus.speakers, pending.caucus_current);
    // a file that lost motion_serial still hands new motions a later serial than the listed ones
    MotionList *motions = &session->motions;
    for (int i = 0; i < motions->count; i++) {
        if (motions->items[i].serial >= motions->serial) motions->serial = motions->items[i].serial + 1;
    }
    return true;
}

bool load_session(Session *session, const char *path) {
    if (session == NULL) return false;
    init_session(session);
    if (path == NULL) return false;
    if (load_file(session, path)) return true;
    char backup[PATH_SIZE];
    return with_suffix(path, ".bak", backup, sizeof(backup)) && load_file(session, backup);
}

// the local time as format, fallback when the clock can't be read
static void local_stamp(const char *format, char *buffer, int size, const char *fallback) {
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (local == NULL || strftime(buffer, (size_t)size, format, local) == 0) copy_text(buffer, size, fallback);
}

// closes an export, removing the partial file when a write failed
static bool finish_export(Writer *out, const char *path) {
    if (close_writer(out, false)) return true;
    remove(path);
    return false;
}

bool export_log(const Session *session, const char *path) {
    if (session == NULL || path == NULL) return false;
    FILE *file = fopen(path, "w");
    if (file == NULL) return false;

    char committee[TITLE_SIZE];
    char agenda[TOPIC_SIZE];
    char date[32];
    local_stamp("%Y-%m-%d %H:%M", date, sizeof(date), "unknown");
    Writer out = {.file = file, .ok = true};
    put(&out, "Committee: %s\nTopic: %s\nExported: %s\n\n", flat_copy(session->committee, committee, sizeof(committee)),
        flat_copy(session->agenda, agenda, sizeof(agenda)), date);

    const Log *log = &session->log;
    char at[sizeof(log->items[0].time)];
    char text[LOG_TEXT];
    int entries = clamp_int(log->count, 0, MAX_LOG);
    for (int i = 0; i < entries; i++) {
        put(&out, "%s  %s\n", flat_copy(log->items[i].time, at, sizeof(at)),
            flat_copy(log->items[i].text, text, sizeof(text)));
    }
    return finish_export(&out, path);
}

// one CSV field. RFC 4180: a field with a comma, a quote or a line break is quoted and its quotes doubled
static void put_csv(Writer *out, const char *text) {
    if (strpbrk(text, ",\"\r\n") == NULL) {
        put(out, "%s", text);
        return;
    }
    put(out, "\"");
    for (const char *quote = strchr(text, '"'); quote != NULL; quote = strchr(text, '"')) {
        put(out, "%.*s\"\"", (int)(quote - text), text);
        text = quote + 1;
    }
    put(out, "%s\"", text);
}

static const char *attendance_text(Attendance attendance) {
    switch (attendance) {
        case PRESENT: return "Present";
        case PRESENT_VOTING: return "Present and voting";
        default: return "Absent";
    }
}

// rounded to whole seconds, 0 for anything that is not a positive number
static int whole_seconds(double seconds) {
    if (!(seconds > 0.0)) return 0;
    if (seconds > 1e9) return 1000000000;
    return (int)(seconds + 0.5);
}

bool export_stats(const Session *session, const char *path) {
    if (session == NULL || path == NULL) return false;
    FILE *file = fopen(path, "w");
    if (file == NULL) return false;

    Writer out = {.file = file, .ok = true};
    // the byte order mark tells Excel the file is UTF-8, without it accented names come out garbled
    put(&out, "\xEF\xBB\xBF");
    put(&out, "Delegation,Attendance,GSL speeches,Caucus speeches,Speeches,Seconds spoken,Time spoken\n");
    const Roster *roster = &session->roster;
    int count = clamp_int(roster->count, 0, MAX_DELEGATES);
    for (int i = 0; i < count; i++) {
        const Delegate *delegate = &roster->items[i];
        // the ranges a session file holds, so the sum can't overflow
        int gsl = clamp_int(delegate->gsl_speeches, 0, MAX_COUNT);
        int caucus = clamp_int(delegate->caucus_speeches, 0, MAX_COUNT);
        int seconds = whole_seconds(delegate->spoken);
        char spoken[24];
        format_clock(seconds, spoken, sizeof(spoken));
        put_csv(&out, delegate->name);
        const char *attendance = roster->roll_taken ? attendance_text(delegate->attendance) : "Not taken";
        put(&out, ",%s,%d,%d,%d,%d,%s\n", attendance, gsl, caucus, gsl + caucus, seconds, spoken);
    }
    return finish_export(&out, path);
}

void stamped_name(const char *prefix, const char *extension, char *buffer, int size) {
    if (buffer == NULL || size <= 0) return;
    char stamp[24];
    local_stamp("%Y%m%d_%H%M", stamp, sizeof(stamp), "00000000_0000");
    int written = snprintf(buffer, (size_t)size, "%s_%s.%s", prefix != NULL ? prefix : "", stamp,
                           extension != NULL ? extension : "");
    // a cut name would write a file nobody asked for
    if (written < 0 || written >= size) buffer[0] = '\0';
}
