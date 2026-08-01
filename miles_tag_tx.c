#include "miles_tag_tx.h"

#include <furi.h>
#include <infrared_transmit.h>

#define TAG "MilesTagTx"

#define MILES_TAG_TX_QUEUE_SIZE 4
#define MILES_TAG_TX_STACK_SIZE 2048

typedef struct {
    MilesTagPacket packet;
    uint8_t repeat;
    uint8_t gap_ms;
} MilesTagTxRequest;

struct MilesTagTx {
    FuriThread* thread;
    FuriMessageQueue* queue;
    MilesTagTxCallback callback;
    void* callback_context;
    volatile bool running;
    volatile bool busy;
    volatile uint32_t sent_count;

    /* Sweep state. The mutex only guards these; transmission happens outside it. */
    FuriMutex* sweep_mutex;
    MilesTagConfig sweep_base;
    MilesTagSweepOptions sweep_options;
    MilesTagSweepProgress sweep_progress;
    volatile bool sweep_active;
    volatile bool sweep_stop_requested;

    /* Owned by the TX thread once a sweep is running. */
    MilesTagLog* log;
    char log_plan[40];
    volatile bool log_open_requested;
};

/** Number of values a field contributes to the sweep: all of them, or just one. */
static uint32_t miles_tag_sweep_dimension(bool sweeping, uint32_t count) {
    return sweeping ? count : 1;
}

/** True if this signal type carries a command byte worth sweeping. */
static bool miles_tag_type_has_command(MilesTagSignalType type) {
    return type == MilesTagSignalCommand;
}

/** True if this signal type carries player/team/damage worth sweeping. */
static bool miles_tag_type_has_shot_fields(MilesTagSignalType type) {
    return type == MilesTagSignalShot;
}

uint32_t miles_tag_sweep_total(const MilesTagConfig* base, const MilesTagSweepOptions* options) {
    furi_check(base);
    furi_check(options);

    uint32_t total =
        miles_tag_sweep_dimension(options->frequency, (uint32_t)miles_tag_frequency_count);

    if(miles_tag_type_has_shot_fields(base->type)) {
        total *= miles_tag_sweep_dimension(options->player, MILES_TAG_PLAYER_COUNT);
        total *= miles_tag_sweep_dimension(options->team, MILES_TAG_TEAM_COUNT);
        total *= miles_tag_sweep_dimension(options->damage, MILES_TAG_DAMAGE_COUNT);
    } else if(miles_tag_type_has_command(base->type)) {
        total *= miles_tag_sweep_dimension(options->command, MILES_TAG_COMMAND_COUNT);
    }

    return total;
}

/**
 * Turn a flat combination index into a config.
 *
 * The swept fields are treated as digits of a mixed-radix number, so the sweep
 * is a simple counter rather than a stack of nested loops - which makes it
 * trivial to stop and resume at any point.
 */
static void miles_tag_sweep_config_at(
    const MilesTagConfig* base,
    const MilesTagSweepOptions* options,
    uint32_t index,
    MilesTagConfig* out) {
    *out = *base;

    if(miles_tag_type_has_shot_fields(base->type)) {
        if(options->damage) {
            out->damage_index = index % MILES_TAG_DAMAGE_COUNT;
            index /= MILES_TAG_DAMAGE_COUNT;
        }
        if(options->team) {
            out->team_id = index % MILES_TAG_TEAM_COUNT;
            index /= MILES_TAG_TEAM_COUNT;
        }
        if(options->player) {
            out->player_id = index % MILES_TAG_PLAYER_COUNT;
            index /= MILES_TAG_PLAYER_COUNT;
        }
    } else if(miles_tag_type_has_command(base->type)) {
        if(options->command) {
            out->command = index % MILES_TAG_COMMAND_COUNT;
            index /= MILES_TAG_COMMAND_COUNT;
        }
    }

    if(options->frequency) {
        out->frequency_index = index % miles_tag_frequency_count;
    }
}

/** Send one combination of a sweep. Returns false once the sweep is over. */
static bool miles_tag_tx_sweep_step(MilesTagTx* tx) {
    MilesTagConfig base;
    MilesTagSweepOptions options;
    uint32_t index;
    uint32_t total;

    furi_mutex_acquire(tx->sweep_mutex, FuriWaitForever);
    base = tx->sweep_base;
    options = tx->sweep_options;
    index = tx->sweep_progress.index;
    total = tx->sweep_progress.total;
    furi_mutex_release(tx->sweep_mutex);

    if(index >= total) return false;

    MilesTagConfig config;
    miles_tag_sweep_config_at(&base, &options, index, &config);

    MilesTagPacket packet;
    miles_tag_encode(&config, &packet);

    uint8_t repeat = options.repeat ? options.repeat : 1;
    for(uint8_t i = 0; i < repeat && tx->running && !tx->sweep_stop_requested; i++) {
        infrared_send_raw_ext(
            packet.timings, packet.timing_count, true, packet.frequency, packet.duty_cycle);
        tx->sent_count++;
    }

    /* Logged after transmission, so the file only ever claims what really went
     * out - and in the exact order it went out. */
    if(options.log) {
        miles_tag_log_append(tx->log, index + 1, &config);
    }

    furi_mutex_acquire(tx->sweep_mutex, FuriWaitForever);
    tx->sweep_progress.index = index + 1;
    tx->sweep_progress.current = config;
    tx->sweep_progress.logged = miles_tag_log_count(tx->log);
    furi_mutex_release(tx->sweep_mutex);

    if(options.delay_ms) {
        /* Sliced so a stop request is acted on quickly even with a long delay. */
        uint16_t waited = 0;
        while(waited < options.delay_ms && tx->running && !tx->sweep_stop_requested) {
            uint16_t slice = (options.delay_ms - waited) > 20 ? 20 : (options.delay_ms - waited);
            furi_delay_ms(slice);
            waited += slice;
        }
    }

    return true;
}

