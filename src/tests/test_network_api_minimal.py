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
from qmf2 import QmfAgentException

qmf = None

# Initialization
# =====================================================
def setUpModule():
    global connection, qmf
    connection = NetworkTestsSetup()
    qmf = connection.qmf

def tearDownModule():
    global connection
    connection.tearDown()

class NetworkTestsSetup(testUtil.TestsSetup):
    def __init__(self):
        testUtil.TestsSetup.__init__(self, "matahari-qmf-networkd", "Network", "Network")

class TestNetworkApi(unittest.TestCase):

    # TEST - getProperties()
    # =====================================================
    def test_hostname_property(self):
        value = qmf.props.get('hostname')
        self.assertEquals(value, cmd.getoutput("hostname"), "hostname not matching")
