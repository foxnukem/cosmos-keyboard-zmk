/*
 * Two-line status screen for the 128x32 OLED on the right half.
 *
 *   <bt profile>  R <right batt>  L <left batt>
 *   <layer name>
 *
 * The right half is the split central, which is what makes this possible at
 * all: the layer and output status widgets are central-only in ZMK, and the
 * left half's battery level only reaches us because
 * CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING is enabled.
 */

#include <stdio.h>
#include <zephyr/kernel.h>

#include <lvgl.h>

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/display/widgets/output_status.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/split/central.h>

/* ------------------------------------------------------------------ *
 * Dual battery widget: this half's charge plus the peripheral's.
 * ------------------------------------------------------------------ */

struct dual_battery_state {
    uint8_t central;
    uint8_t peripheral;
};

static lv_obj_t *dual_battery_label;

static void dual_battery_update_cb(struct dual_battery_state state) {
    if (dual_battery_label == NULL) {
        return;
    }

    char text[16] = {};
    snprintf(text, sizeof(text), "R%3u%% L%3u%%", state.central, state.peripheral);
    lv_label_set_text(dual_battery_label, text);
}

static struct dual_battery_state dual_battery_get_state(const zmk_event_t *eh) {
    struct dual_battery_state state = {
        .central = zmk_battery_state_of_charge(),
        .peripheral = 0,
    };

    /*
     * Peripheral levels arrive as events; between them, ask the split central
     * for the last known value so the label is correct on first paint too.
     */
    uint8_t level = 0;
    if (zmk_split_central_get_peripheral_battery_level(0, &level) == 0) {
        state.peripheral = level;
    }

    /* eh is NULL on the initial paint, and as_zmk_*() does not guard for it. */
    const struct zmk_peripheral_battery_state_changed *ev =
        (eh != NULL) ? as_zmk_peripheral_battery_state_changed(eh) : NULL;
    if (ev != NULL && ev->source == 0) {
        state.peripheral = ev->state_of_charge;
    }

    return state;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_dual_battery, struct dual_battery_state, dual_battery_update_cb,
                            dual_battery_get_state)
ZMK_SUBSCRIPTION(widget_dual_battery, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(widget_dual_battery, zmk_peripheral_battery_state_changed);

/* ------------------------------------------------------------------ *
 * Screen assembly
 * ------------------------------------------------------------------ */

static struct zmk_widget_output_status output_status_widget;
static struct zmk_widget_layer_status layer_status_widget;

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    const lv_font_t *small_font = lv_theme_get_font_small(screen);

    /* Top line: BT profile on the left, both battery levels on the right. */
    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_t *output_obj = zmk_widget_output_status_obj(&output_status_widget);
    lv_obj_set_style_text_font(output_obj, small_font, LV_PART_MAIN);
    lv_obj_align(output_obj, LV_ALIGN_TOP_LEFT, 0, 0);

    dual_battery_label = lv_label_create(screen);
    lv_obj_set_style_text_font(dual_battery_label, small_font, LV_PART_MAIN);
    lv_obj_align(dual_battery_label, LV_ALIGN_TOP_RIGHT, 0, 0);
    widget_dual_battery_init();

    /* Bottom line: the active layer's display-name. */
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_BOTTOM_LEFT, 0, 0);

    return screen;
}
