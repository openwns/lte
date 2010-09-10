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

#include <WNS/ldk/FUNConfigCreator.hpp>

#include <LTE/timing/events/rap/Events.hpp>
#include <LTE/timing/TimingScheduler.hpp>
/* deleted by chen */
// #include <LTE/timing/ResourceScheduler.hpp>
// #include <LTE/controlplane/MapHandler.hpp>
// #include <LTE/macr/RACH.hpp>
// #include <LTE/controlplane/bch/BCHUnit.hpp>
/* inserted by chen */
// #include <LTE/timing/ResourceSchedulerInterface.hpp>
// #include <LTE/controlplane/MapHandlerInterface.hpp>
// #include <LTE/macr/RACHInterface.hpp>
// #include <LTE/controlplane/bch/BCHUnitInterface.hpp>
#include <LTE/lteDummy.hpp>

using namespace lte::timing;
using namespace lte::timing::events;

// objects of these classes are created by name:
// the names are specified in Timing.py as __plugin__
STATIC_FACTORY_REGISTER_WITH_CREATOR(rap::StartBCH,  rap::EventBase, "lte.timing.BCH", wns::ldk::FUNConfigCreator);
STATIC_FACTORY_REGISTER_WITH_CREATOR(rap::StartRACH, rap::EventBase, "lte.timing.RACH", wns::ldk::FUNConfigCreator);
STATIC_FACTORY_REGISTER_WITH_CREATOR(rap::StartMap,  rap::EventBase, "lte.timing.Map", wns::ldk::FUNConfigCreator);
STATIC_FACTORY_REGISTER_WITH_CREATOR(rap::StartData, rap::EventBase, "lte.timing.Data", wns::ldk::FUNConfigCreator);


rap::StartRACH::StartRACH(wns::ldk::fun::FUN* _fun, const wns::pyconfig::View& config) :
    EventBase(_fun, config),

/* deleted by chen */
//     rach(_fun->findFriend<lte::macr::IRachTimingRx*>(mode+separator+"rach"))

/* inserted by chen */
    rach(_fun->findFriend<lte::lteDummy*>(mode+separator+"rach"))
{}

void
rap::StartRACH::execute()
{
    assure(timer, "Timer has not been set in Event!");
    // Transmit all pending RACH compounds
    MESSAGE_SINGLE(NORMAL, logger, "Event StartRACH (as RAP)");
    setStateRxTx(lte::macr::PhyUser::Rx);
    rach->startRx(duration);
}

rap::StartBCH::StartBCH(wns::ldk::fun::FUN* _fun, const wns::pyconfig::View& config) :
    EventBase(_fun, config),

/* deleted by chen */
//     bch(_fun->findFriend<lte::controlplane::bch::IBCHTimingTx*>(mode+separator+"bch"))

/* inserted by chen */
    bch(_fun->findFriend<lte::lteDummy*>(mode+separator+"bch"))

{}

void
rap::StartBCH::execute()
{
    assure(timer, "Timer has not been set in Event!");
    MESSAGE_SINGLE(NORMAL, logger, "Event StartBCH (as RAP)");
    setStateRxTx(lte::macr::PhyUser::Tx);
    bch->sendBCH(duration);
}


rap::StartMap::StartMap(wns::ldk::fun::FUN* _fun, const wns::pyconfig::View& config) :
    EventBase(_fun, config),

/* deleted by chen */
//     rstx(_fun->findFriend<lte::timing::MasterScheduler*>(mode+separator+rsNameSuffix+"TX")),
//     rsrx(_fun->findFriend<lte::timing::MasterScheduler*>(mode+separator+rsNameSuffix+"RX")),
//     mapHandler(_fun->findFriend<lte::controlplane::IMapHandlerTiming*>(mode+separator+"mapHandler")),

/* inserted by chen */
    rstx(_fun->findFriend<lte::lteDummy*>(mode+separator+rsNameSuffix+"TX")),
    rsrx(_fun->findFriend<lte::lteDummy*>(mode+separator+rsNameSuffix+"RX")),
    mapHandler(_fun->findFriend<lte::lteDummy*>(mode+separator+"mapHandler")),

    frameNr(config.get<int>("frameNr")),
    framesPerSuperFrame(config.get<int>("framesPerSuperFrame")),
    useMapResourcesInUL(config.get<bool>("useMapResourcesInUL"))
{}

