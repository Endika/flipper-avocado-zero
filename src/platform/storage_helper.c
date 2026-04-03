#include "include/platform/storage_helper.h"
#include <furi.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <string.h>

#define APPS_DATA_DIR "/ext/apps_data/avocado_zero"
#define STORAGE_PATH APPS_DATA_DIR "/data.bin"
#define STORAGE_PATH_LEGACY APPS_DATA_DIR "/data.txt"
#define ONBOARDING_DONE_PATH APPS_DATA_DIR "/onboarding_done"

#define AVOCADO_SAVE_MAGIC 0x41564143u

typedef struct {
    uint32_t magic;
    uint32_t last_timestamp;
    uint8_t dirty_level;
    uint8_t roots_length;
    uint8_t victory_seen;
    uint8_t reserved;
} AvocadoDataFile;

typedef struct {
    uint32_t last_timestamp;
    uint8_t dirty_level;
    uint8_t roots_length;
} AvocadoDataFileV1;

static void avocado_data_apply_defaults(AvocadoData *data) {
    data->last_timestamp = furi_hal_rtc_get_timestamp();
    data->dirty_level = 0;
    data->roots_length = 0;
    data->victory_seen = 0;
}

static bool avocado_parse_save_buffer(const uint8_t *buf, size_t read, AvocadoData *data) {
    if (read == sizeof(AvocadoDataFile)) {
        const AvocadoDataFile *f = (const AvocadoDataFile *)buf;
        if (f->magic == AVOCADO_SAVE_MAGIC) {
            data->last_timestamp = f->last_timestamp;
            data->dirty_level = f->dirty_level;
            data->roots_length = f->roots_length;
            data->victory_seen = f->victory_seen;
            return true;
        }
    }
    if (read == sizeof(AvocadoDataFileV1)) {
        const AvocadoDataFileV1 *v1 = (const AvocadoDataFileV1 *)buf;
        data->last_timestamp = v1->last_timestamp;
        data->dirty_level = v1->dirty_level;
        data->roots_length = v1->roots_length;
        data->victory_seen = (v1->roots_length >= AVOCADO_ROOTS_MAX) ? 1u : 0u;
        return true;
    }
    return false;
}

bool avocado_data_load(AvocadoData *data) {
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);
    uint8_t buf[sizeof(AvocadoDataFile)];

    const char *paths[] = {STORAGE_PATH, STORAGE_PATH_LEGACY};
    bool loaded = false;
    bool legacy = false;

    for (size_t i = 0; i < 2 && !loaded; i++) {
        if (!storage_file_open(file, paths[i], FSAM_READ, FSOM_OPEN_EXISTING)) {
            continue;
        }
        size_t read = storage_file_read(file, buf, sizeof(buf));
        storage_file_close(file);
        if (avocado_parse_save_buffer(buf, read, data)) {
            loaded = true;
            legacy = (i == 1);
        }
    }

    if (loaded && legacy) {
        storage_simply_remove(storage, STORAGE_PATH_LEGACY);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if (loaded) {
        return true;
    }

    avocado_data_apply_defaults(data);
    return false;
}

bool avocado_onboarding_should_show(void) {
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);

    const bool exists =
        storage_file_open(file, ONBOARDING_DONE_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return !exists;
}

void avocado_onboarding_complete(void) {
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);

    storage_common_mkdir(storage, APPS_DATA_DIR);
    if (storage_file_open(file, ONBOARDING_DONE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const uint8_t marker = 1;
        storage_file_write(file, &marker, sizeof(marker));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void avocado_data_save(const AvocadoData *data) {
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);

    storage_common_mkdir(storage, APPS_DATA_DIR);
    if (storage_file_open(file, STORAGE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        AvocadoDataFile f = {
            .magic = AVOCADO_SAVE_MAGIC,
            .last_timestamp = data->last_timestamp,
            .dirty_level = data->dirty_level,
            .roots_length = data->roots_length,
            .victory_seen = data->victory_seen,
            .reserved = 0,
        };
        storage_file_write(file, &f, sizeof(f));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
