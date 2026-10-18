#ifndef CHAIR_H
#define CHAIR_H

#include <stdbool.h>
#include <session.h>

// The chair's actions on a Session. now is GetTime() in the app and any increasing number in tests.
// Every action that changes the record of the committee writes a line to session->log.
//
// "The floor" is the moderated caucus, consultation or round robin while one is active
// (caucus.active and the type has speakers), otherwise the general speakers' list (GSL).
// An active unmod has no speakers: the floor actions below act on its total clock or do nothing.
//
// Calling a speaker: the next pending entry of the floor list becomes current and its clock is
// set to its limit: the entry's own limit, else settings.gsl_time on the GSL, else the caucus
// speaking time (0, counting up, for a consultation). In a mod or round robin:
//   SLOTS       a speaker is called only while a full limit is left in the caucus total
//               (clock_left >= limit - 0.5); a remainder shorter than a slot is dropped
//   SPEECH, CONTINUOUS  the limit is cut to the whole seconds left in the total (ceil of clock_left)
//               when that is less, and nobody is called once clock_left <= 0
//
// The caucus total and the speaker clock in a mod or round robin, by settings.mod_clock:
//   SLOTS       the total runs exactly while the speaker clock runs, and crossing out a speaker
//               charges the total limit - spoke (negative refunds overtime), so every finished
//               speech has cost exactly its limit
//   SPEECH      the total runs exactly while the speaker clock runs
//   CONTINUOUS  starting any speaker starts the total too, pausing a speaker leaves it running;
//               toggle_caucus_clock pauses and resumes it
// A consultation behaves like CONTINUOUS. An unmod has only its total.
// Clocks never stop on their own: an expired clock keeps counting into overtime.
//
// Crossing out a speaker stores the seconds they spoke. When that is at least 1 s the
// delegate of the same name (if on the roster) gets gsl_speeches or caucus_speeches + 1 and the
// seconds added to spoken, and the log gets "Brazil spoke 0:43 (GSL)" or "(mod)" / "(consult)" / "(round robin)".

// the speakers' list of the floor
SpeakerList *floor_list(Session *session);
// the speaker clock of the floor, NULL during an active unmod
Clock *floor_clock(Session *session);
// MOD, CONSULT and ROUND_ROBIN
bool caucus_has_speakers(MotionType type);

// Space. With a current speaker: toggles their clock (and the caucus total as above).
// Without one: calls the next speaker and starts them. During an unmod: toggles its total.
// false when nothing could start: nobody pending, the caucus total used up, or the phase is not DEBATE
bool toggle_floor(Session *session, double now);
// Next. Crosses out the current speaker (if any) and calls the next pending one, whose clock
// starts when settings.auto_start and the floor clock was running, otherwise stays paused at full time.
// With nobody to call the floor is left with no current speaker, its clock reset to the list's usual time
// (paused), and a mod/round robin total paused except CONTINUOUS. false when there was neither a current
// speaker nor anyone pending
bool next_speaker(Session *session, double now);
// the current speaker's clock back to their full limit, paused. In SLOTS and SPEECH the caucus total gets
// back the seconds that clock had run and is paused with it; CONTINUOUS leaves the total alone.
// During an unmod it resets the caucus total to its limit, paused
void restart_speaker(Session *session, double now);

// GSL yields. Each is false when the GSL has no current speaker or a caucus is active.
// crosses out with the note "yielded to the chair" and calls the next speaker like next_speaker
bool yield_to_chair(Session *session, double now);
// notes "questions" on the current speaker and logs it; the clock is left alone
bool yield_to_questions(Session *session, double now);
// the current speaker is crossed out (note "yielded to <name>") and name is inserted right after
// them as the new current speaker with the note "yielded by <old>" and, as limit, the whole seconds
// left on the clock (ceil, at least 1). Their clock keeps running if it was running.
// false also when name is empty, already waiting on the GSL, or no time is left
bool yield_to_delegate(Session *session, const char *name, double now);

// right of reply: name is inserted as the next speaker of the GSL (right after the current one,
// or first among the pending) with a 30 s limit and the note "right of reply", and it is logged.
// false when name is empty or already waiting on the GSL
bool grant_reply(Session *session, const char *name);
// add_speaker on list, returning what it returns. no log line
int add_to_list(Session *session, SpeakerList *list, const char *name);
// remove_speaker; removing the current speaker of the floor list also resets the floor clock
// (paused, full gsl or caucus time, and the mod total paused unless CONTINUOUS)
bool remove_from_list(Session *session, SpeakerList *list, int index, double now);

