#include "include/app/avocado_session.h"
#include "include/domain/avocado_rules.h"
#include "include/platform/storage_helper.h"

void avocado_session_bootstrap(AvocadoData *data, uint32_t now_ts) {
    avocado_data_load(data);
    avocado_rules_apply_elapsed_days(data, now_ts);
    avocado_data_save(data);
}
