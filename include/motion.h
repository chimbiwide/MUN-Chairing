#ifndef MOTION_H
#define MOTION_H

#include <stdbool.h>
#include <text.h>
#include <roster.h>

#define MAX_MOTIONS 32

// listed from most to least disruptive, the default voting order (HMUN R36, MUNUC, MUNPrep)
typedef enum {
    MOTION_ADJOURN,         // adjourn the meeting
    MOTION_SUSPEND,         // suspend the meeting
    MOTION_EXTEND,          // extend the caucus that just ran: total
    MOTION_UNMOD,           // unmoderated caucus: total
    MOTION_CONSULT,         // consultation of the whole: total, speakers without a time limit
    MOTION_ROUND_ROBIN,     // every delegation present speaks once: speaking time
    MOTION_MOD,             // moderated caucus: total and speaking time
    MOTION_SPEAKING_TIME,   // set the general speakers' list speaking time: speaking time
    MOTION_INTRODUCE,       // introduce a draft resolution or working paper: topic names it
    MOTION_TABLE,           // table (postpone) debate on the topic
    MOTION_CLOSE_DEBATE,    // close debate and move into voting procedure
    MOTION_OTHER,           // anything else, the topic says what
    MOTION_TYPE_COUNT
} MotionType;

typedef enum {
    VOTE_SIMPLE,            // more than half
    VOTE_TWO_THIRDS,
} VoteRule;

typedef struct {
    MotionType type;
    char proposer[NAME_SIZE];
    char topic[TOPIC_SIZE];
    int total;              // seconds: caucus length or extension
    int speaker;            // seconds: speaking time of a mod, a round robin or the speakers' list
    int serial;             // order raised, breaks ties
} Motion;

typedef struct {
    Motion items[MAX_MOTIONS];
    int count;
    int serial;             // next serial to hand out
} MotionList;

// "Moderated caucus", "Unmoderated caucus", "Consultation of the whole", "Round robin",
// "Extend caucus", "Set speaking time", "Close debate", "Table debate", "Suspend the meeting",
// "Adjourn the meeting", "Introduce document", "Other motion"
const char *motion_name(MotionType type);
// "Mod", "Unmod", "Consult", "Round robin", "Extend", "Speaking time", "Close debate",
// "Table", "Suspend", "Adjourn", "Introduce", "Other"
const char *motion_label(MotionType type);
bool motion_uses_total(MotionType type);       // UNMOD, CONSULT, MOD, EXTEND
bool motion_uses_speaker(MotionType type);     // MOD, ROUND_ROBIN, SPEAKING_TIME
bool motion_uses_topic(MotionType type);       // MOD, UNMOD, CONSULT, ROUND_ROBIN, INTRODUCE, OTHER
bool motion_starts_caucus(MotionType type);    // UNMOD, CONSULT, MOD, ROUND_ROBIN
// CLOSE_DEBATE and TABLE need two thirds, everything else a simple majority
VoteRule motion_rule(MotionType type);
// yes votes needed when all of present vote: simple_majority or two_thirds
int votes_needed(VoteRule rule, int present);
// procedural votes count the votes cast, nobody abstains: simple yes > no, two thirds yes >= 2 * no.
// both need at least one yes, so a tie or 0-0 fails
bool motion_carries(VoteRule rule, int yes, int no);

// false with a short message when the type is missing what it needs:
// a total for UNMOD, CONSULT and EXTEND, a total and a speaking time no longer than it for MOD,
// a speaking time for ROUND_ROBIN and SPEAKING_TIME, a topic for INTRODUCE and OTHER
bool check_motion(const Motion *motion, char *error, int error_size);
// one line for the motion list, parts joined with " · " and empty parts left out:
// "Mod 9:45 (12 speakers) · Climate finance · Brazil", "Unmod 10 min · France",
// "Round robin 1:00 each · Water · Chad", "Speaking time 1:30 · Peru", "Extend 5 min · Fiji",
// "Close debate · Peru", and for OTHER just the topic and the proposer
void describe_motion(const Motion *motion, char *buffer, int size);

// Reads a motion typed on one line: "[kind] length [proposer] [topic]" with "by <proposer>" and
// "on <topic>" also understood.
//   kind:     mod|moderated|mc, unmod|unmoderated|umc|uc, consult|consultation|cotw, rr|robin|round,
//             ext|extend|extension, st|speaking, close|cloture, table|postpone, suspend|recess,
//             adjourn, intro|introduce|dr|wp, other ("round robin", "speaking time", "close debate",
//             "moderated caucus" and "unmoderated caucus" also work as two words). Without a kind it is a
//             MOD, and a proposer may come first ("france 9:45 trade")
//   length:   one token, parse_caucus for MOD, parse_speaking for ROUND_ROBIN/SPEAKING_TIME, and for
//             UNMOD/CONSULT/EXTEND one span where "7:30" is 7 min 30 s (otherwise parse_caucus's total);
//             kinds without a length (close, table, suspend, adjourn, intro, other) take none
//   proposer: the longest run of up to 5 words right after the length, or right after a "by", that names
//             a roster delegate: same_name with the delegate or with one of its country aliases ("usa" finds
//             "United States" on the roster), or a single word of 3+ letters that is a fold_name prefix of
//             exactly one delegate ("fra" finds "France"). The roster's spelling is stored
//   topic:    every other word as typed, in order (also words after the "by <proposer>" run), a leading "on" dropped
// "mod 9:45 brazil climate finance" and "9:45 climate finance by brazil" -> MOD 540/45, "Brazil", "climate finance".
// Fails with a short message when the kind is unknown, a needed length is missing or invalid, or
// check_motion refuses the result.
bool parse_motion_line(const char *text, const Roster *roster, Motion *out, char *error, int error_size);

void clear_motions(MotionList *list);
// copies motion in and stamps the next serial, -1 when full
int add_motion(MotionList *list, const Motion *motion);
bool remove_motion(MotionList *list, int index);
// swaps with the neighbour delta away (-1 up, +1 down), false at either end
bool move_motion(MotionList *list, int index, int delta);
// negative when a should be voted on before b: type order first, then within UNMOD, CONSULT,
// MOD and EXTEND the longer total first, then within MOD the shorter speaking time (more speakers)
// first, then serial
int compare_motions(const Motion *a, const Motion *b);
// stable sort by compare_motions
void sort_motions(MotionList *list);

#endif