/**
 * Wind the sweep down, whether it ran out of combinations or the user paused it.
 *
 * The position is deliberately left alone so a pause can be resumed; only an
 * explicit reset or a fresh start rewinds it.
 */
static void miles_tag_tx_sweep_finish(MilesTagTx* tx) {
    uint32_t index;
    uint32_t total;

    furi_mutex_acquire(tx->sweep_mutex, FuriWaitForever);
    tx->sweep_progress.running = false;
    index = tx->sweep_progress.index;
    total = tx->sweep_progress.total;
    furi_mutex_release(tx->sweep_mutex);

    if(miles_tag_log_is_open(tx->log)) {
        miles_tag_log_close(tx->log, index, index >= total);
    }

    tx->sweep_active = false;
    tx->sweep_stop_requested = false;
    tx->busy = false;

    if(tx->callback) {
        tx->callback(tx->callback_context);
    }
}

static int32_t miles_tag_tx_thread(void* context) {
    MilesTagTx* tx = context;
    MilesTagTxRequest request;

    while(tx->running) {
        if(tx->sweep_active) {
            tx->busy = true;

            /* Storage work belongs on this thread, not on the GUI's. */
            if(tx->log_open_requested) {
                tx->log_open_requested = false;

                furi_mutex_acquire(tx->sweep_mutex, FuriWaitForever);
                uint32_t total = tx->sweep_progress.total;
                bool resumed = tx->sweep_progress.index > 0;
                furi_mutex_release(tx->sweep_mutex);

                miles_tag_log_open(tx->log, tx->log_plan, total, resumed);
            }

            if(tx->sweep_stop_requested || !miles_tag_tx_sweep_step(tx)) {
                miles_tag_tx_sweep_finish(tx);
            }
            continue;
        }

        /* Wake up regularly so a stop request is noticed promptly. */
        if(furi_message_queue_get(tx->queue, &request, 100) != FuriStatusOk) {
            tx->busy = false;
            continue;
        }
        if(!tx->running) break;

        tx->busy = true;

        uint8_t repeat = request.repeat ? request.repeat : 1;
        for(uint8_t i = 0; i < repeat && tx->running; i++) {
            infrared_send_raw_ext(
                request.packet.timings,
                request.packet.timing_count,
                true, /* the MilesTag header is a mark, so we start high */
                request.packet.frequency,
                request.packet.duty_cycle);
            tx->sent_count++;

            if(request.gap_ms && (i + 1) < repeat) {
                furi_delay_ms(request.gap_ms);
            }
        }

        /* Only report idle once nothing else is waiting behind us. */
        if(furi_message_queue_get_count(tx->queue) == 0) {
            tx->busy = false;
            if(tx->callback) {
                tx->callback(tx->callback_context);
            }
        }
    }

    tx->busy = false;
    return 0;
}

MilesTagTx* miles_tag_tx_alloc(void) {
    MilesTagTx* tx = malloc(sizeof(MilesTagTx));

    tx->queue = furi_message_queue_alloc(MILES_TAG_TX_QUEUE_SIZE, sizeof(MilesTagTxRequest));
    tx->sweep_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    tx->callback = NULL;
    tx->callback_context = NULL;
    tx->running = true;
    tx->busy = false;
    tx->sent_count = 0;
    tx->sweep_active = false;
    tx->sweep_stop_requested = false;
    tx->log = miles_tag_log_alloc();
    tx->log_plan[0] = '\0';
    tx->log_open_requested = false;
    memset(&tx->sweep_progress, 0, sizeof(tx->sweep_progress));
    memset(&tx->sweep_options, 0, sizeof(tx->sweep_options));
    miles_tag_config_init(&tx->sweep_base);

    tx->thread = furi_thread_alloc_ex(TAG, MILES_TAG_TX_STACK_SIZE, miles_tag_tx_thread, tx);
    furi_thread_start(tx->thread);

    return tx;
}

void miles_tag_tx_free(MilesTagTx* tx) {
    furi_check(tx);

    tx->sweep_stop_requested = true;
    tx->running = false;
    furi_thread_join(tx->thread);
    furi_thread_free(tx->thread);

    /* The thread is gone, so closing the log here cannot race with it. */
    miles_tag_log_free(tx->log);
    furi_message_queue_free(tx->queue);
    furi_mutex_free(tx->sweep_mutex);
    free(tx);
}

