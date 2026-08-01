/** Brute-force run screen: progress bar plus whatever is on the air right now. */
#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MilesTagSweepView MilesTagSweepView;

/** OK: start, pause a running sweep, or resume a paused one. */
typedef void (*MilesTagSweepViewToggleCallback)(void* context);
/** Left: rewind a paused or finished sweep back to the first combination. */
typedef void (*MilesTagSweepViewResetCallback)(void* context);

MilesTagSweepView* miles_tag_sweep_view_alloc(void);
void miles_tag_sweep_view_free(MilesTagSweepView* sweep_view);
View* miles_tag_sweep_view_get_view(MilesTagSweepView* sweep_view);

void miles_tag_sweep_view_set_callbacks(
    MilesTagSweepView* sweep_view,
    MilesTagSweepViewToggleCallback toggle_callback,
    MilesTagSweepViewResetCallback reset_callback,
    void* context);

/** What is being swept, e.g. "Player+Team @ all freqs". */
void miles_tag_sweep_view_set_plan(MilesTagSweepView* sweep_view, const char* plan);

/**
 * Update progress.
 *
 * @param current      description of the combination just transmitted
 * @param logged       lines written to the sweep log, or 0 when logging is off
 * @param eta_seconds  estimate for the combinations still to go
 */
void miles_tag_sweep_view_set_progress(
    MilesTagSweepView* sweep_view,
    bool running,
    uint32_t index,
    uint32_t total,
    const char* current,
    uint32_t logged,
    uint32_t eta_seconds);

#ifdef __cplusplus
}
#endif
