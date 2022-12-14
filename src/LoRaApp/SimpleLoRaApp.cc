//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "SimpleLoRaApp.h"
//#include "inet/mobility/static/StationaryMobility.h"
#include "inet/mobility/single/RandomWPMobility.h"
//#include "inet/mobility/single/GaussMarkovMobility.h"
namespace inet {

Define_Module(SimpleLoRaApp);

void SimpleLoRaApp::initialize(int stage)
{
    cSimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        std::pair<double,double> coordsValues = std::make_pair(-1, -1);
        cModule *host = getContainingNode(this);
        // Generate random location for nodes if circle deployment type
        if (strcmp(host->par("deploymentType").stringValue(), "circle")==0) {
           coordsValues = generateUniformCircleCoordinates(host->par("maxGatewayDistance").doubleValue(), host->par("gatewayX").doubleValue(), host->par("gatewayY").doubleValue());
           //StationaryMobility *mobility = check_and_cast<StationaryMobility *>(host->getSubmodule("mobility"));
            RandomWPMobility *mobility = check_and_cast<RandomWPMobility *>(host->getSubmodule("mobility"));
           // GaussMarkovMobility *mobility = check_and_cast<GaussMarkovMobility *>(host->getSubmodule("mobility"));
           mobility->par("initialX").setDoubleValue(coordsValues.first);
           mobility->par("initialY").setDoubleValue(coordsValues.second);
        }
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        bool isOperational;
        NodeStatus *nodeStatus = dynamic_cast<NodeStatus *>(findContainingNode(this)->getSubmodule("status"));
        isOperational = (!nodeStatus) || nodeStatus->getState() == NodeStatus::UP;
        if (!isOperational)
            throw cRuntimeError("This module doesn't support starting in node DOWN state");
        do {
            timeToFirstPacket = par("timeToFirstPacket");
            EV << "Wylosowalem czas :" << timeToFirstPacket << endl;
            //if(timeToNextPacket < 5) error("Time to next packet must be grater than 3");
        } while(timeToFirstPacket <= 5);

        //timeToFirstPacket = par("timeToFirstPacket");
        sendMeasurements = new cMessage("sendMeasurements");
        scheduleAt(simTime()+timeToFirstPacket, sendMeasurements);

        sentPackets = 0;
        receivedADRCommands = 0;
        numberOfPacketsToSend = par("numberOfPacketsToSend");

        LoRa_AppPacketSent = registerSignal("LoRa_AppPacketSent");

        //LoRa physical layer parameters
        loRaTP = par("initialLoRaTP").doubleValue();
        loRaCF = units::values::Hz(par("initialLoRaCF").doubleValue());
        loRaSF = par("initialLoRaSF");
        loRaBW = inet::units::values::Hz(par("initialLoRaBW").doubleValue());
        loRaCR = par("initialLoRaCR");
        loRaUseHeader = par("initialUseHeader");
        evaluateADRinNode = par("evaluateADRinNode");
        sfVector.setName("SF Vector");
        tpVector.setName("TP Vector");
    }
}

std::pair<double,double> SimpleLoRaApp::generateUniformCircleCoordinates(double radius, double gatewayX, double gatewayY)
{
    double randomValueRadius = uniform(0,(radius*radius));
    double randomTheta = uniform(0,2*M_PI);

    // generate coordinates for circle with origin at 0,0
    double x = sqrt(randomValueRadius) * cos(randomTheta);
    double y = sqrt(randomValueRadius) * sin(randomTheta);
    // Change coordinates based on coordinate system used in OMNeT, with origin at top left
    x = x + gatewayX;
    y = gatewayY - y;
    std::pair<double,double> coordValues = std::make_pair(x,y);
    return coordValues;
}

void SimpleLoRaApp::finish()
{
    cModule *host = getContainingNode(this);
   // StationaryMobility *mobility = check_and_cast<StationaryMobility *>(host->getSubmodule("mobility"));
    RandomWPMobility *mobility = check_and_cast<RandomWPMobility *>(host->getSubmodule("mobility"));
       //GaussMarkovMobility *mobility = check_and_cast<GaussMarkovMobility *>(host->getSubmodule("mobility"));
    Coord coord = mobility->getCurrentPosition();
    recordScalar("positionX", coord.x);
    recordScalar("positionY", coord.y);
    recordScalar("finalTP", loRaTP);
    recordScalar("finalSF", loRaSF);
    recordScalar("sentPackets", sentPackets);
    recordScalar("receivedADRCommands", receivedADRCommands);
}

