/*
 * Searching.cc
 *
 *  Created on: Mar 5, 2018
 *      Author: Andrew
 *
 *      Handles routing and search conditions for SearcherNodes
 */


#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include <map>
#include <omnetpp.h>
#include "Packet_m.h"

using namespace omnetpp;

/**
 * Demonstrates dynamic routing and search
 */
class Searching : public cSimpleModule
{
  private:
    int myAddress;
    bool hasFile;

    typedef std::map<int, int> RoutingTable;  // destaddr -> gateindex
    RoutingTable rtable;

    simsignal_t dropSignal;
    simsignal_t outputIfSignal;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
};

Define_Module(Searching);

void Searching::initialize()
{
    myAddress = getParentModule()->par("address");
    hasFile = getParentModule()->par("hasFile");
    dropSignal = registerSignal("drop");
    outputIfSignal = registerSignal("outputIf");

    //
    // Brute force approach -- every node does topology discovery on its own,
    // and finds routes to all other nodes independently, at the beginning
    // of the simulation. This could be improved: (1) central routing database,
    // (2) on-demand route calculation
    //
    cTopology *topo = new cTopology("topo");

    std::vector<std::string> nedTypes;
    nedTypes.push_back(getParentModule()->getNedTypeName());
    topo->extractByNedTypeName(nedTypes);
    EV << "cTopology found " << topo->getNumNodes() << " nodes\n";


    EV << "hasFile: " << hasFile << "\n";
    cTopology::Node *thisNode = topo->getNodeFor(getParentModule());

    // find and store next hops - This is setting up the routing table
    for (int i = 0; i < topo->getNumNodes(); i++) {
        if (topo->getNode(i) == thisNode)
            continue;  // skip ourselves
        topo->calculateUnweightedSingleShortestPathsTo(topo->getNode(i));

        if (thisNode->getNumPaths() == 0)
            continue;  // not connected

        cGate *parentModuleGate = thisNode->getPath(0)->getLocalGate();
        int gateIndex = parentModuleGate->getIndex();
        int address = topo->getNode(i)->getModule()->par("address");
        rtable[address] = gateIndex;
        EV << "  towards address " << address << " gateIndex is " << gateIndex << endl;
    }
    delete topo;
}

void Searching::handleMessage(cMessage *msg)
{

    EV << "PACKET'S BACK ON THE MENU BOYS" << endl;
    Packet *pk = check_and_cast<Packet *>(msg);
    int destAddr = pk->getDestAddr();

    static bool carrying = pk->getCarryingFile();
    bool origSender = false;
    if(myAddress == pk->getSrcAddr())
        origSender = true;

    EV << "destAddr: " << destAddr << endl;

    if (destAddr == myAddress) {
                    // Search logic
                    if(hasFile && carrying && origSender){
                        delete pk;
                        EV << "Something went wrong, we already had the file!" << endl;  // If the original searcher has the file, something went wrong
                    }                                                                    // in initialize.
                    else if(hasFile && carrying && !origSender){
                        pk->setDestAddr(pk->getSrcAddr());                               // If the packet is carrying, send it back along to the original
                        send(pk, "out", rtable[pk->getDestAddr()]);                                                 // searcher.
                        EV << "I'm the second case!" << endl;
                    }
                    else if(hasFile && !carrying && origSender){
                        delete pk;                                                       //Same as first case.
                        EV << "Something went wrong, we already had the file!" << endl;
                    }
                    else if(!hasFile && carrying && origSender){
                        //hasFile = true;                                                // We are back to the original searcher with the file, time to
                        getParentModule()->par("hasFile").setBoolValue(true);            // update and remove the packet.
                        //EV << "File returned! Node status updated!" << endl;
                        delete pk;
                        EV << "And I'm the fourth case, fuck you!" << endl;
                    }
                    else if(!hasFile && !carrying && origSender){                        // We are back to where we started somehow, send out randomly.
                        pk->setDestAddr(rtable[intuniform(0, rtable.size()-1)]);
                        while(pk->getDestAddr() == myAddress){
                            pk->setDestAddr(rtable[intuniform(0, rtable.size()-1)]);     // Loopback guard
                        }
                        send(pk, "out", rtable[pk->getDestAddr()]);
                        EV << "That was uncalled for.  Fifth case here" << endl;
                    }
                    else if(!hasFile && carrying && !origSender){
                         pk->setDestAddr(pk->getSrcAddr());                              // Forward packet along to original searcher.  Do not update
                         send(pk, "out", rtable[pk->getDestAddr()]);                                                // any nodes along the way.
                         EV << "Guys, is 4 gone? He scares me. I'm the sixth case, don't hurt me" << endl;
                    }
                    else if(hasFile && !carrying && !origSender){
                        pk->setCarryingFile(true);                                       // We found the file, time to go back to the original
                        EV << "File found! Returning!" << endl;                        // searcher.
                        pk->setDestAddr(pk->getSrcAddr());
                        send(pk, "out", rtable[pk->getDestAddr()]);
                    }
                    else if(!hasFile && !carrying && !origSender){                       // The node we chose doesn't have the file, forward to another.
                        EV << "File not found, continuing search." << endl;
                        pk->setDestAddr(rtable[intuniform(0, rtable.size()-1)]);
                        while(pk->getDestAddr() == myAddress){
                            pk->setDestAddr(rtable[intuniform(0, rtable.size()-1)]);
                        }
                        send(pk, "out", rtable[pk->getDestAddr()]);
                    }
            emit(outputIfSignal, -1);  // -1: local
            return;
        }

    //If we can't go anywhere drop packet
    RoutingTable::iterator it = rtable.find(destAddr);
    if (it == rtable.end()) {
        EV << "address " << destAddr << " unreachable, discarding packet " << pk->getName() << endl;
        emit(dropSignal, (long)pk->getByteLength());
        delete pk;
        return;
    }

    //Final forwarding
    int outGateIndex = (*it).second;
    EV << "forwarding packet " << pk->getName() << " on gate index " << outGateIndex << endl;
    pk->setHopCount(pk->getHopCount()+1);
    emit(outputIfSignal, outGateIndex);


}