void
rap::StartMap::execute()
{
    assure(timer, "Timer has not been set in Event!");
    MESSAGE_SINGLE(NORMAL, logger, "Event StartMap (as RAP) on current frame="<<frameNr);
    setStateRxTx(lte::macr::PhyUser::Tx);
    simTimeType now = wns::simulator::getEventScheduler()->getTime();

    // needed to reset resources at the and of a frame
    mapHandler->setCurrentPhase();
    // ^ mapHandler sets currentPhase via friends.timer->phaseNumberAtOffset(0=now)

    int workingFrameNr;
    int schedulingOffset = timer->getSchedulingOffset();
    int numberOfFrames = timer->getNumberOfFramesToSchedule();
    std::vector<int> frameNumbers;

    MESSAGE_SINGLE(NORMAL, logger, "startMap: Scheduling for " << numberOfFrames << " frames,starting with frameNr=" << frameNr+schedulingOffset);
    int j = 0;
    int i = 0;

    // if we allow UL traffic during the map phase we need to set the
    // expectations (only valid for FDD):
    if (useMapResourcesInUL)
    {
        MESSAGE_SINGLE(NORMAL, logger, "Setting Expectations at beginning of Map -> Thus starting UL");
        //rstx->setExpectations(frameNr);
        rsrx->finishCollection(/*current*/frameNr, now); // only beamforming etc.
    }

    // Now we can start the scheduling as normal
    // this is scheduling in advance...
    while (i < numberOfFrames)
    {
        // make sure that the RN only schedules frames where it is in RAP phase!
        // (i, j)
        workingFrameNr = (frameNr + schedulingOffset + i + j) % framesPerSuperFrame;

        int taskPhase = timer->stationTaskAtFrame(workingFrameNr);
        MESSAGE_SINGLE(NORMAL, logger, "startMap: taskPhase=" << taskPhase);
        if (taskPhase == StationTasks::RAP())
        {
            // first DL
            MESSAGE_SINGLE(NORMAL, logger, "startMap(): DL Scheduling for frameNr=" << workingFrameNr);
            rstx->startCollection(workingFrameNr);

            // now UL
            MESSAGE_SINGLE(NORMAL, logger, "startMap(): UL Scheduling for frameNr=" << workingFrameNr);
            rsrx->startCollection(workingFrameNr);

            // finally save the map
            //mapHandler->saveMap(workingFrameNr); // now done in ResourceScheduler.cpp
            frameNumbers.push_back(workingFrameNr);
            i++;
        } else {
            j++;
        }
    }
    rsrx->resetHARQScheduledPeerRetransmissions();
    MESSAGE_SINGLE(NORMAL, logger, "startMap(): scheduled frameNumbers=" << frameNumbers.size());
    timer->frameTrigger();
    mapHandler->startMapTx(duration, frameNumbers, frameNumbers);
    // duration is needed for phyCommand->local.stop = startTime + duration
} // rap::StartMap::execute()

rap::StartData::StartData(wns::ldk::fun::FUN* _fun, const wns::pyconfig::View& config) :
    EventBase(_fun, config),

/* deleted by chen */
//     rstx(_fun->findFriend<lte::timing::MasterScheduler*>(mode+separator+rsNameSuffix+"TX")),
//     rsrx(_fun->findFriend<lte::timing::MasterScheduler*>(mode+separator+rsNameSuffix+"RX")),
//     mapHandler(_fun->findFriend<lte::controlplane::IMapHandlerTiming*>(mode+separator+"mapHandler")),

/* inserted by chen */
    rstx(_fun->findFriend<lte::lteDummy*>(mode+separator+rsNameSuffix+"TX")),
    rsrx(_fun->findFriend<lte::lteDummy*>(mode+separator+rsNameSuffix+"RX")),
    mapHandler(_fun->findFriend<lte::lteDummy*>(mode+separator+"mapHandler")),

    mySwitchingPointEvent(_fun, config),
    myStopDataEvent(_fun, config),
    frameNr(config.get<int>("frameNr")),
    useMapResourcesInUL(config.get<bool>("useMapResourcesInUL"))
{}

