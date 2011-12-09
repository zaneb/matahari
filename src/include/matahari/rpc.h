/* rpc.h - Copyright (C) 2011 Red Hat, Inc.
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

#ifndef __MH_RPC_H__
#define __MH_RPC_H__

#include "matahari/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mh_rpc_plugin_impl;

typedef struct {
    char *name;
    unsigned int type;
    struct mh_rpc_plugin_impl *impl;
} mh_rpc_plugin_t;

struct mh_rpc;
typedef struct mh_rpc* mh_rpc_t;

typedef struct {
    char *result;
    char *exc_type;
    char *exc_value;
    char *exc_traceback;
} mh_rpc_result_t;


/**
 * Initialise the RPC library with the given CLI options to rpcd.
 */
void
mh_rpc_init(int argc, char **argv);

/**
 * De-initialise the RPC library and clean up.
 */
void
mh_rpc_deinit(void);

/**
 * Load the available RPC plugins.
 * \param count pointer in which to store the plugin count. Should be
 * initialised to 0.
 * \param plugins pointer in which to store the list of plugins. Should be
 * initialised to NULL.
 * \return zero on success, non-zero otherwise.
 */
int
mh_rpc_load_plugins(size_t *count, mh_rpc_plugin_t **plugins);

/**
 * Create a Remote Method Invocation.
 * This should be freed with mh_rpc_rmi_destroy() when done.
 * \param pcall pointer in which to store the RMI data.
 * \param procedure the name of the procedure to call.
 * \return zero on success, non-zero otherwise.
 */
int
mh_rpc_call_create(mh_rpc_t *pcall,
                   const char *procedure);

/**
 * Clean up a Remote Method Invocation.
 * \param call the data to clean up.
 */
int
mh_rpc_call_destroy(mh_rpc_t call);

/**
 * Add a positional argument to the argument list for the RMI.
 * \param call the RMI data.
 * \param arg the argument to add.
 */
int
mh_rpc_call_add_arg(mh_rpc_t call, const char *arg);

/**
 * Add a keyword argument to the keyword argument map for the RMI.
 * \param call the RMI data.
 * \param key the argument keyword.
 * \param arg the argument to add.
 */
int
mh_rpc_call_add_kw_arg(mh_rpc_t call, const char *key, const char *arg);

/**
 * Invoke a remote procedure call.
 * \param plugin the plugin to invoke the method from.
 * \param call the RMI data.
 * \param output pointer in which to store the result string (in JSON format).
 * This needs to be freed with free().
 * \return the return status code.
 */
enum mh_result
mh_rpc_invoke(const mh_rpc_plugin_t *plugin,
              const struct mh_rpc *call,
              mh_rpc_result_t *output);

#ifdef __cplusplus
}
#endif

#endif
