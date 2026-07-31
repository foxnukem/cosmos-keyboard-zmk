/*
 * Re-asserts the PMW3610's RES_STEP register once motion settles.
 *
 * RES_STEP (0x85) packs SWAP_XY/INV_X/INV_Y in bits 7-5 and CPI in bits 4-0.
 * The driver writes it during init and never again, so when it takes a bad
 * value at runtime the axes swap and sensitivity drops together and stay that
 * way until the next power cycle. Observed to follow queue overflow under heavy
 * movement; the mechanism is unidentified, this only heals it.
 *
 * sensor_attr_set(PMW3610_ATTR_CPI) re-runs the driver's pmw3610_set_cpi() with
 * the devicetree orientation and CPI - the same write init does, without the
 * reboot. Scheduled after the last motion event so it never lands mid-movement,
 * and it runs on the system workqueue, which is also where the driver's burst
 * reads run, so the two cannot overlap.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pmw3610_cpi_reassert, CONFIG_ZMK_LOG_LEVEL);

#define TRACKBALL_NODE DT_NODELABEL(input_device_left0)

/* Not a `const struct device *` variable: INPUT_CALLBACK_DEFINE builds a static
 * initializer, so its device argument has to be a constant expression. */
#define TRACKBALL_DEV DEVICE_DT_GET(TRACKBALL_NODE)

#if !DT_NODE_HAS_STATUS(TRACKBALL_NODE, okay)
#error "pmw3610_cpi_reassert built without an enabled input_device_left0"
#endif

/* First member of `enum pmw3610_attribute` in the driver's src/pmw3610.h. That
 * header is unreachable: the driver's CMakeLists only adds
 * ${APPLICATION_SOURCE_DIR}/include to the include path, not its own src.
 *
 * west.yml tracks the zmk-0.3 BRANCH, so this is not pinned. If the driver ever
 * reorders that enum this silently becomes a downshift-time write instead, with
 * no build error. Re-check on any driver update.
 */
#define PMW3610_ATTR_CPI 0

/* Long enough that a pause mid-drag does not trigger a write, short enough that
 * a corrupted register is not lived with. */
#define SETTLE_MS 1000

static void reassert_cpi(struct k_work *work) {
    ARG_UNUSED(work);

    const struct device *trackball = TRACKBALL_DEV;

    if (!device_is_ready(trackball)) {
        return;
    }

    /* Driver takes CPI straight off val1. Sourced from the same devicetree
     * property the driver used at init, so the two cannot drift apart. */
    struct sensor_value val = {.val1 = DT_PROP(TRACKBALL_NODE, cpi)};

    int err = sensor_attr_set(trackball, SENSOR_CHAN_ALL,
                              (enum sensor_attribute)PMW3610_ATTR_CPI, &val);
    if (err) {
        /* -EBUSY means init never completed, so the sensor is dead anyway. */
        LOG_WRN("CPI re-assert failed (%d)", err);
    } else {
        LOG_DBG("CPI re-asserted at %d", val.val1);
    }
}

static K_WORK_DELAYABLE_DEFINE(reassert_work, reassert_cpi);

static void on_trackball_event(struct input_event *evt) {
    ARG_UNUSED(evt);

    /* Every event pushes the write further out, so it lands once per movement
     * burst rather than once per report. */
    k_work_reschedule(&reassert_work, K_MSEC(SETTLE_MS));
}

INPUT_CALLBACK_DEFINE(TRACKBALL_DEV, on_trackball_event);
