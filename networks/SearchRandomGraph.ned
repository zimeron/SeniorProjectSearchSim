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

package networks;

import node.SearchNode;


network SearchRandomGraph
{
    parameters:
        double n; 
        string addresses ="";
    submodules:
        node[n]: SearchNode {
            parameters:
                address = index;
                @display("p=100,100,ring");
            gates:
                port[n];
        }
    connections allowunconnected:
        for i=0..n-2, for j=i+1..n-1 {
            node[i].port[j] <--> {  delay = 0.1ms; datarate = 1Gbps; } <--> node[j].port[i] if uniform(0,1)<0.15;
        }
}