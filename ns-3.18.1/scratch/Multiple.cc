#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <vector>


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4.h"
#include "ns3/packet.h"
#include "ns3/ppp-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/tcp-header.h"

#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"

#include "ns3/mymacros.h"
#include <sstream>
#include <iostream>
#include <algorithm>
#include <functional>
#include <vector>
#include <ctime>
#include <cstdlib>

#define ICTCP_MODE_DEF					  0
#define RTCP_MODE_DEF					  0
#define DCTCP_MODE_DEF					  0
#define RDTCP_MODE_DEF					  0
#define RDTCP_CONTROLLER_MODE_DEF		  0
#define ECN_MODE_DEF                      1
#define USE_RED_QUEUE_DEF                 0
#define MACHINES_PER_RACK_DEF             40
#define NUMBER_OF_RACKS_DEF               25
#define MACHINES_TO_TOR_RATE_DEF          1
#define LINK_DELAY_DEF                    "40us"
#define NUMBER_OF_APPS_DEF                5
#define QUEUELENGTH_TOR_DEF               80 // interms of packets
#define QUEUELENGTH_FABRIC_DEF            80  // interms of packets
#define THRESHOLD_TOR_DEF_MIN             20 // interms of % occupancy
#define THRESHOLD_TOR_DEF_MAX             40  // interms of % occupancy
//#define THRESHOLD_FABRIC_DEF              10  // interms of % occupancy
#define QUERIES_PER_APP_DEF               2
#define INTERARRIVAL_MULTIPLIER_DEF       2.0
#define STOP_DEF                          10
#define IGNORE_ECN_PORT                   1000
#define LONG_FLOW_PORT                    900
#define USE_LONG_FLOWS                    1
#define TIGHT_DEADLINE                    0
#define PRINT_STATUS                      1
#define SEGMENT_SIZE                      1446
#define RTO_MIN                           10


using namespace std;

// A typical data center has racks of machines connected to a ToR switch using p2p links
// Each ToR swicth connects to another fabric swicth, it is a 2-level tree

using namespace ns3;

// Primary knobs
uint32_t        ICTCPMode                 = ICTCP_MODE_DEF;
uint32_t        RTCPMode                  = RTCP_MODE_DEF;
uint32_t        DCTCPMode                 = DCTCP_MODE_DEF;
uint32_t        RDTCPMode                 = RDTCP_MODE_DEF;
uint32_t        RDTCP_CONTROLLER_Mode     = RDTCP_CONTROLLER_MODE_DEF		  ;

uint32_t        ECNMode                   = ECN_MODE_DEF;
uint32_t        Use_RED_Mode              = USE_RED_QUEUE_DEF;
uint32_t        MachinesPerRack           = MACHINES_PER_RACK_DEF;
uint32_t        NumberofRacks             = NUMBER_OF_RACKS_DEF;
uint32_t        Machines2ToRRate          = MACHINES_TO_TOR_RATE_DEF;
string          LinkDelay                 = LINK_DELAY_DEF;
uint32_t        NumberofApps              = NUMBER_OF_APPS_DEF;
uint32_t        QueuelengthToR            = QUEUELENGTH_TOR_DEF;
uint32_t        QueuelengthFabric         = QUEUELENGTH_FABRIC_DEF;
uint32_t        ThresholdToR_min          = THRESHOLD_TOR_DEF_MIN;
uint32_t        ThresholdToR_max          = THRESHOLD_TOR_DEF_MAX;
//uint32_t        ThresholdFabric           = THRESHOLD_FABRIC_DEF;
//uint32_t        QueriesPerApp             = QUERIES_PER_APP_DEF;
double          InterarrivalMultiplier    = INTERARRIVAL_MULTIPLIER_DEF; 
uint32_t        Stop                      = STOP_DEF;
bool            UseLongFlows              = USE_LONG_FLOWS;
int             TightDeadlines            = TIGHT_DEADLINE;
int             Print_st                  = PRINT_STATUS;
uint32_t        SegmentSize               = SEGMENT_SIZE;
uint32_t        rto_min                   = RTO_MIN;
uint32_t        delack_timeout            = RTO_MIN;
uint16_t			offerload				  = 0;
uint32_t        longflowsize              = 1; //(MB). in 1000 nodes and 5 apps and 50kb short flows data center. due to 5x/(990*50+5x)=0.75, x=29700 kb
uint16_t			uniform_distr		  	  = 1;
uint32_t			servers_in_group	  	  = 20;


// TODO: If these change per app, may need to parameterize that

// derived knobs
uint32_t        NumberofEndPoints; 
uint32_t        ToR2FabricRate;
std::string     Machines2ToRRateStr;
std::string     ToR2FabricRateStr;
uint32_t        NodesPerApp;

//declare a set of deadlines/one per app - used as ports too
//uint16_t Deadlines[]      = {30,   30,   30,   30,   30,  30,  30,30,30,30};
//note that do not exceed 128k
uint16_t Start_Interval[] = {300,         400,         500,         600,         700,  }; //us
uint16_t Deadlines[]      = {20,          30,          35,          40,          45,  }; //ms
uint32_t MaxBytesPerApp[] = {1024 * 1024,   32 * 1024,   32 * 1024,   32 * 1024,   32 * 1024};
//uint32_t delaytime = 0; //add by xl for two 4M long flows test.
//uint32_t MaxBytesPerApp[] = {15 * 1024,   20 * 1024,   25 * 1024,   30 * 1024,   35 * 1024};
//uint32_t MaxBytesPerApp[] = {90 * 1024,   120 * 1024,   140 * 1024,   160 * 1024,   180 * 1024};
//uint32_t MaxBytesPerApp[] = {2 * 1024,   6 * 1024,   10 * 1024,   14 * 1024,   18 * 1024};

// declare the node pointers
vector < Ptr <Node> > nEnd;
vector < Ptr <Node> > nToR;
Ptr<Node> nFabric;

NS_LOG_COMPONENT_DEFINE ("DataCenter");

string uint2str(uint32_t number)
{

  std::stringstream out;
  out << number;
  return out.str();
}