rap::StartData::~StartData()
{
}

// wns::CloneableInterface*
// rap::StartData::clone() const
// {
//     // use default implementation of copy c'tor
//     rap::StartData* that = new rap::StartData(*this);
//     // and make deep copies of the contained events
//     that->mySwitchingPointEvent = mySwitchingPointEvent;
//     that->myStopDataEvent = myStopDataEvent;
//     return that;
// }

void
rap::StartData::setTimer(lte::timing::TimingScheduler* _timer)
{
    EventBase::setTimer(_timer);
    mySwitchingPointEvent.setTimer(_timer);
    myStopDataEvent.setTimer(_timer);
}

void
rap::StartData::execute()
{
    //MESSAGE_SINGLE(NORMAL, logger, "Event StartData");
    assure(timer, "Timer has not been set in Event!");
    simTimeType now = wns::simulator::getEventScheduler()->getTime();

    MESSAGE_SINGLE(NORMAL, logger, "Event StartData (as RAP). frameNr="<<frameNr);

    //setExpectation
    if (!useMapResourcesInUL)
    {
        MESSAGE_SINGLE(NORMAL, logger, "useMapResourcesInUL="<<useMapResourcesInUL);
        //rstx->setExpectations(frameNr); //might have been done in StartMap
    }

    setStateRxTx(lte::macr::PhyUser::Tx);
    rstx->finishCollection(frameNr, now);

    if (timer->switchingPointOffset > 0.0) {
        // TDD, in case of FDD we don't have a switching point
        wns::simulator::getEventScheduler()->schedule(mySwitchingPointEvent,
                                                      now + timer->switchingPointOffset);
    } else {
        // FDD
        if (!useMapResourcesInUL)         //might have been done in StartMap
        {
            rsrx->finishCollection(frameNr, now); // only beamforming etc.,
        }
        mapHandler->resetResources(frameNr);
    }
    wns::simulator::getEventScheduler()->schedule(myStopDataEvent,
                                                  now + duration);
}

rap::StartData::SwitchingPoint::SwitchingPoint(wns::ldk::fun::FUN* _fun, const wns::pyconfig::View& config) :
    EventBase(_fun, config),

/* deleted by chen */
//     rsrx(_fun->findFriend<lte::timing::MasterScheduler*>(mode+separator+rsNameSuffix+"RX")),
//     mapHandler(_fun->findFriend<lte::controlplane::IMapHandlerTiming*>(mode+separator+"mapHandler")),

/* inserted by chen */
    rsrx(_fun->findFriend<lte::lteDummy*>(mode+separator+rsNameSuffix+"RX")),
    mapHandler(_fun->findFriend<lte::lteDummy*>(mode+separator+"mapHandler")),

    frameNr(config.get<int>("frameNr"))
{
}

void
rap::StartData::SwitchingPoint::execute()
{
    assure(timer, "Timer has not been set in Event!");
    assure(mapHandler, "mapHandler not set!");
    MESSAGE_SINGLE(NORMAL, logger, "SwitchingPoint reached (as RAP). From Tx to Rx, DL to UL");
    setStateRxTx(lte::macr::PhyUser::Rx);
    simTimeType now = wns::simulator::getEventScheduler()->getTime();
    rsrx->finishCollection(frameNr, now); // only beamforming etc.
    mapHandler->resetResources(frameNr);
}

rap::StartData::StopData::StopData(wns::ldk::fun::FUN* _fun, const wns::pyconfig::View& config) :
    EventBase(_fun, config),

/* deleted by chen */
//     rstx(_fun->findFriend<lte::timing::SchedulerIncoming*>(mode+separator+rsNameSuffix+"TX"))

/* inserted by chen */
    rstx(_fun->findFriend<lte::lteDummy*>(mode+separator+rsNameSuffix+"TX"))

{}

void
rap::StartData::StopData::execute()
{
    MESSAGE_SINGLE(NORMAL, logger, "Event StopData");
    // in case of a UT in TDD mode here deliverReceived() has no effect, because
    // it has already been called at the swtiching point
    rstx->deliverReceived(); // obsolete
}




