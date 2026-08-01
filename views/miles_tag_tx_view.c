#include "miles_tag_tx_view.h"

#include <furi.h>
#include <gui/elements.h>

#define MILES_TAG_TX_VIEW_TITLE_SIZE   24
#define MILES_TAG_TX_VIEW_SUMMARY_SIZE 40
#define MILES_TAG_TX_VIEW_HEX_SIZE     16
#define MILES_TAG_TX_VIEW_BITS_SIZE    32

typedef struct {
    char title[MILES_TAG_TX_VIEW_TITLE_SIZE];
    char summary[MILES_TAG_TX_VIEW_SUMMARY_SIZE];
    char hex[MILES_TAG_TX_VIEW_HEX_SIZE];
    char bits[MILES_TAG_TX_VIEW_BITS_SIZE];
    uint32_t frequency;
    uint32_t sent_count;
    uint8_t bit_count;
    uint8_t repeat;
    bool sending;
} MilesTagTxViewModel;

struct MilesTagTxView {
    View* view;
    MilesTagTxViewFireCallback fire_callback;
    void* fire_context;
};

static void miles_tag_tx_view_draw_callback(Canvas* canvas, void* _model) {
    MilesTagTxViewModel* model = _model;

    canvas_clear(canvas);

    /* Title bar: what we are about to send, and on which carrier. */
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, model->title);

    char carrier[16];
    snprintf(carrier, sizeof(carrier), "%lu kHz", model->frequency / 1000UL);
    canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, carrier);
    canvas_set_color(canvas, ColorBlack);

    /* What the packet actually does, in words. */
    canvas_set_font(canvas, FontSecondary);
    FuriString* summary = furi_string_alloc_set(model->summary);
    elements_string_fit_width(canvas, summary, 124);
    canvas_draw_str(canvas, 2, 23, furi_string_get_cstr(summary));
    furi_string_free(summary);

    /* ...and on the wire. */
    char line[48];
    snprintf(line, sizeof(line), "%s  (%u bits)", model->hex, model->bit_count);
    canvas_draw_str(canvas, 2, 34, line);
    canvas_draw_str(canvas, 2, 44, model->bits);

    /* Status: sending indicator, repeat setting and lifetime packet count. */
    if(model->sending) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 55, "Sending...");
        canvas_set_font(canvas, FontSecondary);
    } else {
        snprintf(line, sizeof(line), "x%u per press", model->repeat);
        canvas_draw_str(canvas, 2, 55, line);
    }

    snprintf(line, sizeof(line), "TX:%lu", model->sent_count);
    canvas_draw_str_aligned(canvas, 126, 55, AlignRight, AlignBottom, line);

    elements_button_center(canvas, "Fire");
}

static bool miles_tag_tx_view_input_callback(InputEvent* event, void* context) {
    MilesTagTxView* tx_view = context;

    /* Short press fires once; holding down repeats at the OS key-repeat rate. */
    if(event->key == InputKeyOk &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        if(tx_view->fire_callback) {
            tx_view->fire_callback(tx_view->fire_context);
        }
        return true;
    }

    return false;
}

MilesTagTxView* miles_tag_tx_view_alloc(void) {
    MilesTagTxView* tx_view = malloc(sizeof(MilesTagTxView));

    tx_view->view = view_alloc();
    tx_view->fire_callback = NULL;
    tx_view->fire_context = NULL;

    view_allocate_model(tx_view->view, ViewModelTypeLocking, sizeof(MilesTagTxViewModel));
    view_set_context(tx_view->view, tx_view);
    view_set_draw_callback(tx_view->view, miles_tag_tx_view_draw_callback);
    view_set_input_callback(tx_view->view, miles_tag_tx_view_input_callback);

    return tx_view;
}

void miles_tag_tx_view_free(MilesTagTxView* tx_view) {
    furi_check(tx_view);

    view_free(tx_view->view);
    free(tx_view);
}

View* miles_tag_tx_view_get_view(MilesTagTxView* tx_view) {
    furi_check(tx_view);
    return tx_view->view;
}

void miles_tag_tx_view_set_fire_callback(
    MilesTagTxView* tx_view,
    MilesTagTxViewFireCallback callback,
    void* context) {
    furi_check(tx_view);

    tx_view->fire_callback = callback;
    tx_view->fire_context = context;
}

void miles_tag_tx_view_set_packet(
    MilesTagTxView* tx_view,
    const char* title,
    const char* summary,
    const char* hex,
    const char* bits,
    uint32_t frequency,
    uint8_t bit_count,
    uint8_t repeat) {
    furi_check(tx_view);

    with_view_model(
        tx_view->view,
        MilesTagTxViewModel * model,
        {
            strncpy(model->title, title, sizeof(model->title) - 1);
            model->title[sizeof(model->title) - 1] = '\0';
            strncpy(model->summary, summary, sizeof(model->summary) - 1);
            model->summary[sizeof(model->summary) - 1] = '\0';
            strncpy(model->hex, hex, sizeof(model->hex) - 1);
            model->hex[sizeof(model->hex) - 1] = '\0';
            strncpy(model->bits, bits, sizeof(model->bits) - 1);
            model->bits[sizeof(model->bits) - 1] = '\0';
            model->frequency = frequency;
            model->bit_count = bit_count;
            model->repeat = repeat;
        },
        true);
}

void miles_tag_tx_view_set_sending(MilesTagTxView* tx_view, bool sending) {
    furi_check(tx_view);

    with_view_model(
        tx_view->view, MilesTagTxViewModel * model, { model->sending = sending; }, true);
}

void miles_tag_tx_view_set_sent_count(MilesTagTxView* tx_view, uint32_t count) {
    furi_check(tx_view);

    with_view_model(
        tx_view->view, MilesTagTxViewModel * model, { model->sent_count = count; }, true);
}
