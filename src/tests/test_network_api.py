#!/usr/bin/env python

"""
  test_network_api.py - Copyright (c) 2011 Red Hat, Inc.
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
import unittest
import time
import sys

qmf = None
dbus = None

# Initialization
# =====================================================
def setUpModule():
    global connection, qmf, dbus
    connection = NetworkTestsSetup()
    qmf = connection.qmf
    dbus = connection.dbus

def tearDownModule():
    global connection
    connection.tearDown()

class NetworkTestsSetup(testUtil.TestsSetup):
    def __init__(self):
        testUtil.TestsSetup.__init__(self, "matahari-qmf-networkd", "Network", "Network",
                                           "matahari-dbus-networkd", ("org.matahariproject.Network",
                                                                      "/org/matahariproject/Network",
                                                                      "org.matahariproject.Network"))

def ifconfig_nic_list():
    nic_list = cmd.getoutput("ip link | grep -v '^  ' | cut -f2 -d: | tr -d ' ' | tr '\n' '|'").rsplit('|')
    nic_list.pop()
    return nic_list

def select_nic_for_test():
    nic_list = ifconfig_nic_list()
    known_list = ["eth2", "eth1", "eth0", "em2", "em1", "em0"]
    for i in known_list:
        if i in nic_list:
            return i
    print "Error determining NIC for testing."
    sys.exit(1)

def ifup(dev):
    ip_link("up", dev)

def ifdown(dev):
    ip_link("down", dev)

DOWN, UP = 0, 1
def status(dev):
    if len(cmd.getoutput('ip link show %s | grep "<.*UP.*>"' % dev)) > 3:
        return UP
    else:
        return DOWN

def ip_link(status, dev):
    cmd.getoutput("ip link set %s %s" % (status, dev))

class TestNetworkApi(unittest.TestCase):
    def setUp(self):
        self.nic_ut = select_nic_for_test()

    # TEARDOWN
    # ================================================================
    def tearDown(self):
        pass

    # TEST - getProperties()
    # =====================================================
    def test_hostname_property(self):
        value = cmd.getoutput("hostname")
        qmf_value = qmf.props.get('hostname')
        self.assertEquals(qmf_value, value, "hostname not matching")

        if testUtil.haveDBus:
            self.assertEquals(str(dbus.get('hostname')), value, "hostname not matching")

    # TEST - list()
    # =====================================================
    def test_nic_list(self):
        result = qmf.list()
        qmf_found_list = result.get("iface_map")
        output = cmd.getoutput("cat /proc/net/dev | awk '{print $1}' | grep : | sed 's/^\(.*\):\{1\}.*$/\\1/' | tr '\n' '|'").rsplit('|')
        output.pop()
        self.assertEquals(len(output), len(qmf_found_list), "QMF: nic count not matching")
        if testUtil.haveDBus:
            dbus_found_list = dbus.list()
            self.assertEquals(len(output), len(dbus_found_list), "DBus: nic count not matching")

        for nic in output:
            if not nic in qmf_found_list:
                self.fail("QMF: nic ("+nic+") not found")
            if testUtil.haveDBus:
                if not nic in dbus_found_list:
                    self.fail("DBus: nic ("+nic+") not found")

    # TEST - start()
    # ================================================================
    def test_nic_start_qmf(self):
        # first, make sure it is stopped
        ifdown(self.nic_ut)
        if (status(self.nic_ut) != DOWN):
            self.fail("pre-req error: " + self.nic_ut + " not stopping for start test")
        else:
            # do test
            results = qmf.start(self.nic_ut)
        # verify up
        time.sleep(3)
        if (status(self.nic_ut) != UP):
            self.fail("QMF: " + self.nic_ut + " fails to start")

    @unittest.skipIf(not testUtil.haveDBus, "skipping test_nic_start_dbus test")
    def test_nic_start_dbus(self):
        # first, make sure it is stopped
        ifdown(self.nic_ut)
        if (status(self.nic_ut) != DOWN):
            self.fail("pre-req error: " + self.nic_ut + " not stopping for start test")
        else:
            # do test
            results = dbus.start(self.nic_ut)
        # verify up
        time.sleep(3)
        if (status(self.nic_ut) != UP):
            self.fail("DBus: " + self.nic_ut + " fails to start")

    def test_nic_start_bad_value(self):
        results = qmf.start("bad")
        self.assertEquals(results.get('status'), 1, "QMF: start of invalid interface returns %d instead of 1" % int(results.get('status')))
        if testUtil.haveDBus:
            dbus_value = int(dbus.start('bad'))
            self.assertEquals(dbus_value, 1, "DBus: start of invalid interface returns %d instead of 1" % dbus_value)

    # TEST - stop()
    # ================================================================
    def test_nic_stop_qmf(self):
        ifup(self.nic_ut)
        if (status(self.nic_ut) != UP):
            self.fail("pre-req error: " + self.nic_ut + " not started for stop test")
        else:
            # do test
            result = qmf.stop(self.nic_ut)
        # verify down
        time.sleep(3)
        if (status(self.nic_ut) != DOWN):
            self.fail("QMF: " + self.nic_ut + " did not stop")

    @unittest.skipIf(not testUtil.haveDBus, "skipping test_nic_stop_dbus test")
    def test_nic_stop_dbus(self):
        ifup(self.nic_ut)
        if (status(self.nic_ut) != UP):
            self.fail("pre-req error: " + self.nic_ut + " not started for stop test")
        else:
            # do test
            result = dbus.stop(self.nic_ut)
        # verify down
        time.sleep(3)
        if (status(self.nic_ut) != DOWN):
            self.fail("DBus: " + self.nic_ut + " did not stop")

    def test_nic_stop_bad_value(self):
        results = qmf.stop("bad")
        self.assertEquals(results.get('status'), 1, "QMF: stop of invalid interface returns %d instead of 1" % int(results.get('status')))
        if testUtil.haveDBus:
            dbus_value = int(dbus.stop('bad'))
            self.assertEquals(dbus_value, 1, "DBus: stop of invalid interface returns %d instead of 1" % dbus_value)

    # TEST - status()
    # ================================================================
    def test_nic_stop_status(self):
        ifdown(self.nic_ut)
        time.sleep(3)
        if (status(self.nic_ut) != DOWN):
            self.fail("pre-req error: " + self.nic_ut + " not started for stop test")
        result = qmf.status(self.nic_ut)
        stop_value = result.get("status")
        self.assertTrue(stop_value == 1, "QMF: status for stopped interface is not 1")

        if testUtil.haveDBus:
            result = dbus.status(self.nic_ut)
            self.assertTrue(result == 1, "DBus: status for stopped interface is not 1")

    def test_nic_start_status(self):
        ifup(self.nic_ut)
        if (status(self.nic_ut) != UP):
            self.fail("pre-req error: " + self.nic_ut + " not started for stop test")
        result = qmf.status(self.nic_ut)
        start_value = result.get("status")
        self.assertTrue(start_value == 0, "QMF: status for stopped interface is not 0")

        if testUtil.haveDBus:
            result = dbus.status(self.nic_ut)
            self.assertTrue(result == 0, "DBus: status for stopped interface is not 0")

    def test_nic_status_bad_value(self):
        results = qmf.status("bad")
        self.assertTrue(results.get('status') == 1, "QMF: status for invalid interface is not 1")

        if testUtil.haveDBus:
            result = dbus.status("bad")
            self.assertTrue(result == 1, "DBus: status for invalid interface is not 1")

    # TEST - get_ip_address()
    # ================================================================
    def test_get_ip_address(self):
        output = cmd.getoutput("ifconfig "+self.nic_ut+" | grep 'inet addr' | gawk -F: '{print $2}' | gawk '{print $1}'")
        result = qmf.get_ip_address(self.nic_ut)
        ip_value = result.get("ip")
        if output == "":
            output = "0.0.0.0"
        self.assertTrue(output == ip_value, "QMF: IP (%s) is not equal to ifconfig (%s)" % (str(output), str(ip_value)))

        if testUtil.haveDBus:
            ip_value = dbus.get_ip_address(self.nic_ut)
            self.assertTrue(output == ip_value, "DBus: IP (%s) is not equal to ifconfig (%s)" % (str(output), str(ip_value)))

    def test_get_ip_addr_bad_value(self):
        results = qmf.get_ip_address("bad")
        self.assertTrue(results.get('ip') == '', "QMF: Bad IP for invalid interface, expecting empty string")

        if testUtil.haveDBus:
            self.assertTrue(dbus.get_ip_address("bad") == '', "DBus: Bad IP for invalid interface, expecting empty string")

    # TEST - get_mac_address()
    # ================================================================
    def test_get_mac_address(self):
        result = qmf.get_mac_address(self.nic_ut)
        mac_value = result.get("mac")
        output = cmd.getoutput("ifconfig "+self.nic_ut+" | grep 'HWaddr' | sed 's/^.*HWaddr \(.*\)\{1\}.*$/\\1/'").strip()
        self.assertTrue(output == mac_value, "QMF: MAC (%s) is not equal to ifconfig (%s)" % (str(output), str(mac_value)))

        if testUtil.haveDBus:
            result = dbus.get_mac_address(self.nic_ut)
            self.assertTrue(output == result, "DBus: MAC (%s) is not equal to ifconfig (%s)" % (str(output), str(mac_value)))


    def test_get_mac_addr_bad_value(self):
        results = qmf.get_mac_address("bad")
        self.assertTrue(results.get('mac') == '', "QMF: Bad MAC for invalid interface, expecting empty string")

        if testUtil.haveDBus:
            self.assertTrue(dbus.get_mac_address('mac') == '', "DBus: Bad MAC for invalid interface, expecting empty string")
