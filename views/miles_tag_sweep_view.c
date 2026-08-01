#include "miles_tag_sweep_view.h"

#include <furi.h>
#include <gui/elements.h>

#define MILES_TAG_SWEEP_PLAN_SIZE    40
#define MILES_TAG_SWEEP_CURRENT_SIZE 40

typedef struct {
    char plan[MILES_TAG_SWEEP_PLAN_SIZE];
    char current[MILES_TAG_SWEEP_CURRENT_SIZE];
    uint32_t index;
    uint32_t total;
    uint32_t logged;
    uint32_t eta_seconds;
    bool running;
} MilesTagSweepViewModel;

struct MilesTagSweepView {
    View* view;
    MilesTagSweepViewToggleCallback toggle_callback;
    MilesTagSweepViewResetCallback reset_callback;
    void* context;
};

/** Paused means stopped part-way, as opposed to never started or finished. */
static bool miles_tag_sweep_view_is_paused(const MilesTagSweepViewModel* model) {
    return !model->running && model->index > 0 && model->index < model->total;
}

static bool miles_tag_sweep_view_is_finished(const MilesTagSweepViewModel* model) {
    return !model->running && model->total > 0 && model->index >= model->total;
}

static void miles_tag_sweep_view_draw_callback(Canvas* canvas, void* _model) {
    MilesTagSweepViewModel* model = _model;

    canvas_clear(canvas);

    const char* state = "READY";
    if(model->running) {
        state = "RUNNING";
    } else if(miles_tag_sweep_view_is_finished(model)) {
        state = "DONE";
    } else if(miles_tag_sweep_view_is_paused(model)) {
        state = "PAUSED";
    }

    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "Brute force");
    canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, state);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 21, model->plan);

    /* What is on the air right now, or what will go out next. */
    FuriString* current = furi_string_alloc_set(model->current);
    elements_string_fit_width(canvas, current, 124);
    canvas_draw_str(canvas, 2, 31, furi_string_get_cstr(current));
    furi_string_free(current);

    float progress = model->total ? (float)model->index / (float)model->total : 0.0f;
    if(progress > 1.0f) progress = 1.0f;
    elements_progress_bar(canvas, 2, 34, 124, progress);

    char line[40];
    snprintf(line, sizeof(line), "%lu/%lu", model->index, model->total);
    canvas_draw_str(canvas, 2, 50, line);

    /* Right of the counter: time left while running, log size when stopped. */
    if(model->running) {
        snprintf(line, sizeof(line), "~%lus left", model->eta_seconds);
    } else if(model->logged) {
        snprintf(line, sizeof(line), "logged %lu", model->logged);
    } else {
        line[0] = '\0';
    }
    if(line[0]) {
        canvas_draw_str_aligned(canvas, 126, 50, AlignRight, AlignBottom, line);
    }

    if(model->running) {
        elements_button_center(canvas, "Pause");
    } else if(miles_tag_sweep_view_is_paused(model)) {
        elements_button_center(canvas, "Resume");
        elements_button_left(canvas, "Reset");
    } else if(miles_tag_sweep_view_is_finished(model)) {
        elements_button_left(canvas, "Reset");
    } else {
        elements_button_center(canvas, "Start");
    }
}

static bool miles_tag_sweep_view_input_callback(InputEvent* event, void* context) {
    MilesTagSweepView* sweep_view = context;

    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk) {
        if(sweep_view->toggle_callback) {
            sweep_view->toggle_callback(sweep_view->context);
        }
        return true;
    }

    if(event->key == InputKeyLeft) {
        /* Only meaningful when stopped part-way or at the end. */
        bool resettable = false;
        with_view_model(
            sweep_view->view,
            MilesTagSweepViewModel * model,
            {
                resettable = !model->running && model->index > 0;
            },
            false);

        if(resettable && sweep_view->reset_callback) {
            sweep_view->reset_callback(sweep_view->context);
        }
        return resettable;
    }

    return false;
}

MilesTagSweepView* miles_tag_sweep_view_alloc(void) {
    MilesTagSweepView* sweep_view = malloc(sizeof(MilesTagSweepView));

    sweep_view->view = view_alloc();
    sweep_view->toggle_callback = NULL;
    sweep_view->reset_callback = NULL;
    sweep_view->context = NULL;

    view_allocate_model(sweep_view->view, ViewModelTypeLocking, sizeof(MilesTagSweepViewModel));
    view_set_context(sweep_view->view, sweep_view);
    view_set_draw_callback(sweep_view->view, miles_tag_sweep_view_draw_callback);
    view_set_input_callback(sweep_view->view, miles_tag_sweep_view_input_callback);

    return sweep_view;
}

void miles_tag_sweep_view_free(MilesTagSweepView* sweep_view) {
    furi_check(sweep_view);

    view_free(sweep_view->view);
    free(sweep_view);
}

View* miles_tag_sweep_view_get_view(MilesTagSweepView* sweep_view) {
    furi_check(sweep_view);
    return sweep_view->view;
}

void miles_tag_sweep_view_set_callbacks(
    MilesTagSweepView* sweep_view,
    MilesTagSweepViewToggleCallback toggle_callback,
    MilesTagSweepViewResetCallback reset_callback,
    void* context) {
    furi_check(sweep_view);

    sweep_view->toggle_callback = toggle_callback;
    sweep_view->reset_callback = reset_callback;
    sweep_view->context = context;
}

void miles_tag_sweep_view_set_plan(MilesTagSweepView* sweep_view, const char* plan) {
    furi_check(sweep_view);

    with_view_model(
        sweep_view->view,
        MilesTagSweepViewModel * model,
        {
            strncpy(model->plan, plan, sizeof(model->plan) - 1);
            model->plan[sizeof(model->plan) - 1] = '\0';
        },
        true);
}

void miles_tag_sweep_view_set_progress(
    MilesTagSweepView* sweep_view,
    bool running,
    uint32_t index,
    uint32_t total,
    const char* current,
    uint32_t logged,
    uint32_t eta_seconds) {
    furi_check(sweep_view);

    with_view_model(
        sweep_view->view,
        MilesTagSweepViewModel * model,
        {
            model->running = running;
            model->index = index;
            model->total = total;
            model->logged = logged;
            model->eta_seconds = eta_seconds;
            strncpy(model->current, current, sizeof(model->current) - 1);
            model->current[sizeof(model->current) - 1] = '\0';
        },
        true);
}
