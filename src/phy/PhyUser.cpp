/******************************************************************************
 * WinProSt (WINNER Protocol Stack)                                           *
 * __________________________________________________________________________ *
 * Copyright (C) 2004-2006                                                    *
 * Chair of Communication Networks (ComNets)                                  *
 * Kopernikusstr. 16, D-52074 Aachen, Germany                                 *
 * phone: ++49-241-80-27910 (phone), fax: ++49-241-80-22242                   *
 * email: wns@comnets.rwth-aachen.de                                          *
 * www: http://wns.comnets.rwth-aachen.de                                     *
 ******************************************************************************/

#include <LTE/phy/PhyUser.hpp>
//#include <WINPROST/timing/ResourceScheduler.hpp>

#include <WNS/service/dll/StationTypes.hpp>
#include <WNS/service/phy/ofdma/DataTransmission.hpp>
#include <WNS/StaticFactory.hpp>
#include <WNS/ldk/helper/FakePDU.hpp>
#include <WNS/ldk/fun/FUN.hpp>
#include <WNS/ldk/sar/Soft.hpp>
#include <WNS/pyconfig/View.hpp>

#include <DLL/services/management/InterferenceCache.hpp>
#include <DLL/Layer2.hpp>
#include <DLL/StationManager.hpp>
#include <DLL/services/control/Association.hpp>

#include <cmath>
#include <iomanip>
#include <iostream>

using namespace lte::phy;

#define A2N(a) layer2->getStationManager()->getStationByMAC(a)->getName()

STATIC_FACTORY_REGISTER_WITH_CREATOR(PhyUser, wns::ldk::FunctionalUnit, "winprost.macr.PhyUser", wns::ldk::FUNConfigCreator);

PhyUser::PhyUser(wns::ldk::fun::FUN* fun, const wns::pyconfig::View& pyConfigView) :
        wns::ldk::fu::Plain<PhyUser, PhyCommand>(fun),
        lte::helper::HasModeName(pyConfigView),
#ifndef NDEBUG
        //schedulerCommandReader_(NULL),
#endif
        config_(pyConfigView),
        layer2(NULL),
        stateRxTx(Rx),
        logger(pyConfigView.get("logger")),
        activeSubBands(),
        fddCapable((pyConfigView.get<std::string>("plm.mac.duplex") == "FDD")),
        safetyFraction(pyConfigView.get<simTimeType>("plm.mac.safetyFraction")),
        es(wns::simulator::getEventScheduler()),
        iCache(NULL),
        bfTransmission(NULL),
        transmission(NULL),
        notificationService(NULL),
        mobility(NULL),
        associationService(NULL),
        stationManager(NULL)
{
    MESSAGE_BEGIN(NORMAL, logger, m, "PhyUser() created.");
    if (fddCapable) m << " fddCapable";
    MESSAGE_END();
} // PhyUser


PhyUser::~PhyUser()
{
    iCache = NULL;
    bfTransmission = NULL;
    transmission = NULL;
    notificationService = NULL;
    mobility = NULL;
    associationService = NULL;
    stationManager = NULL;
} // ~PhyUser

void
PhyUser::onFUNCreated()
{
    wns::ldk::fun::FUN* fun = getFUN();

    layer2 = fun->getLayer<dll::ILayer2*>();

    iCache = layer2->getManagementService<dll::services::management::InterferenceCache>("INTERFERENCECACHE"+modeBase);

    associationService =
        layer2->getControlService<dll::services::control::Association>("ASSOCIATION"+modeBase);

    stationManager =
        layer2->getStationManager();

#ifndef NDEBUG
    //schedulerCommandReader_ = getFUN()->getCommandReader(config_.get<std::string>("schedulingCommandReaderName"));
#endif

    //jsonTracingCC_ = wns::probe::bus::ContextCollectorPtr(new wns::probe::bus::ContextCollector(getFUN()->getLayer()->getContextProviderCollection(),  "phyTrace"));
}