void QueueBytes0 (uint32_t oldbytes, uint32_t newbytes)
{//xl
	  PRINT_SIMPLE(newbytes   <<"   =newbytes0,   "<< oldbytes<<  "  =oldbytes0");
}
void QueuePackets0 (uint32_t oldpackets, uint32_t newpackets)
{// by xl
	  PRINT_SIMPLE (newpackets<<"   =newpackets0, "<< oldpackets<<"  =oldpackets0");
}
void DropTrace0 (Ptr<Packet const> p)
{// by xl
	PppHeader ppp;
	Ipv4Header iph;
	TcpHeader tcph;
	Ptr <Packet> packet = p->Copy();
	packet->RemoveHeader (ppp);
	packet->RemoveHeader (iph);
	packet->RemoveHeader (tcph);
	  PRINT_SIMPLE ("droptrace0: original packet size:"<<p->GetSize() << ", data size:"<<packet->GetSize()
			  <<", pppheader size:"<<ppp.GetSerializedSize() <<", ipv4header size:"<<iph.GetSerializedSize() 
			  <<", tcpheader size:"<<tcph.GetSerializedSize() 
			  //<<",  ip source("<<iph.GetSource() <<") to ip destination("<<iph.GetDestination() 
			  //<<"), source port:"<<tcph.GetSourcePort() <<", destination port:"<<tcph.GetDestinationPort() 
			  //<<", seq:"<<tcph.GetSequenceNumber() <<", ack:"<<tcph.GetAckNumber() 
			  <<",  ppph:"<<ppp <<",  ipv4h:"<<iph <<",  tcph:"<<tcph);
}
void EnQTrace0 (Ptr<Packet const> p)
{// by xl
	PppHeader ppp;
	Ipv4Header iph;
	TcpHeader tcph;
	Ptr <Packet> packet = p->Copy();
	packet->RemoveHeader (ppp);
	packet->RemoveHeader (iph);
	packet->RemoveHeader (tcph);
	  PRINT_SIMPLE ("enqtrace0: packet size:"<<p->GetSize() << ", data size:"<<packet->GetSize()
			  <<",  ip source("<<iph.GetSource() <<") to ip destination("<<iph.GetDestination() 
			  //<<"), source port:"<<tcph.GetSourcePort() <<", destination port:"<<tcph.GetDestinationPort() 
			  //<<", seq:"<<tcph.GetSequenceNumber() <<", ack:"<<tcph.GetAckNumber() 
			  <<",  tcph:"<<tcph);
}
void DeQTrace0 (Ptr<Packet const> p)
{// by xl
	PppHeader ppp;
	Ipv4Header iph;
	TcpHeader tcph;
	Ptr <Packet> packet = p->Copy();
	packet->RemoveHeader (ppp);
	packet->RemoveHeader (iph);
	packet->RemoveHeader (tcph);
	  PRINT_SIMPLE ("deqtrace0: packet size:"<<p->GetSize() << ", data size:"<<packet->GetSize()
			  <<",  ip source("<<iph.GetSource() <<") to ip destination("<<iph.GetDestination() 
			  //<<"), source port:"<<tcph.GetSourcePort() <<", destination port:"<<tcph.GetDestinationPort() 
			  //<<", seq:"<<tcph.GetSequenceNumber() <<", ack:"<<tcph.GetAckNumber() 
			  <<",  tcph:"<<tcph);
}

void QueueBytes1 (uint32_t oldbytes, uint32_t newbytes)
{//xl
	  PRINT_SIMPLE(newbytes   <<"   =newbytes1,   "<< oldbytes<<  "  =oldbytes1");
}
void QueuePackets1 (uint32_t oldpackets, uint32_t newpackets)
{// by xl
	  PRINT_SIMPLE (newpackets<<"   =newpackets1, "<< oldpackets<<"  =oldpackets1");
}
void DropTrace1 (Ptr<Packet const> p)
{// by xl
	PppHeader ppp;
	Ipv4Header iph;
	TcpHeader tcph;
	Ptr <Packet> packet = p->Copy();
	packet->RemoveHeader (ppp);
	packet->RemoveHeader (iph);
	packet->RemoveHeader (tcph);
	  PRINT_SIMPLE ("droptrace1: original packet size:"<<p->GetSize() << ", data size:"<<packet->GetSize()
			  <<", pppheader size:"<<ppp.GetSerializedSize() <<", ipv4header size:"<<iph.GetSerializedSize() 
			  <<", tcpheader size:"<<tcph.GetSerializedSize() 
			  //<<",  ip source("<<iph.GetSource() <<") to ip destination("<<iph.GetDestination() 
			  //<<"), source port:"<<tcph.GetSourcePort() <<", destination port:"<<tcph.GetDestinationPort() 
			  //<<", seq:"<<tcph.GetSequenceNumber() <<", ack:"<<tcph.GetAckNumber() 
			  <<",  ppph:"<<ppp <<",  ipv4h:"<<iph <<",  tcph:"<<tcph);
}
void EnQTrace1 (Ptr<Packet const> p)
{// by xl
	PppHeader ppp;
	Ipv4Header iph;
	TcpHeader tcph;
	Ptr <Packet> packet = p->Copy();
	packet->RemoveHeader (ppp);
	packet->RemoveHeader (iph);
	packet->RemoveHeader (tcph);
	  PRINT_SIMPLE ("enqtrace1: packet size:"<<p->GetSize() << ", data size:"<<packet->GetSize()
			  <<",  ip source("<<iph.GetSource() <<") to ip destination("<<iph.GetDestination() 
			  //<<"), source port:"<<tcph.GetSourcePort() <<", destination port:"<<tcph.GetDestinationPort() 
			  //<<", seq:"<<tcph.GetSequenceNumber() <<", ack:"<<tcph.GetAckNumber() 
			  <<",  tcph:"<<tcph);
}
void DeQTrace1 (Ptr<Packet const> p)
{// by xl
	PppHeader ppp;
	Ipv4Header iph;
	TcpHeader tcph;
	Ptr <Packet> packet = p->Copy();
	packet->RemoveHeader (ppp);
	packet->RemoveHeader (iph);
	packet->RemoveHeader (tcph);
	  PRINT_SIMPLE ("deqtrace1: packet size:"<<p->GetSize() << ", data size:"<<packet->GetSize()
			  <<",  ip source("<<iph.GetSource() <<") to ip destination("<<iph.GetDestination() 
			  //<<"), source port:"<<tcph.GetSourcePort() <<", destination port:"<<tcph.GetDestinationPort() 
			  //<<", seq:"<<tcph.GetSequenceNumber() <<", ack:"<<tcph.GetAckNumber() 
			  <<",  tcph:"<<tcph);
}

