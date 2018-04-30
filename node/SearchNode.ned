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

package node;

//
// A "SearchNode" consists of a Routing plus a SearchApp module
// 
//
module SearchNode
{
    parameters:
        int address;
        string appType;
        bool hasFile;
        bool hasConnections;
        @display("i=misc/node_vs,gold");
    gates:
        inout port[];
    submodules:
        searching: Routing {
            parameters:
                @display("p=140,130");
            gates:
                in[sizeof(port)];
                out[sizeof(port)];
        }
        app: <appType> like IApp {
            parameters:
                address = address;
                @display("p=140,60");
        }
        queue[sizeof(port)]: L2Queue {
            parameters:
                @display("p=80,200,row");
        }
    connections:
        searching.localOut --> app.in;
        searching.localIn <-- app.out;
        for i=0..sizeof(port)-1 {
            searching.out[i] --> queue[i].in;
            searching.in[i] <-- queue[i].out;
            queue[i].line <--> port[i];
        }
}