std::string
PhyUser::getTaskForCompound(const wns::ldk::CompoundPtr& compound) const
{
    PhyCommand* myCommand = getCommand(compound->getCommandPool());

    wns::service::dll::UnicastAddress source =
        stationManager->getStationByNode(myCommand->magic.source)->getDLLAddress();

    if (associationService->hasAssociated(source))
        // the source is hierarchically lower, it is an uplink packet
        return std::string("BS");
    else if (associationService->hasAssociation())
    {
        // the source is hierarchically higher, it is a downlink packet
        if (associationService->getAssociation() == source)
            return std::string("UT");
    }
    // we are neither associated to the source of the packet nor is the source
    // associated to us.
    return std::string("BS");
}

bool
PhyUser::doIsAccepting(const wns::ldk::CompoundPtr& /*compound*/) const
{
    return true;
} // isAccepting


void
PhyUser::doSendData(const wns::ldk::CompoundPtr& compound)
{
    assure(compound, "sendData called with an invalid compound.");
    assure(getFUN()->getProxy()->commandIsActivated( compound->getCommandPool(), this),
           "PhyCommand not specified. PhyUser can not handle this compound!");

    // finally commit CommandPool Size
    this->commitSizes(compound->getCommandPool());

    PhyCommand* myCommand = getCommand(compound->getCommandPool());
    if (myCommand->local.modeRxTx == lte::phy::PhyCommand::Tx)
    {
        MESSAGE_SINGLE(NORMAL, logger,"doSendData(Tx): start="<< myCommand->local.start <<"s..stop=" << myCommand->local.stop <<"s => d="<<(myCommand->local.stop-myCommand->local.start)*1e6<<"us, subBand=" << myCommand->local.subBand << ", len="<<compound->getLengthInBits() << "bits");

        simTimeType startTime = myCommand->local.start;

        es->schedule(StartTxEvent(compound, this), startTime);
    }
    else
    { // reception (Rx)

        MESSAGE_SINGLE(NORMAL, logger,"doSendData(Rx): startTime="<< myCommand->local.start <<", stopTime=" << myCommand->local.stop << ", subBand=" << myCommand->local.subBand << ", len="<<compound->getLengthInBits() << "bits" << " SHALL NOT OCCUR");

        assure(false,"doSendData(Rx): startTime="<< myCommand->local.start <<", stopTime=" << myCommand->local.stop << ", subBand=" << myCommand->local.subBand << ", len="<<compound->getLengthInBits() << "bits" << " SHALL NOT OCCUR");
    }

    // stamp link to my InterferenceCache into the Command
    myCommand->magic.remoteCache = iCache;
} // doSendData


void
PhyUser::doOnData(const wns::ldk::CompoundPtr& compound)
{
    assure(compound, "onData called with an invalid compound.");
    getDeliverer()->getAcceptor(compound)->onData(compound);
} // doOnData

