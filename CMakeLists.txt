# Sources contributed by this config repo, built as a ZMK module.
# Everything here must stay guarded: this file is processed for BOTH halves,
# and the left half builds without a display.

# zephyr_library() must be called before any sources are added, but only once,
# and only when at least one source survives its guard - the halves share no
# sources at all.
if(CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM OR CONFIG_PMW3610)
  zephyr_library()
endif()

# Right half only.
if(CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
  zephyr_library_sources(src/custom_status_screen.c)
endif()

# Left half only - needs the trackball devicetree node to exist.
if(CONFIG_PMW3610)
  zephyr_library_sources(src/pmw3610_cpi_reassert.c)
endif()
