/**
 * Domain model: persisted simulation state (no Flipper / Furi types).
 */
#pragma once

#include <stdint.h>

/** Grime level at or above this ends the run (UI shows game over). */
#define AVOCADO_GRIME_GAME_OVER 10u

/** Maximum roots length the rules allow. */
#define AVOCADO_ROOTS_MAX 10u

typedef struct {
    uint32_t last_timestamp;
    uint8_t dirty_level;
    uint8_t roots_length;
    /** 0 = show victory screen once at max roots; 1 = player dismissed it. */
    uint8_t victory_seen;
} AvocadoData;