/*
#ifndef NDEBUG
void
PhyUser::traceIncoming(wns::ldk::CompoundPtr compound, wns::service::phy::power::PowerMeasurementPtr rxPowerMeasurement)
{
    wns::probe::bus::json::Object objdoc;

    PhyCommand* myCommand = getCommand(compound->getCommandPool());

    objdoc["Transmission"]["ReceiverID"] = wns::probe::bus::json::String(getFUN()->getLayer()->getNodeName());
    objdoc["Transmission"]["SenderID"] = wns::probe::bus::json::String(myCommand->magic.source->getName());
    objdoc["Transmission"]["SourceID"] = wns::probe::bus::json::String(myCommand->magic.source->getName());

    if(myCommand->magic.destination == NULL)
    {
        objdoc["Transmission"]["DestinationID"] = wns::probe::bus::json::String("Broadcast");
    }
    else
    {
        objdoc["Transmission"]["DestinationID"] = wns::probe::bus::json::String(myCommand->magic.destination->getName());
    }

    objdoc["Transmission"]["Start"] = wns::probe::bus::json::Number(myCommand->local.start);
    objdoc["Transmission"]["Stop"] = wns::probe::bus::json::Number(myCommand->local.stop);
    objdoc["Transmission"]["Subchannel"] = wns::probe::bus::json::Number(myCommand->local.subBand);
    objdoc["Transmission"]["TxPower"] = wns::probe::bus::json::Number(myCommand->magic.txp.get_dBm());
    objdoc["Transmission"]["RxPower"] = wns::probe::bus::json::Number(rxPowerMeasurement->getRxPower().get_dBm());
    objdoc["Transmission"]["InterferencePower"] = wns::probe::bus::json::Number(rxPowerMeasurement->getInterferencePower().get_dBm());

    if (myCommand->magic.estimatedSINR.C != wns::Power() &&
        myCommand->magic.estimatedSINR.I != wns::Power())
    {
        objdoc["SINREst"]["C"] = wns::probe::bus::json::Number(myCommand->magic.estimatedSINR.C.get_dBm());
        objdoc["SINREst"]["I"] = wns::probe::bus::json::Number(myCommand->magic.estimatedSINR.I.get_dBm());
    }

    if (schedulerCommandReader_->commandIsActivated(compound->getCommandPool()))
    {

        // Now we have a look at the scheduling time slot
        winprost::timing::SchedulerCommand* schedCommand = schedulerCommandReader_->readCommand<winprost::timing::SchedulerCommand>(compound->getCommandPool());

        wns::scheduler::SchedulingTimeSlotPtr ts = schedCommand->magic.schedulingTimeSlotPtr;

        wns::probe::bus::json::Array a;
        for (wns::scheduler::PhysicalResourceBlockVector::iterator it= ts->physicalResources.begin(); it != ts->physicalResources.end(); ++it)
        {
            wns::probe::bus::json::Object pr;
            pr["NetBits"] = wns::probe::bus::json::Number(it->getNetBlockSizeInBits());
            a.Insert(pr);
        }
        objdoc["SchedulingTimeSlot"]["PhysicalResources"] = a;
        objdoc["SchedulingTimeSlot"]["HARQ"]["enabled"] = wns::probe::bus::json::Boolean(ts->isHARQEnabled());
        objdoc["SchedulingTimeSlot"]["HARQ"]["ProcessID"] = wns::probe::bus::json::Number(ts->harq.processID);
        objdoc["SchedulingTimeSlot"]["HARQ"]["NDI"] = wns::probe::bus::json::Boolean(ts->harq.NDI);
        objdoc["SchedulingTimeSlot"]["HARQ"]["TransportBlockID"] = wns::probe::bus::json::Number(ts->harq.transportBlockID);
        objdoc["SchedulingTimeSlot"]["HARQ"]["RetryCounter"] = wns::probe::bus::json::Number(ts->harq.retryCounter);
    }
    wns::probe::bus::json::probeJSON(jsonTracingCC_, objdoc);
}
#endif
*/
void
PhyUser::onData(wns::osi::PDUPtr pdu, wns::service::phy::power::PowerMeasurementPtr rxPowerMeasurement)
{
    MESSAGE_BEGIN(VERBOSE, logger, m, "DATAInd");
    m << " while in state: " << getStateRxTx();
    MESSAGE_END();

    // If we are not in receiving state this PDU can't be meant for us.
    if (!(stateRxTx==Rx || stateRxTx==BothRxTx))
        return;

    assure(wns::dynamicCast<wns::ldk::Compound>(pdu), "Wrong type of PDU!");

    // take a copy!
    wns::ldk::CompoundPtr compound = wns::staticCast<wns::ldk::Compound>(pdu)->copy();
    PhyCommand* myCommand = getCommand(compound->getCommandPool());

    //const
    wns::node::Interface* source = rxPowerMeasurement->getSourceNode();
    assure(source == myCommand->magic.source,"measurement source mismatch");
    // set receiver Ptr in command pool, for later Probing
    compound->getCommandPool()->setReceiver(getFUN());

    wns::service::phy::phymode::PhyModeInterfacePtr phyModePtr = rxPowerMeasurement->getPhyMode();
    assure(myCommand->local.phyModePtr == phyModePtr, "getPhyMode error");

    bool broadcast = (myCommand->magic.destination == NULL);
    if (broadcast) {
        MESSAGE_SINGLE(NORMAL,logger, "RECEIVED BROADCAST COMPOUND FROM " <<source->getName());
    }

    // perform the first part of the LL mapping, the second part (Mapping from
    // MIB to PER) is performed when all segments belonging to a code-block are
    // re-assembled (SAR-FU).
    // Store measurements in the phyCommand, e.g. for evaluation in the BCHUnit
    myCommand->local.rxPowerMeasurementPtr = rxPowerMeasurement;
    //assure(myCommand->local.subBand == rxPowerMeasurement->getSubChannel(),"subChannel mismatch");

    /** magic distance calculation. In a real world, the receiver would have to
     * use info about the timing advance or the synchronization for this, but we
     * can use this shortcut.
     */
    // TODO [rs]: shift this functionality into rxPowerMeasurement and get rid of myCommand->local.distance
    assure(mobility!=NULL,"mobility==NULL");
    myCommand->local.distance = mobility->getDistance( source->getService<wns::PositionableInterface*>("mobility") );
    //myCommand->local.distance = rxPowerMeasurement->getDistance();
    // myCommand->magic.source is a wns::node::Interface*
    // wns::PositionableInterface* mobility we get by PhyUser::setMobility() called by ILayer2 class:
    // phyUser->setMobility( getNode()->getService<wns::PositionableInterface*>("mobility") );

    // estimate pathloss (including tx and rx antenna gains)
    //wns::Ratio pathloss = rxPowerMeasurement->getPathLoss();
    //myCommand->local.pathloss = rxPowerMeasurement->getPathLoss();

    MESSAGE_BEGIN(NORMAL, logger, m, "DataInd ");
    wns::service::dll::UnicastAddress sourceAddress =
        stationManager->getStationByNode(source)->getDLLAddress();
    m << "from " << A2N(sourceAddress) << ":";
    m << " SubBand=" << myCommand->local.subBand;
    //m << " SubBand=" << rxPowerMeasurement->getSubChannel();
    m << ", PhyMode=" << *phyModePtr;
    m << std::fixed << std::setprecision( 1 ); // Nachkommastellen
    m << ", "<<rxPowerMeasurement->getString();
    m << std::fixed << std::setprecision( 2 ); // Nachkommastellen
    m << ", MIB=" << rxPowerMeasurement->getMIB();
    MESSAGE_END();

    // During Broadcast Phases, Interference is not representative, therefore we
    // do not store it
    if (broadcast == false)
    {
        // store pathloss measurement for source station in local InterferenceCache
        iCache->storePathloss( source,
                               rxPowerMeasurement->getPathLoss(),
                               dll::services::management::InterferenceCache::Local );
        /* ^ this is ok, but doing the following seems to mix up UL and DL vached values:
           iCache->storeMeasurements( source,
           rxPowerMeasurement,
           dll::services::management::InterferenceCache::Local );
        */
        // "magically" store my measurements in remote InterferenceCache
        myCommand->magic.remoteCache->storeMeasurements( getFUN()->getLayer<dll::ILayer2*>()->getNode(),
                rxPowerMeasurement,
                dll::services::management::InterferenceCache::Remote );

        // write values into Probes: ? Where are probes written ? Cannot see it [rs].
        wns::Ratio sinrEstimation = wns::Ratio::from_dB(0.0);
        if (myCommand->magic.estimatedSINR.I.get_mW() != 0.0) {
            sinrEstimation = myCommand->magic.estimatedSINR.C / myCommand->magic.estimatedSINR.I;
            MESSAGE_BEGIN(NORMAL, logger, m, "DataInd: ");
            m << "SINR Estimation was: (S=" << myCommand->magic.estimatedSINR.C
            << ", I+N=" << myCommand->magic.estimatedSINR.I
            << ", Error=" << sinrEstimation-rxPowerMeasurement->getSINR() << ")\n";
            MESSAGE_END();
        }
    }
    /* Use braodcast to determine pathloss */
    else
    {
        myCommand->magic.remoteCache->storePathloss(getFUN()->getLayer<dll::ILayer2*>()->getNode(),
                rxPowerMeasurement->getPathLoss(),
                dll::services::management::InterferenceCache::Remote );
    }

    /**
     * @todo dbn: Re-enable tracing by implementing a ITracing strategy and
     * include it here
     */
#ifndef NDEBUG
    //traceIncoming(compound, rxPowerMeasurement);
#endif

    // deliver compound
    doOnData(compound);
}

