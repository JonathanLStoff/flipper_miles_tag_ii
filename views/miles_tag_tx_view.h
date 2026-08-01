/** The "pull the trigger" screen: shows the encoded packet and fires it. */
#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MilesTagTxView MilesTagTxView;

/** Called when the user asks to transmit (OK pressed or held). */
typedef void (*MilesTagTxViewFireCallback)(void* context);

MilesTagTxView* miles_tag_tx_view_alloc(void);
void miles_tag_tx_view_free(MilesTagTxView* tx_view);
View* miles_tag_tx_view_get_view(MilesTagTxView* tx_view);

void miles_tag_tx_view_set_fire_callback(
    MilesTagTxView* tx_view,
    MilesTagTxViewFireCallback callback,
    void* context);

/** Set what the screen describes: title, summary line, hex dump and bit string. */
void miles_tag_tx_view_set_packet(
    MilesTagTxView* tx_view,
    const char* title,
    const char* summary,
    const char* hex,
    const char* bits,
    uint32_t frequency,
    uint8_t bit_count,
    uint8_t repeat);

void miles_tag_tx_view_set_sending(MilesTagTxView* tx_view, bool sending);
void miles_tag_tx_view_set_sent_count(MilesTagTxView* tx_view, uint32_t count);

#ifdef __cplusplus
}
#endif
