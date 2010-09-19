###############################################################################
# This file is part of openWNS (open Wireless Network Simulator)
# _____________________________________________________________________________
#
# Copyright (C) 2004-2007
# Chair of Communication Networks (ComNets)
# Kopernikusstr. 16, D-52074 Aachen, Germany
# phone: ++49-241-80-27910,
# fax: ++49-241-80-22242
# email: info@openwns.org
# www: http://www.openwns.org
# _____________________________________________________________________________
#
# openWNS is free software; you can redistribute it and/or modify it under the
# terms of the GNU Lesser General Public License version 2 as published by the
# Free Software Foundation;
#
# openWNS is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
###############################################################################

import lte.nodes

import scenarios.interfaces

import rise.Mobility

import openwns.geometry.position

rang = None

class BSCreator(scenarios.interfaces.INodeCreator):

    def __init__(self, config):
        self.config = config

    def _createRANG(self):
        global rang

        if rang is None:
            rang = lte.nodes.RANG()
            import openwns.simulator
            openwns.simulator.getSimulator().simulationModel.nodes.append(rang)

    def create(self):

        self._createRANG()

        position = openwns.geometry.position.Position(x = 0.0, y = 0.0, z = 0.0)

        mobility = rise.Mobility.No(position)

        bs = lte.nodes.BS(self.config, mobility)

        global rang
        rang.tunnel.addBS(bs)

        return bs
