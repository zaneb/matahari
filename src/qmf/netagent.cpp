/* netagent.cpp - Copyright (C) 2010 Red Hat, Inc.
 * Written by Adam Stokes <astokes@fedoraproject.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.  A copy of the GNU General Public License is
 * also available at http://www.gnu.org/copyleft/gpl.html.
 */

#ifndef WIN32
#include "config.h"
#endif

#include "netagent.h"
#include <qpid/agent/ManagementAgent.h>
#include "qmf/com/redhat/matahari/net/ArgsNetworkList.h"
#include "qmf/com/redhat/matahari/net/ArgsNetworkStop.h"
#include "qmf/com/redhat/matahari/net/ArgsNetworkStart.h"
#include "qmf/com/redhat/matahari/net/ArgsNetworkStatus.h"
#include "qmf/com/redhat/matahari/net/ArgsNetworkDescribe.h"
#include "qmf/com/redhat/matahari/net/ArgsNetworkDestroy.h"

extern "C" { 
#include <netcf.h> 
};
#include <string.h>


struct netcf *ncf;

NetAgent::NetAgent(ManagementAgent* agent, char *_name)
{
    if(	ncf == NULL) {
	return;
    }
    this->_agent = agent;
    this->_management_object = new _qmf::Network(agent, this);
    agent->addObject(this->_management_object);
}

NetAgent::~NetAgent() { }

int
NetAgent::setup(ManagementAgent* agent)
{
    if (ncf_init(&ncf, NULL) < 0) {
	return -1;
    }
}

Manageable::status_t
NetAgent::ManagementMethod(uint32_t method, Args& arguments, string& text)
{
    int rc = 0;
    struct netcf_if *nif;
    unsigned int flags = NETCF_IFACE_ACTIVE;

    if(	ncf == NULL) {
	return Manageable::STATUS_NOT_IMPLEMENTED;
    }
    
    switch(method)
	{
	case _qmf::Network::METHOD_LIST:
	    {
		_qmf::ArgsNetworkList& ioArgs = (_qmf::ArgsNetworkList&) arguments;
		int max = 0, lpc = 0;
		char **iface_list = NULL;
		ioArgs.o_max = ncf_num_of_interfaces(ncf, NETCF_IFACE_ACTIVE|NETCF_IFACE_INACTIVE);
		
		iface_list = (char**)calloc(ioArgs.o_max+1, sizeof(char*));
		if(ncf_list_interfaces(ncf, ioArgs.o_max, iface_list, NETCF_IFACE_ACTIVE|NETCF_IFACE_INACTIVE) < 0) {
		    ioArgs.o_max = 0;
		}
		
		for(lpc = 0; lpc < ioArgs.o_max; lpc++) {
		    char *name = iface_list[lpc];
		    nif = ncf_lookup_by_name(ncf, name);
		    // ioArgs.o_iface_map[string(name)] = ncf_if_mac_string(nif);
		}
	    }
	    return Manageable::STATUS_OK;

	case _qmf::Network::METHOD_START:
	    {
		_qmf::ArgsNetworkStart& ioArgs = (_qmf::ArgsNetworkStart&) arguments;
		nif = ncf_lookup_by_name(ncf, ioArgs.i_iface.c_str());
		if(nif == NULL) {
		    ioArgs.o_status = -1;
		    return Manageable::STATUS_OK;

		} else if(ncf_if_status(nif, &flags) == 0) {
		    return Manageable::STATUS_OK;
		    
		} else if(ncf_if_up(nif) < 0)  {
		    return Manageable::STATUS_EXCEPTION;
		}

		if(ncf_if_status(nif, &flags) == 0) {
		    ioArgs.o_status = 1;
		} else {
		    ioArgs.o_status = 0;
		}
	    }
	    return Manageable::STATUS_OK;
	    
	case _qmf::Network::METHOD_STOP:
	    {
		_qmf::ArgsNetworkStop& ioArgs = (_qmf::ArgsNetworkStop&) arguments;
		nif = ncf_lookup_by_name(ncf, ioArgs.i_iface.c_str());
		if(nif == NULL) {
		    ioArgs.o_status = -1;
		    return Manageable::STATUS_OK;

		} else if(ncf_if_status(nif, &flags) < 0) {
		    return Manageable::STATUS_OK;
		    
		} else if(ncf_if_down(nif) < 0)  {
		    return Manageable::STATUS_EXCEPTION;
		}

		if(ncf_if_status(nif, &flags) == 0) {
		    ioArgs.o_status = 1;
		} else {
		    ioArgs.o_status = 0;
		}
	    }
	    return Manageable::STATUS_OK;

	case _qmf::Network::METHOD_STATUS:
	    {
		_qmf::ArgsNetworkStatus& ioArgs = (_qmf::ArgsNetworkStatus&) arguments;
		nif = ncf_lookup_by_name(ncf, ioArgs.i_iface.c_str());
		if(nif == NULL) {
		    ioArgs.o_status = -1;
		} else if(ncf_if_status(nif, &flags) == 0) {
		    ioArgs.o_status = 1;
		} else {
		    ioArgs.o_status = 0;
		}
	    }
	    return Manageable::STATUS_OK;

	case _qmf::Network::METHOD_DESCRIBE:
	    {
		_qmf::ArgsNetworkDescribe& ioArgs = (_qmf::ArgsNetworkDescribe&) arguments;
		nif = ncf_lookup_by_name(ncf, ioArgs.i_iface.c_str());
		if(nif != NULL) {
		    ioArgs.o_xml = ncf_if_xml_desc(nif);
		}
	    }
	    return Manageable::STATUS_OK;

	case _qmf::Network::METHOD_DESTROY:
	    {
		_qmf::ArgsNetworkDestroy& ioArgs = (_qmf::ArgsNetworkDestroy&) arguments;
		nif = ncf_lookup_by_name(ncf, ioArgs.i_iface.c_str());
		if(nif != NULL) {
		    ncf_if_undefine(nif);
		}
	    }
	    return Manageable::STATUS_OK;
	}
    return Manageable::STATUS_NOT_IMPLEMENTED;
}