void
PhyUser::doWakeup()
{
    // calls wakeup method of upper functional unit(s)
    getReceptor()->wakeup();
} // wakeup


void
PhyUser::stopTransmission(wns::osi::PDUPtr pdu, int subBand)
{
    transmission->stopTransmission(pdu, subBand);
    activeSubBands.remove(std::pair<wns::osi::PDUPtr, int>(pdu, subBand));
}

void
PhyUser::setReceiveAntennaPattern(wns::node::Interface* destination, wns::service::phy::ofdma::PatternPtr pattern)
{
    assure(pattern, "No Beamforming Pattern set."); // set correct pattern in PhyUser
    bfTransmission->insertReceivePattern(destination, pattern);
}

void
PhyUser::deleteReceiveAntennaPatterns()
{
    // Delete old Rx Antenna Patterns
    std::map<wns::node::Interface*, wns::service::phy::ofdma::PatternPtr> emptyMap;
    bfTransmission->setCurrentReceivePatterns(emptyMap);
}

bool
PhyUser::checkIdle()
{
    return ( activeSubBands.empty() && !transmission->isReceiving() );
}

void
PhyUser::setStateRxTx(StateRxTx _state)
{
    switch (_state)
    {
    case Tx:
        assure(!transmission->isReceiving(), "Tried to switch to Tx while still receiving!");
        notificationService->disableReception();
        break;
    case Rx:
        assure(activeSubBands.empty(), "Tried to switch to Rx while still transmitting!");
        // Cleanup old Antenna Patterns
        this->deleteReceiveAntennaPatterns();
        notificationService->enableReception();
        MESSAGE_SINGLE(VERBOSE,logger,"Cleared old Antenna Patterns");
        break;
    case BothRxTx:
        assure(fddCapable, "only FDD capable stations can set 'state' to 'BothRxTx'!");
        notificationService->enableReception();
        break;
    default:
        assure(false, "You tried to set an unknown state!");
        break;
    }

    stateRxTx=_state;

    MESSAGE_SINGLE(VERBOSE,logger,"Set PhyUser State to: "+getStateRxTx());
}

