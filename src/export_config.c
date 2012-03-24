/*
  Copyright (c) 2010 Fizians SAS. <http://www.fizians.com>
  This file is part of Rozofs.

  Rozofs is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation; either version 3 of the License,
  or (at your option) any later version.

  Rozofs is distributed in the hope that it will be useful, but
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
#include "export_config.h"

static int setting_to_export_config_ms(struct config_setting_t *setting,
                                       export_config_ms_t *
                                       export_config_ms) {

    int status;
    const char *uuid;
    const char *host;

    DEBUG_FUNCTION;

    if (config_setting_lookup_string(setting, "uuid", &uuid) == CONFIG_FALSE) {
        errno = ENOKEY;
        severe("can't find uuid.");
        status = -1;
        goto out;
    }

    if (config_setting_lookup_string(setting, "host", &host) == CONFIG_FALSE) {
        errno = ENOKEY;
        severe("can't find host.");
        status = -1;
        goto out;
    }

    uuid_parse(uuid, export_config_ms->uuid);
    strcpy(export_config_ms->host, host);

    status = 0;
out:
    return status;
}

static int setting_to_export_config_mfs(struct config_setting_t *setting,
                                        export_config_mfs_t *
                                        export_config_mfs) {

    int status;
    const char *root;
    const char *passwd;

    DEBUG_FUNCTION;

    if (config_setting_lookup_string(setting, "root", &root) == CONFIG_FALSE) {
        errno = ENOKEY;
        severe("can't find root.");
        status = -1;
        goto out;
    }

    if (config_setting_lookup_string(setting, "passwd", &passwd) ==
        CONFIG_FALSE) {
        errno = ENOKEY;
        severe("can't find passwd.");
        status = -1;
        goto out;
    }
    strcpy(export_config_mfs->root, root);
    memcpy(export_config_mfs->md5pass, passwd);

    status = 0;
out:
    return status;
}

int export_config_initialize(export_config_t * export_config,
                             const char *path) {

    int status, i;
    struct config_t config;
    struct config_setting_t *mss_setting = NULL;
    struct config_setting_t *mfss_setting = NULL;

    DEBUG_FUNCTION;

    list_init(&export_config->mss);
    list_init(&export_config->mfss);

    config_init(&config);

    if (config_read_file(&config, path) == CONFIG_FALSE) {
        severe("can't read config file: %s.", config_error_text(&config));
        status = -1;
        goto out;
    }

    if ((mss_setting = config_lookup(&config, "volume")) == NULL) {
        errno = ENOKEY;
        severe("can't find volume.");
        status = -1;
        goto out;
    }

    for (i = 0; i < config_setting_length(mss_setting); i++) {
        struct config_setting_t *ms_setting;
        export_config_ms_entry_t *entry;

        if ((entry = malloc(sizeof (export_config_ms_entry_t))) == NULL) {
            severe("malloc failed: %s.", strerror(errno));
            status = -1;
            goto out;
        }

        if ((ms_setting = config_setting_get_elem(mss_setting, i)) == NULL) {
            errno = EIO;        //XXX
            severe("can't get setting element: %s.",
                   config_error_text(&config));
            status = -1;
            goto out;
        }

        if (setting_to_export_config_ms(ms_setting, &entry->export_config_ms)
            != 0) {
            severe("can't get meta storage from setting: %s.",
                   strerror(errno));
            status = -1;
            goto out;
        }
        list_push_back(&export_config->mss, &entry->list);
    }

    if ((mfss_setting = config_lookup(&config, "exports")) == NULL) {
        errno = ENOKEY;
        severe("can't find exports.");
        status = -1;
        goto out;
    }

    for (i = 0; i < config_setting_length(mfss_setting); i++) {
        struct config_setting_t *mfs_setting;
        export_config_mfs_entry_t *entry;

        if ((entry = malloc(sizeof (export_config_mfs_entry_t))) == NULL) {
            severe("malloc failed: %s.", strerror(errno));
            status = -1;
            goto out;
        }

        if ((mfs_setting = config_setting_get_elem(mfss_setting, i)) == NULL) {
            errno = EIO;        //XXX
            severe("can't get setting element: %s.",
                   config_error_text(&config));
            status = -1;
            goto out;
        }

        if (setting_to_export_config_mfs
            (mfs_setting, &entry->export_config_mfs) != 0) {
            severe("can't get export from setting: %s.", strerror(errno));
            status = -1;
            goto out;
        }
        list_push_back(&export_config->mfss, &entry->list);
    }

    status = 0;
out:
    if (status != 0)
        errno = EIO;
    config_destroy(&config);
    return status;
}

int export_config_release(export_config_t * config) {

    DEBUG_FUNCTION;
    list_t *p, *q;

    if (config) {

        list_for_each_forward_safe(p, q, &config->mss) {
            export_config_ms_entry_t *entry =
                list_entry(p, export_config_ms_entry_t, list);
            list_remove(p);
            free(entry);
        }

        list_for_each_forward_safe(p, q, &config->mfss) {
            export_config_mfs_entry_t *entry =
                list_entry(p, export_config_mfs_entry_t, list);
            list_remove(p);
            free(entry);
        }
    }
    return 0;
}

void export_config_print(export_config_t * export_config) {

    list_t *iterator;

    DEBUG_FUNCTION;

    printf("export_config: %d mss, %d mfss\n", list_size(&export_config->mss),
           list_size(&export_config->mfss));
    puts("volume:");

    list_for_each_forward(iterator, &export_config->mss) {
        char uuid[37];
        export_config_ms_entry_t *entry =
            list_entry(iterator, export_config_ms_entry_t, list);
        uuid_unparse(entry->export_config_ms.uuid, uuid);
        printf("uuid: %s, host: %s\n", uuid, entry->export_config_ms.host);
    }

    puts("exports:");
    list_for_each_forward(iterator, &export_config->mfss) {
        export_config_mfs_entry_t *entry =
            list_entry(iterator, export_config_mfs_entry_t, list);
        printf("root: %s\n", entry->export_config_mfs->root);
        printf("passwd: %s\n", entry->export_config_mfs->md5pass);
    }
}
