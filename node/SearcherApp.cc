/*
 * SearcherApp.cc
 *
 *  Created on: Mar 4, 2018
 *      Author: Andrew Fisher
 */


#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include <vector>
#include <omnetpp.h>
#include <string>
#include "Packet_m.h"

using namespace omnetpp;

/**
 * Generates traffic for the network.
 */
class SearcherApp : public cSimpleModule
{
  private:
    // configuration
    int myAddress;
    std::vector<int> destAddresses;
    cPar *sendIATime;
    cPar *packetLengthBytes;
    int sentPackets; // Records how many packets we have sent from this node to ward off cycles between nodes without
                     // the file.

    // state
    cMessage *generatePacket;
    long pkCounter;

    // signals
    simsignal_t endToEndDelaySignal;
    simsignal_t hopCountSignal;
    simsignal_t sourceAddressSignal;

    //search target
    bool hasFile;

  public:
    SearcherApp();
    virtual ~SearcherApp();

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
};

Define_Module(SearcherApp);

SearcherApp::SearcherApp()
{
    generatePacket = nullptr;
}

SearcherApp::~SearcherApp()
{
    cancelAndDelete(generatePacket);
}

void SearcherApp::initialize()
{
    myAddress = par("address");
    packetLengthBytes = &par("packetLength");
    sendIATime = &par("sendIaTime");  // volatile parameter
    pkCounter = 0;
    sentPackets = 0;

    //Add this node to network address list
    cModule *node = getParentModule();
    cModule *network = node->getParentModule();
    bool hasConnections = node->par("hasConnections");
    if(hasConnections)
        network->par("addresses").setStringValue(network->par("addresses").stdstringValue() + " " + std::to_string(myAddress));
    else
        cancelAndDelete(generatePacket);

    EV << network->par("addresses").stringValue() << endl;

    if(intuniform(0,1) == 1){
        getParentModule()->par("hasFile").setBoolValue(true);
    }

    hasFile = getParentModule()->par("hasFile").boolValue();

    WATCH(pkCounter);
    WATCH(myAddress);

    if(!hasFile && hasConnections){
        EV << "Don't have the file, generating packet" << endl;
        generatePacket = new cMessage("nextPacket");

        scheduleAt(sendIATime->doubleValue(), generatePacket);
    }
    endToEndDelaySignal = registerSignal("endToEndDelay");
    hopCountSignal = registerSignal("hopCount");
    sourceAddressSignal = registerSignal("sourceAddress");


}

void SearcherApp::handleMessage(cMessage *msg)
{

    const char *destAddressesPar = getParentModule()->getParentModule()->par("addresses");
    cStringTokenizer tokenizer(destAddressesPar);
    const char *token;

    while ((token = tokenizer.nextToken()) != nullptr && destAddresses.size() < getParentModule()->getParentModule()->par("n").longValue())
        destAddresses.push_back(atoi(token));

    if (destAddresses.size() == 0)
        throw cRuntimeError("At least one address must be specified in the destAddresses parameter!");

    if (msg == generatePacket) {

    // Sending packet
    int destAddress = destAddresses[intuniform(0, destAddresses.size()-1)];

    char pkname[40];
    sprintf(pkname, "pk-%d-to-%d-#%ld", myAddress, destAddress, pkCounter++);
    EV << "generating packet " << pkname << endl;

    Packet *pk = new Packet(pkname);
    pk->setByteLength(packetLengthBytes->longValue());
    pk->setKind(intuniform(0, 7));
    pk->setSrcAddr(myAddress);
    pk->setDestAddr(destAddress);

    while(destAddress == myAddress){
        destAddress = destAddresses[intuniform(0,destAddresses.size()-1)];
        pk->setDestAddr(destAddress);
    }
    if(!hasFile)
        send(pk, "out");

    scheduleAt(simTime() + sendIATime->doubleValue(), generatePacket);
    if (hasGUI())
        getParentModule()->bubble("Generating packet...");
}

    else {
        // Handle incoming packet
        Packet *pk = check_and_cast<Packet *>(msg);
        EV << "received packet " << pk->getName() << " after " << pk->getHopCount() << "hops" << endl;
        emit(endToEndDelaySignal, simTime() - pk->getCreationTime());
        emit(hopCountSignal, pk->getHopCount());
        emit(sourceAddressSignal, pk->getSrcAddr());

        int destAddr = pk->getDestAddr();

        bool carrying = pk->getCarryingFile();
        bool origSender = false;

        if(myAddress == pk->getSrcAddr())
            origSender = true;

        if (destAddr == myAddress) {
                            // Search logic
                            if(hasFile && carrying && origSender){
                                delete pk;
                                cancelEvent(generatePacket);
                                EV << "Something went wrong, we already had the file!" << endl;  // If the original searcher has the file, something went wrong
                            }                                                                    // in initialize.
                            else if(hasFile && carrying && !origSender){
                                pk->setDestAddr(pk->getSrcAddr());                               // If the packet is carrying, send it back along to the original
                                send(pk, "out");                                                 // searcher.
                                EV << "Packet carrying file, forwarding to " << pk->getSrcAddr() << endl;
                            }
                            else if(hasFile && !carrying && origSender){
                                delete pk;
                                cancelEvent(generatePacket);                                     //Same as first case.
                                EV << "Something went wrong, we already had the file!" << endl;
                            }
                            else if(!hasFile && carrying && origSender){
                                hasFile = true;                                                  // We are back to the original searcher with the file, time to
                                getParentModule()->par("hasFile").setBoolValue(true);            // update and remove the packet.
                                EV << "File returned! Node status updated!" << endl;
                                cancelEvent(generatePacket);
                                delete pk;
                            }
                            else if(!hasFile && !carrying && origSender){                        // We are back to where we started somehow, send out randomly.
                                pk->setDestAddr(destAddresses[intuniform(0,destAddresses.size()-1)]);
                                while(pk->getDestAddr() == myAddress){
                                    pk->setDestAddr(destAddresses[intuniform(0,destAddresses.size()-1)]);     // Loopback guard
                                }
                                send(pk, "out");
                            }
                            else if(!hasFile && carrying && !origSender){
                                 pk->setDestAddr(pk->getSrcAddr());                              // Forward packet along to original searcher.  Do not update
                                 send(pk, "out");                                                // any nodes along the way.
                                 EV << "Guys, is 4 gone? He scares me. I'm the sixth case, don't hurt me" << endl;
                            }
                            else if(hasFile && !carrying && !origSender){
                                pk->setCarryingFile(true);                                       // We found the file, time to go back to the original
                                EV << "File found! Returning!" << endl;                        // searcher.
                                pk->setDestAddr(pk->getSrcAddr());
                                send(pk, "out");
                            }
                            else if(!hasFile && !carrying && !origSender){                       // The node we chose doesn't have the file, forward to another.
                                EV << "File not found, continuing search." << endl;
                                pk->setDestAddr(destAddresses[intuniform(0,destAddresses.size()-1)]);
                                while(pk->getDestAddr() == myAddress){
                                    pk->setDestAddr(destAddresses[intuniform(0,destAddresses.size()-1)]);
                                }
                                send(pk, "out");
                            }

       }





    if (hasGUI())
        getParentModule()->bubble("Arrived!");
    }
}