void miles_tag_tx_set_callback(MilesTagTx* tx, MilesTagTxCallback callback, void* context) {
    furi_check(tx);

    tx->callback = callback;
    tx->callback_context = context;
}

bool miles_tag_tx_send(
    const MilesTagPacket* packet,
    uint8_t repeat,
    uint8_t gap_ms,
    MilesTagTx* tx) {
    furi_check(tx);
    furi_check(packet);

    /* A running sweep owns the radio. */
    if(tx->sweep_active) return false;

    MilesTagTxRequest request = {
        .repeat = repeat,
        .gap_ms = gap_ms,
    };
    memcpy(&request.packet, packet, sizeof(MilesTagPacket));

    /* Never block the GUI thread: a full queue means we drop this burst. */
    if(furi_message_queue_put(tx->queue, &request, 0) != FuriStatusOk) {
        return false;
    }

    tx->busy = true;
    return true;
}

bool miles_tag_tx_is_busy(const MilesTagTx* tx) {
    furi_check(tx);
    return tx->busy;
}

uint32_t miles_tag_tx_sent_count(const MilesTagTx* tx) {
    furi_check(tx);
    return tx->sent_count;
}

void miles_tag_tx_sweep_prepare(
    MilesTagTx* tx,
    const MilesTagConfig* base,
    const MilesTagSweepOptions* options) {
    furi_check(tx);
    furi_check(base);
    furi_check(options);

    if(tx->sweep_active) return;

    furi_mutex_acquire(tx->sweep_mutex, FuriWaitForever);
    tx->sweep_base = *base;
    tx->sweep_options = *options;
    tx->sweep_progress.index = 0;
    tx->sweep_progress.total = miles_tag_sweep_total(base, options);
    tx->sweep_progress.logged = 0;
    tx->sweep_progress.running = false;
    miles_tag_sweep_config_at(base, options, 0, &tx->sweep_progress.current);
    furi_mutex_release(tx->sweep_mutex);
}

void miles_tag_tx_sweep_run(MilesTagTx* tx, const char* plan) {
    furi_check(tx);
    furi_check(plan);

    if(tx->sweep_active) return;

    furi_mutex_acquire(tx->sweep_mutex, FuriWaitForever);
    bool has_work = tx->sweep_progress.index < tx->sweep_progress.total;
    bool log = tx->sweep_options.log;
    if(has_work) tx->sweep_progress.running = true;
    furi_mutex_release(tx->sweep_mutex);

    if(!has_work) return;

    strncpy(tx->log_plan, plan, sizeof(tx->log_plan) - 1);
    tx->log_plan[sizeof(tx->log_plan) - 1] = '\0';
    tx->log_open_requested = log;

    tx->sweep_stop_requested = false;
    tx->sweep_active = true;
}

void miles_tag_tx_sweep_pause(MilesTagTx* tx) {
    furi_check(tx);
    tx->sweep_stop_requested = true;
}

bool miles_tag_tx_sweep_is_finished(MilesTagTx* tx) {
    furi_check(tx);

    furi_mutex_acquire(tx->sweep_mutex, FuriWaitForever);
    bool finished =
        tx->sweep_progress.total > 0 && tx->sweep_progress.index >= tx->sweep_progress.total;
    furi_mutex_release(tx->sweep_mutex);

    return finished;
}

bool miles_tag_tx_sweep_matches(
    MilesTagTx* tx,
    const MilesTagConfig* base,
    const MilesTagSweepOptions* options) {
    furi_check(tx);
    furi_check(base);
    furi_check(options);

    furi_mutex_acquire(tx->sweep_mutex, FuriWaitForever);
    /* Compared field by field rather than with memcmp: these structs have
     * padding, and a stale byte in a hole would look like a changed setting. */
    bool same = tx->sweep_progress.total > 0 && tx->sweep_base.type == base->type &&
                tx->sweep_base.frequency_index == base->frequency_index &&
                tx->sweep_base.duty_index == base->duty_index &&
                tx->sweep_base.player_id == base->player_id &&
                tx->sweep_base.team_id == base->team_id &&
                tx->sweep_base.damage_index == base->damage_index &&
                tx->sweep_base.command == base->command &&
                tx->sweep_options.frequency == options->frequency &&
                tx->sweep_options.player == options->player &&
                tx->sweep_options.team == options->team &&
                tx->sweep_options.damage == options->damage &&
                tx->sweep_options.command == options->command &&
                tx->sweep_options.log == options->log &&
                tx->sweep_options.repeat == options->repeat &&
                tx->sweep_options.delay_ms == options->delay_ms;
    furi_mutex_release(tx->sweep_mutex);

    return same;
}

void miles_tag_tx_sweep_progress(MilesTagTx* tx, MilesTagSweepProgress* progress) {
    furi_check(tx);
    furi_check(progress);

    furi_mutex_acquire(tx->sweep_mutex, FuriWaitForever);
    *progress = tx->sweep_progress;
    furi_mutex_release(tx->sweep_mutex);
}