std::string
PhyUser::getStateRxTx() const
{
    switch (stateRxTx)
    {
    case Tx:
        return "Tx";
        break;
    case Rx:
        return "Rx";
        break;
    case BothRxTx:
        return "BothRxTx";
        break;
    default:
        assure(false, "Unknown stateTxRx in PhyUser!");
        return "";
    }
    return "";
}

void
PhyUser::rapTuning()
{
    if (getFUN()->getLayer<dll::ILayer2*>()->getStationType() == wns::service::dll::StationTypes::FRS())
        transmission->setTxRxSwap(false);
}

void
PhyUser::utTuning()
{
    if (getFUN()->getLayer<dll::ILayer2*>()->getStationType() == wns::service::dll::StationTypes::FRS())
        transmission->setTxRxSwap(true);
}

void
PhyUser::startTransmission(const wns::ldk::CompoundPtr& compound)
{
    assure(getFUN()->getProxy()->commandIsActivated( compound->getCommandPool(), this),
           "PhyCommand not specified. PhyUser can not handle this compound!");
    PhyCommand* myCommand = getCommand(compound->getCommandPool());
    MESSAGE_SINGLE(NORMAL, logger,"startTransmission(): start="<< myCommand->local.start <<"s..stop=" << myCommand->local.stop <<"s => d="<<myCommand->local.stop-myCommand->local.start<<"s, subBand=" << myCommand->local.subBand);

    wns::Power txPower = myCommand->magic.txp;

    int subBand = myCommand->local.subBand;
    int beam = myCommand->local.beam;
    wns::service::phy::phymode::PhyModeInterfacePtr phyModePtr = myCommand->local.phyModePtr;
    simTimeType duration = myCommand->local.stop - myCommand->local.start; // duration should be multiple of OFDM symbol length
    assure(myCommand->local.start == es->getTime(), "myCommand->local.start is not now");
    assure(phyModePtr->dataRateIsValid(),"!dataRateIsValid for "<<*phyModePtr);
    int capacity = phyModePtr->getBitCapacityFractional(duration);
    MESSAGE_SINGLE(NORMAL, logger,"PhyMode="<<*phyModePtr<<" supports "<<phyModePtr->getBitCapacityFractional(1.0)<<" bit/s/subChannel");
    MESSAGE_BEGIN(NORMAL, logger, m, "startTransmission on subBand=");
    m << subBand
    //<< std::ios::fixed // puts "4" into output!?
    << ", PhyMode=" << *phyModePtr
    //<< std::ios::floatfield << std::setprecision(3) // puts "260" into output!?
    << ", D=" << duration*1e6 << "us"
    << ", " << compound->getLengthInBits() << " bit"
    << ", cap=" << capacity << " bit"
    << ", source=" << myCommand->magic.source->getName()
    << ", dest=" << (myCommand->magic.destination == NULL ? "BROADCAST" : myCommand->magic.destination->getName())
    << ", P=" << txPower;
    MESSAGE_END();

    assure(compound->getLengthInBits() <= capacity , "SDU too long: len="<<compound->getLengthInBits()<<" <= cap="<<capacity<<" ("<<phyModePtr->getString()<<", D="<<duration<<"s)");

    if (myCommand->magic.destination == 0) {
        // no destination, send broadcast
        assure(beam==0,"broadcast is only possible with beam==0, but beam="<<beam);
        transmission->startBroadcast(compound, subBand, txPower, phyModePtr);
    } else {
        // we have a destination, this is not a broadcast
        if (myCommand->local.beamforming == true) {
            assure(myCommand->local.pattern, "No Beamforming Pattern set.");
            // call startTransmission method of dataTransmission service
            // which is located in WNS/service/phy/ofdma/Station.cpp: Station::startTransmission
            bfTransmission->startTransmission(compound,
                                              myCommand->magic.destination,
                                              subBand,
                                              //beam, // <- cannot be used here
                                              myCommand->local.pattern,
                                              txPower,
                                              phyModePtr);
        } else {
            transmission->startUnicast(compound,
                                       myCommand->magic.destination,
                                       subBand,
                                       //beam, // for MIMO
                                       txPower,
                                       phyModePtr);
        }
    }
    activeSubBands.push_back( std::pair<wns::osi::PDUPtr, int>(compound, subBand) );
    // prepare the stopEvent
    es->schedule(StopTxEvent(compound, subBand, this), myCommand->local.stop-safetyFraction);
} // startTransmission

