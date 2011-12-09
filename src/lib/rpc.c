/* rpc.c - Copyright (C) 2011 Red Hat, Inc.
 * Written by Zane Bitter <zbitter@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "matahari/logging.h"
#include "matahari/rpc.h"
#include "rpc_private.h"


#define FOREACH_API(P_API, API)                 \
    for ((P_API) = &apis[0], (API) = *(P_API);  \
         api_in_range((P_API) - apis);          \
         (API) = *++(P_API))                    \
        if ((API) != NULL)


const struct mh_rpc_api * const apis[] = {
    [MH_RPC_PYTHON] = &mh_rpc_python_api,
};


static int
api_in_range(size_t api_index)
{
    return api_index >= 0 && api_index < (sizeof(apis) / sizeof(*apis));
}

static int
api_defined(size_t api_index)
{
    return api_in_range(api_index) && apis[api_index] != NULL;
}

void
mh_rpc_init(int argc, char **argv)
{
    const struct mh_rpc_api * const *p_api, *api;

    FOREACH_API(p_api, api) {
        api->init(argc, argv);
    }
}

void
mh_rpc_deinit(void)
{
    const struct mh_rpc_api * const *p_api, *api;

    FOREACH_API(p_api, api) {
        api->deinit();
    }
}

int
mh_rpc_load_plugins(size_t *count, mh_rpc_plugin_t **plugins)
{
    const struct mh_rpc_api * const *p_api, *api;

    FOREACH_API(p_api, api) {
        api->load_plugins(count, plugins);
    }

    return 0;
}

int
mh_rpc_call_create(mh_rpc_t *pcall,
                   const char *procedure)
{
    *pcall = calloc(1, sizeof(**pcall));
    if (!*pcall) {
        mh_err("Failed to allocate RPC call structure");
        return -1;
    }

    (*pcall)->procedure = strdup(procedure);
    if (!(*pcall)->procedure) {
        mh_err("Failed to allocation RPC call name");
        free(*pcall);
        *pcall = NULL;
        return -1;
    }

    return 0;
}

static void
arg_list_destroy(struct rpc_arg_list *args)
{
    size_t i;

    for (i = 0; i < args->len; i++) {
        struct rpc_arg *arg = &args->list[i];
        free(arg->key);
        arg->key = NULL;
        free(arg->val);
        arg->val = NULL;
    }

    free(args->list);
    args->len = 0;
    args->list = NULL;
}

int
mh_rpc_call_destroy(mh_rpc_t call)
{
    free(call->procedure);
    call->procedure = NULL;

    arg_list_destroy(&call->args);
    arg_list_destroy(&call->kwargs);

    free(call);
    return 0;
}

static int
add_arg(struct rpc_arg_list *args, char *key, char *val)
{
    struct rpc_arg *new_arg;
    struct rpc_arg *new_list = realloc(args->list,
                                       sizeof(*args->list) * (args->len + 1));
    if (!new_list) {
        return -1;
    }

    args->list = new_list;
    new_arg = &new_list[args->len++];

    new_arg->key = key;
    new_arg->val = val;

    return 0;
}

int
mh_rpc_call_add_arg(mh_rpc_t call, const char *arg)
{
    int ret = -1;
    char *stored = strdup(arg);
    if (!stored) {
        return -1;
    }

    ret = add_arg(&call->args, NULL, stored);

    if (ret) {
        free(stored);
    }

    return ret;
}

int
mh_rpc_call_add_kw_arg(mh_rpc_t call, const char *key, const char *arg)
{
    int ret = -1;
    char *stored_arg, *stored_key;

    stored_arg = strdup(arg);
    if (!stored_arg) {
        return -1;
    }
    stored_key = strdup(key);
    if (!stored_key) {
        goto cleanup_arg;
    }

    ret = add_arg(&call->kwargs, stored_key, stored_arg);

    if (ret) {
        free(stored_key);
cleanup_arg:
        free(stored_arg);
    }

    return ret;
}


enum mh_result
mh_rpc_invoke(const mh_rpc_plugin_t *plugin,
              const struct mh_rpc *call,
              mh_rpc_result_t *output)
{
    memset(output, 0, sizeof(*output));

    if (!api_defined(plugin->type)) {
        return MH_RES_BACKEND_ERROR;
    }

    return apis[plugin->type]->invoke(plugin->impl, call, output);
}
