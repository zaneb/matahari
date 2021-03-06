/* sysconfig.h - Copyright (C) 2011 Red Hat, Inc.
 * Written by Adam Stokes <astokes@fedoraproject.org>
 * Written by Russell Bryant <rbryant@redhat.com>
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

/**
 * \file
 * \brief Config API
 * \ingroup coreapi
 */

#ifndef __MH_SYSCONFIG_H__
#define __MH_SYSCONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <glib.h>

#include "matahari/errors.h"

/*! Supported FLAGS */
#define MH_SYSCONFIG_FLAG_FORCE    (1 << 0)

/**
 * Callback for run_uri or run_string requests.
 *
 * \param[in] data cb_data provided with a run_string or run_uri request
 * \param[in] res exit code from executed request
 */
typedef void (*mh_sysconfig_result_cb)(void *data, int res);

/**
 * Download and process URI for configuration
 *
 * \param[in] uri the url of configuration item
 * \param[in] flags flags used
 * \param[in] scheme the type of configuration i.e. puppet
 * \param[in] key configuration key for keeping track of existing
 *            configuration runs
 * \param[in] result_cb This request may have to be executed asynchronously.  If this
 *            function returns success (0), then this result callback will be called with the
 *            final result of the request.
 * \param[in] cb_data custom data to be passed to the result callback.
 *
 * \return See enum mh_result
 */
enum mh_result
mh_sysconfig_run_uri(const char *uri, uint32_t flags, const char *scheme, const char *key,
                     mh_sysconfig_result_cb result_cb, void *cb_data);

/**
 * Process a text blob
 *
 * \param[in] string the text blob to process, accepts a range of data from XML
 *            to Puppet classes or newline-separated Augeas commands.
 * \param[in] flags flags used
 * \param[in] scheme the type of configuration i.e. puppet
 * \param[in] key configuration key for keeping track of existing
 *            configuration runs
 * \param[in] result_cb This request may have to be executed asynchronously.  If this
 *            function returns success (0), then this result callback will be called with the
 *            final result of the request.
 * \param[in] cb_data custom data to be passed to the result callback.
 *
 * \return See enum mh_result
 */
enum mh_result
mh_sysconfig_run_string(const char *string, uint32_t flags, const char *scheme,
                        const char *key, mh_sysconfig_result_cb result_cb,
                        void *cb_data);

/**
 * Query against a configuration object on the system
 *
 * \param[in] query the query command used
 * \param[in] flags flags used
 * \param[in] scheme the type of configuration i.e. puppet
 *
 * \note The return of this routine must be freed with free()
 *
 * \return DATA of returned query result
 */
char *
mh_sysconfig_query(const char *query, uint32_t flags, const char *scheme);

/**
 * Set system as configured
 *
 * \param[in] key config item to define
 * \param[in] contents specifies success/fail with error where applicable
 *
 * \return See enum mh_result
 */
enum mh_result
mh_sysconfig_set_configured(const char *key, const char *contents);

/**
 * Test if system has been configured
 *
 * \param[in] key config item to test if used
 *
 * \note The return of this routine must be freed with free()
 *
 * \return STATUS of key
 */
char *
mh_sysconfig_is_configured(const char *key);

#ifdef __cplusplus
}
#endif

#endif // __MH_SYSCONFIG_H__
