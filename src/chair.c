#include <chair.h>
#include <duration.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// seconds a right of reply gets
#define REPLY_SECONDS 30
// room to build a log line or a note in before copy_text fits it to its field
#define LINE_SIZE 512
// describe_motion's longest line: its kind, a topic and a proposer
#define DESCRIBE_SIZE (TOPIC_SIZE + NAME_SIZE + 128)
// the note that marks time given by another delegate, which can't be yielded on
#define YIELDED_BY "yielded by "

static void log_line(Session *session, const char *format, ...) __attribute__((format(printf, 2, 3)));

// one line in the log, cut at a character boundary to fit
static void log_line(Session *session, const char *format, ...) {
    char line[LINE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    char fitted[LOG_TEXT];
    copy_text(fitted, sizeof(fitted), line);
    add_log(&session->log, "%s", fitted);
}

// whole seconds of a clock reading, rounded and kept inside int
static int round_seconds(double seconds) {
    if (seconds >= (double)INT_MAX) return INT_MAX;
    if (seconds <= (double)INT_MIN) return INT_MIN;
    return (int)lround(seconds);
}

// counts kept inside their arrays in case a caller wrote bad ones
static int roster_size(const Roster *roster) {
    if (roster->count < 0) return 0;
    return roster->count < MAX_DELEGATES ? roster->count : MAX_DELEGATES;
}

static int motion_count(const MotionList *list) {
    if (list->count < 0) return 0;
    return list->count < MAX_MOTIONS ? list->count : MAX_MOTIONS;
}

// index of the speaker holding the floor of list, -1 when nobody
static int holder(const SpeakerList *list) {
    return list->current >= 0 && list->current < list->count ? list->current : -1;
}

bool caucus_has_speakers(MotionType type) {
    return type == MOTION_MOD || type == MOTION_CONSULT || type == MOTION_ROUND_ROBIN;
}

// a mod, consultation or round robin is running
static bool caucus_listed(const Session *session) {
    return session->caucus.active && caucus_has_speakers(session->caucus.motion.type);
}

// an unmod is running: the floor has no speakers
static bool unmod_active(const Session *session) {
    return session->caucus.active && !caucus_has_speakers(session->caucus.motion.type);
}

SpeakerList *floor_list(Session *session) {
    return caucus_listed(session) ? &session->caucus.speakers : &session->gsl;
}

Clock *floor_clock(Session *session) {
    if (caucus_listed(session)) return &session->caucus.speaker;
    if (session->caucus.active) return NULL;
    return &session->gsl_clock;
}

// the caucus total runs on between speakers: CONTINUOUS, and every consultation
static bool runs_continuously(const Session *session) {
    return session->caucus.motion.type == MOTION_CONSULT || session->settings.mod_clock == MOD_CLOCK_CONTINUOUS;
}

// every speech of the caucus costs exactly its slot
static bool costs_slots(const Session *session) {
    return session->caucus.motion.type != MOTION_CONSULT && session->settings.mod_clock == MOD_CLOCK_SLOTS;
}

// seconds a speech on list gets when its entry has no limit of its own, 0 to count up
static int usual_time(const Session *session, const SpeakerList *list) {
    if (list == &session->gsl) return session->settings.gsl_time;
    const Motion *motion = &session->caucus.motion;
    if (motion->type == MOTION_MOD || motion->type == MOTION_ROUND_ROBIN) return motion->speaker;
    return 0;
}

// the clock the speaker holding the floor of list runs on, NULL for a list of neither
static Clock *list_clock(Session *session, const SpeakerList *list) {
    if (list == &session->gsl) return &session->gsl_clock;
    if (list == &session->caucus.speakers) return &session->caucus.speaker;
    return NULL;
}

// nobody holds the floor of list: its clock back to the usual time, paused, and a caucus total
// that runs only with its speakers paused too
static void reset_list(Session *session, const SpeakerList *list, double now) {
    Clock *clock = list_clock(session, list);
    if (clock == NULL) return;
    set_clock(clock, usual_time(session, list));
    if (list == &session->caucus.speakers && !runs_continuously(session)) pause_clock(&session->caucus.total, now);
}

// starts the floor speaker clock. a caucus total starts with it
static void start_speaker(Session *session, double now) {
    Clock *clock = floor_clock(session);
    if (clock == NULL) return;
    start_clock(clock, now);
    if (caucus_listed(session)) start_clock(&session->caucus.total, now);
}

// pauses the floor speaker clock. the total pauses with it unless it runs continuously
static void pause_speaker(Session *session, double now) {
    Clock *clock = floor_clock(session);
    if (clock == NULL) return;
    pause_clock(clock, now);
    if (caucus_listed(session) && !runs_continuously(session)) pause_clock(&session->caucus.total, now);
}

// calling a speaker: the next pending entry of the floor becomes current with the floor clock set to
// its limit, paused. false, changing nothing, when nobody is pending or the caucus has no room for them
static bool call_next(Session *session, double now) {
    SpeakerList *list = floor_list(session);
    Clock *clock = floor_clock(session);
    int next = next_pending(list);
    if (clock == NULL || next < 0) return false;

    double limit = list->items[next].limit > 0 ? list->items[next].limit : usual_time(session, list);
    if (caucus_listed(session)) {
        double left = clock_left(&session->caucus.total, now);
        if (left <= 0.0) return false;
        // a full slot must be left. otherwise the speech shrinks to the whole seconds that are
        if (costs_slots(session)) {
            if (left < limit - 0.5) return false;
        }
        else if (limit > 0.0 && ceil(left) < limit) limit = ceil(left);
    }
    list->current = next;
    set_clock(clock, limit);
    return true;
}

// a SLOTS total has spent whole slots once a speech is settled, so a fraction left by charging or refunding
// is float rounding: dropped, or the face shows 3:49 for 3:48 and a used-up caucus never runs out
static void settle_slots(Clock *total) {
    double whole = round(total->banked);
    if (fabs(total->banked - whole) < 1e-6) total->banked = whole;
}

// how the log names the list a speech was given on
static const char *speech_kind(const Session *session, const SpeakerList *list) {
    if (list == &session->gsl) return "GSL";
    switch (session->caucus.motion.type) {
        case MOTION_CONSULT: return "consult";
        case MOTION_ROUND_ROBIN: return "round robin";
        default: return "mod";
    }
}

// crosses out the speaker holding the floor and stores what they spoke. a speech of 1 s or more counts
// for their delegate and is logged. in SLOTS it costs the caucus exactly its limit, a skip nothing
static bool finish(Session *session, double now) {
    SpeakerList *list = floor_list(session);
    Clock *clock = floor_clock(session);
    int index = holder(list);
    if (clock == NULL || index < 0) return false;

    double spoke = clock_elapsed(clock, now);
    pause_clock(clock, now);
    set_done(list, index, true, spoke);
    list->current = -1;

    bool caucus = list != &session->gsl;
    bool spoken = spoke >= 1.0;
    if (spoken) {
        const char *name = list->items[index].name;
        int delegate = find_delegate(&session->roster, name);
        if (delegate >= 0) {
            Delegate *speaker = &session->roster.items[delegate];
            if (caucus) speaker->caucus_speeches++;
            else speaker->gsl_speeches++;
            speaker->spoken += spoke;
        }
        char length[32];
        format_clock(round_seconds(spoke), length, sizeof(length));
        log_line(session, "%s spoke %s (%s)", name, length, speech_kind(session, list));
    }
    if (caucus) {
        Clock *total = &session->caucus.total;
        if (!runs_continuously(session)) pause_clock(total, now);
        if (costs_slots(session)) {
            charge_clock(total, (spoken ? clock->limit : 0.0) - spoke);
            settle_slots(total);
        }
    }
    return true;
}

// after a crossing out: the next speaker is called, and started when auto_start and the floor ran.
// with nobody to call the floor is reset
static void move_on(Session *session, bool was_running, double now) {
    if (call_next(session, now)) {
        if (was_running && session->settings.auto_start && session->phase == PHASE_DEBATE) start_speaker(session, now);
        return;
    }
    reset_list(session, floor_list(session), now);
}

bool toggle_floor(Session *session, double now) {
    if (session->phase != PHASE_DEBATE) return false;
    if (unmod_active(session)) {
        toggle_clock(&session->caucus.total, now);
        return true;
    }
    Clock *clock = floor_clock(session);
    if (clock == NULL) return false;
    if (holder(floor_list(session)) >= 0) {
        if (clock->running) pause_speaker(session, now);
        else start_speaker(session, now);
        return true;
    }
    if (!call_next(session, now)) return false;
    start_speaker(session, now);
    return true;
}

bool next_speaker(Session *session, double now) {
    SpeakerList *list = floor_list(session);
    Clock *clock = floor_clock(session);
    if (clock == NULL) return false;
    if (holder(list) < 0 && next_pending(list) < 0) return false;
    bool was_running = clock->running;
    finish(session, now);
    move_on(session, was_running, now);
    return true;
}

void restart_speaker(Session *session, double now) {
    Caucus *caucus = &session->caucus;
    if (unmod_active(session)) {
        reset_clock(&caucus->total);
        return;
    }
    Clock *clock = floor_clock(session);
    if (clock == NULL || holder(floor_list(session)) < 0) return;
    // the total ran exactly with this speech, so it gets those seconds back
    if (caucus_listed(session) && !runs_continuously(session)) {
        double ran = clock_elapsed(clock, now);
        pause_clock(&caucus->total, now);
        charge_clock(&caucus->total, -ran);
        if (costs_slots(session)) settle_slots(&caucus->total);
    }
    reset_clock(clock);
}

// a speaker holds the floor of the GSL and no caucus runs
static bool gsl_speaking(const Session *session) {
    return !session->caucus.active && holder(&session->gsl) >= 0;
}

// the speaker holds time another delegate yielded, which can't be passed on
static bool yielded_time(const Speaker *speaker) {
    return strncmp(speaker->note, YIELDED_BY, strlen(YIELDED_BY)) == 0;
}

bool yield_to_chair(Session *session, double now) {
    if (!gsl_speaking(session)) return false;
    Speaker *speaker = &session->gsl.items[session->gsl.current];
    char name[NAME_SIZE];
    copy_text(name, sizeof(name), speaker->name);
    copy_text(speaker->note, NOTE_SIZE, "yielded to the chair");
    bool was_running = session->gsl_clock.running;
    finish(session, now);
    log_line(session, "%s yielded to the chair", name);
    move_on(session, was_running, now);
    return true;
}

bool yield_to_questions(Session *session, double now) {
    (void)now;
    if (!gsl_speaking(session)) return false;
    Speaker *speaker = &session->gsl.items[session->gsl.current];
    // yielded time keeps saying so, or it could be yielded on
    if (!yielded_time(speaker)) copy_text(speaker->note, NOTE_SIZE, "questions");
    else if (strstr(speaker->note, "questions") == NULL) {
        char text[LINE_SIZE];
        snprintf(text, sizeof(text), "%s · questions", speaker->note);
        copy_text(speaker->note, NOTE_SIZE, text);
    }
    log_line(session, "%s yielded to questions", speaker->name);
    return true;
}

bool yield_to_delegate(Session *session, const char *name, double now) {
    if (!gsl_speaking(session)) return false;
    SpeakerList *gsl = &session->gsl;
    Clock *clock = &session->gsl_clock;
    int giver = gsl->current;
    char taker[NAME_SIZE];
    copy_text(taker, sizeof(taker), name);
    trim_text(taker);
    double left = clock_left(clock, now);
    if (taker[0] == '\0' || left <= 0.0 || yielded_time(&gsl->items[giver]) || is_waiting(gsl, taker)) return false;

    // inserted first: a full list refuses and nothing has changed
    int at = insert_speaker(gsl, giver + 1, taker);
    if (at < 0) return false;
    double whole = ceil(left);
    int limit = whole >= (double)INT_MAX ? INT_MAX : whole < 1.0 ? 1 : (int)whole;
    bool was_running = clock->running;
    char from[NAME_SIZE];
    copy_text(from, sizeof(from), gsl->items[giver].name);

    char text[LINE_SIZE];
    snprintf(text, sizeof(text), "yielded to %s", gsl->items[at].name);
    copy_text(gsl->items[giver].note, NOTE_SIZE, text);
    finish(session, now);

    Speaker *speaker = &gsl->items[at];
    speaker->limit = limit;
    snprintf(text, sizeof(text), YIELDED_BY "%s", from);
    copy_text(speaker->note, NOTE_SIZE, text);
    gsl->current = at;
    set_clock(clock, limit);
    if (was_running) start_clock(clock, now);

    char length[32];
    format_clock(limit, length, sizeof(length));
    log_line(session, "%s yielded %s to %s", from, length, speaker->name);
    return true;
}

bool grant_reply(Session *session, const char *name) {
    SpeakerList *gsl = &session->gsl;
    int at = holder(gsl) >= 0 ? gsl->current + 1 : next_pending(gsl);
    if (at < 0) at = gsl->count;
    int index = insert_speaker(gsl, at, name);
    if (index < 0) return false;
    Speaker *speaker = &gsl->items[index];
    speaker->limit = REPLY_SECONDS;
    copy_text(speaker->note, NOTE_SIZE, "right of reply");
    log_line(session, "Right of reply granted to %s", speaker->name);
    return true;
}

int add_to_list(Session *session, SpeakerList *list, const char *name) {
    (void)session;
    if (list == NULL) return SPEAKER_FULL;
    return add_speaker(list, name);
}

bool remove_from_list(Session *session, SpeakerList *list, int index, double now) {
    if (list == NULL || index < 0 || index >= list->count) return false;
    bool holding = index == holder(list);
    // in SLOTS a speech taken off the list is no speech: like a skip it costs no slot, so the total
    // gets back what ran with it
    bool refund = holding && list == &session->caucus.speakers && caucus_listed(session) && costs_slots(session);
    double ran = refund ? clock_elapsed(&session->caucus.speaker, now) : 0.0;
    if (!remove_speaker(list, index)) return false;
    if (holding) reset_list(session, list, now);
    if (refund) {
        charge_clock(&session->caucus.total, -ran);
        settle_slots(&session->caucus.total);
    }
    return true;
}

// " (14-6)" when the votes were counted, nothing otherwise
static void vote_count(int yes, int no, char *buffer, int size) {
    if (yes >= 0 && no >= 0) snprintf(buffer, (size_t)size, " (%d-%d)", yes, no);
    else copy_text(buffer, size, "");
}

// where the motion with serial sits, fallback when it isn't there
static int find_serial(const MotionList *list, int serial, int fallback) {
    int count = motion_count(list);
    for (int i = 0; i < count; i++) {
        if (list->items[i].serial == serial) return i;
    }
    return fallback;
}

int propose_motion(Session *session, const Motion *motion, char *error, int error_size) {
    if (!check_motion(motion, error, error_size)) return -1;
    MotionList *motions = &session->motions;
    int index = add_motion(motions, motion);
    if (index < 0) {
        copy_text(error, error_size, "The motion list is full");
        return -1;
    }
    if (session->settings.auto_sort) {
        int serial = motions->items[index].serial;
        sort_motions(motions);
        index = find_serial(motions, serial, index);
    }
    char what[DESCRIBE_SIZE];
    describe_motion(&motions->items[index], what, sizeof(what));
    log_line(session, "Raised: %s", what);
    return index;
}

// there is a caucus to give seconds more
static bool can_extend(const Session *session, int seconds) {
    return session->caucus.held && seconds > 0;
}

// the GSL speaking time, and the GSL clock's limit when it hasn't run and isn't a speech of its own length
static void set_speaking_time(Session *session, int seconds, double now) {
    if (seconds <= 0) return;
    session->settings.gsl_time = seconds;
    Clock *clock = &session->gsl_clock;
    int current = holder(&session->gsl);
    bool own_limit = current >= 0 && session->gsl.items[current].limit > 0;
    if (!clock->running && clock_elapsed(clock, now) <= 0.0 && !own_limit) set_clock(clock, seconds);
}

bool pass_motion(Session *session, int index, int yes, int no, double now) {
    MotionList *motions = &session->motions;
    if (index < 0 || index >= motion_count(motions)) return false;
    // a copy: the list shifts and clears below
    Motion motion = motions->items[index];
    if (motion.type == MOTION_EXTEND && !can_extend(session, motion.total)) return false;

    remove_motion(motions, index);
    char what[DESCRIBE_SIZE];
    char count[32];
    describe_motion(&motion, what, sizeof(what));
    vote_count(yes, no, count, sizeof(count));
    log_line(session, "Passed: %s%s", what, count);

    switch (motion.type) {
        case MOTION_UNMOD:
        case MOTION_CONSULT:
        case MOTION_MOD:
        case MOTION_ROUND_ROBIN:
            start_caucus(session, &motion, now);
            break;
        case MOTION_EXTEND:
            extend_caucus(session, motion.total, now);
            break;
        case MOTION_SPEAKING_TIME:
            set_speaking_time(session, motion.speaker, now);
            break;
        case MOTION_CLOSE_DEBATE:
            set_phase(session, PHASE_VOTING, now);
            break;
        case MOTION_SUSPEND:
            set_phase(session, PHASE_SUSPENDED, now);
            break;
        case MOTION_ADJOURN:
            set_phase(session, PHASE_ADJOURNED, now);
            break;
        default:
            break;
    }

    int dropped = motion_count(motions);
    if (session->settings.drop_motions && dropped > 0) {
        clear_motions(motions);
        log_line(session, "Dropped %d other %s", dropped, dropped == 1 ? "motion" : "motions");
    }
    return true;
}

bool fail_motion(Session *session, int index, int yes, int no) {
    MotionList *motions = &session->motions;
    if (index < 0 || index >= motion_count(motions)) return false;
    char what[DESCRIBE_SIZE];
    char count[32];
    describe_motion(&motions->items[index], what, sizeof(what));
    vote_count(yes, no, count, sizeof(count));
    remove_motion(motions, index);
    log_line(session, "Failed: %s%s", what, count);
    return true;
}

bool withdraw_motion(Session *session, int index) {
    MotionList *motions = &session->motions;
    if (index < 0 || index >= motion_count(motions)) return false;
    char what[DESCRIBE_SIZE];
    describe_motion(&motions->items[index], what, sizeof(what));
    remove_motion(motions, index);
    log_line(session, "Withdrawn: %s", what);
    return true;
}

// whether count_present counts the delegate at index
static bool counted(const Roster *roster, int index) {
    Attendance attendance = roster->items[index].attendance;
    return !roster->roll_taken || attendance == PRESENT || attendance == PRESENT_VOTING;
}

// a round robin's order: every delegate count_present counts, in roster order from the proposer
// when they are one of them
static void list_round(Session *session, int proposer) {
    const Roster *roster = &session->roster;
    int count = roster_size(roster);
    int start = proposer >= 0 && proposer < count && counted(roster, proposer) ? proposer : 0;
    for (int i = 0; i < count; i++) {
        int index = (start + i) % count;
        if (counted(roster, index)) add_speaker(&session->caucus.speakers, roster->items[index].name);
    }
}

// crossed out after speaking 1 s or more
static int count_speeches(const SpeakerList *list) {
    int count = 0;
    for (int i = 0; i < list->count && i < MAX_SPEAKERS; i++) {
        if (list->items[i].done && list->items[i].spoke >= 1.0) count++;
    }
    return count;
}

void start_caucus(Session *session, const Motion *motion, double now) {
    if (motion == NULL || !motion_starts_caucus(motion->type)) return;
    // a copy: motion may point into the session
    Motion passed = *motion;
    end_caucus(session, now);

    Caucus *caucus = &session->caucus;
    caucus->motion = passed;
    caucus->held = true;
    caucus->active = true;
    clear_speakers(&caucus->speakers);

    const Roster *roster = &session->roster;
    int proposer = find_delegate(roster, passed.proposer);
    if (passed.type == MOTION_MOD && session->settings.proposer_first && proposer >= 0) {
        add_speaker(&caucus->speakers, roster->items[proposer].name);
    }
    double total = passed.total;
    if (passed.type == MOTION_ROUND_ROBIN) {
        list_round(session, proposer);
        total = (double)passed.speaker * caucus->speakers.count;
    }
    set_clock(&caucus->total, total);
    set_clock(&caucus->speaker, usual_time(session, &caucus->speakers));
    pause_clock(&session->gsl_clock, now);

    char what[DESCRIBE_SIZE];
    describe_motion(&passed, what, sizeof(what));
    log_line(session, "%s started: %s", motion_label(passed.type), what);
}

void end_caucus(Session *session, double now) {
    Caucus *caucus = &session->caucus;
    if (!caucus->active) return;
    // only a caucus with speakers has one to cross out: during an unmod the floor list is the GSL
    bool listed = caucus_has_speakers(caucus->motion.type);
    if (listed) finish(session, now);
    pause_clock(&caucus->total, now);
    pause_clock(&caucus->speaker, now);
    caucus->active = false;

    const char *label = motion_label(caucus->motion.type);
    if (!listed) {
        log_line(session, "%s ended", label);
        return;
    }
    int speeches = count_speeches(&caucus->speakers);
    log_line(session, "%s ended after %d %s", label, speeches, speeches == 1 ? "speech" : "speeches");
}

bool extend_caucus(Session *session, int seconds, double now) {
    if (!can_extend(session, seconds)) return false;
    Caucus *caucus = &session->caucus;
    Clock *total = &caucus->total;
    // overtime is forgiven, so exactly seconds remain then. set directly: extend_clock skips a limit of 0
    double left = clock_left(total, now);
    // but not in SLOTS the overtime of the speech on the floor: crossing them out refunds it anyway
    if (caucus_listed(session) && costs_slots(session) && holder(&caucus->speakers) >= 0) {
        double over = clock_elapsed(&caucus->speaker, now) - caucus->speaker.limit;
        if (over > 0.0) left += over;
    }
    total->limit += seconds + (left < 0.0 ? -left : 0.0);
    if (!caucus->active) {
        // the caucus that ended comes back with nobody on its floor
        caucus->active = true;
        caucus->speakers.current = -1;
        set_clock(&caucus->speaker, usual_time(session, &caucus->speakers));
        pause_clock(&session->gsl_clock, now);
    }
    char span[32];
    format_span(seconds, span, sizeof(span));
    log_line(session, "Caucus extended by %s", span);
    return true;
}

void toggle_caucus_clock(Session *session, double now) {
    Caucus *caucus = &session->caucus;
    if (!caucus->active) return;
    if (caucus->total.running) {
        pause_clock(&caucus->total, now);
        pause_clock(&caucus->speaker, now);
    }
    else if (session->phase == PHASE_DEBATE) start_clock(&caucus->total, now);
}

void set_phase(Session *session, Phase phase, double now) {
    if ((int)phase < (int)PHASE_DEBATE || (int)phase > (int)PHASE_ADJOURNED) return;
    if (phase != PHASE_DEBATE) {
        end_caucus(session, now);
        pause_clock(&session->gsl_clock, now);
        pause_clock(&session->caucus.total, now);
        pause_clock(&session->caucus.speaker, now);
    }
    if (phase == session->phase) return;
    session->phase = phase;
    log_line(session, "Phase: %s", phase_name(phase));
}

void set_attendance(Session *session, int index, Attendance attendance) {
    Roster *roster = &session->roster;
    int value = (int)attendance;
    if (index < 0 || index >= roster_size(roster) || value < ABSENT || value > PRESENT_VOTING) return;
    roster->items[index].attendance = attendance;
    roster->roll_taken = true;
}

void finish_roll_call(Session *session) {
    Roster *roster = &session->roster;
    // a finished roll call is a taken one, also when nobody was marked
    roster->roll_taken = true;
    int voting = count_attendance(roster, PRESENT_VOTING);
    int present = count_attendance(roster, PRESENT) + voting;
    log_line(session, "Roll call: %d present (%d present and voting) of %d", present, voting, roster_size(roster));
}

bool substantive_passes(const Session *session, Tally tally) {
    if (tally.yes <= tally.no) return false;
    if (session->settings.security_council && (tally.yes < 9 || tally.vetoed)) return false;
    return true;
}

void record_vote(Session *session, const char *what, Tally tally, bool passed) {
    const char *result = passed ? "passed" : "failed";
    if (what != NULL && what[0] != '\0') {
        log_line(session, "Vote on %s: %s %d-%d-%d", what, result, tally.yes, tally.no, tally.abstain);
    }
    else log_line(session, "Vote: %s %d-%d-%d", result, tally.yes, tally.no, tally.abstain);
    clear_ballots(&session->roster);
}

// a clock that ran since it was set. one set to a short limit has not entered its last seconds yet
static bool has_run(const Clock *clock, double now) {
    return clock->running || clock_elapsed(clock, now) > 0.0;
}

int update_floor(Session *session, double now) {
    double warn_at = session->settings.warn_time;
    unsigned bits = 0;
    if (unmod_active(session)) {
        const Clock *total = &session->caucus.total;
        if (has_run(total, now) && clock_warning(total, now, warn_at)) bits |= ALERT_WARNING;
        if (clock_expired(total, now)) bits |= ALERT_CAUCUS;
    }
    else {
        const Clock *clock = floor_clock(session);
        if (clock != NULL && holder(floor_list(session)) >= 0) {
            if (has_run(clock, now) && clock_warning(clock, now, warn_at)) bits |= ALERT_WARNING;
            if (clock_expired(clock, now)) bits |= ALERT_SPEAKER;
        }
        if (caucus_listed(session) && clock_expired(&session->caucus.total, now)) bits |= ALERT_CAUCUS;
    }
    unsigned events = bits & ~session->alerts;
    session->alerts = bits;
    return (int)events;
}

// count and newest kept inside the ring in case a caller wrote bad ones
static int history_size(const History *history) {
    if (history->count < 0) return 0;
    return history->count < UNDO_DEPTH ? history->count : UNDO_DEPTH;
}

static int newest_slot(const History *history) {
    return history->newest >= 0 && history->newest < UNDO_DEPTH ? history->newest : 0;
}

void remember(History *history, const Session *session, const char *label) {
    if (history == NULL || session == NULL) return;
    int count = history_size(history);
    // when full the next slot is the oldest
    int slot = count > 0 ? (newest_slot(history) + 1) % UNDO_DEPTH : 0;
    memcpy(&history->states[slot], session, sizeof(Session));
    copy_text(history->labels[slot], UNDO_LABEL, label);
    history->newest = slot;
    history->count = count < UNDO_DEPTH ? count + 1 : UNDO_DEPTH;
}

void forget_last(History *history) {
    if (history == NULL) return;
    int count = history_size(history);
    if (count == 0) return;
    history->count = count - 1;
    history->newest = (newest_slot(history) + UNDO_DEPTH - 1) % UNDO_DEPTH;
}

bool undo(History *history, Session *session) {
    if (history == NULL || session == NULL || history_size(history) == 0) return false;
    memcpy(session, &history->states[newest_slot(history)], sizeof(Session));
    forget_last(history);
    return true;
}

const char *undo_label(const History *history) {
    if (history == NULL || history_size(history) == 0) return NULL;
    return history->labels[newest_slot(history)];
}

void forget_history(History *history) {
    if (history == NULL) return;
    history->count = 0;
    history->newest = 0;
}