void QueueBytesfab (uint32_t oldbytes, uint32_t newbytes)
{//xl
	  PRINT_SIMPLE(newbytes   <<"   =newbytesfab,   "<< oldbytes<<  "  =oldbytesfab");
}
void QueuePacketsfab (uint32_t oldpackets, uint32_t newpackets)
{// by xl
	  PRINT_SIMPLE (newpackets<<"   =newpacketsfab, "<< oldpackets<<"  =oldpacketsfab");
}
void DropTracefab (Ptr<Packet const> p)
{// by xl
	PppHeader ppp;
	Ipv4Header iph;
	TcpHeader tcph;
	Ptr <Packet> packet = p->Copy();
	packet->RemoveHeader (ppp);
	packet->RemoveHeader (iph);
	packet->RemoveHeader (tcph);
	  PRINT_SIMPLE ("droptracefab: original packet size:"<<p->GetSize() << ", data size:"<<packet->GetSize()
			  <<", pppheader size:"<<ppp.GetSerializedSize() <<", ipv4header size:"<<iph.GetSerializedSize() 
			  <<", tcpheader size:"<<tcph.GetSerializedSize() 
			  //<<",  ip source("<<iph.GetSource() <<") to ip destination("<<iph.GetDestination() 
			  //<<"), source port:"<<tcph.GetSourcePort() <<", destination port:"<<tcph.GetDestinationPort() 
			  //<<", seq:"<<tcph.GetSequenceNumber() <<", ack:"<<tcph.GetAckNumber() 
			  <<",  ppph:"<<ppp <<",  ipv4h:"<<iph <<",  tcph:"<<tcph);
}
void EnQTracefab (Ptr<Packet const> p)
{// by xl
	PppHeader ppp;
	Ipv4Header iph;
	TcpHeader tcph;
	Ptr <Packet> packet = p->Copy();
	packet->RemoveHeader (ppp);
	packet->RemoveHeader (iph);
	packet->RemoveHeader (tcph);
	  PRINT_SIMPLE ("enqtracefab: packet size:"<<p->GetSize() << ", data size:"<<packet->GetSize()
			  <<",  ip source("<<iph.GetSource() <<") to ip destination("<<iph.GetDestination() 
			  //<<"), source port:"<<tcph.GetSourcePort() <<", destination port:"<<tcph.GetDestinationPort() 
			  //<<", seq:"<<tcph.GetSequenceNumber() <<", ack:"<<tcph.GetAckNumber() 
			  <<",  tcph:"<<tcph);
}
void DeQTracefab (Ptr<Packet const> p)
{// by xl
	PppHeader ppp;
	Ipv4Header iph;
	TcpHeader tcph;
	Ptr <Packet> packet = p->Copy();
	packet->RemoveHeader (ppp);
	packet->RemoveHeader (iph);
	packet->RemoveHeader (tcph);
	  PRINT_SIMPLE ("deqtracefab: packet size:"<<p->GetSize() << ", data size:"<<packet->GetSize()
			  <<",  ip source("<<iph.GetSource() <<") to ip destination("<<iph.GetDestination() 
			  //<<"), source port:"<<tcph.GetSourcePort() <<", destination port:"<<tcph.GetDestinationPort() 
			  //<<", seq:"<<tcph.GetSequenceNumber() <<", ack:"<<tcph.GetAckNumber() 
			  <<",  tcph:"<<tcph);
}



void SetupToR()
{
  for (uint32_t i=0; i < servers_in_group * 4 + 1 ; i++)
  {
    // Each EndPoint 1 (ToR) + 1 (default)
    for (uint8_t j=1;j<nEnd[i]->GetNDevices();j++)
    {
      Ptr <PointToPointNetDevice> P2PNetDevice = nEnd[i]->GetDevice(j)->GetObject <PointToPointNetDevice> ();
      NS_ASSERT (P2PNetDevice);
      Ptr<DropTailQueue> dtq = P2PNetDevice->GetQueue()->GetObject <DropTailQueue> ();
      NS_ASSERT (dtq);
      dtq->SetAttribute ("MaxPackets",UintegerValue(QueuelengthToR));
    }
  }

  for (uint32_t i=0;i<NumberofRacks;i++)
  {
    // Each ToR switch has "MachinesPerRack" + 1 (Fabric) + 1 (default) p2p devices
    for (uint8_t j=1;j<nToR[i]->GetNDevices() - 1;j++)
    {
      Ptr <PointToPointNetDevice> P2PNetDevice = nToR[i]->GetDevice(j)->GetObject <PointToPointNetDevice> ();
      NS_ASSERT (P2PNetDevice);

	  if (Use_RED_Mode              == 1) {
          Ptr<RedQueue> redq = CreateObject<RedQueue> ();
          P2PNetDevice->SetQueue (redq);
	  }
	  else {

          Ptr<DropTailQueue> dtq = P2PNetDevice->GetQueue()->GetObject <DropTailQueue> ();
          NS_ASSERT (dtq);
          dtq->SetAttribute ("MaxPackets",UintegerValue(QueuelengthToR));
	  }
    }
  }
}

void SetupFabric()
{
  for (uint32_t i=0;i<NumberofRacks;i++)
  {
    // Each ToR switch has "MachinesPerRack" + 1 (Fabric) + 1 (default) p2p devices
    for (uint8_t j=nToR[i]->GetNDevices() - 1;j<nToR[i]->GetNDevices();j++)
    {
      Ptr <PointToPointNetDevice> P2PNetDevice = nToR[i]->GetDevice(j)->GetObject <PointToPointNetDevice> ();
      NS_ASSERT (P2PNetDevice);
      
	  if (Use_RED_Mode              == 1) {
	      Ptr<RedQueue> redq = CreateObject<RedQueue> ();
          P2PNetDevice->SetQueue (redq);
	  }
	  else {

	      Ptr<DropTailQueue> dtq = P2PNetDevice->GetQueue()->GetObject <DropTailQueue> ();
	      NS_ASSERT (dtq);
          dtq->SetAttribute ("MaxPackets",UintegerValue(QueuelengthFabric));
	  }
    }
  }

  for (uint8_t j=1;j<nFabric->GetNDevices();j++)
  {
    Ptr <PointToPointNetDevice> P2PNetDevice = nFabric->GetDevice(j)->GetObject <PointToPointNetDevice> ();
    NS_ASSERT (P2PNetDevice);
	
	if (Use_RED_Mode              == 1) {
	    Ptr<RedQueue> redq = CreateObject<RedQueue> ();
	    redq->SetQueueLimit (QueuelengthFabric);
	    //redq->SetTh (ThresholdFabric, ThresholdFabric);
        P2PNetDevice->SetQueue (redq);
	}
	else {
        Ptr<DropTailQueue> dtq = P2PNetDevice->GetQueue()->GetObject <DropTailQueue> ();
        NS_ASSERT (dtq);
        dtq->SetAttribute ("MaxPackets",UintegerValue(QueuelengthFabric));
	}
  }
}


