/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author:  Tom Henderson (tomhend@u.washington.edu)
 */
#include "ns3/address.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "packet-sink.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PacketSink");
NS_OBJECT_ENSURE_REGISTERED (PacketSink);

TypeId 
PacketSink::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PacketSink")
    .SetParent<Application> ()
    .AddConstructor<PacketSink> ()
    .AddAttribute ("Local", "The Address on which to Bind the rx socket.",
                   AddressValue (),
                   MakeAddressAccessor (&PacketSink::m_local),
                   MakeAddressChecker ())
    .AddAttribute ("Protocol", "The type id of the protocol to use for the rx socket.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&PacketSink::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Rx", "A packet has been received",
                     MakeTraceSourceAccessor (&PacketSink::m_rxTrace))


	//new protocol, xl
	.AddAttribute ("DCTCP_MODE", 
                   "The DCTCP mode for datacenter ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&PacketSink::m_dctcp_mode),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("ICTCP_MODE", 
                   "The ICTCP mode for datacenter ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&PacketSink::m_ictcp_mode),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("RTCP_MODE", 
                   "The receiver TCP mode for datacenter ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&PacketSink::m_rtcp_mode),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("RDTCP_MODE", 
                   "The receiver DCTCP mode for datacenter ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&PacketSink::m_rdtcp_mode),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("RDTCP_MAXQUEUE_SIZE", 
                   "The max queue size for RDTCP controller ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&PacketSink::m_rdtcp_queue_maxsize),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("RDTCP_CONTROLLER_MODE", 
                   "The RDTCP controller is active when set 1.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&PacketSink::m_rdtcp_controller_mode),
                   MakeUintegerChecker<uint32_t> ())

	.AddAttribute ("RDTCP_no_priority", 
                   "RDTCP does not use flow priority.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&PacketSink::m_rdtcp_no_priority),
                   MakeUintegerChecker<uint32_t> ())


  ;
  return tid;
}

PacketSink::PacketSink ()
 : m_dctcp_mode (0),  //xl
  m_ictcp_mode(0),  //xl
  m_rtcp_mode(0),  //xl
  m_rdtcp_mode(0),  //xl
  m_rdtcp_queue_maxsize(0),
  m_rdtcp_controller_mode(0),
  m_rdtcp_no_priority(0)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_totalRx = 0;
}

PacketSink::~PacketSink()
{
  NS_LOG_FUNCTION (this);
}

uint32_t PacketSink::GetTotalRx () const
{
  NS_LOG_FUNCTION (this);
  return m_totalRx;
}

Ptr<Socket>
PacketSink::GetListeningSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

std::list<Ptr<Socket> >
PacketSink::GetAcceptedSockets (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socketList;
}

void PacketSink::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socketList.clear ();

  // chain up
  Application::DoDispose ();
}


// Application Methods
void PacketSink::StartApplication ()    // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  // Create the socket if not already
  if (!m_socket)
    {

      m_socket = Socket::CreateSocket (GetNode (), m_tid);

  m_socket->SetAttribute ("DCTCP_MODE", UintegerValue (m_dctcp_mode)); // by xl
  m_socket->SetAttribute ("RTCP_MODE",  UintegerValue (m_rtcp_mode)); // by xl
  m_socket->SetAttribute ("ICTCP_MODE", UintegerValue (m_ictcp_mode)); // by xl
  m_socket->SetAttribute ("RDTCP_MODE", UintegerValue (m_rdtcp_mode)); // by xl
  m_socket->SetAttribute ("RDTCP_MAXQUEUE_SIZE", UintegerValue (m_rdtcp_queue_maxsize)); // by xl
  m_socket->SetAttribute ("RDTCP_CONTROLLER_MODE", UintegerValue (m_rdtcp_controller_mode)); // by xl
  m_socket->SetAttribute ("RDTCP_no_priority", UintegerValue (m_rdtcp_no_priority)); // by xl
  NS_LOG_DEBUG ("DCTCP_MODE=" << m_dctcp_mode
		  <<",RTCP_MODE=" << m_rtcp_mode
		  <<",RDTCP_MODE=" << m_rdtcp_mode
		  <<",RDTCP_CONTROLLER_MODE="<< m_rdtcp_controller_mode
		  <<",RDTCP_MAXQUEUE_SIZE=" << m_rdtcp_queue_maxsize
		  <<",RDTCP_no_priority=" << m_rdtcp_no_priority
		  <<",ICTCP_MODE=" << m_ictcp_mode
		  );

      m_socket->Bind (m_local);
      m_socket->Listen ();
      m_socket->ShutdownSend ();
      if (addressUtils::IsMulticast (m_local))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, m_local);
            }
          else
            {
              NS_FATAL_ERROR ("Error: joining multicast on a non-UDP socket");
            }
        }
    }


  m_socket->SetRecvCallback (MakeCallback (&PacketSink::HandleRead, this));
  m_socket->SetAcceptCallback (
    MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
    MakeCallback (&PacketSink::HandleAccept, this));
  m_socket->SetCloseCallbacks (
    MakeCallback (&PacketSink::HandlePeerClose, this),
    MakeCallback (&PacketSink::HandlePeerError, this));

	  m_socket->SetAttribute ("RcvBufSize", UintegerValue (64*1024* 1024));
	  m_socket->SetAttribute ("SndBufSize", UintegerValue (64*1024* 1024));


}

void PacketSink::StopApplication ()     // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
  while(!m_socketList.empty ()) //these are accepted sockets, close them
    {
      Ptr<Socket> acceptedSocket = m_socketList.front ();
      m_socketList.pop_front ();
      acceptedSocket->Close ();
    }
  if (m_socket) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void PacketSink::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (packet->GetSize () == 0)
        { //EOF
          break;
        }
      m_totalRx += packet->GetSize ();
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                       << "s packet sink received "
                       <<  packet->GetSize () << " bytes from "
                       << InetSocketAddress::ConvertFrom(from).GetIpv4 ()
                       << " port " << InetSocketAddress::ConvertFrom (from).GetPort ()
                       << " total Rx " << m_totalRx << " bytes");
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                       << "s packet sink received "
                       <<  packet->GetSize () << " bytes from "
                       << Inet6SocketAddress::ConvertFrom(from).GetIpv6 ()
                       << " port " << Inet6SocketAddress::ConvertFrom (from).GetPort ()
                       << " total Rx " << m_totalRx << " bytes");
        }
      m_rxTrace (packet, from);
    }
}


void PacketSink::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}
 
void PacketSink::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}
 

void PacketSink::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_FUNCTION (this << s << from);
  s->SetRecvCallback (MakeCallback (&PacketSink::HandleRead, this));
  m_socketList.push_back (s);
}

} // Namespace ns3
