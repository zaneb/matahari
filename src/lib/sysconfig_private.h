/* sysconfig_private.h - Copyright (C) 2011 Red Hat, Inc.
 * Written by Adam Stokes <astokes@fedoraproject.org>
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

#ifndef __MH_SYSCONFIG_PRIVATE_H_
#define __MH_SYSCONFIG_PRIVATE_H_

enum mh_result
sysconfig_os_run_uri(const char *uri, uint32_t flags, const char *scheme,
                     const char *key, mh_sysconfig_result_cb result_cb,
                     void *cb_data);

enum mh_result
sysconfig_os_run_string(const char *string, uint32_t flags, const char *scheme,
                        const char *key, mh_sysconfig_result_cb result_cb,
                        void *cb_data);

char *
sysconfig_os_query(const char *query, uint32_t flags, const char *scheme);

#endif /* __MH_SYSCONFIG_PRIVATE_H_ */
