#include <clock.h>

void set_clock(Clock *clock, double limit) {
    clock->limit = limit > 0.0 ? limit : 0.0;
    reset_clock(clock);
}

void reset_clock(Clock *clock) {
    clock->banked = 0.0;
    clock->started = 0.0;
    clock->running = false;
}

void start_clock(Clock *clock, double now) {
    if (clock->running) return;
    clock->started = now;
    clock->running = true;
}

void pause_clock(Clock *clock, double now) {
    if (!clock->running) return;
    clock->banked = clock_elapsed(clock, now);
    clock->running = false;
}

void toggle_clock(Clock *clock, double now) {
    if (clock->running) pause_clock(clock, now);
    else start_clock(clock, now);
}

void extend_clock(Clock *clock, double seconds) {
    if (clock->limit <= 0.0) return;
    clock->limit += seconds;
    if (clock->limit < 0.0) clock->limit = 0.0;
}

void charge_clock(Clock *clock, double seconds) {
    clock->banked += seconds;
    if (clock->banked < 0.0) clock->banked = 0.0;
}

double clock_elapsed(const Clock *clock, double now) {
    double elapsed = clock->banked;
    if (clock->running) elapsed += now - clock->started;
    return elapsed > 0.0 ? elapsed : 0.0;
}

double clock_left(const Clock *clock, double now) {
    return clock->limit - clock_elapsed(clock, now);
}

bool clock_expired(const Clock *clock, double now) {
    return clock->limit > 0.0 && clock_elapsed(clock, now) >= clock->limit;
}

bool clock_warning(const Clock *clock, double now, double warn_at) {
    if (clock->limit <= 0.0) return false;
    double left = clock_left(clock, now);
    return left > 0.0 && left <= warn_at;
}

float clock_progress(const Clock *clock, double now) {
    if (clock->limit <= 0.0) return 0.0f;
    double progress = clock_elapsed(clock, now) / clock->limit;
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    return (float)progress;
}
