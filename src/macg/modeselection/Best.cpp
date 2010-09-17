/*******************************************************************************
 * This file is part of openWNS (open Wireless Network Simulator)
 * _____________________________________________________________________________
 *
 * Copyright (C) 2004-2007
 * Chair of Communication Networks (ComNets)
 * Kopernikusstr. 5, D-52074 Aachen, Germany
 * phone: ++49-241-80-27910,
 * fax: ++49-241-80-22242
 * email: info@openwns.org
 * www: http://www.openwns.org
 * _____________________________________________________________________________
 *
 * openWNS is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License version 2 as published by the
 * Free Software Foundation;
 *
 * openWNS is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#include <LTE/macg/modeselection/Best.hpp>
#include <LTE/helper/Route.hpp>

using namespace lte::macg::modeselection;

STATIC_FACTORY_REGISTER(Best, Strategy, "lte.macg.strategy.Best");

lte::helper::Route
Best::getRoute(const wns::ldk::CompoundPtr& compound, LayerContainer layers, ScorerContainer scorers)
{
    lte::helper::Route route;
    lte::helper::Route preferredRoute;

    for(unsigned int modeCounter = 0 ; modeCounter < layers.size(); ++modeCounter)
	{
	    route = scorers.at(modeCounter)->score(compound);

	    // first check the association (route is valid only if association of the mode exists)
	    // then check the compound is accepting
	    if (route.valid())
		{
		    if (layers.at(modeCounter)->isAccepting(compound))
			{
			    route.mode = modeCounter;
			    if (route.score > preferredRoute.score)
				// update preferredRoute which always has the best score
				preferredRoute = route;
			} // else do nothing.
		}
	}

    return preferredRoute;
} // getRoute


