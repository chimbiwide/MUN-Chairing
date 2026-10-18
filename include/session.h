#ifndef SESSION_H
#define SESSION_H

#include <stdbool.h>
#include <text.h>
#include <clock.h>
#include <speakers.h>
#include <roster.h>
#include <motion.h>

#define MAX_LOG 1500
#define LOG_TEXT 120

// how a moderated caucus (and a round robin) spends its total time
typedef enum {
    MOD_CLOCK_SLOTS,        // every speech costs exactly its slot (HMUN R10), a remainder too short for a slot is dropped
    MOD_CLOCK_SPEECH,       // the total only runs while someone speaks, unused time stays in the caucus
    MOD_CLOCK_CONTINUOUS,   // the total runs from the first speech until the chair pauses it
} ModClock;

typedef enum {
    PHASE_DEBATE,
    PHASE_VOTING,           // debate closed, voting procedure
    PHASE_SUSPENDED,
    PHASE_ADJOURNED,
} Phase;

typedef struct {
    int gsl_time;           // seconds per speech on the general speakers' list
    int warn_time;          // seconds left when a clock turns amber
    ModClock mod_clock;
    bool auto_start;        // Next starts the next speaker's clock if the floor was running
    bool proposer_first;    // the proposer opens their own moderated caucus
    bool drop_motions;      // the other motions fall away once one passes
    bool auto_sort;         // keep motions in voting order as they are raised
    bool security_council;  // substantive votes need 9 yes and no veto
    bool quarter_quorum;    // quorum is a quarter of the members (Harvard rules) instead of a third
    bool dark;              // projector theme
} Settings;

typedef struct {
    bool held;              // a caucus has run in this session
    bool active;            // it is running now. after it ends the rest is kept for an extension
    Motion motion;          // what passed: type, total, speaking time, topic, proposer
    Clock total;
    Clock speaker;          // unused by an unmod
    SpeakerList speakers;   // unused by an unmod
} Caucus;

typedef struct {
    char time[8];           // local "14:05"
    char text[LOG_TEXT];
} LogEntry;

typedef struct {
    LogEntry items[MAX_LOG];
    int count;
} Log;

typedef struct {
    char committee[TITLE_SIZE];
    char agenda[TOPIC_SIZE];
    Settings settings;
    Phase phase;
    Roster roster;
    SpeakerList gsl;
    Clock gsl_clock;
    MotionList motions;
    Caucus caucus;
    unsigned alerts;        // clock states seen by the last update_floor
    Log log;
} Session;

// gsl_time 60, warn_time 10, mod_clock SLOTS, proposer_first, drop_motions and auto_sort on,
// auto_start, security_council, quarter_quorum and the dark theme off
void default_settings(Settings *settings);
// everything empty, default settings, phase DEBATE, gsl_clock limited to gsl_time, current speakers -1
void init_session(Session *session);
// printf style, stamped with the local time. when full the oldest entry goes
void add_log(Log *log, const char *format, ...) __attribute__((format(printf, 2, 3)));

const char *phase_name(Phase phase);            // "Debate", "Voting procedure", "Suspended", "Adjourned"
const char *mod_clock_name(ModClock mod_clock); // "Full slot per speech", "Runs while speaking", "Runs continuously"

#endif
