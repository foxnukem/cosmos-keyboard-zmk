# Sources contributed by this config repo, built as a ZMK module.
# Everything here must stay guarded: this file is processed for BOTH halves,
# and the left half builds without a display.

if(CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
  zephyr_library()
  zephyr_library_sources(src/custom_status_screen.c)
endif()