/*void
CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
  PRINT_SIMPLE (">>>>>>>>>>>>>>>>>>>>\t" << newCwnd);
  //cout << ">>>>>>>>>>>>>>>>>" << newCwnd; by xl on Sun May 19 02:14:01 CST 2013
}*/

int main (int argc, char *argv[])
{
  CommandLine cmd;
  
  // add cmd line arguments here
  cmd.AddValue ("RTCPMode",               "use RTCP  or not   (0 or 1)",                  RTCPMode);
  cmd.AddValue ("ICTCPMode",              "use ICTCP or not   (0 or 1)",                  ICTCPMode);
  cmd.AddValue ("DCTCPMode",              "use TCP   or DCTCP (0 or 1)",                  DCTCPMode);
  cmd.AddValue ("RTCPMode",               "use TCP   or RTCP (0 or 1)",                    RTCPMode);
  cmd.AddValue ("RDTCPMode",              "use TCP   or RDTCP (0 or 1)",                  RDTCPMode);
  cmd.AddValue ("RDTCP_CONTROLLER_Mode",  "use RDTCP   or RDTCP controller(0 or 1)",      RDTCP_CONTROLLER_Mode);
  cmd.AddValue ("ECNMode",                "ECN Mode  (0 or 1)",                           ECNMode);
  cmd.AddValue ("Use_RED_Mode",           "Use RED queue for queue in switch.",           Use_RED_Mode);
  cmd.AddValue ("MachinesPerRack",        "Number of nodes",                              MachinesPerRack);
  cmd.AddValue ("NumberofRacks",          "Number of Racks",                              NumberofRacks);
  cmd.AddValue ("Machines2ToRRate",       "Link rate from machines to ToR switch",        Machines2ToRRate);
  cmd.AddValue ("LinkDelay",              "Propagation delay in all links",               LinkDelay);
  cmd.AddValue ("NumberofApps",           "Number of Apps",                               NumberofApps);
  cmd.AddValue ("QueuelengthToR",         "Queuelength of ToR Switch",                    QueuelengthToR);
  cmd.AddValue ("QueuelengthFabric",      "Queuelength of Fabric Switch",                 QueuelengthFabric);
  cmd.AddValue ("ThresholdToR_min",       "Threshold min of ToR queue",                   ThresholdToR_min);
  cmd.AddValue ("ThresholdToR_max",       "Threshold max of ToR queue",                   ThresholdToR_max);
//  cmd.AddValue ("ThresholdFabric",        "Threshold of Fabric queue",                    ThresholdFabric);
  cmd.AddValue ("InterarrivalMultiplier", "InterarrivalMultiplier",                       InterarrivalMultiplier);
  cmd.AddValue ("Stop",                   "Time to stop the simulation",                  Stop);
  cmd.AddValue ("UseLongFlows",           "Inject long flows as welln",                   UseLongFlows);
  cmd.AddValue ("TightDeadlines",         "% to shave off deadlines",                     TightDeadlines);
  cmd.AddValue ("SegmentSize",            "SegmentSize for tcp",                          SegmentSize);
  cmd.AddValue ("rto_min",                "rto_min for retransmission",                   rto_min);
  cmd.AddValue ("offerload",              "offer load (%)",                               offerload);
  cmd.AddValue ("longflowsize",           "long flow size should occupy 75th of total traffic.",  longflowsize);
  cmd.AddValue ("uniform_distr",          "use uniform distribution for short flow size.",  uniform_distr);
  cmd.AddValue ("servers_in_group",       "servers_in_group.",                            servers_in_group);

  cmd.Parse (argc,argv);

  // calculate the derived knobs

  NumberofEndPoints     = servers_in_group * 4 + 1;
  NumberofRacks         = 2;
  ToR2FabricRate        = MachinesPerRack * Machines2ToRRate;

  Machines2ToRRateStr   = uint2str(1);
  Machines2ToRRateStr   += "Gbps";
  ToR2FabricRateStr     = uint2str(10); //ToR2FabricRate        
  ToR2FabricRateStr     += "Gbps";

  NodesPerApp           = servers_in_group * 4 + 1;
  MachinesPerRack       = servers_in_group * 2;

  //assert(NumberofEndPoints % NumberofApps == 0);

  PRINT_SIMPLE("RTCPMode                          : " << RTCPMode);
  PRINT_SIMPLE("ICTCPMode                         : " << ICTCPMode);
  PRINT_SIMPLE("DCTCPMode                         : " << DCTCPMode);
  PRINT_SIMPLE("RDTCPMode                         : " << RDTCPMode);
  PRINT_SIMPLE("RDTCP_CONTROLLER_Mode             : " << RDTCP_CONTROLLER_Mode);
  PRINT_SIMPLE("ECNMode                           : " << ECNMode);
  PRINT_SIMPLE("Use_RED_Mode                      : " << Use_RED_Mode);
  PRINT_SIMPLE("MachinesPerRack                   : " << MachinesPerRack);
  PRINT_SIMPLE("NumberofRacks(ToR Swicthes)       : " << NumberofRacks);
  PRINT_SIMPLE("Machines2ToRRate                  : " << Machines2ToRRateStr.c_str());
  PRINT_SIMPLE("LinkDelay                         : " << LinkDelay.c_str());
  PRINT_SIMPLE("ToR2FabricRate                    : " << ToR2FabricRateStr.c_str());
  PRINT_SIMPLE("NumberofEndPoints                 : " << NumberofEndPoints);
  PRINT_SIMPLE("NumberofApps                      : " << NumberofApps);
  PRINT_SIMPLE("NodesPerApp                       : " << NodesPerApp);
  PRINT_SIMPLE("QueuelengthToR                    : " << QueuelengthToR);
  PRINT_SIMPLE("QueuelengthFabric                 : " << QueuelengthFabric);
  PRINT_SIMPLE("ThresholdToR_min                  : " << ThresholdToR_min);
  PRINT_SIMPLE("ThresholdToR_max                  : " << ThresholdToR_max);
//  PRINT_SIMPLE("ThresholdFabric                   : " << ThresholdFabric);
  PRINT_SIMPLE("InterarrivalMultiplier            : " << InterarrivalMultiplier);
  PRINT_SIMPLE("Stop                              : " << Stop);
  PRINT_SIMPLE("UseLongFLows                      : " << UseLongFlows);
  PRINT_SIMPLE("TightDeadlines                    : " << TightDeadlines);
  PRINT_SIMPLE("SegmentSize                       : " << SegmentSize);
  PRINT_SIMPLE("rto_min                           : " << rto_min);
  PRINT_SIMPLE("offerload                         : " << offerload);
  PRINT_SIMPLE("longflowsize                      : " << longflowsize << " B");
  PRINT_SIMPLE("uniform_distr                     : " << uniform_distr);
  PRINT_SIMPLE("servers_in_group                  : " << servers_in_group);

  //LogComponentEnable("RedQueue", LOG_LEVEL_DEBUG);
  //LogComponentEnable("DropTailQueue", LOG_LEVEL_DEBUG);
  //LogComponentEnable("BulkSendApplication", LOG_LEVEL_ALL);
  //LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
  LogComponentEnable("TcpNewReno", LOG_LEVEL_DEBUG);
  LogComponentEnable("TcpSocketBase", LOG_LEVEL_DEBUG);
  //LogComponentEnable("TcpSocketBase", LOG_LEVEL_ALL);
  //LogComponentEnable("TcpL4Protocol", LOG_LEVEL_ALL);
  //LogComponentEnable("TcpTxBuffer", LOG_LEVEL_ALL);
  //LogComponentEnable("TcpRxBuffer", LOG_LEVEL_ALL);
  //LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_ALL);
  //LogComponentEnable("Ipv4Interface", LOG_LEVEL_ALL);
  //LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_ALL);
  //LogComponentEnable("PointToPointChannel", LOG_LEVEL_ALL);
  //LogComponentEnable("Node", LOG_LEVEL_ALL);
  //LogComponentEnable("Ipv4ListRouting", LOG_LEVEL_ALL);
  //LogComponentEnable("Ipv4GlobalRouting", LOG_LEVEL_ALL);
  //LogComponentEnable("Ipv4EndPointDemux", LOG_LEVEL_ALL);
  //LogComponentEnable("InternetStackHelper", LOG_LEVEL_DEBUG);
  //LogComponentEnable("ObjectFactory", LOG_LEVEL_DEBUG);
  //LogComponentEnable("ObjectBase", LOG_LEVEL_DEBUG);
  //LogComponentEnable("TypeId", LOG_LEVEL_ALL);


  // Configure ECN QueueThresholds
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  Config::SetDefault ("ns3::Ipv4L3Protocol::Offer_Load", UintegerValue ((uint8_t)offerload ));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (SegmentSize));
  Config::SetDefault ("ns3::PointToPointNetDevice::Mtu", UintegerValue (SegmentSize+42+4+4+4)); //4 for tcp option header for flowsize for rdtcp controller.
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1024* 128)); //buffer size 128k
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1024* 128));

  //if (RTCPMode) 
   // delack_timeout            = rto_min / 2;
 // else delack_timeout            = rto_min;
    delack_timeout            = rto_min;
  Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue(MilliSeconds(rto_min))); // by xl on Mon May  6 01:28:40 CST 2013
  if (offerload > 50)
      Config::SetDefault ("ns3::TcpSocket::ConnCount", UintegerValue (20)); // by xl on Mon May  6 01:28:40 CST 2013
  Config::SetDefault ("ns3::TcpSocket::DelAckTimeout", TimeValue(MilliSeconds(delack_timeout))); // by xl on Mon May  6 01:28:40 CST 2013
  Config::SetDefault ("ns3::RttEstimator::MinRTO", TimeValue(MilliSeconds(rto_min))); // by xl on Mon May  6 01:28:40 CST 2013
  Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue(MilliSeconds(rto_min))); // by xl on Mon May  6 01:28:40 CST 2013
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));

  //Config::SetDefault ("ns3::TcpSocketBase::DCTCP_MODE", UintegerValue (DCTCPMode)); // by xl
  //Config::SetDefault ("ns3::TcpSocketBase::RTCP_MODE",  UintegerValue (RTCPMode)); // by xl
  //Config::SetDefault ("ns3::TcpSocketBase::ICTCP_MODE", UintegerValue (ICTCPMode)); // by xl
  //Config::SetDefault ("ns3::TcpSocketBase::RDTCP_MODE", UintegerValue (RDTCPMode)); // by xl
  //Config::SetDefault ("ns3::TcpSocketBase::RDTCP_MAXQUEUE_SIZE", UintegerValue (ThresholdToR_max)); // by xl
  //Config::SetDefault ("ns3::TcpSocketBase::RDTCP_CONTROLLER_MODE", UintegerValue (RDTCP_CONTROLLER_Mode)); // by xl

  Config::SetDefault ("ns3::RedQueue::Mode", StringValue ("QUEUE_MODE_PACKETS")); //real world configure by xl 
  Config::SetDefault ("ns3::RedQueue::MeanPktSize", UintegerValue (1500));
  //Config::SetDefault ("ns3::RedQueue::Wait", BooleanValue (true));
  //Config::SetDefault ("ns3::RedQueue::Gentle", BooleanValue (true));
  //Config::SetDefault ("ns3::RedQueue::QW", DoubleValue (0.002));
  Config::SetDefault ("ns3::RedQueue::MinTh", DoubleValue (ThresholdToR_min)); //10 default, low queue latency is important, by xl
  Config::SetDefault ("ns3::RedQueue::MaxTh", DoubleValue (ThresholdToR_max)); // max has no effect, and when use byte, min=128kb/2.
  Config::SetDefault ("ns3::RedQueue::QueueLimit", UintegerValue (QueuelengthToR));//128*1024B buffer, 128*1024/1500 = 87
  Config::SetDefault ("ns3::RedQueue::ECN", UintegerValue (ECNMode));

  Config::SetDefault ("ns3::DropTailQueue::Mode", StringValue ("QUEUE_MODE_PACKETS")); //real world configure by xl 
  Config::SetDefault ("ns3::DropTailQueue::ECN", UintegerValue (ECNMode));
  Config::SetDefault ("ns3::DropTailQueue::MaxPackets", UintegerValue (QueuelengthToR));
  Config::SetDefault ("ns3::DropTailQueue::MaxBytes", UintegerValue (128*1024));
  Config::SetDefault ("ns3::DropTailQueue::Offer_Load", UintegerValue ((uint8_t)offerload));
  //for QUEUE_MODE_PACKETS mode
  Config::SetDefault ("ns3::DropTailQueue::ECN_Threshold", UintegerValue (ThresholdToR_max));

  
  // Declare EndPoints
  for (uint32_t i=0;i<NumberofEndPoints;i++)
    nEnd.push_back(CreateObject<Node> ());

  PRINT_SIMPLE("Allocated " << nEnd.size() << " EndPoint nodes..");

  // Declare ToR Swictch
  for (uint32_t i=0;i<NumberofRacks;i++)
    nToR.push_back(CreateObject<Node> ());
  
  PRINT_SIMPLE("Allocated " << nToR.size() << " ToR nodes..");
  
  // Declare Fabric switch
  nFabric = CreateObject<Node> ();
  
  PRINT_SIMPLE("Allocated Fabric node..");

  // Install IP
  InternetStackHelper internet;
  for (uint32_t i=0;i<NumberofEndPoints;i++)
  {
    //PRINT_SIMPLE("internet.Install (nEnd["<<i<<"]):"<<nEnd[i]->GetId());
    internet.Install (nEnd[i]);
  }
  for (uint32_t i=0;i<NumberofRacks;i++)
    internet.Install (nToR[i]);
  internet.Install (nFabric);

  // Install p2p links between EndPoints and ToR Swicthes
  PointToPointHelper p2pEnd2ToR;
  p2pEnd2ToR.SetDeviceAttribute           ("DataRate",  StringValue (Machines2ToRRateStr.c_str()));
  p2pEnd2ToR.SetChannelAttribute          ("Delay",     StringValue (LinkDelay.c_str()));
  vector <NetDeviceContainer> dEnddToR;

  uint32_t i = 0;
  for (uint32_t j=0;j<servers_in_group * 2 ;j++)
    dEnddToR.push_back(p2pEnd2ToR.Install (nEnd[j], nToR[i]));
  i = 1;
  for (uint32_t j=0;j<servers_in_group * 2 + 1;j++)
    dEnddToR.push_back(p2pEnd2ToR.Install (nEnd[servers_in_group * 2 + j], nToR[i]));


  // Install p2p links between ToR Swicthes and Fabric Switch
  PointToPointHelper p2pToR2Fabric;
  p2pToR2Fabric.SetDeviceAttribute        ("DataRate",  StringValue (ToR2FabricRateStr.c_str()));
  p2pToR2Fabric.SetChannelAttribute       ("Delay",     StringValue (LinkDelay.c_str()));
  vector <NetDeviceContainer> dToRdFabric;
  for (uint32_t i=0;i<NumberofRacks;i++)
    dToRdFabric.push_back(p2pToR2Fabric.Install (nToR[i],nFabric));

  // Assign IP address at each interface - first on the interface between EndPoints and TOR switches
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");

  // At the interface between Ends and ToR
  vector <Ipv4InterfaceContainer> iEndiToR;
  i =0;
    for (uint32_t j=0;j<servers_in_group * 2;j++)
    {
      iEndiToR.push_back(ipv4.Assign ( dEnddToR[j] ));
      ipv4.NewNetwork();
    }
 i =1;
    for (uint32_t j=0;j<servers_in_group * 2 + 1;j++)
    {
      iEndiToR.push_back(ipv4.Assign (dEnddToR[servers_in_group * 2 + j]));
      ipv4.NewNetwork();
    }
  // At the interface between TOR and Fabric
  vector <Ipv4InterfaceContainer> iToRiFabric;
  for (i=0;i<NumberofRacks;i++)
  {
      iToRiFabric.push_back(ipv4.Assign (dToRdFabric[i]));
      ipv4.NewNetwork();
  }


  SetupToR();
  SetupFabric();

  // Walk thro all the nodes, and print their IP addresses
  PRINT_SIMPLE("=====IP Assignment at EndPoints=========");
  for (uint32_t i=0;i<NumberofEndPoints;i++)
    for (uint32_t j=1;j < nEnd[i]->GetObject<Ipv4>()->GetNInterfaces();j++)
      PRINT_SIMPLE("Node " << nEnd[i]->GetId() << ": " << nEnd[i]->GetObject<Ipv4>()->GetAddress(j,0).GetLocal());

  PRINT_SIMPLE("=====IP Assignment at ToR=========");
  for (uint32_t i=0;i<NumberofRacks;i++)
    for (uint32_t j=1;j < nToR[i]->GetObject<Ipv4>()->GetNInterfaces();j++)
      PRINT_SIMPLE("Node " << nToR[i]->GetId() << ": " << nToR[i]->GetObject<Ipv4>()->GetAddress(j,0).GetLocal());
  
  PRINT_SIMPLE("=====IP Assignment at Fabric=========");
  for (uint32_t j=1;j < nFabric->GetObject<Ipv4>()->GetNInterfaces();j++)
      PRINT_SIMPLE("Node " << nFabric->GetId() << ": " << nFabric->GetObject<Ipv4>()->GetAddress(j,0).GetLocal());

  // setup ip routing tables to get total ip-level connectivity.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  
  vector <ExponentialVariable> StartDelay;
  for (i=0;i<servers_in_group * 4 + 1;i++)
  {
    ExponentialVariable tmp (10, 20);
    StartDelay.push_back(tmp);
  }

  PRINT_SIMPLE("============================================================");
  PRINT_SIMPLE("Sink: " << servers_in_group * 4);
  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 32));
      sink.SetAttribute ("DCTCP_MODE", UintegerValue (DCTCPMode)); // by xl
      sink.SetAttribute ("RTCP_MODE",  UintegerValue (RTCPMode)); // by xl
      sink.SetAttribute ("ICTCP_MODE", UintegerValue (ICTCPMode)); // by xl
      sink.SetAttribute ("RDTCP_MODE", UintegerValue (RDTCPMode)); // by xl
      sink.SetAttribute ("RDTCP_MAXQUEUE_SIZE", UintegerValue (ThresholdToR_max)); // by xl
      sink.SetAttribute ("RDTCP_CONTROLLER_MODE", UintegerValue (RDTCP_CONTROLLER_Mode)); // by xl
  ApplicationContainer sinkApps1 = sink.Install (nEnd[servers_in_group * 4]);
  sinkApps1.Start  (Seconds (0.0));
  uint32_t  num_flows = 0;

  //first remote group to the receiver
  for (i=0; i<servers_in_group ;i++)
  {
      PRINT_SIMPLE("Source Node " << i << ": ---> Sink Node "<< servers_in_group * 4 <<", size: "<< longflowsize <<" B");
      BulkSendHelper source ("ns3::TcpSocketFactory",InetSocketAddress(nEnd[servers_in_group * 4]->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 32));
      source.SetAttribute ("MaxBytes", UintegerValue (longflowsize ));
      source.SetAttribute ("SendSize", UintegerValue (longflowsize ));
      source.SetAttribute ("Deadline", UintegerValue (Deadlines[0]*1000000)); // cannot call send() for size larger than TCP buffer = 128 MB
      source.SetAttribute ("FlowId", UintegerValue (num_flows)); // cannot call send() for size larger than TCP buffer = 128 MB
      source.SetAttribute ("DCTCP_MODE", UintegerValue (DCTCPMode)); // by xl
      source.SetAttribute ("RTCP_MODE",  UintegerValue (RTCPMode)); // by xl
      source.SetAttribute ("ICTCP_MODE", UintegerValue (ICTCPMode)); // by xl
      source.SetAttribute ("RDTCP_MODE", UintegerValue (RDTCPMode)); // by xl
      source.SetAttribute ("RDTCP_MAXQUEUE_SIZE", UintegerValue (ThresholdToR_max)); // by xl
      source.SetAttribute ("RDTCP_CONTROLLER_MODE", UintegerValue (RDTCP_CONTROLLER_Mode)); // by xl

      ApplicationContainer sourceApps = source.Install (nEnd[i]);
	  //Time startt = MilliSeconds (StartDelay[i].GetValue());
	  Time startt = MilliSeconds (10);
      //PRINT_SIMPLE("Start app at " << i <<  ", at time = " << startt );
      PRINT_SIMPLE("Start app at " << i <<  ", at time = " << startt.GetNanoSeconds()<<" flowid= "<<num_flows
				  << " size: "<< longflowsize  <<" B");
      sourceApps.Start  ( startt );
	  num_flows ++;
   }

  //second near group to the receiver
  for (i=servers_in_group * 2; i < servers_in_group * 3;i++)
  {
      PRINT_SIMPLE("Source Node " << i << ": ---> Sink Node "<< servers_in_group * 4<<", size: "<< longflowsize  <<" B");
      BulkSendHelper source ("ns3::TcpSocketFactory",InetSocketAddress(nEnd[servers_in_group * 4]->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 32));
      source.SetAttribute ("MaxBytes", UintegerValue (longflowsize  ));
      source.SetAttribute ("SendSize", UintegerValue (longflowsize  ));
      source.SetAttribute ("Deadline", UintegerValue (Deadlines[0]*1000000)); // cannot call send() for size larger than TCP buffer = 128 MB
      source.SetAttribute ("FlowId", UintegerValue (num_flows)); // cannot call send() for size larger than TCP buffer = 128 MB
      source.SetAttribute ("DCTCP_MODE", UintegerValue (DCTCPMode)); // by xl
      source.SetAttribute ("RTCP_MODE",  UintegerValue (RTCPMode)); // by xl
      source.SetAttribute ("ICTCP_MODE", UintegerValue (ICTCPMode)); // by xl
      source.SetAttribute ("RDTCP_MODE", UintegerValue (RDTCPMode)); // by xl
      source.SetAttribute ("RDTCP_MAXQUEUE_SIZE", UintegerValue (ThresholdToR_max)); // by xl
      source.SetAttribute ("RDTCP_CONTROLLER_MODE", UintegerValue (RDTCP_CONTROLLER_Mode)); // by xl

      ApplicationContainer sourceApps = source.Install (nEnd[i]);
	  //Time startt = MilliSeconds (StartDelay[i].GetValue());
	  Time startt = MilliSeconds (10);
      PRINT_SIMPLE("Start app at " << i <<  ", at time = " << startt.GetNanoSeconds() <<" flowid= "<<num_flows
				  << " size: "<< longflowsize   <<" B");
      sourceApps.Start  (startt );
	  num_flows ++;
   }

  for (i=servers_in_group ; i < servers_in_group * 2;i++)
  {
      PRINT_SIMPLE("Sink: "<< i+servers_in_group * 2);
      PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 32));
      sink.SetAttribute ("DCTCP_MODE", UintegerValue (DCTCPMode)); // by xl
      sink.SetAttribute ("RTCP_MODE",  UintegerValue (RTCPMode)); // by xl
      sink.SetAttribute ("ICTCP_MODE", UintegerValue (ICTCPMode)); // by xl
      sink.SetAttribute ("RDTCP_MODE", UintegerValue (RDTCPMode)); // by xl
      sink.SetAttribute ("RDTCP_MAXQUEUE_SIZE", UintegerValue (ThresholdToR_max)); // by xl
      sink.SetAttribute ("RDTCP_CONTROLLER_MODE", UintegerValue (RDTCP_CONTROLLER_Mode)); // by xl
      ApplicationContainer sinkApps1 = sink.Install (nEnd[i+servers_in_group * 2]);
      sinkApps1.Start (Seconds (0.0));

      PRINT_SIMPLE("Source Node " << i << ": ---> Sink Node "<< i + servers_in_group * 2 << ", size: "<<longflowsize  <<" B");
      BulkSendHelper source ("ns3::TcpSocketFactory",InetSocketAddress(nEnd[i+servers_in_group * 2]->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 32));
      source.SetAttribute ("MaxBytes", UintegerValue (longflowsize  ));
      source.SetAttribute ("SendSize", UintegerValue (longflowsize  ));
      source.SetAttribute ("Deadline", UintegerValue (Deadlines[0]*1000000)); // cannot call send() for size larger than TCP buffer = 128 MB
      source.SetAttribute ("FlowId", UintegerValue (num_flows)); // cannot call send() for size larger than TCP buffer = 128 MB
      source.SetAttribute ("DCTCP_MODE", UintegerValue (DCTCPMode)); // by xl
      source.SetAttribute ("RTCP_MODE",  UintegerValue (RTCPMode)); // by xl
      source.SetAttribute ("ICTCP_MODE", UintegerValue (ICTCPMode)); // by xl
      source.SetAttribute ("RDTCP_MODE", UintegerValue (RDTCPMode)); // by xl
      source.SetAttribute ("RDTCP_MAXQUEUE_SIZE", UintegerValue (ThresholdToR_max)); // by xl
      source.SetAttribute ("RDTCP_CONTROLLER_MODE", UintegerValue (RDTCP_CONTROLLER_Mode)); // by xl

      ApplicationContainer sourceApps = source.Install (nEnd[i]);
	  //Time startt = MilliSeconds (StartDelay[i].GetValue());
	  Time startt = MilliSeconds (10);
      PRINT_SIMPLE("Start app at " << i <<  ", at time = " << startt.GetNanoSeconds()<<" flowid= "<<num_flows
				  << " size: "<< longflowsize   <<" B");
      sourceApps.Start  (startt );
	  num_flows ++;
   }


 
  
    Print_st = 1;
    if (Print_st)
	{
		//0 for 127.0.0.1, 1--servers_in_group * 2 for two group, servers_in_group * 2 + 2 is to fabric
		Ptr<Queue> tor_to_sink_device_queue = nToR[1]->GetDevice(servers_in_group * 2 + 1)->GetObject <PointToPointNetDevice> ()->GetQueue(); 
		tor_to_sink_device_queue ->TraceConnectWithoutContext ("QueueBytes", MakeCallback (QueueBytes1 )); //xl
		tor_to_sink_device_queue ->TraceConnectWithoutContext ("QueuePackets", MakeCallback (QueuePackets1 )); //xl
        tor_to_sink_device_queue ->TraceConnectWithoutContext ("Drop", MakeCallback (DropTrace1 )); //xl
		tor_to_sink_device_queue ->TraceConnectWithoutContext ("Enqueue", MakeCallback (EnQTrace1 )); //xl
		tor_to_sink_device_queue ->TraceConnectWithoutContext ("Dequeue", MakeCallback (DeQTrace1 )); //xl
  
		//0 for 127.0.0.1, 1--servers_in_group * 2 for two group 
		Ptr<Queue> tor_to_sink_device_queue2 = nToR[0]->GetDevice(servers_in_group * 2 + 1)->GetObject <PointToPointNetDevice> ()->GetQueue(); 
		tor_to_sink_device_queue2 ->TraceConnectWithoutContext ("QueueBytes", MakeCallback (QueueBytes0 )); //xl
		tor_to_sink_device_queue2 ->TraceConnectWithoutContext ("QueuePackets", MakeCallback (QueuePackets0 )); //xl
        tor_to_sink_device_queue2 ->TraceConnectWithoutContext ("Drop", MakeCallback (DropTrace0 )); //xl
		tor_to_sink_device_queue2 ->TraceConnectWithoutContext ("Enqueue", MakeCallback (EnQTrace0 )); //xl
		tor_to_sink_device_queue2 ->TraceConnectWithoutContext ("Dequeue", MakeCallback (DeQTrace0 )); //xl

		//0 for 127.0.0.1, 1 for first ToR
		Ptr<Queue> fabric_to_sinkToR_device_queue = nFabric->GetDevice(2)->GetObject <PointToPointNetDevice> ()->GetQueue(); 
		fabric_to_sinkToR_device_queue ->TraceConnectWithoutContext ("QueueBytes", MakeCallback (QueueBytesfab )); //xl
		fabric_to_sinkToR_device_queue ->TraceConnectWithoutContext ("QueuePackets", MakeCallback (QueuePacketsfab )); //xl
		fabric_to_sinkToR_device_queue ->TraceConnectWithoutContext ("Drop", MakeCallback (DropTracefab )); //xl
		fabric_to_sinkToR_device_queue ->TraceConnectWithoutContext ("Enqueue", MakeCallback (EnQTracefab )); //xl
		fabric_to_sinkToR_device_queue ->TraceConnectWithoutContext ("Dequeue", MakeCallback (DeQTracefab )); //xl

	}



  // test
  Simulator::Run ();


    if (Print_st)
    {
       for (uint32_t i=0;i<NumberofRacks;i++)
	   {
          cout << "*** all ToR=["<< NumberofRacks <<"], from ToR["<<i<<"] device num = ["<<nToR[i]->GetNDevices() - 1<<"]" << endl;
         // Each ToR switch has "MachinesPerRack" + 1 (Fabric) + 1 (default) p2p devices
         for (uint8_t j=1; j<nToR[i]->GetNDevices() ; j++)
       {
          Ptr <PointToPointNetDevice> nd = nToR[i]->GetDevice(j)->GetObject <PointToPointNetDevice> ();
		  if (StaticCast<DropTailQueue> (nd->GetQueue ())->m_ecns > 0)
		  {
          cout << "*** DropTailQueue stats from ToR["<<i<<"] device[ "<<(uint32_t)j<<" ] queue ***" << endl;
          cout << "\t " << StaticCast<DropTailQueue> (nd->GetQueue ())->Get_Droptimes ()  << " drops due queue full" << endl;
          cout << "\t " << StaticCast<DropTailQueue> (nd->GetQueue ())->m_ecns << " marks of ecn" << endl;
		  }
       }
	   }
	   
	   //for incast 
	   for (uint8_t k=0; k<2; k++) {
		
		   Ptr<Queue> tor_to_sink_device_queue = 
			nToR[k]->GetDevice(servers_in_group * 2 + 1)->GetObject <PointToPointNetDevice> ()->GetQueue(); 
		
		   cout <<"servers_in_group * 2 + 1:"<< servers_in_group * 2 + 1
			   << ", k="<< (uint32_t) k <<", nToR[k]->GetNDevices()="<< nToR[k]->GetNDevices()<<endl;
           // Each ToR switch has "MachinesPerRack" + 1 (Fabric) + 1 (default) p2p devices
		
		   cout << " tor link ip:" <<nToR[k]->GetObject<Ipv4>()->GetAddress(servers_in_group * 2 + 1, 0).GetLocal() <<endl;



        cout << "\tToR " << StaticCast<DropTailQueue> (tor_to_sink_device_queue)->Get_Droptimes ()  
			  << " drops due queue full at bottleneck" << endl;
        cout << "\tToR " << StaticCast<DropTailQueue> (tor_to_sink_device_queue)->m_ecns 
			  << " marks of ecn at bottleneck" << endl;
          
		}
		  
 
		Ptr<Queue> fabric_to_sinkToR_device_queue = nFabric->GetDevice(2)->GetObject <PointToPointNetDevice> ()->GetQueue(); 
		cout << "\tFabric link ip:" << nFabric->GetObject<Ipv4>()->GetAddress(2 , 0).GetLocal() 
			<<", "<< StaticCast<DropTailQueue> (fabric_to_sinkToR_device_queue)->Get_Droptimes ()  
			  << " drops due queue full at bottleneck" << endl;
        cout << "\tFabric " << StaticCast<DropTailQueue> (fabric_to_sinkToR_device_queue)->m_ecns 
			  << " marks of ecn at bottleneck" << endl;
    }
    
  Simulator::Destroy ();

  PRINT_SIMPLE("Num Flows: " << num_flows);

  return 0;
}
