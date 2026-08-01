/**
 * Sweep log.
 *
 * Records every combination a brute-force sweep puts on the air, in the order it
 * was tried, so that when a tagger finally reacts you can work backwards and see
 * which player ID, team colour and damage did it.
 *
 * Lines are buffered and flushed in batches: the SD card is far slower than the
 * radio, and a sweep with no delay would otherwise be paced by the filesystem.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "miles_tag_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MilesTagLog MilesTagLog;

/** Path of the log file, for showing to the user. */
const char* miles_tag_log_path(void);

MilesTagLog* miles_tag_log_alloc(void);
void miles_tag_log_free(MilesTagLog* log);

/**
 * Open the log and write a session header.
 *
 * Appends to any existing log, so a paused-and-resumed sweep - or a second run -
 * keeps the earlier history.
 *
 * @param plan  human description of what is being swept, e.g. "Player+Team @ all freqs"
 * @return false if the file could not be opened; logging is then simply skipped
 */
bool miles_tag_log_open(MilesTagLog* log, const char* plan, uint32_t total, bool resumed);

/** Record one transmitted combination. `index` is 1-based in the file. */
void miles_tag_log_append(MilesTagLog* log, uint32_t index, const MilesTagConfig* config);

/** Write a closing note, flush and close. `completed` distinguishes end from pause. */
void miles_tag_log_close(MilesTagLog* log, uint32_t index, bool completed);

/** True while a log file is open. */
bool miles_tag_log_is_open(const MilesTagLog* log);

/** Lines written during this session, for the UI counter. */
uint32_t miles_tag_log_count(const MilesTagLog* log);

#ifdef __cplusplus
}
#endif
