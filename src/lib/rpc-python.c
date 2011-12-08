/* rpc-python.c - Copyright (C) 2011 Red Hat, Inc.
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

#include <Python.h>
#include <stdio.h>

#include "matahari/rpc.h"
#include "rpc_private.h"
#include "matahari/logging.h"
#include "matahari/errors.h"


struct mh_rpc_plugin_impl {
    PyObject *plugin;
};


static void
init(int argc, char **argv)
{
    if (!Py_IsInitialized()) {
        Py_Initialize();
    }
    PySys_SetArgvEx(argc, argv, 0);
}

static void
deinit(void)
{
    if (Py_IsInitialized()) {
        Py_Finalize();
    }
}

/**
 * Call a function in the rpc module.
 *
 * This function creates a new reference to the returned object.
 *
 * \param funcname the name of the function to call.
 * \param args a sequence of arguments to pass to the function, or NULL if no
 * arguments are required.
 * \return the result of the function call.
 */
static PyObject *
call_rpc_server_func(const char *funcname, PyObject *args)
{
    PyObject *rpc_mod, *func, *result;

    rpc_mod = PyImport_ImportModule("rpc"); // TODO: move to matahari package
    if (!rpc_mod) {
        mh_err("Error importing Matahari RPC Python module");
        PyErr_Print();
        return NULL;
    }

    func = PyObject_GetAttrString(rpc_mod, funcname);
    if (!func) {
        mh_err("Error getting %s() function from Matahari RPC module", funcname);
        PyErr_Print();
        Py_DECREF(rpc_mod);
        return NULL;
    }

    result = PyObject_CallObject(func, args);
    if (!result) {
        mh_err("Error calling %s() in Matahari RPC module", funcname);
        PyErr_Print();
    }

    Py_DECREF(func);
    Py_DECREF(rpc_mod);
    return result;
}

/**
 * Get the name of a plugin as a C string. The string must later be released
 * by calling free().
 * \param plugin an rpc.Plugin object.
 * \return a newly-malloc()ed string containing the plugin name, or NULL on
 * failure.
 */
static char *
get_plugin_name(PyObject *plugin)
{
    char *name_str, *dup_str;
    PyObject *name = PyObject_GetAttrString(plugin, "name");
    if (!name) {
        mh_err("Failed to get plugin name");
        PyErr_Print();
        return NULL;
    }

    name_str = PyString_AsString(name);
    if (!name_str) {
        mh_err("Plugin name is not a string");
        PyErr_Print();
        Py_DECREF(name);
        return NULL;
    }

    dup_str = strdup(name_str);
    Py_DECREF(name);

    return dup_str;
}

static int
load_plugins(size_t *count, mh_rpc_plugin_t **plugins)
{
    PyObject *pluginList;
    Py_ssize_t length, i;
    int result = 1;

    if (!Py_IsInitialized()) {
        mh_err("Python not initialised");
        return 1;
    }

    pluginList = call_rpc_server_func("plugins", NULL);
    if (!pluginList) {
        return 1;
    }

    if (!PySequence_Check(pluginList)) {
        mh_err("Plugin list is not a sequence");
        PyErr_Print();
        goto cleanup;
    }

    length = PySequence_Length(pluginList);
    if (length < 0) {
        mh_err("Error getting length of plugin list");
        PyErr_Print();
        goto cleanup;
    }

    for (i = 0; i < length; i++) {
        mh_rpc_plugin_t *tmp_list;
        mh_rpc_plugin_t new_plugin;
        struct mh_rpc_plugin_impl *new_impl;

        PyObject *plugin = PySequence_GetItem(pluginList, i);
        if (!plugin) {
            mh_err("Error retrieving plugin module");
            PyErr_Print();
            continue;
        }

        new_plugin.name = get_plugin_name(plugin);
        if (!new_plugin.name) {
            Py_DECREF(plugin);
            continue;
        }

        tmp_list = realloc(*plugins, sizeof(mh_rpc_plugin_t) * (*count + 1));
        if (!tmp_list) {
            mh_err("Out of Memory");
            Py_DECREF(plugin);
            goto cleanup;
        }
        *plugins = tmp_list;

        new_impl = calloc(1, sizeof(*new_impl));
        if (!new_impl) {
            mh_err("Out of Memory");
            Py_DECREF(plugin);
            goto cleanup;
        }
        new_impl->plugin = plugin;

        new_plugin.type = MH_RPC_PYTHON;
        new_plugin.impl = new_impl;

        memcpy(&tmp_list[(*count)++], &new_plugin, sizeof(new_plugin));
    }

    result = 0;

cleanup:
    Py_DECREF(pluginList);

    return result;
}

/**
 * Convert the supplied Python string to a newly-allocated C string. This
 * string must later be released with a call to free().
 *
 * This function consumes a reference to the input Python string.
 *
 * \param object the Python string object.
 * \return pointer to a new C string containing the Python object's string.
 */
static char *
py_to_c_string(PyObject *object)
{
    char *str;

    if (!object) {
        return NULL;
    }

    str = PyString_AsString(object);
    if (!str) {
        mh_err("Error getting string");
        PyErr_Print();
        Py_DECREF(object);
        return NULL;
    }
    str = strdup(str);

    Py_DECREF(object);
    return str;
}

