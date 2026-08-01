/**
 * Background infrared transmitter.
 *
 * infrared_send_raw_ext() blocks for the whole duration of the burst, so it runs
 * on its own thread. The GUI enqueues packets and gets a callback when the queue
 * drains, which keeps the UI responsive while the user holds OK to spam shots.
 *
 * The same thread also runs brute-force sweeps: for when you do not know the
 * player ID, team colour or carrier frequency you need, it walks every
 * combination of the fields you mark as unknown and transmits each one.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "miles_tag_log.h"
#include "miles_tag_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MilesTagTx MilesTagTx;

/** Called from the TX thread once a queued burst or a sweep has finished. */
typedef void (*MilesTagTxCallback)(void* context);

/** Which fields a sweep should walk instead of holding at their fixed value. */
typedef struct {
    bool frequency; /**< try 38, 40 and 56 kHz */
    bool player; /**< try all 128 player IDs */
    bool team; /**< try all 4 team colours */
    bool damage; /**< try all 16 damage codes */
    bool command; /**< try all 24 command bytes */
    bool log; /**< write every combination tried to the log file */
    uint8_t repeat; /**< copies of each combination, 1..10 */
    uint16_t delay_ms; /**< pause between combinations */
} MilesTagSweepOptions;

/** Live sweep state, safe to read from the GUI thread. */
typedef struct {
    bool running;
    uint32_t index; /**< combinations already sent */
    uint32_t total; /**< combinations in the whole sweep */
    uint32_t logged; /**< lines written to the log this session */
    MilesTagConfig current; /**< what just went on the air */
} MilesTagSweepProgress;

MilesTagTx* miles_tag_tx_alloc(void);
void miles_tag_tx_free(MilesTagTx* tx);

void miles_tag_tx_set_callback(MilesTagTx* tx, MilesTagTxCallback callback, void* context);

/**
 * Queue a burst.
 *
 * @param repeat  how many copies of the packet to send, at least 1
 * @param gap_ms  pause between copies
 * @return false if the queue is full (the caller is transmitting faster than the
 *         air allows), in which case the burst is dropped rather than buffered
 */
bool miles_tag_tx_send(const MilesTagPacket* packet, uint8_t repeat, uint8_t gap_ms, MilesTagTx* tx);

/** True while a burst is on the air or waiting in the queue. */
bool miles_tag_tx_is_busy(const MilesTagTx* tx);

/** Total packets put on the air since the app started. */
uint32_t miles_tag_tx_sent_count(const MilesTagTx* tx);

/** How many combinations a sweep with these options would transmit. */
uint32_t miles_tag_sweep_total(const MilesTagConfig* base, const MilesTagSweepOptions* options);

/**
 * Load a sweep and rewind it to the first combination.
 *
 * `base` supplies the value of every field not being swept. Ignored while a
 * sweep is running - pause first.
 */
void miles_tag_tx_sweep_prepare(
    MilesTagTx* tx,
    const MilesTagConfig* base,
    const MilesTagSweepOptions* options);

/**
 * Start transmitting from the current position, so this both starts a fresh
 * sweep and resumes a paused one.
 *
 * `plan` is the human description written into the log header.
 */
void miles_tag_tx_sweep_run(MilesTagTx* tx, const char* plan);

/**
 * Pause after the combination currently on the air.
 *
 * The position is kept, so a later miles_tag_tx_sweep_run() picks up at the next
 * untried combination. The log is closed with a "paused" marker and reopened in
 * append mode on resume, which means the file on the card is complete and
 * readable while the sweep sits paused.
 */
void miles_tag_tx_sweep_pause(MilesTagTx* tx);

/** True once every combination has been transmitted. */
bool miles_tag_tx_sweep_is_finished(MilesTagTx* tx);

/** True if the loaded sweep is still the one described by these settings. */
bool miles_tag_tx_sweep_matches(
    MilesTagTx* tx,
    const MilesTagConfig* base,
    const MilesTagSweepOptions* options);

/** Copy out the current sweep progress. */
void miles_tag_tx_sweep_progress(MilesTagTx* tx, MilesTagSweepProgress* progress);

#ifdef __cplusplus
}
#endif
