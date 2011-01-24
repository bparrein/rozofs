/*
  Copyright (c) 2010 Fizians SAS. <http://www.fizians.com>
  This file is part of Rozo.

  Rozo is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation; either version 3 of the License,
  or (at your option) any later version.

  Rozo is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see
  <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libconfig.h>

#include "log.h"
#include "storage_config.h"

static int setting_to_storage_config_ms(struct config_setting_t *setting, storage_config_ms_t *storage_config_ms) {
    
    int status;
    const char *uuid;
    const char *root;

    DEBUG_FUNCTION;

    if (config_setting_lookup_string(setting, "uuid", &uuid) == CONFIG_FALSE) {
        status = -1;
        goto out;
    }

    if (config_setting_lookup_string(setting, "root", &root) == CONFIG_FALSE) {
        status = -1;
        goto out;
    }

    uuid_parse(uuid, storage_config_ms->uuid);
    strcpy(storage_config_ms->root, root);
    if (storage_config_ms->root[strlen(storage_config_ms->root)] != '/') strcat(storage_config_ms->root, "/");

    status = 0;
out:
    return status;
}

int storage_config_initialize(storage_config_t *storage_config, const char* path) {

    int status, i;
    struct config_t config;
    struct config_setting_t *storages_setting = NULL;
    long config_value;

    DEBUG_FUNCTION;

    list_init(storage_config);

    config_init(&config);

    if (config_read_file(&config, path) == CONFIG_FALSE) {
        severe("failed: %s", config_error_text(&config));
        status = -1;
        goto out;
    }

    if ((storages_setting = config_lookup(&config, "storages")) == NULL) {
        status = -1;
        goto out;
    }

    for (i = 0; i < config_setting_length(storages_setting); i++) {
        struct config_setting_t *ms_setting;
        storage_config_ms_entry_t *entry;

        if ((entry = malloc(sizeof(storage_config_ms_entry_t))) == NULL) {
            status = -1;
            goto out;
        }

        if ((ms_setting = config_setting_get_elem(storages_setting, i)) == NULL) {
            status = -1;
            goto out;
        }

        if (setting_to_storage_config_ms(ms_setting, &entry->storage_config_ms) != 0) {
            status = -1;
            goto out;
        }
        list_push_back(storage_config, &entry->list);
    }

    status = 0;
out:
    if (status != 0) errno = EIO;
    config_destroy(&config);
    return status;
}

int storage_config_release(storage_config_t *config) {

    DEBUG_FUNCTION;
    list_t *p, *q;

    if (config) {
        list_for_each_forward_safe(p, q, config) {
            storage_config_ms_entry_t *entry = list_entry(p, storage_config_ms_entry_t, list);
            list_remove(p);
            free(entry);
        }
    }
    return 0;
}

void storage_config_print(storage_config_t *storage_config) {

    int i;
    list_t *iterator;

    DEBUG_FUNCTION;

    printf("storage_config: %d storages\n", list_size(storage_config));

    list_for_each_forward(iterator, storage_config) {
        char uuid[37];
        storage_config_ms_entry_t *entry = list_entry(iterator, storage_config_ms_entry_t, list);
        uuid_unparse(entry->storage_config_ms.uuid, uuid);
        printf("uuid: %s, root: %s\n", uuid, entry->storage_config_ms.root);
    }
}