// check_motion, then add_motion and sort_motions when settings.auto_sort.
// returns the index the motion landed at, -1 when invalid (message in error) or the list is full
int propose_motion(Session *session, const Motion *motion, char *error, int error_size);
// Applies motions->items[index] and removes it. yes and no are the counted votes, -1 when not counted.
//   UNMOD, CONSULT, MOD, ROUND_ROBIN   start_caucus
//   EXTEND                             extend_caucus (false and nothing changes when there is nothing to extend)
//   SPEAKING_TIME                      settings.gsl_time = speaker, and the GSL clock limit too when it has not run yet
//   CLOSE_DEBATE                       phase VOTING
//   SUSPEND / ADJOURN                  phase SUSPENDED / ADJOURNED
//   TABLE, INTRODUCE, OTHER            logged only
// then, when settings.drop_motions, clears the other pending motions and logs how many were dropped.
// log: "Passed: <describe_motion> (14-6)" with the count only when given
bool pass_motion(Session *session, int index, int yes, int no, double now);
// removes it and logs "Failed: <describe_motion> (6-14)"
bool fail_motion(Session *session, int index, int yes, int no);
// removes it and logs "Withdrawn: <describe_motion>"
bool withdraw_motion(Session *session, int index);

// Ends any active caucus, then sets up a new one from motion and logs "<Mod> started: ...".
// Clocks start paused: the total at motion->total (a round robin's total is its speaking time times the
// number of delegates listed), the speaker clock at the speaking time. A mod with a proposer on the roster
// and settings.proposer_first lists the proposer first. A round robin lists every delegate count_present
// counts, in roster order starting from the proposer when they are among them. The GSL clock is paused.
void start_caucus(Session *session, const Motion *motion, double now);
// crosses out the current caucus speaker (stats as usual), pauses its clocks, active = false,
// logs "<Mod> ended after 11 speeches". nothing when no caucus is active
void end_caucus(Session *session, double now);
// adds seconds to the caucus total so that max(0, left) + seconds remain. reopens the last caucus when it
// already ended (held but not active). logs "Caucus extended by 5 min". false when no caucus was ever held
bool extend_caucus(Session *session, int seconds, double now);
// pauses or resumes the total of an active caucus on its own; a running speaker clock pauses with it
void toggle_caucus_clock(Session *session, double now);

// logs "Phase: Voting procedure"; SUSPENDED, ADJOURNED and VOTING pause every clock and end an active caucus
void set_phase(Session *session, Phase phase, double now);
// sets roll_taken too
void set_attendance(Session *session, int index, Attendance attendance);
// logs "Roll call: 24 present (9 present and voting) of 26"
void finish_roll_call(Session *session);
// substantive vote: yes > no (abstentions don't count); with settings.security_council
// also yes >= 9 and no veto
bool substantive_passes(const Session *session, Tally tally);
// logs "Vote on <what>: passed 15-3-2" (yes-no-abstain) and clears the ballots
void record_vote(Session *session, const char *what, Tally tally, bool passed);

// what update_floor reports, each once when it becomes true
#define ALERT_WARNING 1     // the floor clock (or an unmod's total) entered its last warn_time seconds
#define ALERT_SPEAKER 2     // the floor speaker clock ran out
#define ALERT_CAUCUS 4      // the active caucus total ran out
// call every frame. returns the ALERT_ bits that are true now but were not on the previous call
int update_floor(Session *session, double now);

#define UNDO_DEPTH 16
#define UNDO_LABEL 40

// snapshots of the whole session for Ctrl+Z
typedef struct {
    Session states[UNDO_DEPTH];
    char labels[UNDO_DEPTH][UNDO_LABEL];
    int count;
    int newest;
} History;

// pushes a copy of session before an action, label says what the action is ("next speaker").
// the oldest snapshot goes when full
void remember(History *history, const Session *session, const char *label);
// drops the newest snapshot (the action it was taken for did nothing)
void forget_last(History *history);
// restores the newest snapshot, false when there is none
bool undo(History *history, Session *session);
// label of the snapshot undo would restore, NULL when empty
const char *undo_label(const History *history);
void forget_history(History *history);

#endif
