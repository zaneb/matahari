#!/usr/bin/env python

"""
  test_resource_api.py - Copyright (c) 2011 Red Hat, Inc.
  Written by Dave Johnson <dajo@redhat.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the
  Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
"""

import commands as cmd
import matahariTest as testUtil
from qmf2 import QmfAgentException
import unittest
import time
import sys
import os

connection = None
qmf = None

# Initialization
# =====================================================
def setUpModule():
    global connection, qmf

    connection = ResourcesTestsSetup()
    qmf = connection.qmf

def tearDownModule():
    global connection
    connection.tearDown()

class ResourcesTestsSetup(testUtil.TestsSetup):
    def __init__(self):
        testUtil.TestsSetup.__init__(self, "matahari-qmf-serviced", "service", "Resources")

class TestResourceApi(unittest.TestCase):

    # TEST - getProperties()
    # =====================================================
    def test_hostname_property(self):
        value = qmf.props.get('hostname')
        self.assertEquals(value, cmd.getoutput("hostname"), "hostname not matching")

    # TEST - fail()
    # =====================================================
    def test_fail_not_implemented(self):
        self.assertRaises(QmfAgentException, qmf.fail, "crond", 0)

    # TEST - describe()
    # =====================================================
    def test_describe_not_implemented(self):
        self.assertRaises(QmfAgentException, qmf.describe, 'ocf','heartbeat','IPaddr')

    # TEST - describe()
    # =====================================================
    #def test_list_standards_empty(self):





#def compareList(results, expected):
#    if len(results) != len(expected):
#        m.error("result count not matching expected")
#    for item in (results):
#        try:
#            expected.index(str(item))
#        except:
#            m.error(str(item)+"not expected, new?")

# TEST - list_standards()
# ================================================================
#m.printHeader('list_standards')
#results = resource.list_standards().outArgs['standards']
#expected = ['ocf', 'lsb', 'windows']
#compareList(results, expected)

# TEST - list_providers()
# ================================================================
#m.printHeader('list_providers')
#results = resource.list_providers('ocf').outArgs['providers']
#expected = ['heartbeat', 'redhat']
#compareList(results, expected)

# TEST - list()
# ================================================================
#m.printHeader('list()')
#expected = ['AoEtarget', 'AudibleAlarm', 'CTDB', 'ClusterMon', 'Delay', 'Dummy', 'EvmsSCC', 'Evmsd', 'Filesystem', 'ICP', 'IPaddr', 'IPaddr2', 'IPsrcaddr', 'IPv6addr', 'LVM', 'LinuxSCSI', 'MailTo', 'ManageRAID', 'ManageVE', 'Pure-FTPd', 'Raid1', 'Route', 'SAPDatabase', 'SAPInstance', 'SendArp', 'ServeRAID', 'SphinxSearchDaemon', 'Squid', 'Stateful', 'SysInfo', 'VIPArip', 'VirtualDomain', 'WAS', 'WAS6', 'WinPopup', 'Xen', 'Xinetd', 'anything', 'apache', 'conntrackd', 'db2', 'drbd', 'eDir88', 'ethmonitor', 'exportfs', 'fio', 'iSCSILogicalUnit', 'iSCSITarget', 'ids', 'iscsi', 'jboss', 'lxc', 'mysql', 'mysql-proxy', 'nfsserver', 'nginx', 'oracle', 'oralsnr', 'pgsql', 'pingd', 'portblock', 'postfix', 'proftpd', 'rsyncd', 'scsi2reservation', 'sfex', 'symlink', 'syslog-ng', 'tomcat', 'vmware']
#results = resource.list('ocf', 'heartbeat').outArgs['agents']
#compareList(results, expected)

# TEST - invoke()
# ================================================================
#m.printHeader('invoke()')
#print "\033[93m[WARN]\033[0m  NO VERIFICATION"

# TEST - cancel()
# ================================================================
#m.printHeader('cancel()')
#print "\033[93m[WARN]\033[0m  NO VERIFICATION"


