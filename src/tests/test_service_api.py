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
import stat

qmf = None
dbus = None
test_svc = 'snmpd'
# TODO: snmpd uses systemd on Fedora >= 16, replace it with other service, like qpidd
#test_svc = 'qpidd'

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

    global connection, qmf, dbus
    connection = ServicesTestsSetup()
    qmf = connection.qmf
    dbus = connection.dbus

def tearDownModule():
    global connection
    connection.tearDown()

def list_executable_files(directory):
    files = []
    for f in os.listdir(directory):
        path = os.path.join(directory, f)
        mode = os.stat(path).st_mode
        if stat.S_ISREG(mode) and (mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)) != 0:
            files.append(f)
    return files

class ServicesTestsSetup(testUtil.TestsSetup):
    def __init__(self):
        testUtil.TestsSetup.__init__(self, "matahari-qmf-serviced", "service", "Services",
                                     "matahari-dbus-serviced", ("org.matahariproject.Services",
                                                                "/org/matahariproject/Services",
                                                                "org.matahariproject.Services"))


class TestServiceApi(unittest.TestCase):

    # TEST - getProperties()
    # =====================================================
    def test_hostname_property(self):
        value = cmd.getoutput("hostname")

        qmf_value = qmf.props.get('hostname')
        self.assertEquals(qmf_value, value, "QMF hostname not matching")

        if testUtil.haveDBus:
            dbus_value = dbus.get('hostname')
            self.assertEquals(dbus_value, value, "DBus hostname not matching")

    # TEST - list()
    # =====================================================
    def test_list(self):
        result = qmf.list()
        dirlist = list_executable_files("/etc/init.d")
        qmf_list = result.get("agents")
        self.assertTrue(len(qmf_list) == len(dirlist), "QMF service count not mataching")

        if testUtil.haveDBus:
            dbus_list = dbus.list()
            self.assertTrue(len(dbus_list) == len(dirlist), "DBus service count not mataching")

        for svc in dirlist:
            if (svc != 'functions'):
                try:
                    qmf_list.index(svc)
                    if testUtil.haveDBus:
                        dbus_list.index(svc)
                except ValueError:
                    self.fail("QMF service %s missing" % str(svc))


    # TEST - disable()
    # =====================================================
    def test_disable_known_service(self):
        # make sure service enabled
        cmd.getoutput("chkconfig "+test_svc+" on")
        # test
        dis = qmf.disable(test_svc)
        # verify
        chk_off = cmd.getoutput("chkconfig --list "+test_svc)
        self.assertFalse("2:off" not in chk_off, "QMF run level 2 wrong")
        self.assertFalse("3:off" not in chk_off, "QMF run level 3 wrong")
        self.assertFalse("4:off" not in chk_off, "QMF run level 4 wrong")
        self.assertFalse("5:off" not in chk_off, "QMF run level 5 wrong")

        if testUtil.haveDBus:
            # make sure service enabled
            cmd.getoutput("chkconfig "+test_svc+" on")
            # test
            dis = dbus.disable(test_svc)
            # verify
            chk_off = cmd.getoutput("chkconfig --list "+test_svc)
            self.assertFalse("2:off" not in chk_off, "DBus run level 2 wrong")
            self.assertFalse("3:off" not in chk_off, "DBus run level 3 wrong")
            self.assertFalse("4:off" not in chk_off, "DBus run level 4 wrong")
            self.assertFalse("5:off" not in chk_off, "DBus run level 5 wrong")

    def test_disable_unknown_service(self):
        result = qmf.disable("zzzzz")
        self.assertFalse(result.get('rc') == 0, "QMF Return code not expected (" + str(result.get('rc')) + ")")

        if testUtil.haveDBus:
            result = dbus.disable("zzzzz")
            self.assertFalse(int(result) == 0, "DBus Return code not expected (" + str(result) + ")")

    # TEST - enable()
    # =====================================================
    def test_enable_known_service(self):
        # make sure service disabled
        cmd.getoutput("chkconfig "+test_svc+" off")
        # test
        dis = qmf.enable(test_svc)
        # verify
        chk_on = cmd.getoutput("chkconfig --list "+test_svc)
        self.assertTrue("2:on" in chk_on, "QMF run level 2 wrong")
        self.assertTrue("3:on" in chk_on, "QMF run level 3 wrong")
        self.assertTrue("4:on" in chk_on, "QMF run level 4 wrong")
        self.assertTrue("5:on" in chk_on, "QMF run level 5 wrong")

        if testUtil.haveDBus:
            # make sure service disabled
            cmd.getoutput("chkconfig "+test_svc+" off")
            # test
            dis = dbus.enable(test_svc)
            # verify
            chk_on = cmd.getoutput("chkconfig --list "+test_svc)
            self.assertTrue("2:on" in chk_on, "DBus run level 2 wrong")
            self.assertTrue("3:on" in chk_on, "DBus run level 3 wrong")
            self.assertTrue("4:on" in chk_on, "DBus run level 4 wrong")
            self.assertTrue("5:on" in chk_on, "DBus run level 5 wrong")

    def test_enable_unknown_service(self):
        result = qmf.enable("zzzzz")
        self.assertFalse(result.get('rc') == 0, "QMF Return code not expected (" + str(result.get('rc')) + ")")

        if testUtil.haveDBus:
            result = dbus.enable("zzzzz")
            self.assertFalse(int(result) == 0, "DBus Return code not expected (" + str(result) + ")")

    # TEST - stop()
    # =====================================================

    def test_stop_running_known_service(self):
        # pre-req
        cmd.getoutput("service "+test_svc+" start")
        # test
        result = qmf.stop(test_svc,10000)
        # verify
        self.assertTrue(result.get('rc') == 0, "QMF Return code not expected (" + str(result.get('rc')) + ")")
        svc_status = cmd.getoutput("service "+test_svc+" status")
        self.assertTrue("running" not in svc_status, "QMF text not found, still running?")

        if testUtil.haveDBus:
            # pre-req
            cmd.getoutput("service "+test_svc+" start")
            # test
            result = dbus.stop(test_svc,10000)
            # verify
            self.assertTrue(int(result) == 0, "DBus Return code not expected (" + str(result) + ")")
            svc_status = cmd.getoutput("service "+test_svc+" status")
            self.assertTrue("running" not in svc_status, "DBus text not found, still running?")

    def test_stop_stopped_known_service(self):
        # pre-req
        cmd.getoutput("service "+test_svc+" stop")
        # test
        result = qmf.stop(test_svc,10000)
        # verify
        self.assertTrue(result.get('rc') == 0, "QMF Return code not expected (" + str(result.get('rc')) + ")")
        svc_status = cmd.getoutput("service "+test_svc+" status")
        self.assertTrue("running" not in svc_status, "QMF text not found, still running?")

        if testUtil.haveDBus:
            # pre-req
            cmd.getoutput("service "+test_svc+" stop")
            # test
            result = dbus.stop(test_svc,10000)
            # verify
            self.assertTrue(int(result) == 0, "DBus Return code not expected (" + str(result) + ")")
            svc_status = cmd.getoutput("service "+test_svc+" status")
            self.assertTrue("running" not in svc_status, "DBus text not found, still running?")

    def test_stop_unknown_service(self):
        result = qmf.stop("zzzzz", 10000)
        self.assertFalse(result.get('rc') == 0, "QMF Return code not expected (" + str(result.get('rc')) + ")")

        if testUtil.haveDBus:
            result = dbus.stop("zzzzz", 10000)
            self.assertFalse(int(result) == 0, "DBus Return code not expected (" + str(result) + ")")

    # TEST - start()
    # =====================================================

    def test_start_stopped_known_service(self):
        # pre-req
        cmd.getoutput("service "+test_svc+" stop")
        # test
        result = qmf.start(test_svc,10000)
        # verify
        self.assertTrue(result.get('rc') == 0, "QMF Return code not expected (" + str(result.get('rc')) + ")")
        svc_status = cmd.getoutput("service "+test_svc+" status")
        self.assertTrue("running" in svc_status, "QMF text not found, still running?")

        if testUtil.haveDBus:
            # pre-req
            cmd.getoutput("service "+test_svc+" stop")
            # test
            result = dbus.start(test_svc,10000)
            # verify
            self.assertTrue(int(result) == 0, "DBus Return code not expected (" + str(result) + ")")
            svc_status = cmd.getoutput("service "+test_svc+" status")
            self.assertTrue("running" in svc_status, "DBus text not found, still running?")

    def test_start_running_known_service(self):
        # pre-req
        cmd.getoutput("service "+test_svc+" start")
        # test
        result = qmf.start(test_svc,10000)
        # verify
        self.assertTrue(result.get('rc') == 0, "QMF Return code not expected (" + str(result.get('rc')) + ")")
        svc_status = cmd.getoutput("service "+test_svc+" status")
        self.assertTrue("running" in svc_status, "QMF text not found, still running?")

        if testUtil.haveDBus:
            # pre-req
            cmd.getoutput("service "+test_svc+" start")
            # test
            result = dbus.start(test_svc,10000)
            # verify
            self.assertTrue(int(result) == 0, "DBus Return code not expected (" + str(result) + ")")
            svc_status = cmd.getoutput("service "+test_svc+" status")
            self.assertTrue("running" in svc_status, "DBus text not found, still running?")

    def test_start_unknown_service(self):
        result = qmf.start("zzzzz", 10000)
        self.assertFalse(result.get('rc') == 0, "QMF Return code not expected (" + str(result.get('rc')) + ")")

        if testUtil.haveDBus:
            result = dbus.start("zzzzz", 10000)
            self.assertFalse(int(result) == 0, "DBus Return code not expected (" + str(result) + ")")

    # TEST - status()
    # =====================================================

    def test_status_stopped_known_service(self):
        # pre-req
        cmd.getoutput("service "+test_svc+" stop")
        # test
        result = qmf.status(test_svc,10000)
        # verify
        self.assertTrue(result.get('rc') == 3, "QMF Return code not expected (" + str(result.get('rc')) + ")")

        if testUtil.haveDBus:
            # test
            result = dbus.status(test_svc,10000)
            # verify
            self.assertTrue(int(result) == 3, "DBus Return code not expected (" + str(result) + ")")

    def test_status_running_known_service(self):
        # pre-req
        cmd.getoutput("service "+test_svc+" start")
        # test
        result = qmf.status(test_svc,10000)
        # verify
        self.assertTrue(result.get('rc') == 0, "QMF Return code not expected (" + str(result.get('rc')) + ")")

        if testUtil.haveDBus:
            # test
            result = dbus.status(test_svc,10000)
            # verify
            self.assertTrue(int(result) == 0, "DBus Return code not expected (" + str(result) + ")")

    def test_status_unknown_service(self):
        result = qmf.status("zzzzz", 10000)
        self.assertFalse(result.get('rc') == 0, "QMF Return code not expected (" + str(result.get('rc')) + ")")

        if testUtil.haveDBus:
            result = dbus.status("zzzzz", 10000)
            self.assertFalse(int(result) == 0, "DBus Return code not expected (" + str(result) + ")")

    # TEST - describe()
    # =====================================================
    def test_describe_not_implemented(self):
        self.assertRaises(QmfAgentException, qmf.describe, test_svc)

        if testUtil.haveDBus:
            from dbus import DBusException
            self.assertRaises(DBusException, dbus.describe, test_svc)

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

