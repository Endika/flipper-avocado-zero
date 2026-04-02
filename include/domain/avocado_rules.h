/**
 * Domain rules: pure game logic (host-testable, no SDK includes in .c).
 */
#pragma once

#include "avocado_state.h"
#include <stdbool.h>
#include <stdint.h>

bool avocado_rules_is_game_over(const AvocadoData *state);

bool avocado_rules_should_show_victory(const AvocadoData *state);

void avocado_rules_apply_elapsed_days(AvocadoData *state, uint32_t now_ts);

/** Primary action: Clean while playing, or Start after game over (new run). */
void avocado_rules_on_primary_action(AvocadoData *state);

/** Dismiss victory overlay after max roots (persist victory_seen). */
void avocado_rules_acknowledge_victory(AvocadoData *state);
