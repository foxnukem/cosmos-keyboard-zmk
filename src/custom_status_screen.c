/*
 * ZMK's built-in status screen plus the peripheral half's battery level.
 *
 * Layout on the 128x32 OLED (parenthesised numbers are font sizes):
 *
 *   <bt profile>   (14)       <this half's battery>  (14)
 *   <layer name>   (12)       <other half's battery> (14)
 *
 * Only the bottom-right slot is ours; the rest are ZMK's own widgets in their
 * stock positions. The two battery readings are told apart by position alone.
 * Font sizes matter: two 14px rows (16px line height) tile exactly into 32px,
 * so raising the default in cosmos_keyboard_right.conf re-crowds the display.
 */

#include <stdio.h>
#include <zephyr/kernel.h>

#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/display/widgets/battery_status.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/display/widgets/output_status.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/split/central.h>

/* ------------------------------------------------------------------ *
 * Peripheral battery widget
 * ------------------------------------------------------------------ */

static lv_obj_t *peripheral_battery_label;

static void peripheral_battery_update_cb(uint8_t level) {
    if (peripheral_battery_label == NULL) {
        return;
    }

    /* Bare percentage, matching the built-in widget's format exactly. */
    char text[10] = {};
    snprintf(text, sizeof(text), "%3u%%", level);
    lv_label_set_text(peripheral_battery_label, text);
}

static uint8_t peripheral_battery_get_state(const zmk_event_t *eh) {
    /* eh is NULL on the initial paint, and as_zmk_*() does not guard for it. */
    const struct zmk_peripheral_battery_state_changed *ev =
        (eh != NULL) ? as_zmk_peripheral_battery_state_changed(eh) : NULL;

    if (ev != NULL) {
        return ev->state_of_charge;
    }

    /* No event yet: ask the split central for the last value it fetched. */
    uint8_t level = 0;
    zmk_split_central_get_peripheral_battery_level(0, &level);
    return level;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_battery, uint8_t, peripheral_battery_update_cb,
                            peripheral_battery_get_state)
ZMK_SUBSCRIPTION(widget_peripheral_battery, zmk_peripheral_battery_state_changed);

/* ------------------------------------------------------------------ *
 * Screen assembly - mirrors app/src/display/status_screen.c
 * ------------------------------------------------------------------ */

static struct zmk_widget_battery_status battery_status_widget;
static struct zmk_widget_output_status output_status_widget;
static struct zmk_widget_layer_status layer_status_widget;

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    zmk_widget_battery_status_init(&battery_status_widget, screen);
    lv_obj_align(zmk_widget_battery_status_obj(&battery_status_widget), LV_ALIGN_TOP_RIGHT, 0, 0);

    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_LEFT, 0, 0);

    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_set_style_text_font(zmk_widget_layer_status_obj(&layer_status_widget),
                               lv_theme_get_font_small(screen), LV_PART_MAIN);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* No set_style_text_font here on purpose: ZMK's battery widget above sets
     * none either, so inheriting the theme default keeps the two readings the
     * same size. Pinning a font here renders this one smaller. */
    peripheral_battery_label = lv_label_create(screen);
    lv_obj_align(peripheral_battery_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    widget_peripheral_battery_init();

    return screen;
}
