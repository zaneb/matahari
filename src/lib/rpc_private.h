/* rpc-private.h - Copyright (C) 2011 Red Hat, Inc.
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

#ifndef __MH_RPC_PRIVATE_H__
#define __MH_RPC_PRIVATE_H__

#include "matahari/rpc.h"

enum mh_rpc_plugin_type {
    MH_RPC_INVALID,
    MH_RPC_PYTHON,
};

struct mh_rpc_api {
    void (*init)(int, char**);
    void (*deinit)(void);
    int (*load_plugins)(size_t*, mh_rpc_plugin_t**);
    enum mh_result (*invoke)(const struct mh_rpc_plugin_impl*,
                             const struct mh_rpc*,
                             mh_rpc_result_t *output);
};

extern struct mh_rpc_api mh_rpc_python_api;


struct rpc_arg {
    char *key;
    char *val;
};

struct rpc_arg_list {
    struct rpc_arg *list;
    size_t len;
};

struct mh_rpc {
    char *procedure;
    struct rpc_arg_list args, kwargs;
};

#endif