char **get_procedures(const struct mh_rpc_plugin_impl *plugin)
{
    PyObject *proclist;
    Py_ssize_t len;
    size_t i;
    char **procs = NULL;

    proclist = PyObject_CallMethod(plugin->plugin, "procedures", NULL);
    if (!proclist) {
        mh_err("Failed to get procedure list");
        PyErr_Print();
        return NULL;
    }

    len = PySequence_Length(proclist);
    if (len < 0) {
        mh_err("Failed to get length of procedure list");
        Py_DECREF(proclist);
        return NULL;
    }

    procs = malloc(sizeof(char*) * (len + 1));
    if (!procs) {
        mh_err("Out of memory");
        return NULL;
    }

    for (i = 0; i < len; i++) {
        procs[i] = py_to_c_string(PySequence_GetItem(proclist, i));
        if (!procs[i]) {
            break;
        }
        mh_debug("Found procedure \"%s\"", procs[i]);
    }
    procs[len] = NULL;

    Py_DECREF(proclist);

    return procs;
}

/**
 * Return a Python string for an argument value.
 *
 * This function creates a new reference to the returned object.
 *
 * \param arg the argument to convert.
 * \return a Python string containg the argument value.
 */
static PyObject *
get_arg_item(const struct rpc_arg* arg)
{
    return PyString_FromString(arg->val);
}

/**
 * Return a (key, value) tuple for a keyword argument.
 *
 * This function creates a new reference to the returned object.
 *
 * \param arg the argument to convert.
 * \return a Python tuple containg the key and value as Python strings.
 */
static PyObject *
get_kwarg_item(const struct rpc_arg* arg)
{
    return Py_BuildValue("(ss)", arg->key, arg->val);
}

/**
 * Return the list of arguments as a Python tuple.
 *
 * This function creates a new reference to the returned object.
 *
 * \param args the argument list.
 * \param get_item callback for turning each argument into a Python object.
 * \return a tuple containing the arguments converted to Python objects.
 */
static PyObject *
get_arg_list(const struct rpc_arg_list *args,
             PyObject *(*get_item)(const struct rpc_arg*))
{
    PyObject *arglist;
    size_t i;

    arglist = PyTuple_New(args->len);
    if (!arglist) {
        return NULL;
    }

    for (i = 0; i < args->len; i++) {
        PyTuple_SetItem(arglist, i, get_item(&args->list[i]));
    }

    return arglist;
}

/**
 * Fetch the latest Python exception and format it as strings suitable for
 * returning in the RPC output.
 * \param output the RPC result output.
 * \return zero on success, non-zero on failure.
 */
static int
encode_exception(mh_rpc_result_t *output)
{
    int ret = 1;
    PyObject *exc_type, *exc_value, *exc_traceback, *tuple, *formatted;

    PyErr_Fetch(&exc_type, &exc_value, &exc_traceback);

    tuple = Py_BuildValue("(OOO)", exc_type, exc_value, exc_traceback);
    if (!tuple) {
        mh_err("Error collating exception data");
        PyErr_Print();
        goto cleanup_exc;
    }

    formatted = call_rpc_server_func("formatException", tuple);
    if (!formatted) {
        mh_err("Error formatting exception data");
        PyErr_Print();
        goto cleanup_tuple;
    }

    output->exc_type = py_to_c_string(PySequence_GetItem(formatted, 0));
    output->exc_value = py_to_c_string(PySequence_GetItem(formatted, 1));
    output->exc_traceback = py_to_c_string(PySequence_GetItem(formatted, 2));

    ret = 0;

    Py_DECREF(formatted);
cleanup_tuple:
    Py_DECREF(tuple);

cleanup_exc:
    Py_DECREF(exc_type);
    Py_DECREF(exc_value);
    Py_DECREF(exc_traceback);

    return ret;
}

static enum mh_result
invoke(const struct mh_rpc_plugin_impl *plugin,
       const struct mh_rpc *call,
       mh_rpc_result_t *output)
{
    PyObject *arg_list, *kwarg_list, *result;

    arg_list = get_arg_list(&call->args, &get_arg_item);
    if (!arg_list) {
        return MH_RES_BACKEND_ERROR;
    }

    kwarg_list = get_arg_list(&call->kwargs, &get_kwarg_item);
    if (!kwarg_list) {
        Py_DECREF(arg_list);
        return MH_RES_BACKEND_ERROR;
    }

    result = PyObject_CallMethod(plugin->plugin, "call", "sOO",
                                 call->procedure, arg_list, kwarg_list);
    if (result) {
        mh_debug("Remote procedure call successful");

        Py_DECREF(kwarg_list);
        Py_DECREF(arg_list);

        output->result = py_to_c_string(result);
        if (!output->result) {
            return MH_RES_BACKEND_ERROR;
        }
    } else {
        int ret;

        mh_debug("Exception invoking remote procedure call");
        ret = encode_exception(output);

        Py_DECREF(kwarg_list);
        Py_DECREF(arg_list);

        if (ret) {
            return MH_RES_BACKEND_ERROR;
        }
    }

    return MH_RES_SUCCESS;
}


struct mh_rpc_api mh_rpc_python_api = {
    init,
    deinit,
    load_plugins,
    get_procedures,
    invoke,
};
