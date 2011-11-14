#!/usr/bin/env python

"""
  test_service_api.py - Copyright (c) 2011 Red Hat, Inc.
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
from qmf2 import QmfAgentException
import matahariTest as testUtil
import unittest
import time
import sys
import os

connection = None
qmf = None
test_svc = 'snmpd'

# Initialization
# =====================================================
def setUpModule():
    result = cmd.getstatusoutput("yum -y install net-snmp")
    if result[0] != 0:
        sys.exit("unable to install snmp server (used for testing service control)")
    # prepatory cleanup
    if os.path.isfile("/etc/init.d/"+test_svc+"-slow"):
        cmd.getoutput("rm -rf /etc/init.d/"+test_svc+"-slow")
    if not os.path.isfile("/etc/init.d/"+test_svc):
        cmd.getoutput("mv /etc/init.d/"+test_svc+".orig /etc/init.d/"+test_svc)

    global connection, qmf
    connection = ServicesTestsSetup()
    qmf = connection.qmf

def tearDownModule():
    global connection
    connection.tearDown()

class ServicesTestsSetup(testUtil.TestsSetup):
    def __init__(self):
        testUtil.TestsSetup.__init__(self, "matahari-qmf-serviced", "service", "Services")


class TestServiceApi(unittest.TestCase):

    # TEST - getProperties()
    # =====================================================
    def test_hostname_property(self):
        value = qmf.props.get('hostname')
        self.assertEquals(value, cmd.getoutput("hostname"), "hostname not matching")

    # TEST - list()
    # =====================================================
    def test_list(self):
        result = qmf.list()
        dirlist = os.listdir("/etc/init.d")
        api_list = result.get("agents")
        self.assertTrue(len(api_list) == (len(dirlist)-1), "service count not mataching")
        for svc in dirlist:
            if (svc != 'functions'):
                try:
                    api_list.index(svc)
                except:
                    self.fail(str(svc) + " service missing")

    # TEST - disable()
    # =====================================================
    def test_disable_known_service(self):
        # make sure service enabled
	cmd.getoutput("chkconfig "+test_svc+" on")
        # test
	dis = qmf.disable(test_svc)
        # verify
	chk_off = cmd.getoutput("chkconfig --list "+test_svc)
	self.assertFalse("2:off" not in chk_off, "run level 2 wrong")
	self.assertFalse("3:off" not in chk_off, "run level 3 wrong")
	self.assertFalse("4:off" not in chk_off, "run level 4 wrong")
	self.assertFalse("5:off" not in chk_off, "run level 5 wrong")

    def test_disable_unknown_service(self):
        result = qmf.enable("zzzzz")
        self.assertFalse(result.get('rc') == 0, "Return code not expected (" + str(result.get('rc')) + ")")

    # TEST - enable()
    # =====================================================
    def test_enable_known_service(self):
        # make sure service disabled
        cmd.getoutput("chkconfig "+test_svc+" off")
        # test
        dis = qmf.enable(test_svc)
        # verify
        chk_on = cmd.getoutput("chkconfig --list "+test_svc)
        self.assertTrue("2:on" in chk_on, "run level 2 wrong")
        self.assertTrue("3:on" in chk_on, "run level 3 wrong")
        self.assertTrue("4:on" in chk_on, "run level 4 wrong")
        self.assertTrue("5:on" in chk_on, "run level 5 wrong")

    def test_enable_unknown_service(self):
        result = qmf.enable("zzzzz")
        self.assertFalse(result.get('rc') == 0, "Return code not expected (" + str(result.get('rc')) + ")")

    # TEST - stop()
    # =====================================================

    def test_stop_running_known_service(self):
        # pre-req
        cmd.getoutput("service "+test_svc+" start")
        # test
        result = qmf.stop(test_svc,10000)
        # verify
        self.assertTrue(result.get('rc') == 0, "Return code not expected (" + str(result.get('rc')) + ")")
        svc_status = cmd.getoutput("service "+test_svc+" status")
        self.assertTrue("running" not in svc_status, "text not found, still running?")

    def test_stop_stopped_known_service(self):
        # pre-req
        cmd.getoutput("service "+test_svc+" stop")
        # test
        result = qmf.stop(test_svc,10000)
        # verify
        self.assertTrue(result.get('rc') == 0, "Return code not expected (" + str(result.get('rc')) + ")")
        svc_status = cmd.getoutput("service "+test_svc+" status")
        self.assertTrue("running" not in svc_status, "text not found, still running?")

    def test_stop_unknown_service(self):
        result = qmf.stop("zzzzz", 10000)
        self.assertFalse(result.get('rc') == 0, "Return code not expected (" + str(result.get('rc')) + ")")

    # TEST - start()
    # =====================================================

    def test_start_stopped_known_service(self):
        # pre-req
        cmd.getoutput("service "+test_svc+" stop")
        # test
        result = qmf.start(test_svc,10000)
        # verify
        self.assertTrue(result.get('rc') == 0, "Return code not expected (" + str(result.get('rc')) + ")")
        svc_status = cmd.getoutput("service "+test_svc+" status")
        self.assertTrue("running" in svc_status, "text not found, still running?")

    def test_start_running_known_service(self):
        # pre-req
        cmd.getoutput("service "+test_svc+" start")
        # test
        result = qmf.start(test_svc,10000)
        # verify
        self.assertTrue(result.get('rc') == 0, "Return code not expected (" + str(result.get('rc')) + ")")
        svc_status = cmd.getoutput("service "+test_svc+" status")
        self.assertTrue("running" in svc_status, "text not found, still running?")

    def test_start_unknown_service(self):
        result = qmf.start("zzzzz", 10000)
        self.assertFalse(result.get('rc') == 0, "Return code not expected (" + str(result.get('rc')) + ")")

    # TEST - status()
    # =====================================================

    def test_status_stopped_known_service(self):
        # pre-req
        cmd.getoutput("service "+test_svc+" stop")
        # test
        result = qmf.status(test_svc,10000)
        # verify
        self.assertTrue(result.get('rc') == 3, "Return code not expected (" + str(result.get('rc')) + ")")

    def test_status_running_known_service(self):
        # pre-req
        cmd.getoutput("service "+test_svc+" start")
        # test
        result = qmf.status(test_svc,10000)
        # verify
        self.assertTrue(result.get('rc') == 0, "Return code not expected (" + str(result.get('rc')) + ")")

    def test_status_unknown_service(self):
        result = qmf.status("zzzzz", 10000)
        self.assertFalse(result.get('rc') == 0, "Return code not expected (" + str(result.get('rc')) + ")")

    # TEST - describe()
    # =====================================================
    def test_describe_not_implemented(self):
        self.assertRaises(QmfAgentException, qmf.describe, test_svc)

class TestMatahariServiceApiTimeouts(unittest.TestCase):
    def setUp(self):
        cmd.getoutput("mv /etc/init.d/"+test_svc+" /etc/init.d/"+test_svc+".orig")
        cmd.getoutput("touch /etc/init.d/"+test_svc+"-slow")
        cmd.getoutput("echo 'sleep 10' >> /etc/init.d/"+test_svc+"-slow")
        cmd.getoutput("echo '/etc/init.d/"+test_svc+".orig $1' >> /etc/init.d/"+test_svc+"-slow")
        cmd.getoutput("chmod 777 /etc/init.d/"+test_svc+"-slow")
        cmd.getoutput("ln -s /etc/init.d/"+test_svc+"-slow /etc/init.d/"+test_svc)
    def tearDown(self):
        cmd.getoutput("rm -rf /etc/init.d/"+test_svc+" /etc/init.d/"+test_svc+"-slow")
        cmd.getoutput("mv /etc/init.d/"+test_svc+".orig /etc/init.d/"+test_svc)
    def test_stop_timeout(self):
        result = qmf.stop(test_svc,5000)
        self.assertTrue(result.get('rc') == 198, "not expected rc198, recieved " + str(result.get('rc')))
    def test_start_timeout(self):
        result = qmf.start(test_svc,5000)
        self.assertTrue(result.get('rc') == 198, "not expected rc198, recieved " + str(result.get('rc')))
    def test_status_timeout(self):
        result = qmf.status(test_svc,5000)
        self.assertTrue(result.get('rc') == 198, "not expected rc198, recieved " + str(result.get('rc')))