void
PhyUser::setDataTransmissionService(wns::service::Service* phy)
{
    assure(phy, "must be non-NULL");
    assureType(phy, wns::service::phy::ofdma::DataTransmission*);

    wns::service::phy::ofdma::DataTransmission* tmp =
        dynamic_cast<wns::service::phy::ofdma::DataTransmission*>(phy);

    bfTransmission = tmp;
    transmission   = tmp;
}

wns::service::phy::ofdma::DataTransmission*
PhyUser::getDataTransmissionService() const
{
    assure(transmission, "no ofdma::DataTransmission set. Did you call setDataTransmission()?");
    assureType(transmission, wns::service::phy::ofdma::DataTransmission*);
    return dynamic_cast<wns::service::phy::ofdma::DataTransmission*>(transmission);
}

// called by ILayer2 class in WinProSt:
void
PhyUser::setNotificationService(wns::service::Service* phy)
{
    MESSAGE_SINGLE(NORMAL, logger, "PhyUser::setNotificationService() called");
    assure(phy, "must be non-NULL");
    assureType(phy, wns::service::phy::ofdma::Notification*);
    notificationService = dynamic_cast<wns::service::phy::ofdma::Notification*>(phy);
    notificationService->registerHandler(this);
}

// currently not called anywhere:
wns::service::phy::ofdma::Notification*
PhyUser::getNotificationService() const
{
    assure(notificationService, "no ofdma::Notification set. Did you call setNotificationService()?");
    return notificationService;
}

void
PhyUser::setMACAddress(const wns::service::dll::UnicastAddress& _address)
{
    address = _address;
    MESSAGE_SINGLE(NORMAL, logger, "setting my MAC address to: " << address);
}

void
PhyUser::setMobility(wns::PositionableInterface* _mobility)
{
    assure(_mobility, "Trying to set invalid mobility Object Pointer");
    mobility = _mobility;
}

bool
PhyUser::isFddCapable() const
{
    return fddCapable;
}




