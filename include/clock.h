#ifndef CLOCK_H
#define CLOCK_H

#include <stdbool.h>

// a pausable countdown. now is GetTime() in the app and any increasing number in tests
typedef struct {
    double limit;     // seconds allowed, 0 means no limit and the clock only counts up
    double banked;    // seconds run before the current start
    double started;   // now at the current start, meaningful while running
    bool running;
} Clock;

// stopped at zero with a new limit
void set_clock(Clock *clock, double limit);
// stopped at zero, same limit
void reset_clock(Clock *clock);
// both do nothing when the clock is already in that state
void start_clock(Clock *clock, double now);
void pause_clock(Clock *clock, double now);
void toggle_clock(Clock *clock, double now);
// adds to the limit (negative takes away, never below 0). a clock with no limit is left alone
void extend_clock(Clock *clock, double seconds);
// moves the clock as if seconds more had run (the unused part of a speaking slot), never below 0
void charge_clock(Clock *clock, double seconds);

double clock_elapsed(const Clock *clock, double now);
// limit - elapsed, negative once over. with no limit it is -elapsed
double clock_left(const Clock *clock, double now);
// has a limit and elapsed >= limit
bool clock_expired(const Clock *clock, double now);
// has a limit and 0 < left <= warn_at
bool clock_warning(const Clock *clock, double now, double warn_at);
// elapsed / limit clamped to 0..1, 0 with no limit
float clock_progress(const Clock *clock, double now);

#endif
