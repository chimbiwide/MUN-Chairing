#include <session.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void default_settings(Settings *settings) {
    *settings = (Settings){
        .gsl_time = 60,
        .warn_time = 10,
        .mod_clock = MOD_CLOCK_SLOTS,
        .auto_start = false,
        .proposer_first = true,
        .drop_motions = true,
        .auto_sort = true,
        .security_council = false,
        .quarter_quorum = false,
        .dark = false,
    };
}

void init_session(Session *session) {
    memset(session, 0, sizeof(*session));
    default_settings(&session->settings);
    session->phase = PHASE_DEBATE;
    session->gsl.current = -1;
    session->caucus.speakers.current = -1;
    set_clock(&session->gsl_clock, session->settings.gsl_time);
}

void add_log(Log *log, const char *format, ...) {
    if (log->count >= MAX_LOG) {
        memmove(&log->items[0], &log->items[1], (MAX_LOG - 1) * sizeof(LogEntry));
        log->count = MAX_LOG - 1;
    }
    LogEntry *entry = &log->items[log->count++];

    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (local == NULL || strftime(entry->time, sizeof(entry->time), "%H:%M", local) == 0) {
        copy_text(entry->time, sizeof(entry->time), "--:--");
    }

    // formatted in full first, so the cut to LOG_TEXT never splits a UTF-8 character
    char line[LOG_TEXT * 4];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    copy_text(entry->text, sizeof(entry->text), line);
    flatten_text(entry->text);
}

const char *phase_name(Phase phase) {
    switch (phase) {
        case PHASE_VOTING: return "Voting procedure";
        case PHASE_SUSPENDED: return "Suspended";
        case PHASE_ADJOURNED: return "Adjourned";
        default: return "Debate";
    }
}

const char *mod_clock_name(ModClock mod_clock) {
    switch (mod_clock) {
        case MOD_CLOCK_SPEECH: return "Runs while speaking";
        case MOD_CLOCK_CONTINUOUS: return "Runs continuously";
        default: return "Full slot per speech";
    }
}
