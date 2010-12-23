/* 
 * Copyright (C) 2010 Andrew Beekhof <andrew@beekhof.net>
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
#include "matahari/logging.h"
#include "matahari/mainloop.h"
#include "matahari/services.h"
#include "services_private.h"

gboolean
services_os_action_execute(svc_action_t* op, gboolean synchronous)
{
    return TRUE;
}

GList *
services_os_get_directory_list(const char *root, gboolean files)
{
    return NULL;
}