void SimpleLoRaApp::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        if (msg == sendMeasurements)
        {
            sendJoinRequest();
            if (simTime() >= getSimulation()->getWarmupPeriod())
                sentPackets++;
            delete msg;
            if(numberOfPacketsToSend == 0 || sentPackets < numberOfPacketsToSend)
            {
                double time;
                if(loRaSF == 7) time = 7.808;
                if(loRaSF == 8) time = 13.9776;
                if(loRaSF == 9) time = 24.6784;
                if(loRaSF == 10) time = 49.3568;
                if(loRaSF == 11) time = 85.6064;
                if(loRaSF == 12) time = 171.2128;
                do {
                    timeToNextPacket = par("timeToNextPacket");
                    //if(timeToNextPacket < 3) error("Time to next packet must be grater than 3");
                } while(timeToNextPacket <= time);
                sendMeasurements = new cMessage("sendMeasurements");
                scheduleAt(simTime() + timeToNextPacket, sendMeasurements);
            }
        }
    }
    else
    {
        handleMessageFromLowerLayer(msg);
        delete msg;
        //cancelAndDelete(sendMeasurements);
        //sendMeasurements = new cMessage("sendMeasurements");
        //scheduleAt(simTime(), sendMeasurements);
    }
}

void SimpleLoRaApp::handleMessageFromLowerLayer(cMessage *msg)
{
    LoRaAppPacket *packet = check_and_cast<LoRaAppPacket *>(msg);
    if (simTime() >= getSimulation()->getWarmupPeriod())
        receivedADRCommands++;
    if(evaluateADRinNode)
    {
        EV<<"handleMessageFromLowerLayer 149"<<endl;
        ADR_ACK_CNT = 0;
        if(packet->getMsgType() == TXCONFIG)
        {
            if(packet->getOptions().getLoRaTP() != -1)
            {
                loRaTP = packet->getOptions().getLoRaTP();
            }
            if(packet->getOptions().getLoRaSF() != -1)
            {
                loRaSF = packet->getOptions().getLoRaSF();
            }
        }
    }
}

bool SimpleLoRaApp::handleOperationStage(LifecycleOperation *operation, int stage, IDoneCallback *doneCallback)
{
    Enter_Method_Silent();

    throw cRuntimeError("Unsupported lifecycle operation '%s'", operation->getClassName());
    return true;
}

void SimpleLoRaApp::sendJoinRequest()
{
    LoRaAppPacket *request = new LoRaAppPacket("DataFrame");
    request->setKind(DATA);
    lastSentMeasurement = rand();
    request->setSampleMeasurement(lastSentMeasurement);

    if(evaluateADRinNode && sendNextPacketWithADRACKReq)
    {
        request->getOptions().setADRACKReq(true);
        sendNextPacketWithADRACKReq = false;
    }

    if(evaluateADRinNode){
        EV<<"Previous SF is "<<loRaSF<<endl;
    if(ADR_ACK_CNT >= 1)
                   {
                       EV<<"calling increaseSF"<<endl;
                       ADR_ACK_CNT = 0;
                       if (loRaSF<12) loRaSF++;//loRaSF=12;//
                   }
    EV<<"Current SF is "<<loRaSF<<endl;

    }
    //add LoRa control info
    LoRaMacControlInfo *cInfo = new LoRaMacControlInfo;
    cInfo->setLoRaTP(loRaTP);
    cInfo->setLoRaCF(loRaCF);
    cInfo->setLoRaSF(loRaSF);
    cInfo->setLoRaBW(loRaBW);
    cInfo->setLoRaCR(loRaCR);

    request->setControlInfo(cInfo);
    sfVector.record(loRaSF);
    tpVector.record(loRaTP);
    send(request, "appOut");
    if(evaluateADRinNode)
    {
        EV<<"199 "<<endl;
        EV<<"ADR_ACK_CNT is  "<<ADR_ACK_CNT<<endl;
        EV<<"ADR_ACK_LIMIT is  "<<ADR_ACK_LIMIT<<endl;
        EV<<"ADR_ACK_DELAY is  "<<ADR_ACK_DELAY<<endl;


        ADR_ACK_CNT++;
        if(ADR_ACK_CNT == ADR_ACK_LIMIT) sendNextPacketWithADRACKReq = true;
        if(ADR_ACK_CNT >= ADR_ACK_LIMIT + ADR_ACK_DELAY)
        {
            EV<<"calling increaseSF"<<endl;
            ADR_ACK_CNT = 0;
            increaseSFIfPossible();
        }

    }
    emit(LoRa_AppPacketSent, loRaSF);
}

void SimpleLoRaApp::increaseSFIfPossible()
{
    if(loRaSF < 12) loRaSF++;
}

} //end namespace inet
