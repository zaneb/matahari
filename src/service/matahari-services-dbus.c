/*
 * matahari-services-dbus.c
 *
 * Copyright (C) 2010 Red Hat, Inc.
 * Written by Radek Novacek <rnovacek@redhat.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <string.h>

#include "mh_dbus_common.h"

/* Services methods */
#include "matahari/services.h"
#include "matahari/utilities.h"

/* Generated properties list */
#include "matahari-services-dbus-properties.h"

/* DBus names */
#define SERVICES_BUS_NAME "org.matahariproject.Services"
#define SERVICES_OBJECT_PATH "/org/matahariproject/Services"
#define SERVICES_INTERFACE_NAME "org.matahariproject.Services"

#define TIMEOUT_MS 60000

/* Dbus methods */

gboolean
Services_list(Matahari *matahari, DBusGMethodInvocation *context)
{
  GError* error = NULL;
  int i = 0;

  if (!check_authorization(SERVICES_BUS_NAME ".list", &error, context))
  {
    dbus_g_method_return_error(context, error);
    return FALSE;
  }

  // Get list of services
  GList *services = services_list();

  // Convert GList to (char **)
  char **list = g_new(char *, g_list_length(services) + 1);
  for (; services != NULL; services = services->next)
    list[i++] = strdup(services->data);
  list[i] = NULL; // Sentinel

  dbus_g_method_return(context, list);
  g_strfreev(list);
  g_list_free(services);
  return TRUE;
}

gboolean
Services_enable(Matahari *matahari, const char *name, DBusGMethodInvocation *context)
{
  GError* error = NULL;
  if (!check_authorization(SERVICES_BUS_NAME ".enable", &error, context))
  {
    dbus_g_method_return_error(context, error);
    return FALSE;
  }
  svc_action_t *op = services_action_create(name, "enable", 0, TIMEOUT_MS);
  gboolean res = services_action_sync(op);
  services_action_free(op);
  dbus_g_method_return(context, res);
  return res;
}

gboolean
Services_disable(Matahari *matahari, const char *name, DBusGMethodInvocation *context)
{
  GError* error = NULL;
  if (!check_authorization(SERVICES_BUS_NAME ".disable", &error, context))
  {
    dbus_g_method_return_error(context, error);
    return FALSE;
  }
  svc_action_t *op = services_action_create(name, "disable", 0, TIMEOUT_MS);
  gboolean res = services_action_sync(op);
  services_action_free(op);
  dbus_g_method_return(context, res);
  return res;
}

gboolean
Services_start(Matahari *matahari, const char *name, unsigned int timeout, DBusGMethodInvocation *context)
{
  GError* error = NULL;
  if (!check_authorization(SERVICES_BUS_NAME ".start", &error, context))
  {
    dbus_g_method_return_error(context, error);
    return FALSE;
  }
  svc_action_t *op = services_action_create(name, "start", 0, timeout);
  services_action_sync(op);
  int rc = op->rc;
  services_action_free(op);
  dbus_g_method_return(context, rc);
  return TRUE;
}

gboolean
Services_stop(Matahari *matahari, const char *name, unsigned int timeout, DBusGMethodInvocation *context)
{
  GError* error = NULL;
  if (!check_authorization(SERVICES_BUS_NAME ".stop", &error, context))
  {
    dbus_g_method_return_error(context, error);
    return FALSE;
  }
  svc_action_t *op = services_action_create(name, "stop", 0, timeout);
  services_action_sync(op);
  int rc = op->rc;
  services_action_free(op);
  dbus_g_method_return(context, rc);
  return TRUE;
}

gboolean
Services_status(Matahari *matahari, const char *name, unsigned int interval, unsigned int timeout, DBusGMethodInvocation *context)
{
  GError* error = NULL;
  if (!check_authorization(SERVICES_BUS_NAME ".status", &error, context))
  {
    dbus_g_method_return_error(context, error);
    return FALSE;
  }
  svc_action_t *op = services_action_create(name, "status", interval, timeout);
  services_action_sync(op);
  int rc = op->rc;
  services_action_free(op);
  dbus_g_method_return(context, rc);
  return TRUE;
}

gboolean
Services_cancel(Matahari *matahari, const char *name, const char *action, unsigned int interval, DBusGMethodInvocation *context)
{
  GError* error = NULL;
  if (!check_authorization(SERVICES_BUS_NAME ".cancel", &error, context))
  {
    dbus_g_method_return_error(context, error);
    return FALSE;
  }
  int res = services_action_cancel(name, action, interval);
  dbus_g_method_return(context);
  return res;
}

gboolean
Services_fail(Matahari *matahari, const char *name, DBusGMethodInvocation *context)
{
  GError* error = NULL;
  if (!check_authorization(SERVICES_BUS_NAME ".fail", &error, context))
  {
    dbus_g_method_return_error(context, error);
    return FALSE;
  }
  // TODO: Implement when implemented in backend
  error = g_error_new(MATAHARI_ERROR, MATAHARI_NOT_IMPLEMENTED, "Action fail is not implemented yet!");
  dbus_g_method_return_error(context, error);
  return TRUE;
}

gboolean
Services_describe(Matahari *matahari, const char *name, DBusGMethodInvocation *context)
{
  GError* error = NULL;
  if (!check_authorization(SERVICES_BUS_NAME ".describe", &error, context))
  {
    dbus_g_method_return_error(context, error);
    return FALSE;
  }
  // TODO: Implement when implemented in backend
  error = g_error_new(MATAHARI_ERROR, MATAHARI_NOT_IMPLEMENTED, "Action describe is not implemented yet!");
  dbus_g_method_return_error(context, error);
  return TRUE;
}

/* Generated dbus stuff for services
 * MUST be after declaration of user defined functions.
 */
#include "matahari-services-dbus-glue.h"

void
matahari_set_property(GObject *object, guint property_id, const GValue *value,
    GParamSpec *pspec)
{
  // We don't have writable other property...
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

void
matahari_get_property(GObject *object, guint property_id, GValue *value,
    GParamSpec *pspec)
{
  switch (property_id)
    {
  case PROP_HOSTNAME:
    g_value_set_string (value, get_hostname());
    break;
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
    }
}

GType matahari_dict_type(int prop)
{
  g_printerr("Type of property %s is map of unknown types\n", properties[prop].name);
  return G_TYPE_VALUE;
}

int
main(int argc, char** argv)
{
  g_type_init();
  return run_dbus_server(SERVICES_BUS_NAME, SERVICES_OBJECT_PATH);
}