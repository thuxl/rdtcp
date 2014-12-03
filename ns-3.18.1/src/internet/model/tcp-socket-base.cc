/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 Georgia Tech Research Corporation
 * Copyright (c) 2010 Adrian Sai-wah Tam
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
 * Author: Adrian Sai-wah Tam <adrian.sw.tam@gmail.com>
 */

#define NS_LOG_APPEND_CONTEXT \
  if (m_node) { std::cout << Simulator::Now ().GetSeconds () << " [node " << m_node->GetId () << ", rec num:"<< m_receiver_order << ","<<this<<"] "; }


#include "ns3/abort.h"
#include "ns3/node.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/ipv4.h"
#include "ns3/ipv6.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv6-route.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv6-routing-protocol.h"
#include "ns3/simulation-singleton.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/trace-source-accessor.h"
#include "tcp-socket-base.h"
#include "tcp-l4-protocol.h"
#include "ipv4-end-point.h"
#include "ipv6-end-point.h"
#include "ipv6-l3-protocol.h"
#include "tcp-header.h"
#include "rtt-estimator.h"
#include "mymacros.h" //xl


#include <algorithm>
#include <iostream>
using namespace std;

NS_LOG_COMPONENT_DEFINE ("TcpSocketBase");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (TcpSocketBase);

TypeId
TcpSocketBase::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpSocketBase")
    .SetParent<TcpSocket> ()
//    .AddAttribute ("TcpState", "State in TCP state machine",
//                   TypeId::ATTR_GET,
//                   EnumValue (CLOSED),
//                   MakeEnumAccessor (&TcpSocketBase::m_state),
//                   MakeEnumChecker (CLOSED, "Closed"))
    .AddAttribute ("MaxSegLifetime",
                   "Maximum segment lifetime in seconds, use for TIME_WAIT state transition to CLOSED state",
                   DoubleValue (120), /* RFC793 says MSL=2 minutes*/
                   MakeDoubleAccessor (&TcpSocketBase::m_msl),
                   MakeDoubleChecker<double> (0))
    .AddAttribute ("MaxWindowSize", "Max size of advertised window",
                   UintegerValue (65535),
                   MakeUintegerAccessor (&TcpSocketBase::m_maxWinSize),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("IcmpCallback", "Callback invoked whenever an icmp error is received on this socket.",
                   CallbackValue (),
                   MakeCallbackAccessor (&TcpSocketBase::m_icmpCallback),
                   MakeCallbackChecker ())
    .AddAttribute ("IcmpCallback6", "Callback invoked whenever an icmpv6 error is received on this socket.",
                   CallbackValue (),
                   MakeCallbackAccessor (&TcpSocketBase::m_icmpCallback6),
                   MakeCallbackChecker ())                   
    .AddTraceSource ("RTO",
                     "Retransmission timeout",
                     MakeTraceSourceAccessor (&TcpSocketBase::m_rto))
    .AddTraceSource ("RTT",
                     "Last RTT sample",
                     MakeTraceSourceAccessor (&TcpSocketBase::m_lastRtt))
    .AddTraceSource ("NextTxSequence",
                     "Next sequence number to send (SND.NXT)",
                     MakeTraceSourceAccessor (&TcpSocketBase::m_nextTxSequence))
    .AddTraceSource ("HighestSequence",
                     "Highest sequence number ever sent in socket's life time",
                     MakeTraceSourceAccessor (&TcpSocketBase::m_highTxMark))
    .AddTraceSource ("State",
                     "TCP state",
                     MakeTraceSourceAccessor (&TcpSocketBase::m_state))
    .AddTraceSource ("RWND",
                     "Remote side's flow control window",
                     MakeTraceSourceAccessor (&TcpSocketBase::m_rWnd))



    //new protocol, xl
	.AddAttribute ("DCTCP_MODE", 
                   "The DCTCP mode for datacenter ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TcpSocketBase::m_dctcp_mode),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("ICTCP_MODE", 
                   "The ICTCP mode for datacenter ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TcpSocketBase::m_ictcp_mode),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("RTCP_MODE", 
                   "The receiver TCP mode for datacenter ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TcpSocketBase::m_rtcp_mode),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("RDTCP_MODE", 
                   "The receiver DCTCP mode for datacenter ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TcpSocketBase::m_rdtcp_mode),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("RDTCP_MAXQUEUE_SIZE", 
                   "The max queue size for RDTCP controller ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TcpSocketBase::m_rdtcp_queue_maxsize),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("RDTCP_CONTROLLER_MODE", 
                   "The RDTCP controller is active when set 1.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TcpSocketBase::m_rdtcp_controller_mode),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("RDTCP_no_priority", 
                   "RDTCP does not use flow priority.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TcpSocketBase::m_rdtcp_no_priority),
                   MakeUintegerChecker<uint32_t> ())

  ;
  return tid;
}

TcpSocketBase::TcpSocketBase (void)
  : m_dupAckCount (0),
    m_delAckCount (0),
    m_endPoint (0),
    m_endPoint6 (0),
    m_node (0),
    m_tcp (0),
    m_rtt (0),
    m_nextTxSequence (0),
    // Change this for non-zero initial sequence number
    m_highTxMark (0),
    m_rxBuffer (0),
    m_txBuffer (0),
    m_state (CLOSED),
    m_errno (ERROR_NOTERROR),
    m_closeNotified (false),
    m_closeOnEmpty (false),
    m_shutdownSend (false),
    m_shutdownRecv (false),
    m_connected (false),
    m_segmentSize (0),
    // For attribute initialization consistency (quiet valgrind)
    m_rWnd (0),

  m_dctcp_mode (0),  //xl
  m_ictcp_mode(0),  //xl
  m_rtcp_mode(0),  //xl
  m_rdtcp_mode(0),  //xl
  m_first_ack_on_established (true), // by xl 
  m_receiver_order (0),
  m_receiver_pre_state_is_ecn(false),
  m_sender_pre_state_is_ecn(false),
  m_receiver_send_ack_times(0) ,
  m_receiver_inFastRec(false),
  m_pre_ack(0),
  m_overRTO_r_times(0),
  m_receive_device(0),
  m_received_dataN(0),
  m_receiver_order_on_dev (0),
  m_firstStart_cal_ictcp(true),
  m_sender_flowsize(0),
  m_rdtcp_queue_maxsize(0),
  m_rdtcp_controller_mode(0),
  m_recCwnd_upper (1),
  m_sender_deadline(0), //in ns.
  m_sender_flowid(0), //in ns.
  m_incre_bytes(0),
  m_rdtcp_no_priority(0)
{
  NS_LOG_FUNCTION (this);
  //NS_LOG_LOGIC ("Invoked the tcpsocketbase constructor, dctcpmode="<< m_dctcp_mode 
		  //<< ", ictcpmode="<< m_ictcp_mode<<", rtcpmode="<< m_rtcp_mode <<", rdtcpmode="<< m_rdtcp_mode 
		  //<< ", m_receiver_order = "<<m_receiver_order
		  //<< ", maxqueue="<< m_rdtcp_queue_maxsize );
}

TcpSocketBase::TcpSocketBase (const TcpSocketBase& sock)
  : TcpSocket (sock),
    //copy object::m_tid and socket::callbacks
    m_dupAckCount (sock.m_dupAckCount),
    m_delAckCount (0),
    m_delAckMaxCount (sock.m_delAckMaxCount),
    m_noDelay (sock.m_noDelay),
    m_cnRetries (sock.m_cnRetries),
    m_delAckTimeout (sock.m_delAckTimeout),
    m_persistTimeout (sock.m_persistTimeout),
    m_cnTimeout (sock.m_cnTimeout),
    m_endPoint (0),
    m_endPoint6 (0),
    m_node (sock.m_node),
    m_tcp (sock.m_tcp),
    m_rtt (0),
    m_nextTxSequence (sock.m_nextTxSequence),
    m_highTxMark (sock.m_highTxMark),
    m_rxBuffer (sock.m_rxBuffer),
    m_txBuffer (sock.m_txBuffer),
    m_state (sock.m_state),
    m_errno (sock.m_errno),
    m_closeNotified (sock.m_closeNotified),
    m_closeOnEmpty (sock.m_closeOnEmpty),
    m_shutdownSend (sock.m_shutdownSend),
    m_shutdownRecv (sock.m_shutdownRecv),
    m_connected (sock.m_connected),
    m_msl (sock.m_msl),
    m_segmentSize (sock.m_segmentSize),
    m_maxWinSize (sock.m_maxWinSize),
    m_rWnd (sock.m_rWnd),

  m_dctcp_mode (sock.m_dctcp_mode ),  //xl
  m_ictcp_mode(sock.m_ictcp_mode),  //xl
  m_rtcp_mode(sock.m_rtcp_mode),  //xl
  m_rdtcp_mode(sock.m_rdtcp_mode),  //xl
  m_first_ack_on_established (false), // by xl 
  m_receiver_order (sock.m_receiver_order),
  m_receiver_pre_state_is_ecn(false),
  m_sender_pre_state_is_ecn(false),
  m_receiver_send_ack_times(0) ,
    m_receiver_inFastRec(false),
    m_pre_ack(0),
    m_overRTO_r_times(0),
    m_receive_device(sock.m_receive_device),
    m_received_dataN(0),
    m_firstStart_cal_ictcp(true),
    m_receiver_last_mod_time (sock.m_receiver_last_mod_time),
    m_sender_flowsize(0),
  m_rdtcp_queue_maxsize(sock.m_rdtcp_queue_maxsize),
  m_rdtcp_controller_mode(sock.m_rdtcp_controller_mode),
  m_recCwnd_upper (1),
  m_sender_deadline(0), //in ns.
  m_sender_flowid(0), //in ns.
  m_incre_bytes(0),
  m_rdtcp_no_priority(sock.m_rdtcp_no_priority)
{
  NS_LOG_FUNCTION (this);
  if (m_ictcp_mode) {
	  m_receiver_order_on_dev = m_receive_device->m_dev_socket_cout;
      NS_LOG_LOGIC ("ictcp:  m_receiver_order_on_dev = "<< m_receiver_order_on_dev );
  }

  NS_LOG_LOGIC ("Invoked the tcpsocketbase copy constructor, dctcpmode="<< m_dctcp_mode 
		  << ", ictcpmode="<< m_ictcp_mode<<", rtcpmode="<< m_rtcp_mode <<", rdtcpmode="<< m_rdtcp_mode 
		  << ", m_receiver_order = "<<m_receiver_order
		  << ", maxqueue="<< m_rdtcp_queue_maxsize );
		  
  // Copy the rtt estimator if it is set
  if (sock.m_rtt)
    {
      m_rtt = sock.m_rtt->Copy ();
    }
  // Reset all callbacks to null
  Callback<void, Ptr< Socket > > vPS = MakeNullCallback<void, Ptr<Socket> > ();
  Callback<void, Ptr<Socket>, const Address &> vPSA = MakeNullCallback<void, Ptr<Socket>, const Address &> ();
  Callback<void, Ptr<Socket>, uint32_t> vPSUI = MakeNullCallback<void, Ptr<Socket>, uint32_t> ();
  SetConnectCallback (vPS, vPS);
  SetDataSentCallback (vPSUI);
  SetSendCallback (vPSUI);
  SetRecvCallback (vPS);
}

TcpSocketBase::~TcpSocketBase (void)
{
  NS_LOG_FUNCTION (this);
  m_node = 0;
  if (m_endPoint != 0)
    {
      NS_ASSERT (m_tcp != 0);
      /*
       * Upon Bind, an Ipv4Endpoint is allocated and set to m_endPoint, and
       * DestroyCallback is set to TcpSocketBase::Destroy. If we called
       * m_tcp->DeAllocate, it wil destroy its Ipv4EndpointDemux::DeAllocate,
       * which in turn destroys my m_endPoint, and in turn invokes
       * TcpSocketBase::Destroy to nullify m_node, m_endPoint, and m_tcp.
       */
      NS_ASSERT (m_endPoint != 0);
      m_tcp->DeAllocate (m_endPoint);
      NS_ASSERT (m_endPoint == 0);
    }
  if (m_endPoint6 != 0)
    {
      NS_ASSERT (m_tcp != 0);
      NS_ASSERT (m_endPoint6 != 0);
      m_tcp->DeAllocate (m_endPoint6);
      NS_ASSERT (m_endPoint6 == 0);
    }
  m_tcp = 0;
  CancelAllTimers ();
}

/** Associate a node with this TCP socket */
void
TcpSocketBase::SetNode (Ptr<Node> node)
{
  m_node = node;
}

/** Associate the L4 protocol (e.g. mux/demux) with this socket */
void
TcpSocketBase::SetTcp (Ptr<TcpL4Protocol> tcp)
{
  m_tcp = tcp;
}

/** Set an RTT estimator with this socket */
void
TcpSocketBase::SetRtt (Ptr<RttEstimator> rtt)
{
  m_rtt = rtt;
}

/** Inherit from Socket class: Returns error code */
enum Socket::SocketErrno
TcpSocketBase::GetErrno (void) const
{
  return m_errno;
}

/** Inherit from Socket class: Returns socket type, NS3_SOCK_STREAM */
enum Socket::SocketType
TcpSocketBase::GetSocketType (void) const
{
  return NS3_SOCK_STREAM;
}

/** Inherit from Socket class: Returns associated node */
Ptr<Node>
TcpSocketBase::GetNode (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_node;
}

/** Inherit from Socket class: Bind socket to an end-point in TcpL4Protocol */
int
TcpSocketBase::Bind (void)
{
  NS_LOG_FUNCTION (this);
  //NS_LOG_LOGIC ("tcpsocketbase bind, dctcpmode="<< m_dctcp_mode 
		  //<< ", ictcpmode="<< m_ictcp_mode<<", rtcpmode="<< m_rtcp_mode <<", rdtcpmode="<< m_rdtcp_mode 
		  //<< ", m_receiver_order = "<<m_receiver_order
		  //<< ", maxqueue="<< m_rdtcp_queue_maxsize );
  m_endPoint = m_tcp->Allocate ();
  if (0 == m_endPoint)
    {
      m_errno = ERROR_ADDRNOTAVAIL;
      return -1;
    }
  m_tcp->m_sockets.push_back (this);
  return SetupCallback ();
}

int
TcpSocketBase::Bind6 (void)
{
  NS_LOG_FUNCTION (this);
  m_endPoint6 = m_tcp->Allocate6 ();
  if (0 == m_endPoint6)
    {
      m_errno = ERROR_ADDRNOTAVAIL;
      return -1;
    }
  m_tcp->m_sockets.push_back (this);
  return SetupCallback ();
}

/** Inherit from Socket class: Bind socket (with specific address) to an end-point in TcpL4Protocol */
int
TcpSocketBase::Bind (const Address &address)
{
  NS_LOG_FUNCTION (this << address);
  if (InetSocketAddress::IsMatchingType (address))
    {
      InetSocketAddress transport = InetSocketAddress::ConvertFrom (address);
      Ipv4Address ipv4 = transport.GetIpv4 ();
      uint16_t port = transport.GetPort ();
      if (ipv4 == Ipv4Address::GetAny () && port == 0)
        {
          m_endPoint = m_tcp->Allocate ();
        }
      else if (ipv4 == Ipv4Address::GetAny () && port != 0)
        {
          m_endPoint = m_tcp->Allocate (port);
        }
      else if (ipv4 != Ipv4Address::GetAny () && port == 0)
        {
          m_endPoint = m_tcp->Allocate (ipv4);
        }
      else if (ipv4 != Ipv4Address::GetAny () && port != 0)
        {
          m_endPoint = m_tcp->Allocate (ipv4, port);
        }
      if (0 == m_endPoint)
        {
          m_errno = port ? ERROR_ADDRINUSE : ERROR_ADDRNOTAVAIL;
          return -1;
        }
    }
  else if (Inet6SocketAddress::IsMatchingType (address))
    {
      Inet6SocketAddress transport = Inet6SocketAddress::ConvertFrom (address);
      Ipv6Address ipv6 = transport.GetIpv6 ();
      uint16_t port = transport.GetPort ();
      if (ipv6 == Ipv6Address::GetAny () && port == 0)
        {
          m_endPoint6 = m_tcp->Allocate6 ();
        }
      else if (ipv6 == Ipv6Address::GetAny () && port != 0)
        {
          m_endPoint6 = m_tcp->Allocate6 (port);
        }
      else if (ipv6 != Ipv6Address::GetAny () && port == 0)
        {
          m_endPoint6 = m_tcp->Allocate6 (ipv6);
        }
      else if (ipv6 != Ipv6Address::GetAny () && port != 0)
        {
          m_endPoint6 = m_tcp->Allocate6 (ipv6, port);
        }
      if (0 == m_endPoint6)
        {
          m_errno = port ? ERROR_ADDRINUSE : ERROR_ADDRNOTAVAIL;
          return -1;
        }
    }
  else
    {
      m_errno = ERROR_INVAL;
      return -1;
    }
  m_tcp->m_sockets.push_back (this);
  NS_LOG_LOGIC ("TcpSocketBase " << this << " got an endpoint: " << m_endPoint);

  return SetupCallback ();
}

/** Inherit from Socket class: Initiate connection to a remote address:port */
int
TcpSocketBase::Connect (const Address & address)
{
  NS_LOG_FUNCTION (this << address);
  //NS_LOG_DEBUG ("DCTCP_MODE=" << m_dctcp_mode
//		  <<",RTCP_MODE=" << m_rtcp_mode
//		  <<",RDTCP_MODE=" << m_rdtcp_mode
//		  <<",RDTCP_CONTROLLER_MODE="<< m_rdtcp_controller_mode
//		  <<",RDTCP_MAXQUEUE_SIZE=" << m_rdtcp_queue_maxsize
//		  <<",ICTCP_MODE=" << m_ictcp_mode
//		  );

  // If haven't do so, Bind() this socket first
  if (InetSocketAddress::IsMatchingType (address) && m_endPoint6 == 0)
    {
      if (m_endPoint == 0)
        {
          if (Bind () == -1)
            {
              NS_ASSERT (m_endPoint == 0);
              return -1; // Bind() failed
            }
          NS_ASSERT (m_endPoint != 0);
        }
      InetSocketAddress transport = InetSocketAddress::ConvertFrom (address);
      m_endPoint->SetPeer (transport.GetIpv4 (), transport.GetPort ());
      m_endPoint6 = 0;

      // Get the appropriate local address and port number from the routing protocol and set up endpoint
      if (SetupEndpoint () != 0)
        { // Route to destination does not exist
          return -1;
        }
    }
  else if (Inet6SocketAddress::IsMatchingType (address)  && m_endPoint == 0)
    {
      // If we are operating on a v4-mapped address, translate the address to
      // a v4 address and re-call this function
      Inet6SocketAddress transport = Inet6SocketAddress::ConvertFrom (address);
      Ipv6Address v6Addr = transport.GetIpv6 ();
      if (v6Addr.IsIpv4MappedAddress () == true)
        {
          Ipv4Address v4Addr = v6Addr.GetIpv4MappedAddress ();
          return Connect (InetSocketAddress (v4Addr, transport.GetPort ()));
        }

      if (m_endPoint6 == 0)
        {
          if (Bind6 () == -1)
            {
              NS_ASSERT (m_endPoint6 == 0);
              return -1; // Bind() failed
            }
          NS_ASSERT (m_endPoint6 != 0);
        }
      m_endPoint6->SetPeer (v6Addr, transport.GetPort ());
      m_endPoint = 0;

      // Get the appropriate local address and port number from the routing protocol and set up endpoint
      if (SetupEndpoint6 () != 0)
        { // Route to destination does not exist
          return -1;
        }
    }
  else
    {
      m_errno = ERROR_INVAL;
      return -1;
    }

  // Re-initialize parameters in case this socket is being reused after CLOSE
  m_rtt->Reset ();
  m_cnCount = m_cnRetries;

  // DoConnect() will do state-checking and send a SYN packet
  return DoConnect ();
}


/** Inherit from Socket class: Initiate connection to a remote address:port */
int
TcpSocketBase::Connect (const Address & address, uint32_t flowsize, uint32_t deadline, uint32_t flowid)
{
  NS_LOG_FUNCTION (this << address << flowsize << deadline << flowid);

  m_sender_flowsize = flowsize; //add by xl for rdtcp controller.
  m_sender_deadline = deadline; //in ns.
  m_sender_flowid = flowid; 
  // If haven't do so, Bind() this socket first
  if (InetSocketAddress::IsMatchingType (address) && m_endPoint6 == 0)
    {
      if (m_endPoint == 0)
        {
          if (Bind () == -1)
            {
              NS_ASSERT (m_endPoint == 0);
              return -1; // Bind() failed
            }
          NS_ASSERT (m_endPoint != 0);
        }
      InetSocketAddress transport = InetSocketAddress::ConvertFrom (address);
      m_endPoint->SetPeer (transport.GetIpv4 (), transport.GetPort ());
      m_endPoint6 = 0;

      // Get the appropriate local address and port number from the routing protocol and set up endpoint
      if (SetupEndpoint () != 0)
        { // Route to destination does not exist
          return -1;
        }
    }
  else if (Inet6SocketAddress::IsMatchingType (address)  && m_endPoint == 0)
    {
      // If we are operating on a v4-mapped address, translate the address to
      // a v4 address and re-call this function
      Inet6SocketAddress transport = Inet6SocketAddress::ConvertFrom (address);
      Ipv6Address v6Addr = transport.GetIpv6 ();
      if (v6Addr.IsIpv4MappedAddress () == true)
        {
          Ipv4Address v4Addr = v6Addr.GetIpv4MappedAddress ();
          return Connect (InetSocketAddress (v4Addr, transport.GetPort ()));
        }

      if (m_endPoint6 == 0)
        {
          if (Bind6 () == -1)
            {
              NS_ASSERT (m_endPoint6 == 0);
              return -1; // Bind() failed
            }
          NS_ASSERT (m_endPoint6 != 0);
        }
      m_endPoint6->SetPeer (v6Addr, transport.GetPort ());
      m_endPoint = 0;

      // Get the appropriate local address and port number from the routing protocol and set up endpoint
      if (SetupEndpoint6 () != 0)
        { // Route to destination does not exist
          return -1;
        }
    }
  else
    {
      m_errno = ERROR_INVAL;
      return -1;
    }

  // Re-initialize parameters in case this socket is being reused after CLOSE
  m_rtt->Reset ();
  m_cnCount = m_cnRetries;

  // DoConnect() will do state-checking and send a SYN packet
  return DoConnect ();
}

/** Inherit from Socket class: Listen on the endpoint for an incoming connection */
int
TcpSocketBase::Listen (void)
{
  NS_LOG_FUNCTION (this);
//  NS_LOG_DEBUG ("DCTCP_MODE=" << m_dctcp_mode
//		  <<",RTCP_MODE=" << m_rtcp_mode
//		  <<",RDTCP_MODE=" << m_rdtcp_mode
//		  <<",RDTCP_CONTROLLER_MODE="<< m_rdtcp_controller_mode
//		  <<",RDTCP_MAXQUEUE_SIZE=" << m_rdtcp_queue_maxsize
//		  <<",ICTCP_MODE=" << m_ictcp_mode
//		  );
  // Linux quits EINVAL if we're not in CLOSED state, so match what they do
  if (m_state != CLOSED)
    {
      m_errno = ERROR_INVAL;
      return -1;
    }
  // In other cases, set the state to LISTEN and done
  NS_LOG_INFO ("CLOSED -> LISTEN");

  if (!m_receive_device)
      m_receive_device = m_node->GetDevice (1); //default for 1?
  //NS_LOG_DEBUG ("m_receive_device ="<<m_receive_device );
  if (/*m_ictcp_mode && */m_receive_device->m_dev_global_not_initial)
  {
      m_receive_device->m_dev_global_not_initial = 0;
	  m_receive_device->m_dev_global_not_start = 1;
      m_ictcp_global_update_Event.Cancel (); 
          
   	  for (uint16_t i = 0; i<MAX_FLOWS_N; i++)
      {
        m_receive_device->m_dev_received_coarsedataN[i] = 0;
        m_receive_device->m_dev_RTT_connection[i] = Time::FromInteger(0, Time::S);
        m_receive_device->m_dev_ictcp_receiver_win[i] = 2 * m_segmentSize;
      }
  

      //PRINT_STUFF ("listen, receiver, device->mtu() = "<<m_receive_device ->GetMtu ()<<",dev="<<m_receive_device ); 
      m_receive_device->SetReceivedDataNum(0);
      m_receive_device->m_dev_socket_cout = 0; //if many Listens, there will be problem???????????
      m_receive_device->m_dev_closed_cout = 0; 
		    
  }
  if (m_rdtcp_mode == 1 && m_rdtcp_controller_mode == 1) {
	  m_receive_device->m_global_rtt = Time::FromInteger (0, Time::S);
	  m_receive_device->m_block_num = 0;
	  m_receive_device->m_global_block_id = 0;
	  m_receive_device->m_rdtcp_closed_count = 0;
	  m_receive_device->m_rdtcp_controller_list.clear();
	  m_receive_device->m_flight_wins = 0;
  }



  m_state = LISTEN;
  return 0;
}

/** Inherit from Socket class: Kill this socket and signal the peer (if any) */
int
TcpSocketBase::Close (void)
{
  NS_LOG_FUNCTION (this);
  /// \internal
  /// First we check to see if there is any unread rx data.
  /// \bugid{426} claims we should send reset in this case.
  if (m_rxBuffer.Size () != 0)
    {
      NS_LOG_INFO ("Socket " << this << " << unread rx data during close.  Sending reset");
      SendRST ();
      return 0;
    }
 
  if (m_txBuffer.SizeFromSequence (m_nextTxSequence) > 0)
    { // App close with pending data must wait until all data transmitted
      if (m_closeOnEmpty == false)
        {
          m_closeOnEmpty = true;
          NS_LOG_INFO ("Socket " << this << " deferring close, state " << TcpStateName[m_state]);
        }
      return 0;
    }
  return DoClose ();
}

/** Inherit from Socket class: Signal a termination of send */
int
TcpSocketBase::ShutdownSend (void)
{
  NS_LOG_FUNCTION (this);
  
  //this prevents data from being added to the buffer
  m_shutdownSend = true;
  m_closeOnEmpty = true;
  //if buffer is already empty, send a fin now
  //otherwise fin will go when buffer empties.
  if (m_txBuffer.Size () == 0)
    {
      if (m_state == ESTABLISHED || m_state == CLOSE_WAIT)
        {
          NS_LOG_INFO("Emtpy tx buffer, send fin");
          SendEmptyPacket (TcpHeader::FIN);  

          if (m_state == ESTABLISHED)
            { // On active close: I am the first one to send FIN
              NS_LOG_INFO ("ESTABLISHED -> FIN_WAIT_1");
              m_state = FIN_WAIT_1;
            }
          else
            { // On passive close: Peer sent me FIN already
              NS_LOG_INFO ("CLOSE_WAIT -> LAST_ACK");
              m_state = LAST_ACK;
            }  
        }
    }
 
  return 0;
}

/** Inherit from Socket class: Signal a termination of receive */
int
TcpSocketBase::ShutdownRecv (void)
{
  NS_LOG_FUNCTION (this);
  m_shutdownRecv = true;
  return 0;
}

/** Inherit from Socket class: Send a packet. Parameter flags is not used.
    Packet has no TCP header. Invoked by upper-layer application */
int
TcpSocketBase::Send (Ptr<Packet> p, uint32_t flags)
{
  NS_LOG_FUNCTION (this << p);
  NS_ABORT_MSG_IF (flags, "use of flags is not supported in TcpSocketBase::Send()");
  if (m_state == ESTABLISHED || m_state == SYN_SENT || m_state == CLOSE_WAIT)
    {
      // Store the packet into Tx buffer
      if (!m_txBuffer.Add (p))
        { // TxBuffer overflow, send failed
          m_errno = ERROR_MSGSIZE;
          return -1;
        }
      if (m_shutdownSend)
        {
          m_errno = ERROR_SHUTDOWN;
          return -1;
        }
      // Submit the data to lower layers
      NS_LOG_LOGIC ("txBufSize=" << m_txBuffer.Size () << " state " << TcpStateName[m_state]);
      if (m_state == ESTABLISHED || m_state == CLOSE_WAIT)
        { // Try to send the data out
          SendPendingData (m_connected);
        }
      return p->GetSize ();
    }
  else
    { // Connection not established yet
      m_errno = ERROR_NOTCONN;
      return -1; // Send failure
    }
}

/** Inherit from Socket class: In TcpSocketBase, it is same as Send() call */
int
TcpSocketBase::SendTo (Ptr<Packet> p, uint32_t flags, const Address &address)
{
  return Send (p, flags); // SendTo() and Send() are the same
}

/** Inherit from Socket class: Return data to upper-layer application. Parameter flags
    is not used. Data is returned as a packet of size no larger than maxSize */
Ptr<Packet>
TcpSocketBase::Recv (uint32_t maxSize, uint32_t flags)
{
  NS_LOG_FUNCTION (this);
  NS_ABORT_MSG_IF (flags, "use of flags is not supported in TcpSocketBase::Recv()");
  if (m_rxBuffer.Size () == 0 && m_state == CLOSE_WAIT)
    {
      return Create<Packet> (); // Send EOF on connection close
    }
  Ptr<Packet> outPacket = m_rxBuffer.Extract (maxSize);
  if (outPacket != 0 && outPacket->GetSize () != 0)
    {
      SocketAddressTag tag;
      if (m_endPoint != 0)
        {
          tag.SetAddress (InetSocketAddress (m_endPoint->GetPeerAddress (), m_endPoint->GetPeerPort ()));
        }
      else if (m_endPoint6 != 0)
        {
          tag.SetAddress (Inet6SocketAddress (m_endPoint6->GetPeerAddress (), m_endPoint6->GetPeerPort ()));
        }
      outPacket->AddPacketTag (tag);
    }
  return outPacket;
}

/** Inherit from Socket class: Recv and return the remote's address */
Ptr<Packet>
TcpSocketBase::RecvFrom (uint32_t maxSize, uint32_t flags, Address &fromAddress)
{
  NS_LOG_FUNCTION (this << maxSize << flags);
  Ptr<Packet> packet = Recv (maxSize, flags);
  // Null packet means no data to read, and an empty packet indicates EOF
  if (packet != 0 && packet->GetSize () != 0)
    {
      if (m_endPoint != 0)
        {
          fromAddress = InetSocketAddress (m_endPoint->GetPeerAddress (), m_endPoint->GetPeerPort ());
        }
      else if (m_endPoint6 != 0)
        {
          fromAddress = Inet6SocketAddress (m_endPoint6->GetPeerAddress (), m_endPoint6->GetPeerPort ());
        }
      else
        {
          fromAddress = InetSocketAddress (Ipv4Address::GetZero (), 0);
        }
    }
  return packet;
}

/** Inherit from Socket class: Get the max number of bytes an app can send */
uint32_t
TcpSocketBase::GetTxAvailable (void) const
{
  NS_LOG_FUNCTION (this);
  return m_txBuffer.Available ();
}

/** Inherit from Socket class: Get the max number of bytes an app can read */
uint32_t
TcpSocketBase::GetRxAvailable (void) const
{
  NS_LOG_FUNCTION (this);
  return m_rxBuffer.Available ();
}

/** Inherit from Socket class: Return local address:port */
int
TcpSocketBase::GetSockName (Address &address) const
{
  NS_LOG_FUNCTION (this);
  if (m_endPoint != 0)
    {
      address = InetSocketAddress (m_endPoint->GetLocalAddress (), m_endPoint->GetLocalPort ());
    }
  else if (m_endPoint6 != 0)
    {
      address = Inet6SocketAddress (m_endPoint6->GetLocalAddress (), m_endPoint6->GetLocalPort ());
    }
  else
    { // It is possible to call this method on a socket without a name
      // in which case, behavior is unspecified
      // Should this return an InetSocketAddress or an Inet6SocketAddress?
      address = InetSocketAddress (Ipv4Address::GetZero (), 0);
    }
  return 0;
}

/** Inherit from Socket class: Bind this socket to the specified NetDevice */
void
TcpSocketBase::BindToNetDevice (Ptr<NetDevice> netdevice)
{
  NS_LOG_FUNCTION (netdevice);
  Socket::BindToNetDevice (netdevice); // Includes sanity check
  if (m_endPoint == 0 && m_endPoint6 == 0)
    {
      if (Bind () == -1)
        {
          NS_ASSERT ((m_endPoint == 0 && m_endPoint6 == 0));
          return;
        }
      NS_ASSERT ((m_endPoint != 0 && m_endPoint6 != 0));
    }

  if (m_endPoint != 0)
    {
      m_endPoint->BindToNetDevice (netdevice);
    }
  // No BindToNetDevice() for Ipv6EndPoint
  return;
}

/** Clean up after Bind. Set up callback functions in the end-point. */
int
TcpSocketBase::SetupCallback (void)
{
  NS_LOG_FUNCTION (this);

  if (m_endPoint == 0 && m_endPoint6 == 0)
    {
      return -1;
    }
  if (m_endPoint != 0)
    {
      m_endPoint->SetRxCallback (MakeCallback (&TcpSocketBase::ForwardUp, Ptr<TcpSocketBase> (this)));
      m_endPoint->SetIcmpCallback (MakeCallback (&TcpSocketBase::ForwardIcmp, Ptr<TcpSocketBase> (this)));
      m_endPoint->SetDestroyCallback (MakeCallback (&TcpSocketBase::Destroy, Ptr<TcpSocketBase> (this)));
    }
  if (m_endPoint6 != 0)
    {
      m_endPoint6->SetRxCallback (MakeCallback (&TcpSocketBase::ForwardUp6, Ptr<TcpSocketBase> (this)));
      m_endPoint6->SetIcmpCallback (MakeCallback (&TcpSocketBase::ForwardIcmp6, Ptr<TcpSocketBase> (this)));
      m_endPoint6->SetDestroyCallback (MakeCallback (&TcpSocketBase::Destroy6, Ptr<TcpSocketBase> (this)));
    }

  return 0;
}

/** Perform the real connection tasks: Send SYN if allowed, RST if invalid */
int
TcpSocketBase::DoConnect (void)
{
  NS_LOG_FUNCTION (this);

  // A new connection is allowed only if this socket does not have a connection
  if (m_state == CLOSED || m_state == LISTEN || m_state == SYN_SENT || m_state == LAST_ACK || m_state == CLOSE_WAIT)
    { // send a SYN packet and change state into SYN_SENT
      SendEmptyPacket (TcpHeader::SYN);
      NS_LOG_INFO (TcpStateName[m_state] << " -> SYN_SENT");
      m_state = SYN_SENT;
    }
  else if (m_state != TIME_WAIT)
    { // In states SYN_RCVD, ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, and CLOSING, an connection
      // exists. We send RST, tear down everything, and close this socket.
      SendRST ();
      CloseAndNotify ();
    }
  return 0;
}

/** Do the action to close the socket. Usually send a packet with appropriate
    flags depended on the current m_state. */
int
TcpSocketBase::DoClose (void)
{
  NS_LOG_FUNCTION (this);
  switch (m_state)
    {
    case SYN_RCVD:
    case ESTABLISHED:
      // send FIN to close the peer
      SendEmptyPacket (TcpHeader::FIN);
      NS_LOG_INFO ("ESTABLISHED -> FIN_WAIT_1");
      m_state = FIN_WAIT_1;
      break;
    case CLOSE_WAIT:
      // send FIN+ACK to close the peer
      SendEmptyPacket (TcpHeader::FIN | TcpHeader::ACK);
      NS_LOG_INFO ("CLOSE_WAIT -> LAST_ACK");
      m_state = LAST_ACK;
      break;
    case SYN_SENT:
    case CLOSING:
      // Send RST if application closes in SYN_SENT and CLOSING
      SendRST ();
      CloseAndNotify ();
      break;
    case LISTEN:
    case LAST_ACK:
      // In these three states, move to CLOSED and tear down the end point
      CloseAndNotify ();
      break;
    case CLOSED:
    case FIN_WAIT_1:
    case FIN_WAIT_2:
    case TIME_WAIT:
    default: /* mute compiler */
      // Do nothing in these four states
      break;
    }
  return 0;
}

/** Peacefully close the socket by notifying the upper layer and deallocate end point */
void
TcpSocketBase::CloseAndNotify (void)
{
  NS_LOG_FUNCTION (this);

  if (!m_closeNotified)
    {
      NotifyNormalClose ();
    }
  if (m_state != TIME_WAIT)
    {
      DeallocateEndPoint ();
    }
  m_closeNotified = true;
  NS_LOG_INFO (TcpStateName[m_state] << " -> CLOSED, receiver order:"<< m_receiver_order);
  if (m_receive_device) {
      m_receive_device->m_dev_closed_cout++; 
      if (m_ictcp_mode ) //only receiver has m_receive_device allocated.
      {
          NS_LOG_DEBUG ("ictcp: receiver order: "<< m_receiver_order_on_dev );
          m_receive_device->m_dev_ictcp_receiver_win [ m_receiver_order_on_dev ] = 0;
          m_receive_device->m_dev_received_coarsedataN [ m_receiver_order_on_dev ] = 0;
          m_receive_device->m_dev_RTT_connection [ m_receiver_order_on_dev ] = Time::FromInteger(0, Time::S);
      }
	  
  }
  CancelAllTimers ();
  m_state = CLOSED;
}


/** Tell if a sequence number range is out side the range that my rx buffer can
    accpet */
bool
TcpSocketBase::OutOfRange (SequenceNumber32 head, SequenceNumber32 tail) const
{
  if (m_state == LISTEN || m_state == SYN_SENT || m_state == SYN_RCVD)
    { // Rx buffer in these states are not initialized.
      return false;
    }
  if (m_state == LAST_ACK || m_state == CLOSING || m_state == CLOSE_WAIT)
    { // In LAST_ACK and CLOSING states, it only wait for an ACK and the
      // sequence number must equals to m_rxBuffer.NextRxSequence ()
      return (m_rxBuffer.NextRxSequence () != head);
    }

  // In all other cases, check if the sequence number is in range
  return (tail < m_rxBuffer.NextRxSequence () || m_rxBuffer.MaxRxSequence () <= head);
}

/** Function called by the L3 protocol when it received a packet to pass on to
    the TCP. This function is registered as the "RxCallback" function in
    SetupCallback(), which invoked by Bind(), and CompleteFork() */
void
TcpSocketBase::ForwardUp (Ptr<Packet> packet, Ipv4Header header, uint16_t port,
                          Ptr<Ipv4Interface> incomingInterface)
{
  DoForwardUp (packet, header, port, incomingInterface);
}

void
TcpSocketBase::ForwardUp6 (Ptr<Packet> packet, Ipv6Header header, uint16_t port)
{
  DoForwardUp (packet, header, port);
}

void
TcpSocketBase::ForwardIcmp (Ipv4Address icmpSource, uint8_t icmpTtl,
                            uint8_t icmpType, uint8_t icmpCode,
                            uint32_t icmpInfo)
{
  NS_LOG_FUNCTION (this << icmpSource << (uint32_t)icmpTtl << (uint32_t)icmpType <<
                   (uint32_t)icmpCode << icmpInfo);
  if (!m_icmpCallback.IsNull ())
    {
      m_icmpCallback (icmpSource, icmpTtl, icmpType, icmpCode, icmpInfo);
    }
}

void
TcpSocketBase::ForwardIcmp6 (Ipv6Address icmpSource, uint8_t icmpTtl,
                            uint8_t icmpType, uint8_t icmpCode,
                            uint32_t icmpInfo)
{
  NS_LOG_FUNCTION (this << icmpSource << (uint32_t)icmpTtl << (uint32_t)icmpType <<
                   (uint32_t)icmpCode << icmpInfo);
  if (!m_icmpCallback6.IsNull ())
    {
      m_icmpCallback6 (icmpSource, icmpTtl, icmpType, icmpCode, icmpInfo);
    }
}

/** The real function to handle the incoming packet from lower layers. This is
    wrapped by ForwardUp() so that this function can be overloaded by daughter
    classes. */
void
TcpSocketBase::DoForwardUp (Ptr<Packet> packet, Ipv4Header header, uint16_t port,
                            Ptr<Ipv4Interface> incomingInterface)
{
  NS_LOG_LOGIC ("Socket " << this << " forward up " <<
                m_endPoint->GetPeerAddress () <<
                ":" << m_endPoint->GetPeerPort () <<
                " to " << m_endPoint->GetLocalAddress () <<
                ":" << m_endPoint->GetLocalPort ());
  Address fromAddress = InetSocketAddress (header.GetSource (), port);
  Address toAddress = InetSocketAddress (header.GetDestination (), m_endPoint->GetLocalPort ());

  // Peel off TCP header and do validity checking
  TcpHeader tcpHeader;
  packet->RemoveHeader (tcpHeader);
  uint8_t tcpflags = tcpHeader.GetFlags (); // xl, get flags

  if (tcpflags & TcpHeader::ACK)
    {
      EstimateRtt (tcpHeader); //for sender.
      //NS_LOG_DEBUG ("receive data for rtt." );
      if (m_rtcp_mode == 1 && (m_state == LAST_ACK || m_state==SYN_RCVD || m_state==ESTABLISHED || tcpflags & TcpHeader::FIN) 
			  && m_rtt->OnSeq (tcpHeader.GetSequenceNumber () ))
	  { //for receiver.
          //NS_LOG_DEBUG (m_rtt->GetCurrentEstimate_r() <<" : receiver RTT 2, seq="<< tcpHeader.GetSequenceNumber ()<<",receiver num="<<m_receiver_order  <<" DoForwardUp. first or last ack ");
       }

    }




  if (tcpflags & TcpHeader::ECE) //xl
  { //PRINT_STUFF("xl: TCP sender received ECN notification from the receiver, need to adjust congestion window");// by xl 

	  if (tcpflags & (TcpHeader::FIN|TcpHeader::SYN|TcpHeader::RST|TcpHeader::PSH|TcpHeader::URG))
    {
      // we only care about ECN for data/ack packets, any other flag just clear ECN
      tcpflags = tcpflags & ~(TcpHeader::ECE);
      tcpHeader.SetFlags(tcpflags);
    } 
	  else if (tcpflags & (TcpHeader::ACK)) {
      //cout << "Balajee: TCP sender received ECN notification from the receiver, need to adjust congestion window" << endl;
    } 
	  else { 
      // TODO clear ECN flag, a data packet with ECN set without request, not sure
      tcpflags = tcpflags & ~(TcpHeader::ECE);
      tcpHeader.SetFlags(tcpflags);
    }
  }



  ReadOptions (tcpHeader);

  // Update Rx window size, i.e. the flow control window
  if (m_rWnd.Get () == 0 && tcpHeader.GetWindowSize () != 0)
    { // persist probes end
      NS_LOG_LOGIC (this << " Leaving zerowindow persist state");
      m_persistEvent.Cancel ();
    }
  m_rWnd = tcpHeader.GetWindowSize ();

  // Discard fully out of range data packets
  if (packet->GetSize ()
      && OutOfRange (tcpHeader.GetSequenceNumber (), tcpHeader.GetSequenceNumber () + packet->GetSize ()))
    {
      NS_LOG_LOGIC ("At state " << TcpStateName[m_state] <<
                    " received packet of seq [" << tcpHeader.GetSequenceNumber () <<
                    ":" << tcpHeader.GetSequenceNumber () + packet->GetSize () <<
                    ") out of range [" << m_rxBuffer.NextRxSequence () << ":" <<
                    m_rxBuffer.MaxRxSequence () << ")");
      // Acknowledgement should be sent for all unacceptable packets (RFC793, p.69)
      if (m_state == ESTABLISHED && !(tcpHeader.GetFlags () & TcpHeader::RST))
        {
          SendEmptyPacket (TcpHeader::ACK);
        }
      return;
    }

  // TCP state machine code in different process functions
  // C.f.: tcp_rcv_state_process() in tcp_input.c in Linux kernel
  switch (m_state)
    {
    case ESTABLISHED:
      ProcessEstablished (packet, tcpHeader, header.GetEcn()); //by xl
      break;
    case LISTEN:
      ProcessListen (packet, tcpHeader, fromAddress, toAddress);
      break;
    case TIME_WAIT:
      // Do nothing
      break;
    case CLOSED:
      // Send RST if the incoming packet is not a RST
      if ((tcpHeader.GetFlags () & ~(TcpHeader::PSH | TcpHeader::URG)) != TcpHeader::RST)
        { // Since m_endPoint is not configured yet, we cannot use SendRST here
          TcpHeader h;
          h.SetFlags (TcpHeader::RST);
          h.SetSequenceNumber (m_nextTxSequence);
          h.SetAckNumber (m_rxBuffer.NextRxSequence ());
          h.SetSourcePort (tcpHeader.GetDestinationPort ());
          h.SetDestinationPort (tcpHeader.GetSourcePort ());
          h.SetWindowSize (AdvertisedWindowSize ());
          AddOptions (h);
          m_tcp->SendPacket (Create<Packet> (), h, header.GetDestination (), header.GetSource (), m_boundnetdevice);
        }
      break;
    case SYN_SENT:
      ProcessSynSent (packet, tcpHeader);
      break;
    case SYN_RCVD:
      ProcessSynRcvd (packet, tcpHeader, fromAddress, toAddress);
      break;
    case FIN_WAIT_1:
    case FIN_WAIT_2:
    case CLOSE_WAIT:
      ProcessWait (packet, tcpHeader);
      break;
    case CLOSING:
      ProcessClosing (packet, tcpHeader);
      break;
    case LAST_ACK:
      ProcessLastAck (packet, tcpHeader);
      break;
    default: // mute compiler
      break;
    }
}

void
TcpSocketBase::DoForwardUp (Ptr<Packet> packet, Ipv6Header header, uint16_t port)
{
  NS_LOG_LOGIC ("Socket " << this << " forward up " <<
                m_endPoint6->GetPeerAddress () <<
                ":" << m_endPoint6->GetPeerPort () <<
                " to " << m_endPoint6->GetLocalAddress () <<
                ":" << m_endPoint6->GetLocalPort ());
  Address fromAddress = Inet6SocketAddress (header.GetSourceAddress (), port);
  Address toAddress = Inet6SocketAddress (header.GetDestinationAddress (), m_endPoint6->GetLocalPort ());

  // Peel off TCP header and do validity checking
  TcpHeader tcpHeader;
  packet->RemoveHeader (tcpHeader);
  if (tcpHeader.GetFlags () & TcpHeader::ACK)
    {
      EstimateRtt (tcpHeader);
    }
  ReadOptions (tcpHeader);

  // Update Rx window size, i.e. the flow control window
  if (m_rWnd.Get () == 0 && tcpHeader.GetWindowSize () != 0)
    { // persist probes end
      NS_LOG_LOGIC (this << " Leaving zerowindow persist state");
      m_persistEvent.Cancel ();
    }
  m_rWnd = tcpHeader.GetWindowSize ();

  // Discard fully out of range packets
  if (packet->GetSize ()
      && OutOfRange (tcpHeader.GetSequenceNumber (), tcpHeader.GetSequenceNumber () + packet->GetSize ()))
    {
      NS_LOG_LOGIC ("At state " << TcpStateName[m_state] <<
                    " received packet of seq [" << tcpHeader.GetSequenceNumber () <<
                    ":" << tcpHeader.GetSequenceNumber () + packet->GetSize () <<
                    ") out of range [" << m_rxBuffer.NextRxSequence () << ":" <<
                    m_rxBuffer.MaxRxSequence () << ")");
      // Acknowledgement should be sent for all unacceptable packets (RFC793, p.69)
      if (m_state == ESTABLISHED && !(tcpHeader.GetFlags () & TcpHeader::RST))
        {
          SendEmptyPacket (TcpHeader::ACK);
        }
      return;
    }

  // TCP state machine code in different process functions
  // C.f.: tcp_rcv_state_process() in tcp_input.c in Linux kernel
  switch (m_state)
    {
    case ESTABLISHED:
      ProcessEstablished (packet, tcpHeader, Ipv4Header::ECN_NotECT); //xl, ipv6 not implement
      break;
    case LISTEN:
      ProcessListen (packet, tcpHeader, fromAddress, toAddress);
      break;
    case TIME_WAIT:
      // Do nothing
      break;
    case CLOSED:
      // Send RST if the incoming packet is not a RST
      if ((tcpHeader.GetFlags () & ~(TcpHeader::PSH | TcpHeader::URG)) != TcpHeader::RST)
        { // Since m_endPoint is not configured yet, we cannot use SendRST here
          TcpHeader h;
          h.SetFlags (TcpHeader::RST);
          h.SetSequenceNumber (m_nextTxSequence);
          h.SetAckNumber (m_rxBuffer.NextRxSequence ());
          h.SetSourcePort (tcpHeader.GetDestinationPort ());
          h.SetDestinationPort (tcpHeader.GetSourcePort ());
          h.SetWindowSize (AdvertisedWindowSize ());
          AddOptions (h);
          m_tcp->SendPacket (Create<Packet> (), h, header.GetDestinationAddress (), header.GetSourceAddress (), m_boundnetdevice);
        }
      break;
    case SYN_SENT:
      ProcessSynSent (packet, tcpHeader);
      break;
    case SYN_RCVD:
      ProcessSynRcvd (packet, tcpHeader, fromAddress, toAddress);
      break;
    case FIN_WAIT_1:
    case FIN_WAIT_2:
    case CLOSE_WAIT:
      ProcessWait (packet, tcpHeader);
      break;
    case CLOSING:
      ProcessClosing (packet, tcpHeader);
      break;
    case LAST_ACK:
      ProcessLastAck (packet, tcpHeader);
      break;
    default: // mute compiler
      break;
    }
}

void
TcpSocketBase::Calculate_Sender_Goodput ()
{ //for sender, it is calculated at the receiver side.
	if (m_received_dataN > 0){
        //PRINT_STUFF ("tcp/dctcp: sender rtt="<< m_rtt->GetCurrentEstimate().GetSeconds()  <<" s, dataN="<<  m_received_dataN   <<" B, s goodput:" <<m_received_dataN * 8.0 / 0.0003 / 1048576 << "Mbps, receiver num:" <<m_receiver_order);
        NS_LOG_DEBUG (" s goodput " <<m_received_dataN * 8.0 / 0.0003 / 1048576 << " Mbps, receiver num:" <<m_receiver_order);
        //NS_LOG_DEBUG (" s goodput " <<m_received_dataN * 8.0 / 0.01 / 1048576 << " Mbps, receiver num:" <<m_receiver_order);
        m_received_dataN = 0;
	}
	//xelse
        //xPRINT_STUFF ("tcp/dctcp: sender rtt="<< m_rtt->GetCurrentEstimate().GetSeconds()  <<" s, dataN="<<  0   <<" B, s goodput:" <<0                                                         << "Mbps, receiver num:" <<m_receiver_order);
    
	m_sender_cal_markEvent = Simulator::Schedule (MicroSeconds(300), &TcpSocketBase::Calculate_Sender_Goodput, this);
	//m_sender_cal_markEvent = Simulator::Schedule (MilliSeconds(10), &TcpSocketBase::Calculate_Sender_Goodput, this); //for convergence test
}

void
TcpSocketBase::Calculate_Receiver_Goodput ()
{ 
	if (m_received_dataN > 0){
        //PRINT_STUFF ("rtcp/rdctcp: receiver rtt="<< m_rtt->GetCurrentEstimate_r().GetSeconds()  
		//		<<" s, dataN=" <<  m_received_dataN   
		//		<<" B, r goodput:" <<m_received_dataN * 8.0 / 0.0003 / 1048576 
		//		<< "Mbps, m_state = "<<m_state << ", receiver num:" <<m_receiver_order);
        NS_LOG_DEBUG (" r goodput " <<m_received_dataN * 8.0 / 0.0003 / 1048576 
        //NS_LOG_DEBUG (" r goodput " <<m_received_dataN * 8.0 / 0.01 / 1048576 
				<< " Mbps, receiver num:" <<m_receiver_order);
        m_received_dataN = 0;
	}
	//xelse
        //xPRINT_STUFF ("rtcp/rdctcp: receiver rtt="<< m_rtt->GetCurrentEstimate_r().GetSeconds()  <<" s, dataN="<<  0   <<" B, r goodput:" <<  0                                                      << "Mbps, m_state = "<<m_state << ", receiver num:" <<m_receiver_order);

    
	if (m_state != CLOSED)
	    m_receiver_cal_markEvent = Simulator::Schedule (MicroSeconds(300), &TcpSocketBase::Calculate_Receiver_Goodput, this);
	    //m_receiver_cal_markEvent = Simulator::Schedule (MilliSeconds(10), &TcpSocketBase::Calculate_Receiver_Goodput, this); //for convergence test
}


void
TcpSocketBase::Calculate_ICTCP_Receiver_Goodput()
{//for single connection.
  NS_LOG_FUNCTION (this);
	if (m_received_dataN > 0){
		double tmp_bs = m_received_dataN * 8.0 / m_receiver_pre_rtt.GetSeconds()  / 1048576 ;
		m_bm = std::max (tmp_bs , 0.9 * m_bm +0.1*tmp_bs ); //Mbps
        //NS_LOG_DEBUG("Calculate_ICTCP_Receiver_Goodput: receiver pre rtt="<< m_receiver_pre_rtt.GetSeconds()  <<" s, dataN="<<  m_received_dataN   <<" B, r goodput:" << tmp_bs << "Mbps,m_bm="<<m_bm <<"Mbps, m_state = "<<m_state << ", receiver num:" <<m_receiver_order<<",order on dev:"<<m_receiver_order_on_dev );
        NS_LOG_DEBUG (" r goodput " << tmp_bs << " Mbps, receiver num:" <<m_receiver_order );
        m_received_dataN = 0;
	}

	if (m_rtt->GetCurrentEstimate_r() > 0 && m_rtt->GetCurrentEstimate_r().GetMilliSeconds() < 2)
	{
	   //PRINT_STUFF ("in slot 2, mod win.num:"<<m_receiver_order);
       m_receive_device->m_dev_BWA = ModifyCwnd_ICTCP (m_receive_device->m_dev_BWA, m_receive_device->m_dev_global_mode  );  //tcp new reno
	   //other connection will use m_dev_BWA, so need record m_dev_BWA change. 
	}
	if (m_state != CLOSED){
	  m_receiver_pre_rtt = m_rtt->GetCurrentEstimate_r();
      if (m_receiver_pre_rtt < Time::FromInteger (160, Time::US))
      //if (m_receiver_pre_rtt < Time::FromInteger (100, Time::MS)) //for convergence test
          m_receiver_pre_rtt = Time::FromInteger (160, Time::US);
      m_ictcp_measure_Event = Simulator::Schedule (MicroSeconds(m_receiver_pre_rtt.GetMicroSeconds()), &TcpSocketBase::Calculate_ICTCP_Receiver_Goodput, this);
	}
}

void TcpSocketBase::ICTCP_UPDATE()
{//for device only.
  NS_LOG_FUNCTION (this);
    if (m_receive_device->m_dev_global_mode == 1)
	{
	    m_receive_device->m_dev_global_mode = 2;
        m_receive_device->SetReceivedDataNum(0);
	}
	else{
	   m_receive_device->m_dev_global_mode = 1;
       uint64_t tmpdev_d = m_receive_device -> GetReceivedDataNum();
       m_receive_device->SetReceivedDataNum(0);
	   double BWT= (double) tmpdev_d * 8.0*1000000 / m_receive_device-> m_dev_global_T.GetMicroSeconds () / 1048576; //Mbps
       m_receive_device->m_dev_BWA = std::max (0.0, 0.9 * 1000 - BWT); //Mbps
       //xPRINT_STUFF ("ICTCP_UPDATE, T="<< m_receive_device->m_dev_global_T<<",dev_d="<< tmpdev_d <<"B,BWT="<< BWT<< "Mbps,BWA="<< m_receive_device->m_dev_BWA  <<"Mbps, m_state="<<m_state);
	   if (m_receive_device->m_dev_BWA < 0.2 * 1000)
	   {//for fairness.
		   ICTCP_share_decrease (m_receive_device->m_dev_socket_cout);
	   }
	}


//update wi
   //PRINT_STUFF ("ictcp update, update wi, m_static_socket_cout="<<m_static_socket_cout);
    uint64_t total_traffic = 0;
	double w[MAX_FLOWS_N];
    for (uint16_t i=0; i < m_receive_device->m_dev_socket_cout; i++){
	    total_traffic += m_receive_device->m_dev_received_coarsedataN[i];
	    //PRINT_STUFF ("ictcp update  m_receive_device->m_dev_received_coarsedataN["<< i<<"]="<< m_receive_device->m_dev_received_coarsedataN[i]<<",m_receive_device->m_dev_RTT_connection["<< i<<"]="<< m_receive_device->m_dev_RTT_connection[i]);
	}
	if (total_traffic > 0)
        for (uint16_t i=0; i<m_receive_device->m_dev_socket_cout ; i++){
    		w[i] = ( double )m_receive_device->m_dev_received_coarsedataN[i] / total_traffic;
	        //NS_LOG_DEBUG ("ictcp update w["<<i <<"]="<<w[i] <<" m_receive_device->m_dev_received_coarsedataN["<< i<<"]="<< m_receive_device->m_dev_received_coarsedataN[i]<<",total_traffic="<< total_traffic);

	}

//update T
	if (total_traffic > 0){
        m_receive_device->m_dev_global_T = Time::FromInteger (0, Time::S);
        for (uint16_t i=0; i < m_receive_device->m_dev_socket_cout; i++)
            m_receive_device->m_dev_global_T += Time::FromDouble (m_receive_device->m_dev_RTT_connection[i].GetNanoSeconds () * w[i], Time::NS);
	}
    if (m_receive_device->m_dev_global_T < Time::FromInteger (160, Time::US))
        m_receive_device->m_dev_global_T = Time::FromInteger (160, Time::US);

	//Time tmp2T= Time::FromDouble (m_receive_device->m_dev_global_T.GetNanoSeconds () * 2, Time::NS);
  
   //global_slot_endtime = Simulator::Now() + m_receive_device->m_dev_global_T;
   //global_secondslot_endtime = Simulator::Now() + tmp2T;
    
    //NS_LOG_DEBUG ("ICTCP_UPDATE, T="<< m_receive_device->m_dev_global_T<<", m_state="<<m_state);
   if (m_receive_device->m_dev_closed_cout < m_receive_device->m_dev_socket_cout )
       m_ictcp_global_update_Event = Simulator::Schedule (m_receive_device->m_dev_global_T, &TcpSocketBase::ICTCP_UPDATE, this);
}

double
TcpSocketBase::ModifyCwnd_ICTCP (double bwa, uint8_t slot_mode)
{
	//in TCP-NewReno
	return bwa;
}

void  TcpSocketBase::ICTCP_share_decrease (uint16_t	connection_count)
{//in tcp new reno
}


void TcpSocketBase::CwndChange_rec (uint32_t oldCwnd, uint32_t newCwnd)
{// by xl on Thu May  9 23:36:12 CST 2013
	  PRINT_STUFF (newCwnd<<"   :newcwnd "<< oldCwnd <<"  :oldcwnd, receiver num:" <<m_receiver_order
			  <<", rec all num: "<<m_all_rec_num
			  <<", total="<<m_receive_device->m_all_connections);
	  //PRINT_STATE ("-_- oldcwnd = "<<oldCwnd<<", newcwnd = " << newCwnd);
	  //PRINT_STATE ("-_- old rec_cwnd = "<<oldCwnd<<", new rec_cwnd = " << newCwnd);
}


void
TcpSocketBase::SetAlpha_DCTCP ()
{//be realized by sub classes, such as TcpNewReno.
  //NS_LOG_DEBUG("SetAlpha_DCTCP, empty funcion. tcpsocketbase ");
}

void
TcpSocketBase::ModifyCwnd_DCTCP ()
{// by xl on Sat Mar  9 21:02:10 CST 2013
  //if (m_ignore_ecn) return;
  //PRINT_STUFF("modifying window by ECN, Call SetAlpha, initial cwnd=" << GetInitialCwnd() ); by xl on Fri May  3 00:16:01 CST 2013
  SetAlpha_DCTCP();
  //Time m_now = Simulator::Now(); del by xl on Thu Mar 14 22:12:10 CST 2013
  //if (m_deadline.GetInteger()) return;
  //if ((m_deadline - m_now).IsStrictlyNegative() && m_state == ESTABLISHED)
  //{
  //  PRINT_STUFF("Deadline2 not met!");
  //  //return;
  //}
  //PRINT_STUFF("ModifyCwnd_DCTCP, rtt=" << m_rtt->GetCurrentEstimate()<<", m_state="<<m_state);
  /*if (m_rtt->GetCurrentEstimate() > Seconds(0.0))
  {
	  PRINT_STUFF ("dctcp: rtt="<< m_rtt->GetCurrentEstimate().GetSeconds()  <<" s, dataN="<<  m_received_dataN   <<" B, goodput:" <<m_received_dataN * 8.0 / m_rtt->GetCurrentEstimate().GetSeconds() / 1048576 << "Mkbps");
	  m_received_dataN = 0;
  }*/

  // reschedule for the next window
  if (m_rtt->GetCurrentEstimate() < MicroSeconds(200)|| m_rtt->GetCurrentEstimate() >= MilliSeconds(10))
  {//NS_LOG_DEBUG("ModifyCwnd after 100us, rtt=" << m_rtt->GetCurrentEstimate()<<"<200us, m_dctcp_mode = "<<m_dctcp_mode);
    m_dctcp_mod_markEvent = Simulator::Schedule (MicroSeconds(200), &TcpSocketBase::ModifyCwnd_DCTCP, this);//Schedule (	Time const & time, MEM mem_ptr, OBJ	obj ); time is relative time from current simulation time.
  }
  else
  {//NS_LOG_DEBUG("ModifyCwnd after rtt=" << m_rtt->GetCurrentEstimate()<<" >=200us, m_dctcp_mode = "<<m_dctcp_mode);
    m_dctcp_mod_markEvent = Simulator::Schedule (m_rtt->GetCurrentEstimate (), &TcpSocketBase::ModifyCwnd_DCTCP, this);
  }
}

void
TcpSocketBase::SetAlpha_RDCTCP()
{//be realized by sub classes, such as TcpNewReno.
  //NS_LOG_DEBUG("SetAlpha_RDCTCP, empty funcion. tcpsocketbase ");
}


void
TcpSocketBase::ModifyCwnd_RDCTCP ()
{// by xl on Sat Mar  9 21:02:10 CST 2013
  //PRINT_STUFF("modifying window by ECN, Call SetAlpha, initial cwnd=" << GetInitialCwnd() ); by xl on Fri May  3 00:16:01 CST 2013
  SetAlpha_RDCTCP();
  //Time m_now = Simulator::Now(); del by xl on Thu Mar 14 22:12:10 CST 2013
  //if (m_deadline.GetInteger()) return;
  //if ((m_deadline - m_now).IsStrictlyNegative() && m_state == ESTABLISHED)
  //{
  //  PRINT_STUFF("Deadline2 not met!");
  //  //return;
  //}
  /*if (m_rtt->GetCurrentEstimate_r() > Seconds(0.0))
  {
	  PRINT_STUFF ("Rdctcp: rtt="<< m_rtt->GetCurrentEstimate_r().GetSeconds()  <<" s, dataN="<<  m_received_dataN   <<" B, goodput:" <<m_received_dataN * 8.0 / m_rtt->GetCurrentEstimate_r().GetSeconds() / 1048576 << "Mkbps");
	  m_received_dataN = 0;
  }*/

      //PRINT_STUFF("ModifyCwnd_RDCTCP rtt_r=" << m_rtt->GetCurrentEstimate_r()<<", m_state="<<m_state);
  //m_rdctcp_mod_markEvent = Simulator::Schedule (MicroSeconds(100), &TcpSocketBase::ModifyCwnd_RDCTCP, this);//Schedule (	Time const & time, MEM mem_ptr, OBJ	obj ); time is relative time from current simulation time. 
  // reschedule for the next window
  //if (m_state != CLOSED){ 
      if (m_rtt->GetCurrentEstimate_r() < MicroSeconds(100)|| m_rtt->GetCurrentEstimate_r() >= MilliSeconds(10))
      {//PRINT_STUFF("xl: ModifyCwnd_RDCTCP after 100us, rtt_r=" << m_rtt->GetCurrentEstimate_r()<<"<200us");
        m_rdctcp_mod_markEvent = Simulator::Schedule (MicroSeconds(100), &TcpSocketBase::ModifyCwnd_RDCTCP, this);//Schedule (	Time const & time, MEM mem_ptr, OBJ	obj ); time is relative time from current simulation time.
      }
      else
      {//xPRINT_STUFF("xl: ModifyCwnd_RDCTCP after rtt_r=" << m_rtt->GetCurrentEstimate_r()<<" >=100s");
        m_rdctcp_mod_markEvent = Simulator::Schedule (m_rtt->GetCurrentEstimate_r (), &TcpSocketBase::ModifyCwnd_RDCTCP, this);
      }
}


void TcpSocketBase::SignalSendEmptyPacket (uint8_t flags)
{
	
  NetDevice::item_t new_item, out_item;
  
  //if (m_node->GetId () == 872)
  NS_LOG_DEBUG ("["<< m_node->GetId () <<"] list size = "<< m_receive_device->m_rdtcp_controller_list.size()
		  <<",m_flight_wins = "<< m_receive_device->m_flight_wins
		  <<", input flags="<<(uint16_t)flags);
  
  /*if (m_receiver_order == 0 && m_node->GetId () == 872)
  for (ReceiverListIterator i=m_receive_device->m_rdtcp_controller_list.begin(); i!=m_receive_device->m_rdtcp_controller_list.end(); ++i)
	{
      ReceiverIterator dev_i = m_receive_device->m_rec_infos.find (i->m_this_ptr);
      if (dev_i !=  m_receive_device->m_rec_infos.end () ) 
          NS_LOG_DEBUG ("before list:[m_this_ptr ("<< std::hex <<  i->m_this_ptr << std::dec
			  <<"),advertised win(" << StaticCast<TcpSocketBase> (i->m_socket_ptr)->AdvertisedWindowSize () / m_segmentSize 
			  <<")], rec order="<< i->m_rec_order  
			  <<",flowsize=" << dev_i->second.m_rec_get_flowsize
			  <<", m_insert_before_times ="<< i->m_insert_before_times 
			  <<", deadline ="<< dev_i->second.m_deadline
			  );
	  else
		   NS_LOG_DEBUG ("before not found in dev rec infos. list:[m_this_ptr ("<< std::hex <<  i->m_this_ptr << std::dec
			  <<"),advertised win(" << StaticCast<TcpSocketBase> (i->m_socket_ptr)->AdvertisedWindowSize () / m_segmentSize 
			  <<")], rec order="<< i->m_rec_order  
			  <<", m_insert_before_times ="<< i->m_insert_before_times 
			  );

	}*/
  new_item.m_socket_ptr = this;
  new_item.m_this_ptr = (uint64_t)this;
  new_item.m_flags = flags;
  new_item.m_insert_before_times = 0;
  new_item.m_rec_order = m_receiver_order;
  if (flags & TcpHeader::PSH) new_item.m_insert_before_times += 150;


  for (ReceiverListIterator i=m_receive_device->m_rdtcp_controller_list.begin(); i!=m_receive_device->m_rdtcp_controller_list.end(); ++i)
	  if ( i->m_this_ptr == (uint64_t)this ) {
          new_item.m_insert_before_times += i->m_insert_before_times; //this will degrade performance of RDTCP, so no use.
		  m_receive_device->m_rdtcp_controller_list.erase (i);
		  break;
	  }


  ReceiverIterator dev_it = m_receive_device->m_rec_infos.find ((uint64_t)this);
  if (dev_it !=  m_receive_device->m_rec_infos.end () && dev_it->second.m_rec_get_flowsize > 0 ) {
	  //the highest priority will be inserted at the head of list.
	  //unless the higher times is 40 or m_rdtcp_queue_maxsize times.
	  int16_t reverse_count = m_receive_device->m_rdtcp_controller_list.size() ;

	  if (reverse_count == 0 || m_rdtcp_no_priority == 1)
          m_receive_device->m_rdtcp_controller_list.push_back(new_item);
	  else { 
		  for ( Reverse_ReceiverListIterator reverse_list_it = m_receive_device->m_rdtcp_controller_list.rbegin(); 
			reverse_list_it != m_receive_device->m_rdtcp_controller_list.rend(); ++reverse_list_it ) {

              ReceiverIterator tmp_dev_it = m_receive_device->m_rec_infos.find (reverse_list_it->m_this_ptr);
              if (tmp_dev_it !=  m_receive_device->m_rec_infos.end () ) 
		        if (  (tmp_dev_it->second.m_deadline > 0 
						&& dev_it->second.m_rec_get_flowsize < tmp_dev_it->second.m_rec_get_flowsize 
						&&  reverse_list_it->m_insert_before_times < 300 
					  )
					|| 
					  ( tmp_dev_it->second.m_deadline == 0 
						&& dev_it->second.m_rec_get_flowsize < tmp_dev_it->second.m_rec_get_flowsize 
						&&  reverse_list_it->m_insert_before_times < 700 
					  )
				  ) {
					//if (tmp_dev_it->second.m_deadline == 0 )
					//NS_LOG_DEBUG ("tmp_dev_it->second.m_deadline="<< tmp_dev_it->second.m_deadline
					//		<<", reverse_list_it->m_rec_order ="<< reverse_list_it->m_rec_order 
					//		<<", reverse_list_it->m_insert_before_times="<<reverse_list_it->m_insert_before_times
					//		);
					reverse_list_it->m_insert_before_times++;
					reverse_count--;
			    }
			    else break;
			  else continue;
	      }

	      if ( reverse_count == (int16_t)m_receive_device->m_rdtcp_controller_list.size() ) {
			 // NS_LOG_DEBUG ("list size ="<< reverse_count <<"; insert tail, new_item.m_this_ptr ="<<new_item.m_this_ptr );
              m_receive_device->m_rdtcp_controller_list.push_back(new_item);
		  }
		  else {
			  ReceiverListIterator list_bg = m_receive_device->m_rdtcp_controller_list.begin() ;
			  for (uint16_t k = 0; k<reverse_count; k++) list_bg++;
			  //NS_LOG_DEBUG ("list size ="<< (int16_t)m_receive_device->m_rdtcp_controller_list.size()
			//		  <<"; insert before this ptr="<< list_bg ->m_this_ptr 
			//		  <<"new_item.m_this_ptr ="<<new_item.m_this_ptr );
              m_receive_device->m_rdtcp_controller_list.insert ( list_bg , new_item );
		  }
	  }
  }
  else //closed or flowsize=0.
    this->SendEmptyPacket (flags);
  
  /*if (m_receiver_order == 0 && m_node->GetId () == 872)
  for (ReceiverListIterator i=m_receive_device->m_rdtcp_controller_list.begin(); i!=m_receive_device->m_rdtcp_controller_list.end(); ++i)
	{
      ReceiverIterator dev_i = m_receive_device->m_rec_infos.find (i->m_this_ptr);
      if (dev_i !=  m_receive_device->m_rec_infos.end () ) 
          NS_LOG_DEBUG ("["<< m_node->GetId ()<<"] list.after.listsize="<<m_receive_device->m_rdtcp_controller_list.size()
			  <<",list:[m_this_ptr ("<< std::hex <<  i->m_this_ptr << std::dec
			  <<"), advertised win(" << StaticCast<TcpSocketBase> (i->m_socket_ptr)->AdvertisedWindowSize () / m_segmentSize 
			  <<")], rec order="<< i->m_rec_order  
			  <<", flowsize=" << dev_i->second.m_rec_get_flowsize
			  <<", m_insert_before_times ="<< i->m_insert_before_times 
			  <<", deadline ="<< dev_i->second.m_deadline
			  );
	  else
		   NS_LOG_DEBUG ("["<< m_node->GetId ()<<"] list.after.listsize="<<m_receive_device->m_rdtcp_controller_list.size()
			  <<",not found in dev rec infos. list:[m_this_ptr ("<< std::hex << i->m_this_ptr << std::dec
			  <<"), advertised win(" << StaticCast<TcpSocketBase> (i->m_socket_ptr)->AdvertisedWindowSize () / m_segmentSize 
			  <<")], rec order="<< i->m_rec_order  
			  <<", m_insert_before_times ="<< i->m_insert_before_times 
			  ); 

	}*/



  while ( (uint32_t)m_receive_device->m_flight_wins < m_rdtcp_queue_maxsize 
		  && !m_receive_device->m_rdtcp_controller_list.empty() ) {
    out_item = m_receive_device->m_rdtcp_controller_list.front();
    m_receive_device->m_rdtcp_controller_list.pop_front();
      
    /*NS_LOG_DEBUG ("output, list size = "<< m_receive_device->m_rdtcp_controller_list.size()
	  <<",m_flight_wins = "<< m_receive_device->m_flight_wins
	  <<", output flags="<<(uint16_t)out_item.m_flags); */

    StaticCast<TcpSocketBase> (out_item.m_socket_ptr)->SendEmptyPacket (out_item.m_flags);
  }


}


Time	
TcpSocketBase::GetDelayTime()
{
  int16_t delay;
   /*if (m_receiver_order == 23) {
	   for (ReceiverIterator i = m_receive_device->m_rec_infos.begin (); i != m_receive_device->m_rec_infos.end (); ++i)
	  NS_LOG_DEBUG ("5. connection count="<< m_receive_device->m_rec_infos.size()
			  << ", i->first(rec ptr)=" << i->first
			  << ", i->second.m_rdtcp_delay_ack_rtts = "<< i->second.m_rdtcp_delay_ack_rtts
			  << ", i->second.m_rec_get_flowsize = "<< i->second.m_rec_get_flowsize 
			  << ", i->second.m_rdtcp_max_win = "<< i->second.m_rdtcp_max_win 
			  << ", i->second.m_block = "<< i->second.m_block 
			  << ", i->second.m_exchange_id=" << (uint16_t)i->second.m_exchange_id
			  );
   }*/

  ReceiverIterator it = m_receive_device->m_rec_infos.find ((uint64_t)this);
  if (it ==  m_receive_device->m_rec_infos.end () ) 
    return Time::FromInteger(0, Time::NS);

  if (m_receive_device->m_rec_infos[ (uint64_t)this ].m_rec_get_flowsize == 0)
    return Time::FromInteger(0, Time::NS);


  if (m_receive_device->m_block_num > 0) {
	if ( m_receive_device->m_rec_infos[ (uint64_t)this ].m_exchange_id == 2 ) {
		//normal win. send at every exchange id.
      if      ( m_receive_device->m_rec_infos[ (uint64_t)this ].m_block > m_receive_device->m_global_block_id )
  	      delay = m_receive_device->m_rec_infos[(uint64_t)this ].m_block 
	          - m_receive_device->m_global_block_id ; // containing current block.
      else if ( m_receive_device->m_rec_infos[(uint64_t)this ].m_block < m_receive_device->m_global_block_id )
		  //the difference between m_block and global block id is surely not larger than m_rdtcp_delay_ack_rtts. 
	      delay = m_receive_device->m_block_num
		      - m_receive_device->m_global_block_id 
		      + m_receive_device->m_rec_infos[(uint64_t)this ].m_block ;
      else {//restric to send 40 wins in one rtt.
        if (m_receive_device->m_sendwin_in_rtt < m_rdtcp_queue_maxsize )
	      delay = 0;
		else 
	      delay = m_receive_device->m_block_num ;
	  }
	}
	else if ( m_receive_device->m_rec_infos[ (uint64_t)this ].m_exchange_id == m_receive_device->m_global_exchange_id ) {
		//lowest win hit the exchange id. need to add m_receive_device->m_block_num.
	  if      ( m_receive_device->m_rec_infos[(uint64_t)this ].m_block > m_receive_device->m_global_block_id )
  	      delay = m_receive_device->m_rec_infos[(uint64_t)this ].m_block 
	          - m_receive_device->m_global_block_id ;
      else if ( m_receive_device->m_rec_infos[(uint64_t)this ].m_block < m_receive_device->m_global_block_id )
		  //never happen, because lowest is last block.
	      delay = m_receive_device->m_block_num
		      - m_receive_device->m_global_block_id 
		      + m_receive_device->m_rec_infos[(uint64_t)this ].m_block 
			  + m_receive_device->m_block_num;
      else {
        if (m_receive_device->m_sendwin_in_rtt < m_rdtcp_queue_maxsize )
	      delay = 0;
		else 
	      delay = m_receive_device->m_block_num
			  + m_receive_device->m_block_num;
	  }
	}
	else //if ( m_receive_device->m_rec_infos[ (uint64_t)this ].m_exchange_id != m_receive_device->m_global_exchange_id ) 
	{ //lowest win is at another exchange id.
	  if      ( m_receive_device->m_rec_infos[(uint64_t)this ].m_block > m_receive_device->m_global_block_id )
  	      delay = m_receive_device->m_rec_infos[(uint64_t)this ].m_block 
	          - m_receive_device->m_global_block_id 
			  + m_receive_device->m_block_num;
      else if ( m_receive_device->m_rec_infos[(uint64_t)this ].m_block < m_receive_device->m_global_block_id )
		  //never happen, because lowest is last block.
	      delay = m_receive_device->m_block_num
		      - m_receive_device->m_global_block_id 
		      + m_receive_device->m_rec_infos[(uint64_t)this ].m_block ;
      else
	      delay = m_receive_device->m_block_num;
	}
  }
  else {//m_receive_device->m_block_num == 0
    if (m_receive_device->m_sendwin_in_rtt < m_rdtcp_queue_maxsize )
	  delay = 0;
	else
	  delay = 1;
  }

	   

  //if (delay < 0 ) delay += m_receive_device->m_block_num; 
  NS_LOG_DEBUG ("block id="<< m_receive_device->m_rec_infos[(uint64_t)this ].m_block 
		  <<", global block id="<< m_receive_device->m_global_block_id 
		  <<", exchage id="<< (uint16_t)m_receive_device->m_rec_infos[ (uint64_t)this ].m_exchange_id
		  <<", global exchage id="<< (uint16_t)m_receive_device->m_global_exchange_id 
		  <<", m_rdtcp_delay_ack_rtts="<< m_receive_device->m_rec_infos[(uint64_t)this ].m_rdtcp_delay_ack_rtts 
		  <<", delay="<< delay <<"* global rtt="<< m_receive_device->m_global_rtt.GetNanoSeconds()
		  <<", result delaytime=" 
		  <<Time::FromInteger(delay * m_receive_device->m_global_rtt.GetNanoSeconds(), Time::NS) + Simulator::Now ()
		  << ", m_receive_device->m_sendwin_in_rtt="<<m_receive_device->m_sendwin_in_rtt
		  );

  return Time::FromInteger(delay * m_receive_device->m_global_rtt.GetNanoSeconds(), Time::NS);

}

void
TcpSocketBase::Update_rdtcp_GlobalRtt ()
{
	//double sum_of_flow_Reciprocal = 0.0;
	uint64_t sum_of_flowsize = 0;
	double weight = 0.0;
	Time largestRtt = Time::FromInteger (0, Time::S);
	Time newrtt = Time::FromInteger (0, Time::S);
	
  if (m_receive_device->m_global_rtt == Time::FromInteger (0, Time::S)) {
	m_receive_device->m_global_rtt = Time::FromInteger (500, Time::US); //4*20us + 1500*40*8/1G s = 80+480us =560us
	m_receive_device->m_global_block_id = 0;
	m_receive_device->m_global_exchange_id = 0;
  }
  else {
    //m_receive_device->m_global_rtt = Time::FromInteger (0, Time::S);
    newrtt = Time::FromInteger (0, Time::S);
    for (ReceiverIterator i = m_receive_device->m_rec_infos.begin (); i != m_receive_device->m_rec_infos.end (); ++i)
	{
	  if (i->second.m_rec_get_flowsize == 0 || i->second.m_connection_rtt == Time::FromInteger (0, Time::S))
		  continue;

	  //sum_of_flow_Reciprocal += i->second.m_rec_get_flowsize_Reciprocal ;
	  sum_of_flowsize += i->second.m_rec_get_flowsize ;
	}
    for (ReceiverIterator i = m_receive_device->m_rec_infos.begin (); i != m_receive_device->m_rec_infos.end (); ++i) {
	  if (i->second.m_rec_get_flowsize == 0 || i->second.m_connection_rtt == Time::FromInteger (0, Time::S))
		  continue;
	  if (largestRtt < i->second.m_connection_rtt)
		  largestRtt = i->second.m_connection_rtt ;
	  //get every weight
      //weight = (double) i->second.m_rec_get_flowsize_Reciprocal / sum_of_flow_Reciprocal;
      weight = (double) i->second.m_rec_get_flowsize / sum_of_flowsize ;
	  newrtt += Time::FromDouble ((double)i->second.m_connection_rtt.GetNanoSeconds () * weight, Time::NS);
      //NS_LOG_DEBUG ("i order ="<< i->first << ", weight="<<weight <<", rtt="<< i->second.m_connection_rtt.GetMicroSeconds ()
	//		  <<"us, flowsize="<< i->second.m_rec_get_flowsize <<", newrtt = "<< newrtt );
    }

    //m_receive_device->m_global_rtt = newrtt ;
    //m_receive_device->m_global_rtt = largestRtt ;
    m_receive_device->m_global_rtt = Time::FromInteger (500, Time::US); //4*20us + 1500*40*8/1G s = 80+480us =560us
    //m_receive_device->m_global_rtt 
	 //  = Time::FromDouble ((double)m_receive_device->m_global_rtt.GetNanoSeconds () * 0.9 + (double)newrtt.GetNanoSeconds () * 0.1, 
	//		   Time::NS);
    m_receive_device->m_global_block_id++;
    if ( m_receive_device->m_block_num > 0 )  {
		if ( m_receive_device->m_global_block_id >= m_receive_device->m_block_num ){
		    m_receive_device->m_global_block_id %= m_receive_device->m_block_num ;
		    m_receive_device->m_global_exchange_id = !m_receive_device->m_global_exchange_id ;
	    }
	}
    else m_receive_device->m_global_block_id = 0;

  }
	
	  
  m_receive_device->m_sendwin_in_rtt = 0;
  NS_LOG_DEBUG ("last block id="<< m_receive_device->m_global_block_id 
		   <<", m_rdtcp_closed_count= " << m_receive_device->m_rdtcp_closed_count
		   <<", m_receive_device->m_dev_socket_cout ="<<m_receive_device->m_dev_socket_cout 
		   <<", m_receive_device->m_block_num="<<m_receive_device->m_block_num
		   <<", m_receive_device->m_global_rtt = "<< m_receive_device->m_global_rtt
		   <<", largestRtt =" <<largestRtt
		   );
   if (m_receive_device->m_rdtcp_closed_count < m_receive_device->m_dev_socket_cout)
       m_update_rdtcp_rtt_Event = Simulator::Schedule (m_receive_device->m_global_rtt , &TcpSocketBase::Update_rdtcp_GlobalRtt, this);
}

void
TcpSocketBase::Update_rdtcp_MaxWin_RttDelay ()
{
  NS_LOG_FUNCTION (this);

	double sum_of_flow_Reciprocal = 0.0;
	double weight = 0.0;
	double max_win = 0.0;
	uint32_t sum_of_win;

	struct weight_ascending_t{
	    uint64_t rec_ptr;
	    double weight ;
	}; 
	struct  weight_ascending_t weight_ascending_array [MAX_FLOWS_N] ;
	int16_t left, right, middle;
	uint16_t number_of_connections; 
	uint16_t invalid_num = 0; 


  //NS_LOG_DEBUG ("0. info size="<< m_receive_device->m_rec_infos.size()<< ". ");
  for (ReceiverIterator i = m_receive_device->m_rec_infos.begin (); i != m_receive_device->m_rec_infos.end (); ++i) {
	  sum_of_flow_Reciprocal += i->second.m_rec_get_flowsize_Reciprocal ;
	  if (i->second.m_rec_get_flowsize == 0) invalid_num ++;
  }
 // NS_LOG_DEBUG ("0.1 . info size="<< m_receive_device->m_rec_infos.size()<< ". ");


  sum_of_win = 0;  number_of_connections = 0;
  for (ReceiverIterator i = m_receive_device->m_rec_infos.begin (); i != m_receive_device->m_rec_infos.end (); ++i) {
	  if (i->second.m_rec_get_flowsize == 0) {
		  i->second.m_rdtcp_max_win = 0;
		  i->second.m_rdtcp_delay_ack_rtts = 0;
		  i->second.m_rdtcp_delay_ack_rtts = 0;
		  i->second.m_exchange_id = 0;
		  //NS_LOG_DEBUG ("--------1. info size="<< m_receive_device->m_rec_infos.size() 
		      //<< ", sum of reciprocal="<< sum_of_flow_Reciprocal
			  //<< ", iterator rec order="<<i->first <<", weight=0"
			  //<< ", flowsize="<< i->second.m_rec_get_flowsize << ", reciprocal="
			  //<< i->second.m_rec_get_flowsize_Reciprocal << "!!!skip!!!");
		  continue;
	  }
	  //get every weight
	  //the smaller the weight is, the less important the win is, and the larger the flow size, and the header it is insert in the array.
      weight = (double) i->second.m_rec_get_flowsize_Reciprocal / sum_of_flow_Reciprocal;
	  if (m_rdtcp_no_priority == 1) {
		  if ( m_receive_device->m_rec_infos.size() > invalid_num )
		      weight = (double) 1 / ( m_receive_device->m_rec_infos.size() - invalid_num );
		  else
			  weight = 1.0;
	  }
	  //NS_LOG_DEBUG ("--------1. info size="<< m_receive_device->m_rec_infos.size() << ", sum of reciprocal="<< sum_of_flow_Reciprocal
			  //<< ", iterator rec order="<<i->first <<", weight="<< weight 
			  //<< ", flowsize="<< i->second.m_rec_get_flowsize << ", reciprocal="
			  //<< i->second.m_rec_get_flowsize_Reciprocal << ".");
	  //insert to weight_ascending_array , binary insert sort. //--insert start--
	  if (number_of_connections == 0 ) {
		  weight_ascending_array [0].rec_ptr = i-> first;
		  weight_ascending_array [0].weight = weight;
		  number_of_connections = 1; left = 0; right = 0;
	  }
	  else {//number_of_connections > 0	  
		  while (left <= right) {
			  //smaller or equal weight is inserted at the header of array.
		    middle = (left + right) / 2; 
		    if (weight <= weight_ascending_array [ middle ].weight )
			  right = middle - 1;
		    else left = middle + 1;
	      }
	      //NS_LOG_DEBUG ("2. left="<< left<<", right="<<right<<", middle="<<middle
				  //<< ", info size="<< m_receive_device->m_rec_infos.size()
				  //<< ", current connections= "<< number_of_connections );
	      for (int16_t k = number_of_connections - 1; k >= left; k--) weight_ascending_array [ k+1 ] = weight_ascending_array [ k ];
	      weight_ascending_array [left].rec_ptr = i-> first;
	      weight_ascending_array [left].weight = weight;
	      number_of_connections++; left = 0; right = number_of_connections - 1;
	  }
	  //--insert end--

	  //get max win, note that the win of each connection may be larger than 1.
	  max_win  = (double)weight * m_rdtcp_queue_maxsize ;
	  if (max_win > 1.0) i->second.m_rdtcp_max_win = max_win;
	  else i->second.m_rdtcp_max_win = 1;
	  if (i->second.m_rdtcp_max_win * m_segmentSize > i->second.m_rec_get_flowsize )
          i->second.m_rdtcp_max_win = ceil ((double)i->second.m_rec_get_flowsize / m_segmentSize);

      sum_of_win += i->second.m_rdtcp_max_win;
  }

  /*for (uint16_t kk=0; kk < number_of_connections; kk++)
	  NS_LOG_DEBUG ("weight_ascending_array: ["<<kk 
			  <<"]: ptr="<< std::hex << weight_ascending_array [kk].rec_ptr << std::dec 
			  <<", weight="<<weight_ascending_array [kk].weight
			  <<", flowsize=" <<m_receive_device->m_rec_infos [ weight_ascending_array [kk].rec_ptr ].m_rec_get_flowsize 
			  <<", m_rdtcp_max_win=" <<m_receive_device->m_rec_infos [ weight_ascending_array [kk].rec_ptr ].m_rdtcp_max_win
			  );
			  */

  int16_t l = number_of_connections - 1; 
  uint16_t number_of_cover_flowsize = 0;
  while ( sum_of_win < m_rdtcp_queue_maxsize ) {
	  //1. occur when number_of_connections < m_rdtcp_queue_maxsize 
	  //e.g. 21 connections < 40
	  //2. occur when large flows and short flows mix
	  //e.g. 30M flow and 2k flow
	  uint64_t flowmap_i = weight_ascending_array [ l ].rec_ptr ;
	  if ( m_receive_device->m_rec_infos [ flowmap_i ].m_rdtcp_max_win * m_segmentSize 
			  - m_receive_device->m_rec_infos [ flowmap_i ].m_rec_get_flowsize 
			  < m_segmentSize ) {
		  if ( ++number_of_cover_flowsize >= number_of_connections )
			  break; // all the flow has a win covering its flowsize, no need allocating.
	  }
	  else {
		  m_receive_device->m_rec_infos [ flowmap_i ].m_rdtcp_max_win++; 
	      sum_of_win++;
	  }
	   
	  if (--l == -1) {
		  number_of_cover_flowsize = 0;
          l = number_of_connections - 1; 
	  }
  }

  /*if (m_node->GetId () == 872)
	  for (uint16_t k = 0; k < number_of_connections; k++) 
		  NS_LOG_DEBUG ("3.weight array, number_of_connections="<<number_of_connections 
				  << ", info size="<< m_receive_device->m_rec_infos.size()
				  <<", k="<<k<<", order=" <<  std::hex <<  weight_ascending_array [ k ].rec_ptr  << std::dec
				  <<", weight=" << weight_ascending_array [ k ].weight
			      <<", flowsize=" <<m_receive_device->m_rec_infos [ weight_ascending_array [k].rec_ptr ].m_rec_get_flowsize 
			      <<", m_rdtcp_max_win=" <<m_receive_device->m_rec_infos [ weight_ascending_array [k].rec_ptr ].m_rdtcp_max_win
				  << ". "); */
  
//	  NS_LOG_DEBUG ("4. sum_of_win ="<<sum_of_win<< ", info size="<< m_receive_device->m_rec_infos.size()<< ". ");
  //calculate rtt delay times for every connections.
  uint16_t numberof_lowestWeight_in_win = sum_of_win % m_rdtcp_queue_maxsize ;
  uint16_t lowestWin_delay_times = ( sum_of_win > m_rdtcp_queue_maxsize ) ? 
	  ( sum_of_win - numberof_lowestWeight_in_win ) / m_rdtcp_queue_maxsize : 0;
  uint16_t other_delay_times = ( sum_of_win > m_rdtcp_queue_maxsize ) ? lowestWin_delay_times - 1 : 0;
  //uint16_t delay_rtt_wins [MAX_FLOWS_N];
	 // NS_LOG_DEBUG ("4.1. numberof_lowestWeight_in_win =" << numberof_lowestWeight_in_win 
		//	  << ", lowestWin_delay_times =" <<lowestWin_delay_times 
			//  << ", other_delay_times ="<<other_delay_times );

  // allocate rtt delay times for each win.
  m_receive_device->m_block_num = lowestWin_delay_times;
  if ( m_receive_device->m_global_block_id >= m_receive_device->m_block_num ) {
	  //this is possible when many sockets close simultaneously.
      if ( m_receive_device->m_block_num > 0 )
		    m_receive_device->m_global_block_id %= m_receive_device->m_block_num ;
      else 
		    m_receive_device->m_global_block_id = 0 ;
  }
  /*	  uint16_t m; 
  for ( m = 0; m < sum_of_win - numberof_lowestWeight_in_win; m++)
	  delay_rtt_wins [m] = other_delay_times ;
  for ( ; m < sum_of_win ; m++) 
	  delay_rtt_wins [m] = other_delay_times + lowestWin_delay_times;
  */

  uint16_t win_index = 0, block = 0 , blockwin=0; 
  //the more import win is arranged latter in the array. Note that array has no win whose flowsize is 0.
  for (int16_t k = number_of_connections - 1; k >= 0; k--)  {
	  //uint16_t k = number_of_connections - 1 - l;
	  //NS_LOG_DEBUG ("4.2 connection k="<<k <<",weight_ascending_array [ k ].rec_ptr="
	//		  << weight_ascending_array [ k ].rec_ptr <<", weight_ascending_array [ k ].weight="
	//		  << weight_ascending_array [ k ].weight 
	//		  << ", m_receive_device->m_rec_infos[ weight_ascending_array [ k ].rec_ptr ].m_rdtcp_delay_ack_rtts="
	//		  << m_receive_device->m_rec_infos[ weight_ascending_array [ k ].rec_ptr ].m_rdtcp_delay_ack_rtts
	//		  << ", m_receive_device->m_rec_infos[ weight_ascending_array [ k ].rec_ptr ].m_rdtcp_max_win="
	//		  << m_receive_device->m_rec_infos[ weight_ascending_array [ k ].rec_ptr ].m_rdtcp_max_win
	//		  << ", win_index="<<win_index);
	  m_receive_device->m_rec_infos[ weight_ascending_array [ k ].rec_ptr ].m_rdtcp_delay_ack_rtts =
		  other_delay_times ;
		  //delay_rtt_wins [ win_index ];
	  m_receive_device->m_rec_infos[ weight_ascending_array [ k ].rec_ptr ].m_block = block;
	  if      ( win_index < sum_of_win - numberof_lowestWeight_in_win * 2 )
		  m_receive_device->m_rec_infos[ weight_ascending_array [ k ].rec_ptr ].m_exchange_id = 2;
	  else if ( win_index < sum_of_win - numberof_lowestWeight_in_win )
		  m_receive_device->m_rec_infos[ weight_ascending_array [ k ].rec_ptr ].m_exchange_id = 0;
	  else
		  m_receive_device->m_rec_infos[ weight_ascending_array [ k ].rec_ptr ].m_exchange_id = 1;
	  //NS_LOG_DEBUG ("4.3, m_receive_device->m_rec_infos[ weight_ascending_array [ k ].rec_ptr ].m_rdtcp_delay_ack_rtts ="
	//		  << m_receive_device->m_rec_infos[ weight_ascending_array [ k ].rec_ptr ].m_rdtcp_delay_ack_rtts );
	  win_index += m_receive_device->m_rec_infos[ weight_ascending_array [ k ].rec_ptr ].m_rdtcp_max_win ;
	  blockwin +=  m_receive_device->m_rec_infos[ weight_ascending_array [ k ].rec_ptr ].m_rdtcp_max_win ;
	  if (blockwin >= m_rdtcp_queue_maxsize &&  win_index < sum_of_win - numberof_lowestWeight_in_win  ) { 
		  block++; blockwin = 0; 
	  } //m_rdtcp_queue_maxsize = 40;
	  //NS_LOG_DEBUG ("4.4, order="<< weight_ascending_array [ k ].rec_ptr <<", k="<<k 
	//		  <<", win_index ="<< win_index<<", queuemax="<< m_rdtcp_queue_maxsize<<", block="<< block
	//		  <<", flowsize="<< m_receive_device->m_rec_infos[ weight_ascending_array [ k ].rec_ptr ].m_rec_get_flowsize
	//		  <<"." );
  }


  /*for (ReceiverIterator i = m_receive_device->m_rec_infos.begin (); i != m_receive_device->m_rec_infos.end (); ++i)
	  NS_LOG_DEBUG ("5. connection count="<< m_receive_device->m_rec_infos.size()
			  << ", i->first(rec ptr)=" << std::hex << i->first << std::dec 
			  //<< ", i->second.m_rdtcp_delay_ack_rtts = "<< i->second.m_rdtcp_delay_ack_rtts
			  << ", i->second.m_rec_get_flowsize = "<< i->second.m_rec_get_flowsize 
			  << ", i->second.m_rdtcp_max_win = "<< i->second.m_rdtcp_max_win 
			  //<< ", i->second.m_block = "<< i->second.m_block 
			  //<< ", i->second.m_exchange_id = "<< (uint16_t)i->second.m_exchange_id 
			  << "    "
			  );
			  */


  NS_LOG_DEBUG ("6. update max win.rec num:"<< m_receiver_order 
		  //<<", weight="<<weight <<", max_win ="
		  //<< (double)weight * m_rdtcp_queue_maxsize 
		  << ",maxqueue="<< m_rdtcp_queue_maxsize
		  << ", sum reciprocal=" << sum_of_flow_Reciprocal 
		  << ", m_receive_device->m_block_num = "<<m_receive_device->m_block_num 
		  << ", info size="<< m_receive_device->m_rec_infos.size() );
}

/** Received a packet upon ESTABLISHED state. This function is mimicking the
    role of tcp_rcv_established() in tcp_input.c in Linux kernel. */
void
TcpSocketBase::ProcessEstablished (Ptr<Packet> packet, const TcpHeader& tcpHeader, Ipv4Header::EcnType ipv4ecn)
{//add ipv4 ecn for receiver. xl
  NS_LOG_FUNCTION (this << tcpHeader);
  if (!m_receive_device)
      NS_LOG_DEBUG ("m_first_ack_on_established ="<<m_first_ack_on_established <<", m_dctcp_mode ="<<m_dctcp_mode );

  // Extract the flags. PSH and URG are not honoured.
  uint8_t tcpflags = tcpHeader.GetFlags () & ~(TcpHeader::PSH | TcpHeader::URG);

  // Different flags are different events
  if (tcpflags == TcpHeader::ACK  || tcpflags == (TcpHeader::ACK|TcpHeader::ECE))
    {
	  ReceivedAck (packet, tcpHeader, ipv4ecn);
      if (m_first_ack_on_established == true && m_dctcp_mode == 1) // by xl on Sun Mar 10 17:26:16 CST 2013
      {
        //xPRINT_STUFF("First ACK upon ESTABLISHED! Call ModifyCwnd() and SetAlpha()!");
        Simulator::ScheduleNow (&TcpSocketBase::ModifyCwnd_DCTCP,this);
        m_first_ack_on_established = false;
      }

    }
  else if (tcpflags == TcpHeader::SYN)
    { // Received SYN, old NS-3 behaviour is to set state to SYN_RCVD and
      // respond with a SYN+ACK. But it is not a legal state transition as of
      // RFC793. Thus this is ignored.
    }
  else if (tcpflags == (TcpHeader::SYN | TcpHeader::ACK))
    { // No action for received SYN+ACK, it is probably a duplicated packet
    }
  else if (tcpflags == TcpHeader::FIN || tcpflags == (TcpHeader::FIN | TcpHeader::ACK))
    { // Received FIN or FIN+ACK, bring down this socket nicely

      PeerClose (packet, tcpHeader);
    }
  else if (tcpflags == 0)
    { // No flags means there is only data
      ReceivedData (packet, tcpHeader, ipv4ecn);
      if (m_rxBuffer.Finished ())
        {
          PeerClose (packet, tcpHeader);
        }
    }
  else
    { // Received RST or the TCP flags is invalid, in either case, terminate this socket
      if (tcpflags != TcpHeader::RST)
        { // this must be an invalid flag, send reset
          NS_LOG_LOGIC ("Illegal flag " << tcpflags << " received. Reset packet is sent.");
          SendRST ();
        }
      CloseAndNotify ();
    }
}

/** Process the newly received ACK */
void
TcpSocketBase::ReceivedAck (Ptr<Packet> packet, const TcpHeader& tcpHeader, Ipv4Header::EcnType ipv4ecn)
{
  NS_LOG_FUNCTION (this << tcpHeader);

  // Received ACK. Compare the ACK number against highest unacked seqno
  if (0 == (tcpHeader.GetFlags () & TcpHeader::ACK))
    { // Ignore if no ACK flag
    }
  else if (tcpHeader.GetAckNumber () < m_txBuffer.HeadSequence ())
    { // Case 1: Old ACK, ignored.
      NS_LOG_LOGIC ("Ignored ack of " << tcpHeader.GetAckNumber ());
    }
  else if (m_rtcp_mode && (tcpHeader.GetFlags () & TcpHeader::PSH)) //add by xl, for rtcp retransmit_r , PSH tcp header.
  {
      if (tcpHeader.GetAckNumber () < m_nextTxSequence && packet->GetSize() == 0) {
          DupAck (tcpHeader, 3);
	  }
  }
  else if (tcpHeader.GetAckNumber () == m_txBuffer.HeadSequence ())
    { // Case 2: Potentially a duplicated ACK
      if (tcpHeader.GetAckNumber () < m_nextTxSequence && packet->GetSize() == 0)
        {
          NS_LOG_LOGIC ("Dupack of " << tcpHeader.GetAckNumber ());
          DupAck (tcpHeader, ++m_dupAckCount);
        }
      // otherwise, the ACK is precisely equal to the nextTxSequence
      NS_ASSERT (tcpHeader.GetAckNumber () <= m_nextTxSequence);
    }
  else if (tcpHeader.GetAckNumber () > m_txBuffer.HeadSequence ())
    { // Case 3: New ACK, reset m_dupAckCount and update m_txBuffer
      NS_LOG_LOGIC ("New ack of " << tcpHeader.GetAckNumber ());
      //NS_LOG_DEBUG ("New ack of "<< tcpHeader.GetAckNumber () <<", its flags:" 
	//		  << std::hex << static_cast<uint32_t> (tcpHeader.GetFlags ()) << std::dec <<".");
     if (m_dctcp_mode && tcpHeader.GetFlags () & TcpHeader::ECE )
       NewAckECN (tcpHeader.GetAckNumber ());
	 else
       NewAck (tcpHeader.GetAckNumber ());
      m_dupAckCount = 0;
    }
  // If there is any data piggybacked, store it into m_rxBuffer
  if (packet->GetSize () > 0)
    {
      ReceivedData (packet, tcpHeader, ipv4ecn);
	  if (m_ictcp_mode){
		  m_receive_device->m_dev_received_coarsedataN[m_receiver_order_on_dev] += packet->GetSize() + 42 + 4 + 4+4;
		  m_receive_device->m_dev_RTT_connection[m_receiver_order_on_dev] = m_rtt->GetCurrentEstimate_r(); 
		  //PRINT_STUFF ("ProcessEstablished, ictcp add m_dev_received_coarsedataN["<< m_receiver_order<<"]="<< m_receive_device->m_dev_received_coarsedataN[m_receiver_order]<<",m_receive_device->m_dev_RTT_connection["<< m_receiver_order<<"]="<< m_receive_device->m_dev_RTT_connection[m_receiver_order]);
	  }
      if (m_rdtcp_mode == 1 && m_rdtcp_controller_mode == 1 )
		  m_receive_device->m_rec_infos[(uint64_t)this ].m_connection_rtt = m_rtt->GetCurrentEstimate_r();

    }
}

/** Received a packet upon LISTEN state. */
void
TcpSocketBase::ProcessListen (Ptr<Packet> packet, const TcpHeader& tcpHeader,
                              const Address& fromAddress, const Address& toAddress)
{
  NS_LOG_FUNCTION (this << tcpHeader);

  // Extract the flags. PSH and URG are not honoured.
  uint8_t tcpflags = tcpHeader.GetFlags () & ~(TcpHeader::PSH | TcpHeader::URG);

  // Fork a socket if received a SYN. Do nothing otherwise.
  // C.f.: the LISTEN part in tcp_v4_do_rcv() in tcp_ipv4.c in Linux kernel
  if (tcpflags != TcpHeader::SYN)
    {
      return;
    }

  // Call socket's notify function to let the server app know we got a SYN
  // If the server app refuses the connection, do nothing
  if (!NotifyConnectionRequest (fromAddress))
    {
      return;
    }
  // Clone the socket, simulate fork
  Ptr<TcpSocketBase> newSock = Fork ();
  NS_LOG_LOGIC ("Cloned a TcpSocketBase " << newSock);
  
  newSock->m_all_rec_num = m_receive_device->m_all_connections;
  m_receive_device->m_all_connections++;
  if (m_ictcp_mode == 1 || m_rdtcp_mode == 1)
	  m_receive_device->m_dev_socket_cout++;
  if (m_rdtcp_mode == 1  && m_rdtcp_controller_mode == 1 && m_receiver_order == 0)
  {
	  //Update_rdtcp_GlobalRtt ();
  }

  

  m_receiver_order++;
  newSock->TraceConnectWithoutContext ("ReceiverWindow", MakeCallback (&TcpSocketBase::CwndChange_rec, Ptr<TcpSocketBase>(newSock) )); //xl

  //if ( m_receive_device->m_sendwin_in_rtt < m_rdtcp_queue_maxsize ){
  /*uint64_t this_addr=(uint64_t )this;
	  NS_LOG_DEBUG ("received syn, order:"<< m_receiver_order - 1
			  <<", m_receive_device->m_sendwin_in_rtt="<< m_receive_device->m_sendwin_in_rtt 
			  <<" < " << m_rdtcp_queue_maxsize <<", m_receive_device->m_global_rtt="<< m_receive_device->m_global_rtt
			  <<", (uint64_t )this="<<this_addr
			  <<", this="<<this);*/
      Simulator::ScheduleNow (&TcpSocketBase::CompleteFork, newSock,
                          packet, tcpHeader, fromAddress, toAddress);
  //}
  /*else { //if ( m_receive_device->m_sendwin_in_rtt < 2 * m_rdtcp_queue_maxsize )
	  NS_LOG_DEBUG ("received syn, order:"<< m_receiver_order - 1
			  <<", m_receive_device->m_sendwin_in_rtt="<< m_receive_device->m_sendwin_in_rtt <<" >= "
			  << m_rdtcp_queue_maxsize <<", m_receive_device->m_global_rtt="<< m_receive_device->m_global_rtt<<".");
      Simulator::Schedule ( m_receive_device->m_global_rtt, &TcpSocketBase::CompleteFork, newSock,
                          packet, tcpHeader, fromAddress, toAddress);
  }*/
}

/** Received a packet upon SYN_SENT */
void
TcpSocketBase::ProcessSynSent (Ptr<Packet> packet, const TcpHeader& tcpHeader)
{
  NS_LOG_FUNCTION (this << tcpHeader);

  // Extract the flags. PSH and URG are not honoured.
  uint8_t tcpflags = tcpHeader.GetFlags () & ~(TcpHeader::PSH | TcpHeader::URG);

  if (tcpflags == 0)
    { // Bare data, accept it and move to ESTABLISHED state. This is not a normal behaviour. Remove this?
      NS_LOG_INFO ("SYN_SENT -> ESTABLISHED");
      m_state = ESTABLISHED;
      m_connected = true;
      m_retxEvent.Cancel ();
      m_delAckCount = m_delAckMaxCount;
      ReceivedData (packet, tcpHeader ,Ipv4Header::ECN_NotECT);
      Simulator::ScheduleNow (&TcpSocketBase::ConnectionSucceeded, this);
    }
  else if (tcpflags == TcpHeader::ACK)
    { // Ignore ACK in SYN_SENT
    }
  else if (tcpflags == TcpHeader::SYN)
    { // Received SYN, move to SYN_RCVD state and respond with SYN+ACK
      NS_LOG_INFO ("SYN_SENT -> SYN_RCVD");
      m_state = SYN_RCVD;
      m_cnCount = m_cnRetries;
      m_rxBuffer.SetNextRxSequence (tcpHeader.GetSequenceNumber () + SequenceNumber32 (1));
      SendEmptyPacket (TcpHeader::SYN | TcpHeader::ACK);
    }
  else if (tcpflags == (TcpHeader::SYN | TcpHeader::ACK)
           && m_nextTxSequence + SequenceNumber32 (1) == tcpHeader.GetAckNumber ())
    { // Handshake completed
      NS_LOG_INFO ("SYN_SENT -> ESTABLISHED");
      m_state = ESTABLISHED;
      m_connected = true;
      m_retxEvent.Cancel ();
      m_rxBuffer.SetNextRxSequence (tcpHeader.GetSequenceNumber () + SequenceNumber32 (1));
      m_highTxMark = ++m_nextTxSequence;
      m_txBuffer.SetHeadSequence (m_nextTxSequence);
      SendEmptyPacket (TcpHeader::ACK);
      SendPendingData (m_connected);
      Simulator::ScheduleNow (&TcpSocketBase::ConnectionSucceeded, this);
      // Always respond to first data packet to speed up the connection.
      // Remove to get the behaviour of old NS-3 code.
      m_delAckCount = m_delAckMaxCount;
    }
  else
    { // Other in-sequence input
      if (tcpflags != TcpHeader::RST)
        { // When (1) rx of FIN+ACK; (2) rx of FIN; (3) rx of bad flags
          NS_LOG_LOGIC ("Illegal flag " << std::hex << static_cast<uint32_t> (tcpflags) << std::dec << " received. Reset packet is sent.");
          SendRST ();
        }
      CloseAndNotify ();
    }
}

/** Received a packet upon SYN_RCVD */
void
TcpSocketBase::ProcessSynRcvd (Ptr<Packet> packet, const TcpHeader& tcpHeader,
                               const Address& fromAddress, const Address& toAddress)
{
  NS_LOG_FUNCTION (this << tcpHeader);

  // Extract the flags. PSH and URG are not honoured.
  uint8_t tcpflags = tcpHeader.GetFlags () & ~(TcpHeader::PSH | TcpHeader::URG);

  if (tcpflags == 0
      || (tcpflags == TcpHeader::ACK
          && m_nextTxSequence + SequenceNumber32 (1) == tcpHeader.GetAckNumber ()))
    { // If it is bare data, accept it and move to ESTABLISHED state. This is
      // possibly due to ACK lost in 3WHS. If in-sequence ACK is received, the
      // handshake is completed nicely.
      NS_LOG_INFO ("SYN_RCVD -> ESTABLISHED");
      m_state = ESTABLISHED;
      m_connected = true;
      m_retxEvent.Cancel ();
      m_highTxMark = ++m_nextTxSequence;
      m_txBuffer.SetHeadSequence (m_nextTxSequence);
	  m_receiver_send_ack_times = 0;  //clear syn+ack tiem.
      if (m_endPoint)
        {
          m_endPoint->SetPeer (InetSocketAddress::ConvertFrom (fromAddress).GetIpv4 (),
                               InetSocketAddress::ConvertFrom (fromAddress).GetPort ());
        }
      else if (m_endPoint6)
        {
          m_endPoint6->SetPeer (Inet6SocketAddress::ConvertFrom (fromAddress).GetIpv6 (),
                                Inet6SocketAddress::ConvertFrom (fromAddress).GetPort ());
        }
      // Always respond to first data packet to speed up the connection.
      // Remove to get the behaviour of old NS-3 code.
      m_delAckCount = m_delAckMaxCount;
      ReceivedAck (packet, tcpHeader, Ipv4Header::ECN_NotECT);
      NotifyNewConnectionCreated (this, fromAddress);
      // As this connection is established, the socket is available to send data now
      if (GetTxAvailable () > 0)
        {
          NotifySend (GetTxAvailable ());
        }

	  if (m_ictcp_mode){
		  m_receive_device->m_dev_received_coarsedataN[m_receiver_order_on_dev ] += packet->GetSize() + 42 + 4+4+4;
		  m_receive_device->m_dev_RTT_connection[m_receiver_order_on_dev ] = m_rtt->GetCurrentEstimate_r(); 
		  //PRINT_STUFF ("ProcessSynRcvd, ictcp add m_receive_device->m_dev_received_coarsedataN["<< m_receiver_order<<"]="<< m_receive_device->m_dev_received_coarsedataN[m_receiver_order]<<",m_receive_device->m_dev_RTT_connection["<< m_receiver_order<<"]="<< m_receive_device->m_dev_RTT_connection[m_receiver_order]);
		  if (m_firstStart_cal_ictcp){//just for every connection once
			  m_firstStart_cal_ictcp = false;
              m_ictcp_measure_Event.Cancel (); 
			  m_receiver_pre_rtt = m_rtt->GetCurrentEstimate_r();
			  m_receiver_last_mod_time = Time::FromInteger(0, Time::S);

			  NS_LOG_DEBUG ("ProcessSynRcvd, first start cal goodput, rtt="<<m_receiver_pre_rtt);
              m_ictcp_measure_Event = Simulator::Schedule (MicroSeconds(m_receiver_pre_rtt.GetMicroSeconds()), &TcpSocketBase::Calculate_ICTCP_Receiver_Goodput, this);
		  }
	      if (m_receive_device->m_dev_global_not_start) { //just invoke once, global.
	          m_receive_device->m_dev_global_not_start = 0;
			  m_receive_device->m_dev_global_mode = 1;
              m_ictcp_global_update_Event = Simulator::ScheduleNow (&TcpSocketBase::ICTCP_UPDATE, this);
          }
      }
      if (m_rdtcp_mode == 1 && m_rdtcp_controller_mode == 1 )
		  m_receive_device->m_rec_infos[(uint64_t)this ].m_connection_rtt = m_rtt->GetCurrentEstimate_r();
    }
  else if (tcpflags == TcpHeader::SYN)
    { // Probably the peer lost my SYN+ACK
      m_rxBuffer.SetNextRxSequence (tcpHeader.GetSequenceNumber () + SequenceNumber32 (1));

	  if (m_rdtcp_mode == 1  && m_rdtcp_controller_mode == 1 ) {
	    SignalSendEmptyPacket (TcpHeader::SYN | TcpHeader::ACK);
      }
      else
        SendEmptyPacket (TcpHeader::SYN | TcpHeader::ACK);
      //SendEmptyPacket (TcpHeader::SYN | TcpHeader::ACK);
    }
  else if (tcpflags == (TcpHeader::FIN | TcpHeader::ACK))
    {
      if (tcpHeader.GetSequenceNumber () == m_rxBuffer.NextRxSequence ())
        { // In-sequence FIN before connection complete. Set up connection and close.
          m_connected = true;
          m_retxEvent.Cancel ();
          m_highTxMark = ++m_nextTxSequence;
          m_txBuffer.SetHeadSequence (m_nextTxSequence);
          if (m_endPoint)
            {
              m_endPoint->SetPeer (InetSocketAddress::ConvertFrom (fromAddress).GetIpv4 (),
                                   InetSocketAddress::ConvertFrom (fromAddress).GetPort ());
            }
          else if (m_endPoint6)
            {
              m_endPoint6->SetPeer (Inet6SocketAddress::ConvertFrom (fromAddress).GetIpv6 (),
                                    Inet6SocketAddress::ConvertFrom (fromAddress).GetPort ());
            }
          PeerClose (packet, tcpHeader);
        }
    }
  else
    { // Other in-sequence input
      if (tcpflags != TcpHeader::RST)
        { // When (1) rx of SYN+ACK; (2) rx of FIN; (3) rx of bad flags
          NS_LOG_LOGIC ("Illegal flag " << tcpflags << " received. Reset packet is sent.");
          if (m_endPoint)
            {
              m_endPoint->SetPeer (InetSocketAddress::ConvertFrom (fromAddress).GetIpv4 (),
                                   InetSocketAddress::ConvertFrom (fromAddress).GetPort ());
            }
          else if (m_endPoint6)
            {
              m_endPoint6->SetPeer (Inet6SocketAddress::ConvertFrom (fromAddress).GetIpv6 (),
                                    Inet6SocketAddress::ConvertFrom (fromAddress).GetPort ());
            }
          SendRST ();
        }
      CloseAndNotify ();
    }
}

/** Received a packet upon CLOSE_WAIT, FIN_WAIT_1, or FIN_WAIT_2 states */
void
TcpSocketBase::ProcessWait (Ptr<Packet> packet, const TcpHeader& tcpHeader)
{
  NS_LOG_FUNCTION (this << tcpHeader);

  // Extract the flags. PSH and URG are not honoured.
  uint8_t tcpflags = tcpHeader.GetFlags () & ~(TcpHeader::PSH | TcpHeader::URG);

  if (packet->GetSize () > 0 && tcpflags != TcpHeader::ACK)
    { // Bare data, accept it
      ReceivedData (packet, tcpHeader, Ipv4Header::ECN_NotECT);
    }
  else if (tcpflags == TcpHeader::ACK || tcpflags == (TcpHeader::ACK | TcpHeader::ECE) )
    { // Process the ACK, and if in FIN_WAIT_1, conditionally move to FIN_WAIT_2
      ReceivedAck (packet, tcpHeader, Ipv4Header::ECN_NotECT);
      if (m_state == FIN_WAIT_1 && m_txBuffer.Size () == 0
          && tcpHeader.GetAckNumber () == m_highTxMark + SequenceNumber32 (1))
        { // This ACK corresponds to the FIN sent
          NS_LOG_INFO ("FIN_WAIT_1 -> FIN_WAIT_2");
          m_state = FIN_WAIT_2;
        }
    }
  else if (tcpflags == TcpHeader::FIN || tcpflags == (TcpHeader::FIN | TcpHeader::ACK) 
		  || tcpflags == (TcpHeader::FIN | TcpHeader::ECE) //add for ecn by xl.
		  || tcpflags == (TcpHeader::FIN | TcpHeader::ACK | TcpHeader::ECE) )
    { // Got FIN, respond with ACK and move to next state
      if (tcpflags & TcpHeader::ACK)
        { // Process the ACK first
          ReceivedAck (packet, tcpHeader, Ipv4Header::ECN_NotECT);
        }
      m_rxBuffer.SetFinSequence (tcpHeader.GetSequenceNumber ());
    }
  else if (tcpflags == TcpHeader::SYN || tcpflags == (TcpHeader::SYN | TcpHeader::ACK))
    { // Duplicated SYN or SYN+ACK, possibly due to spurious retransmission
      return;
    }
  else
    { // This is a RST or bad flags
      if (tcpflags != TcpHeader::RST)
        {
          NS_LOG_LOGIC ("Illegal flag " << (uint16_t)tcpflags << " received. Reset packet is sent.");
          SendRST ();
        }
      CloseAndNotify ();
      return;
    }

  // Check if the close responder sent an in-sequence FIN, if so, respond ACK
  if ((m_state == FIN_WAIT_1 || m_state == FIN_WAIT_2) && m_rxBuffer.Finished ())
    {
      if (m_state == FIN_WAIT_1)
        {
          NS_LOG_INFO ("FIN_WAIT_1 -> CLOSING");
          m_state = CLOSING;
          if (m_txBuffer.Size () == 0
              && tcpHeader.GetAckNumber () == m_highTxMark + SequenceNumber32 (1))
            { // This ACK corresponds to the FIN sent
              TimeWait ();
            }
        }
      else if (m_state == FIN_WAIT_2)
        {
          TimeWait ();
        }
      SendEmptyPacket (TcpHeader::ACK);
      if (!m_shutdownRecv)
        {
          NotifyDataRecv ();
        }
    }
}

/** Received a packet upon CLOSING */
void
TcpSocketBase::ProcessClosing (Ptr<Packet> packet, const TcpHeader& tcpHeader)
{
  NS_LOG_FUNCTION (this << tcpHeader);

  // Extract the flags. PSH and URG are not honoured.
  uint8_t tcpflags = tcpHeader.GetFlags () & ~(TcpHeader::PSH | TcpHeader::URG);

  if (tcpflags == TcpHeader::ACK)
    {
      if (tcpHeader.GetSequenceNumber () == m_rxBuffer.NextRxSequence ())
        { // This ACK corresponds to the FIN sent
          TimeWait ();
        }
    }
  else
    { // CLOSING state means simultaneous close, i.e. no one is sending data to
      // anyone. If anything other than ACK is received, respond with a reset.
      if (tcpflags == TcpHeader::FIN || tcpflags == (TcpHeader::FIN | TcpHeader::ACK))
        { // FIN from the peer as well. We can close immediately.
          SendEmptyPacket (TcpHeader::ACK);
        }
      else if (tcpflags != TcpHeader::RST)
        { // Receive of SYN or SYN+ACK or bad flags or pure data
          NS_LOG_LOGIC ("Illegal flag " << tcpflags << " received. Reset packet is sent.");
          SendRST ();
        }
      CloseAndNotify ();
    }
}

/** Received a packet upon LAST_ACK */
void
TcpSocketBase::ProcessLastAck (Ptr<Packet> packet, const TcpHeader& tcpHeader)
{
  NS_LOG_FUNCTION (this << tcpHeader);

  // Extract the flags. PSH and URG are not honoured.
  uint8_t tcpflags = tcpHeader.GetFlags () & ~(TcpHeader::PSH | TcpHeader::URG);

  if (tcpflags == 0)
    {
      ReceivedData (packet, tcpHeader, Ipv4Header::ECN_NotECT);
    }
  else if (tcpflags == TcpHeader::ACK)
    {
      if (tcpHeader.GetSequenceNumber () == m_rxBuffer.NextRxSequence ())
        { // This ACK corresponds to the FIN sent. This socket closed peacefully.
          CloseAndNotify ();
        }
    }
  else if (tcpflags == TcpHeader::FIN)
    { // Received FIN again, the peer probably lost the FIN+ACK
      SendEmptyPacket (TcpHeader::FIN | TcpHeader::ACK);
    }
  else if (tcpflags == (TcpHeader::FIN | TcpHeader::ACK) || tcpflags == TcpHeader::RST)
    {
      CloseAndNotify ();
    }
  else
    { // Received a SYN or SYN+ACK or bad flags
      NS_LOG_LOGIC ("Illegal flag " << tcpflags << " received. Reset packet is sent.");
      SendRST ();
      CloseAndNotify ();
    }
}

/** Peer sent me a FIN. Remember its sequence in rx buffer. */
void
TcpSocketBase::PeerClose (Ptr<Packet> p, const TcpHeader& tcpHeader)
{
  NS_LOG_FUNCTION (this << tcpHeader);

  // Ignore all out of range packets
  if (tcpHeader.GetSequenceNumber () < m_rxBuffer.NextRxSequence ()
      || tcpHeader.GetSequenceNumber () > m_rxBuffer.MaxRxSequence ())
    {
      return;
    }
  // For any case, remember the FIN position in rx buffer first
  m_rxBuffer.SetFinSequence (tcpHeader.GetSequenceNumber () + SequenceNumber32 (p->GetSize ()));
  NS_LOG_LOGIC ("Accepted FIN at seq " << tcpHeader.GetSequenceNumber () + SequenceNumber32 (p->GetSize ()));
  // If there is any piggybacked data, process it
  if (p->GetSize ())
    {
      ReceivedData (p, tcpHeader, Ipv4Header::ECN_NotECT);
    }
  // Return if FIN is out of sequence, otherwise move to CLOSE_WAIT state by DoPeerClose
  if (!m_rxBuffer.Finished ())
    {
      return;
    }

  // Simultaneous close: Application invoked Close() when we are processing this FIN packet
  if (m_state == FIN_WAIT_1)
    {
      NS_LOG_INFO ("FIN_WAIT_1 -> CLOSING");
      m_state = CLOSING;
      return;
    }

  DoPeerClose (); // Change state, respond with ACK
}

/** Received a in-sequence FIN. Close down this socket. */
void
TcpSocketBase::DoPeerClose (void)
{
  NS_ASSERT (m_state == ESTABLISHED || m_state == SYN_RCVD);

  // Move the state to CLOSE_WAIT
  NS_LOG_INFO (TcpStateName[m_state] << " -> CLOSE_WAIT");
  m_state = CLOSE_WAIT;

  if (m_rec_deadline == 0)
	  PRINT_STUFF ("longflowfinish at "<< Simulator::Now().GetNanoSeconds()<< ", num:"<< m_receiver_order
			  <<". port=900"
			  <<", deadline= "<< m_rec_deadline <<", flowid= "<< m_rec_flowid
			  );
  else
	  PRINT_STUFF ("flow finished at "<< Simulator::Now().GetNanoSeconds()<< ", num:"<< m_receiver_order
			  <<". port=20"
			  <<", deadline= "<< m_rec_deadline <<", flowid= "<< m_rec_flowid
			  );

  if (!m_closeNotified)
    {
      // The normal behaviour for an application is that, when the peer sent a in-sequence
      // FIN, the app should prepare to close. The app has two choices at this point: either
      // respond with ShutdownSend() call to declare that it has nothing more to send and
      // the socket can be closed immediately; or remember the peer's close request, wait
      // until all its existing data are pushed into the TCP socket, then call Close()
      // explicitly.
      NS_LOG_LOGIC ("TCP " << this << " calling NotifyNormalClose");
      NotifyNormalClose ();
      m_closeNotified = true;
    }
    
  if (m_receive_device && m_rdtcp_mode ){
		  
      m_receive_device->m_rdtcp_closed_count++; 
      //ReceiverIterator it = m_receive_device->m_rec_infos.find ((uint64_t)this );
	  //m_receive_device->m_rec_infos.erase ( it );
 	  m_receive_device->m_rec_infos.erase((uint64_t)this );
	  NS_LOG_DEBUG ("dopeerclose, info erase. info size="<< m_receive_device->m_rec_infos.size());

	  // calculate max win by controller
	  Update_rdtcp_MaxWin_RttDelay ();
  }
  if (m_shutdownSend)
    { // The application declares that it would not sent any more, close this socket
      Close ();
    }
  else
    { // Need to ack, the application will close later
      SendEmptyPacket (TcpHeader::ACK);
    }
  if (m_state == LAST_ACK)
    {
      NS_LOG_LOGIC ("TcpSocketBase " << this << " scheduling LATO1");
      m_lastAckEvent = Simulator::Schedule (m_rtt->RetransmitTimeout (),
                                            &TcpSocketBase::LastAckTimeout, this);
    }
}

/** Kill this socket. This is a callback function configured to m_endpoint in
   SetupCallback(), invoked when the endpoint is destroyed. */
void
TcpSocketBase::Destroy (void)
{
  NS_LOG_FUNCTION (this);
  m_endPoint = 0;
  if (m_tcp != 0)
    {
      std::vector<Ptr<TcpSocketBase> >::iterator it
        = std::find (m_tcp->m_sockets.begin (), m_tcp->m_sockets.end (), this);
      if (it != m_tcp->m_sockets.end ())
        {
          m_tcp->m_sockets.erase (it);
        }
    }
  NS_LOG_LOGIC (this << " Cancelled ReTxTimeout event which was set to expire at " <<
                (Simulator::Now () + Simulator::GetDelayLeft (m_retxEvent)).GetSeconds ());
  CancelAllTimers ();
}

/** Kill this socket. This is a callback function configured to m_endpoint in
   SetupCallback(), invoked when the endpoint is destroyed. */
void
TcpSocketBase::Destroy6 (void)
{
  NS_LOG_FUNCTION (this);
  m_endPoint6 = 0;
  if (m_tcp != 0)
    {
      std::vector<Ptr<TcpSocketBase> >::iterator it
        = std::find (m_tcp->m_sockets.begin (), m_tcp->m_sockets.end (), this);
      if (it != m_tcp->m_sockets.end ())
        {
          m_tcp->m_sockets.erase (it);
        }
    }
  NS_LOG_LOGIC (this << " Cancelled ReTxTimeout event which was set to expire at " <<
                (Simulator::Now () + Simulator::GetDelayLeft (m_retxEvent)).GetSeconds ());
  CancelAllTimers ();
}

/** Send an empty packet with specified TCP flags */
void
TcpSocketBase::SendEmptyPacket (uint8_t flags)
{
  NS_LOG_FUNCTION (this << (uint32_t)flags);
  Ptr<Packet> p = Create<Packet> ();
  TcpHeader header;
  SequenceNumber32 s = m_nextTxSequence;

  /*
   * Add tags for each socket option.
   * Note that currently the socket adds both IPv4 tag and IPv6 tag
   * if both options are set. Once the packet got to layer three, only
   * the corresponding tags will be read.
   */
  if (IsManualIpTos ())
    {
      SocketIpTosTag ipTosTag;
      ipTosTag.SetTos (GetIpTos ());
      p->AddPacketTag (ipTosTag);
    }

  if (IsManualIpv6Tclass ())
    {
      SocketIpv6TclassTag ipTclassTag;
      ipTclassTag.SetTclass (GetIpv6Tclass ());
      p->AddPacketTag (ipTclassTag);
    }

  if (IsManualIpTtl ())
    {
      SocketIpTtlTag ipTtlTag;
      ipTtlTag.SetTtl (GetIpTtl ());
      p->AddPacketTag (ipTtlTag);
    }

  if (IsManualIpv6HopLimit ())
    {
      SocketIpv6HopLimitTag ipHopLimitTag;
      ipHopLimitTag.SetHopLimit (GetIpv6HopLimit ());
      p->AddPacketTag (ipHopLimitTag);
    }

  if (m_endPoint == 0 && m_endPoint6 == 0)
    {
      NS_LOG_WARN ("Failed to send empty packet due to null endpoint");
      return;
    }
  if (flags & TcpHeader::FIN)
    {
      flags |= TcpHeader::ACK;
	  if (m_rdtcp_mode) m_sender_flowsize = 0;
    }
  else if (m_state == FIN_WAIT_1 || m_state == LAST_ACK || m_state == CLOSING)
    {
      ++s;
    }

  header.SetFlags (flags);
  header.SetSequenceNumber (s);
  header.SetAckNumber (m_rxBuffer.NextRxSequence ());
  if (m_endPoint != 0)
    {
      header.SetSourcePort (m_endPoint->GetLocalPort ());
      header.SetDestinationPort (m_endPoint->GetPeerPort ());
    }
  else
    {
      header.SetSourcePort (m_endPoint6->GetLocalPort ());
      header.SetDestinationPort (m_endPoint6->GetPeerPort ());
    }
  header.SetWindowSize (AdvertisedWindowSize ());
  AddOptions (header);
  m_rto = m_rtt->RetransmitTimeout ();
  bool hasSyn = flags & TcpHeader::SYN;
  bool hasFin = flags & TcpHeader::FIN;
  bool isAck = flags == TcpHeader::ACK;
  if (hasSyn)
    {
      if (m_cnCount == 0)
        { // No more connection retries, give up
          NS_LOG_LOGIC ("Connection failed.");
          CloseAndNotify ();
          return;
        }
      else
        { // Exponential backoff of connection time out
          int backoffCount = 0x1 << (m_cnRetries - m_cnCount);
          m_rto = m_cnTimeout * backoffCount;
          m_cnCount--;
        }
    }
  if (m_receive_device)
	  NS_LOG_DEBUG ("AdvertisedWindowSize () = "<<AdvertisedWindowSize ()
			  <<", ACK="<<m_rxBuffer.NextRxSequence ()
			  <<", m_receiver_inFastRec="<<m_receiver_inFastRec
			  );
  else
	  NS_LOG_DEBUG ("AdvertisedWindowSize () = "<<AdvertisedWindowSize ()
			  <<", seq="<< s
			  );

  if (m_endPoint != 0)
    {
      m_tcp->SendPacket (p, header, m_endPoint->GetLocalAddress (),
                         m_endPoint->GetPeerAddress (), m_boundnetdevice);
    }
  else
    {
      m_tcp->SendPacket (p, header, m_endPoint6->GetLocalAddress (),
                         m_endPoint6->GetPeerAddress (), m_boundnetdevice);
    }
  if (m_receive_device && m_rdtcp_mode == 1  && m_rdtcp_controller_mode == 1 ) {

    ReceiverIterator it = m_receive_device->m_rec_infos.find ((uint64_t)this );
    if (it !=  m_receive_device->m_rec_infos.end () && it->second.m_rec_get_flowsize > 0 )  {

      m_receive_device->m_sendwin_in_rtt += ceil ((double)AdvertisedWindowSize () / m_segmentSize);

	  if (m_rxBuffer.NextRxSequence ().GetValue() + AdvertisedWindowSize () > m_recCwnd_upper ) {
	  /*if (m_node->GetId () == 872)
	  NS_LOG_DEBUG ("ACK:"<<m_rxBuffer.NextRxSequence ()
			  <<" + AdvertisedWindowSize ():"<<AdvertisedWindowSize ()
			  <<"=newupper =  " <<m_rxBuffer.NextRxSequence ().GetValue() + AdvertisedWindowSize ()
			  <<" > m_recCwnd_upper =" <<m_recCwnd_upper 
			  <<", incre bytes=" << m_rxBuffer.NextRxSequence ().GetValue() + AdvertisedWindowSize () - m_recCwnd_upper 
			  //<<", incre wins="<<ceil ((double)(m_rxBuffer.NextRxSequence ().GetValue() + AdvertisedWindowSize () - m_recCwnd_upper ) / m_segmentSize)
			  <<",before m_flight_wins ="<< m_receive_device->m_flight_wins 
			  <<", m_incre_bytes =" <<m_incre_bytes 
			  );*/
	      m_incre_bytes = m_rxBuffer.NextRxSequence ().GetValue() + AdvertisedWindowSize () - m_recCwnd_upper;
		  m_recCwnd_upper = m_rxBuffer.NextRxSequence ().GetValue() + AdvertisedWindowSize ();
		  while (m_incre_bytes >= m_segmentSize) {
			  m_receive_device->m_flight_wins++; 
			  m_incre_bytes -= m_segmentSize;
		  }

	      //m_receive_device->m_flight_wins    += ceil ((double)(m_rxBuffer.NextRxSequence ().GetValue() + AdvertisedWindowSize () - m_recCwnd_upper ) / m_segmentSize);
	  }
		
	  /*if (m_node->GetId () == 872)
	  NS_LOG_DEBUG ( "AdvertisedWindowSize () = "<<AdvertisedWindowSize ()
			  <<", AdvertisedWindowSize () in wins =" <<ceil ((double)AdvertisedWindowSize () / m_segmentSize)
			  <<",+ ACK="<<m_rxBuffer.NextRxSequence ()
			  <<", = " <<m_rxBuffer.NextRxSequence ().GetValue() + AdvertisedWindowSize ()
			  <<",= m_recCwnd_upper =" <<m_recCwnd_upper 
			  <<",after m_flight_wins ="<< m_receive_device->m_flight_wins 
			  <<", m_incre_bytes =" <<m_incre_bytes 
			  );*/
	  }
  }

  if (flags & TcpHeader::ACK)
    { // If sending an ACK, cancel the delay ACK as well
      m_delAckEvent.Cancel ();
      m_delAckCount = 0;
	  m_receiver_send_ack_times++; //for delay ack,xl
	  if (m_rtcp_mode == 1 && !m_receiver_inFastRec && m_receive_device  ){
	      //by xl
		  //only for receiver.
	        if (flags & TcpHeader::FIN )
		    {
	            //xPRINT_STUFF ("receiver RTT 1, FIN, last ack, ack="<< m_rxBuffer.NextRxSequence ()<< "");
                m_rtt->SentAck (m_rxBuffer.NextRxSequence (), 0, true); // notify the RTT for receiver, by xl
		    }
			else if (flags & TcpHeader::SYN )
                m_rtt->SentAck (m_rxBuffer.NextRxSequence (), 0, false); // notify the RTT for receiver, by xl
		    else {
		        uint32_t distance = AdvertisedWindowSize () - m_segmentSize;
	            if (distance  % m_segmentSize) distance = distance - (distance  % m_segmentSize);
	            //NS_LOG_DEBUG ("receiver RTT 1: ack="<< m_rxBuffer.NextRxSequence ()<< ",orig distance = "<< AdvertisedWindowSize () - m_segmentSize <<",distance= "<<distance<<", SendEmptyPacket");
                m_rtt->SentAck (m_rxBuffer.NextRxSequence (), distance, false); // notify the RTT for receiver, by xl
		    }

		  if ((m_state == ESTABLISHED || m_state == SYN_RCVD) && (m_rxBuffer.NextRxSequence () >= m_pre_ack) && !hasSyn 
				  && !(m_nextTxSequence == SequenceNumber32 (1) && m_rxBuffer.NextRxSequence () == SequenceNumber32 (1)))
		  {//new ack, schedule xl
			  if (m_rxBuffer.NextRxSequence () == m_pre_ack ) {//lost packet
				  if (++m_overRTO_r_times >= 3)
				  {
				      m_overRTO_r_times = 0;
			          m_rtt->IncreaseMultiplier_r();
				  }
			  }
			  else {//normal
			      m_pre_ack = m_rxBuffer.NextRxSequence ();
			      m_rtt->ResetMultiplier_r ();
			  }
              m_retx_rEvent.Cancel ();
              m_rto_r = m_rtt->RetransmitTimeout_r (0);
			  //m_rto_r = MilliSeconds (10); //test 
	          //NS_LOG_DEBUG ("sendempty schedule ack "<< m_pre_ack 
			//		  <<" to expire at time(now+rto) " << (Simulator::Now () + m_rto_r.Get ()).GetSeconds ()
			//		  << ",rto_r="<<m_rto_r<<",rtt="<<m_rtt->GetCurrentEstimate()
			//		  <<",rtt_r = "<<m_rtt->GetCurrentEstimate_r()
			//		  <<",m_retx_rEvent="<< &m_retx_rEvent<< ", isexpire=" <<m_retx_rEvent.IsExpired ()
			//		  <<", m_receiver_inFastRec="<<m_receiver_inFastRec);
			if (!m_ictcp_mode)
              m_retx_rEvent = Simulator::Schedule (m_rto_r, &TcpSocketBase::Retransmit_r, this, flags);
		  }
	  }// for rtcp, xl
    }
  if (m_retxEvent.IsExpired () && (hasSyn || hasFin) && !isAck )
    { // Retransmit SYN / SYN+ACK / FIN / FIN+ACK to guard against lost
      NS_LOG_LOGIC ("Schedule retransmission timeout at time "
                    << Simulator::Now ().GetSeconds () << " to expire at time "
                    << (Simulator::Now () + m_rto.Get ()).GetSeconds ());
      m_retxEvent = Simulator::Schedule (m_rto, &TcpSocketBase::SendEmptyPacket, this, flags);
    }
}

/** This function closes the endpoint completely. Called upon RST_TX action. */
void
TcpSocketBase::SendRST (void)
{
  NS_LOG_FUNCTION (this);
  SendEmptyPacket (TcpHeader::RST);
  NotifyErrorClose ();
  DeallocateEndPoint ();
}

/** Deallocate the end point and cancel all the timers */
void
TcpSocketBase::DeallocateEndPoint (void)
{
  if (m_endPoint != 0)
    {
      m_endPoint->SetDestroyCallback (MakeNullCallback<void> ());
      m_tcp->DeAllocate (m_endPoint);
      m_endPoint = 0;
      std::vector<Ptr<TcpSocketBase> >::iterator it
        = std::find (m_tcp->m_sockets.begin (), m_tcp->m_sockets.end (), this);
      if (it != m_tcp->m_sockets.end ())
        {
          m_tcp->m_sockets.erase (it);
        }
      CancelAllTimers ();
    }
  if (m_endPoint6 != 0)
    {
      m_endPoint6->SetDestroyCallback (MakeNullCallback<void> ());
      m_tcp->DeAllocate (m_endPoint6);
      m_endPoint6 = 0;
      std::vector<Ptr<TcpSocketBase> >::iterator it
        = std::find (m_tcp->m_sockets.begin (), m_tcp->m_sockets.end (), this);
      if (it != m_tcp->m_sockets.end ())
        {
          m_tcp->m_sockets.erase (it);
        }
      CancelAllTimers ();
    }
}

/** Configure the endpoint to a local address. Called by Connect() if Bind() didn't specify one. */
int
TcpSocketBase::SetupEndpoint ()
{
  NS_LOG_FUNCTION (this);
  Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();
  NS_ASSERT (ipv4 != 0);
  if (ipv4->GetRoutingProtocol () == 0)
    {
      NS_FATAL_ERROR ("No Ipv4RoutingProtocol in the node");
    }
  // Create a dummy packet, then ask the routing function for the best output
  // interface's address
  Ipv4Header header;
  header.SetDestination (m_endPoint->GetPeerAddress ());
  Socket::SocketErrno errno_;
  Ptr<Ipv4Route> route;
  Ptr<NetDevice> oif = m_boundnetdevice;
  route = ipv4->GetRoutingProtocol ()->RouteOutput (Ptr<Packet> (), header, oif, errno_);
  if (route == 0)
    {
      NS_LOG_LOGIC ("Route to " << m_endPoint->GetPeerAddress () << " does not exist");
      NS_LOG_ERROR (errno_);
      m_errno = errno_;
      return -1;
    }
  NS_LOG_LOGIC ("Route exists");
  m_endPoint->SetLocalAddress (route->GetSource ());
  return 0;
}

int
TcpSocketBase::SetupEndpoint6 ()
{
  NS_LOG_FUNCTION (this);
  Ptr<Ipv6L3Protocol> ipv6 = m_node->GetObject<Ipv6L3Protocol> ();
  NS_ASSERT (ipv6 != 0);
  if (ipv6->GetRoutingProtocol () == 0)
    {
      NS_FATAL_ERROR ("No Ipv6RoutingProtocol in the node");
    }
  // Create a dummy packet, then ask the routing function for the best output
  // interface's address
  Ipv6Header header;
  header.SetDestinationAddress (m_endPoint6->GetPeerAddress ());
  Socket::SocketErrno errno_;
  Ptr<Ipv6Route> route;
  Ptr<NetDevice> oif = m_boundnetdevice;
  route = ipv6->GetRoutingProtocol ()->RouteOutput (Ptr<Packet> (), header, oif, errno_);
  if (route == 0)
    {
      NS_LOG_LOGIC ("Route to " << m_endPoint6->GetPeerAddress () << " does not exist");
      NS_LOG_ERROR (errno_);
      m_errno = errno_;
      return -1;
    }
  NS_LOG_LOGIC ("Route exists");
  m_endPoint6->SetLocalAddress (route->GetSource ());
  return 0;
}

/** This function is called only if a SYN received in LISTEN state. After
   TcpSocketBase cloned, allocate a new end point to handle the incoming
   connection and send a SYN+ACK to complete the handshake. */
void
TcpSocketBase::CompleteFork (Ptr<Packet> p, const TcpHeader& h,
                             const Address& fromAddress, const Address& toAddress)
{
  NS_LOG_FUNCTION (this << h);
  // Get port and address from peer (connecting host)
  if (InetSocketAddress::IsMatchingType (toAddress))
    {
      m_endPoint = m_tcp->Allocate (InetSocketAddress::ConvertFrom (toAddress).GetIpv4 (),
                                    InetSocketAddress::ConvertFrom (toAddress).GetPort (),
                                    InetSocketAddress::ConvertFrom (fromAddress).GetIpv4 (),
                                    InetSocketAddress::ConvertFrom (fromAddress).GetPort ());
      m_endPoint6 = 0;
    }
  else if (Inet6SocketAddress::IsMatchingType (toAddress))
    {
      m_endPoint6 = m_tcp->Allocate6 (Inet6SocketAddress::ConvertFrom (toAddress).GetIpv6 (),
                                      Inet6SocketAddress::ConvertFrom (toAddress).GetPort (),
                                      Inet6SocketAddress::ConvertFrom (fromAddress).GetIpv6 (),
                                      Inet6SocketAddress::ConvertFrom (fromAddress).GetPort ());
      m_endPoint = 0;
    }
  m_tcp->m_sockets.push_back (this);

  // Change the cloned socket from LISTEN state to SYN_RCVD
  NS_LOG_INFO ("LISTEN -> SYN_RCVD");
  m_state = SYN_RCVD;
  m_cnCount = m_cnRetries;
  SetupCallback ();
  // Set the sequence number and send SYN+ACK
  m_rxBuffer.SetNextRxSequence (h.GetSequenceNumber () + SequenceNumber32 (1));
  
  m_rec_deadline = h.GetDeadline() ; //in ns.
  m_rec_flowid = h.GetFlowId() ; //in ns.
  m_rec_flowsize = h.GetRemainFlowSize(); //in ns.
      NS_LOG_DEBUG ( "at listen, flowid= "  << h.GetFlowId() 
			  <<", flowsize= "<< h.GetRemainFlowSize()
			  <<", deadline= "<< h.GetDeadline() 
			  );
  if (m_rdtcp_mode == 1) {
      Simulator::ScheduleNow (&TcpSocketBase::ModifyCwnd_RDCTCP,this);
	  
	 //controller 
	  m_receive_device->m_rec_infos[(uint64_t)this ].m_rec_get_flowsize = h.GetRemainFlowSize(); //add record.
	  m_receive_device->m_rec_infos[(uint64_t)this ].m_deadline = m_rec_deadline ;
	  if (m_receive_device->m_rec_infos[(uint64_t)this ].m_rec_get_flowsize > 0)
	    m_receive_device->m_rec_infos[(uint64_t)this ].m_rec_get_flowsize_Reciprocal = 
		      (double)1/m_receive_device->m_rec_infos[(uint64_t)this ].m_rec_get_flowsize ;
	  else m_receive_device->m_rec_infos[(uint64_t)this ].m_rec_get_flowsize_Reciprocal = 0;
      NS_LOG_DEBUG ( "at listen, tcp head: flow size=" << h.GetRemainFlowSize() <<", reciprocal=" 
			  << m_receive_device->m_rec_infos[(uint64_t)this ].m_rec_get_flowsize_Reciprocal 
              << ", rec order: "<< m_receiver_order 
				  <<", m_receive_device->m_rec_infos.size()="<<m_receive_device->m_rec_infos.size());
	  // calculate max win by controller
	  Update_rdtcp_MaxWin_RttDelay ();

  }

  if (m_ictcp_mode == 1)
    Simulator::Schedule (MicroSeconds(200), &TcpSocketBase::Calculate_ICTCP_Receiver_Goodput, this);
  else if (m_rtcp_mode == 1)
    Simulator::Schedule (MicroSeconds(300), &TcpSocketBase::Calculate_Receiver_Goodput, this);
  else //for sender, it is calculated at the receiver side.
    Simulator::Schedule (MicroSeconds(300), &TcpSocketBase::Calculate_Sender_Goodput, this);

  if (m_rdtcp_mode == 1  && m_rdtcp_controller_mode == 1 ) {
	  //NS_LOG_DEBUG ("rtt delay times("
	//		  << m_receive_device->m_rec_infos[(uint64_t)this ].m_rdtcp_delay_ack_rtts 
	//		  <<") > 0.");
         //m_delay_ack_Event = Simulator::Schedule ( 
		//	  GetDelayTime(), 
		//	  &TcpSocketBase::SendEmptyPacket, this, TcpHeader::SYN | TcpHeader::ACK );
	//m_recCwnd_upper = m_rxBuffer.NextRxSequence ().GetValue() + AdvertisedWindowSize ();
	SignalSendEmptyPacket (TcpHeader::SYN | TcpHeader::ACK);
  }
  else
    SendEmptyPacket (TcpHeader::SYN | TcpHeader::ACK);
}

void
TcpSocketBase::ConnectionSucceeded ()
{ // Wrapper to protected function NotifyConnectionSucceeded() so that it can
  // be called as a scheduled event
  NotifyConnectionSucceeded ();
  // The if-block below was moved from ProcessSynSent() to here because we need
  // to invoke the NotifySend() only after NotifyConnectionSucceeded() to
  // reflect the behaviour in the real world.
  if (GetTxAvailable () > 0)
    {
      NotifySend (GetTxAvailable ());
    }
}

/** Extract at most maxSize bytes from the TxBuffer at sequence seq, add the
    TCP header, and send to TcpL4Protocol */
uint32_t
TcpSocketBase::SendDataPacket (SequenceNumber32 seq, uint32_t maxSize, bool withAck)
{
  NS_LOG_FUNCTION (this << seq << maxSize << withAck);

  Ptr<Packet> p = m_txBuffer.CopyFromSequence (maxSize, seq);
  uint32_t sz = p->GetSize (); // Size of packet
  uint8_t flags = withAck ? TcpHeader::ACK : 0;
  uint32_t remainingData = m_txBuffer.SizeFromSequence (seq + SequenceNumber32 (sz));
  NS_LOG_DEBUG ("seq="<< seq << ", packet size="<< sz);

  /*
   * Add tags for each socket option.
   * Note that currently the socket adds both IPv4 tag and IPv6 tag
   * if both options are set. Once the packet got to layer three, only
   * the corresponding tags will be read.
   */
  if (IsManualIpTos ())
    {
      SocketIpTosTag ipTosTag;
      ipTosTag.SetTos (GetIpTos ());
      p->AddPacketTag (ipTosTag);
    }

  if (IsManualIpv6Tclass ())
    {
      SocketIpv6TclassTag ipTclassTag;
      ipTclassTag.SetTclass (GetIpv6Tclass ());
      p->AddPacketTag (ipTclassTag);
    }

  if (IsManualIpTtl ())
    {
      SocketIpTtlTag ipTtlTag;
      ipTtlTag.SetTtl (GetIpTtl ());
      p->AddPacketTag (ipTtlTag);
    }

  if (IsManualIpv6HopLimit ())
    {
      SocketIpv6HopLimitTag ipHopLimitTag;
      ipHopLimitTag.SetHopLimit (GetIpv6HopLimit ());
      p->AddPacketTag (ipHopLimitTag);
    }

  if (m_closeOnEmpty && (remainingData == 0))
    {
      flags |= TcpHeader::FIN;
      if (m_state == ESTABLISHED)
        { // On active close: I am the first one to send FIN
          NS_LOG_INFO ("ESTABLISHED -> FIN_WAIT_1");
          m_state = FIN_WAIT_1;
        }
      else if (m_state == CLOSE_WAIT)
        { // On passive close: Peer sent me FIN already
          NS_LOG_INFO ("CLOSE_WAIT -> LAST_ACK");
          m_state = LAST_ACK;
        }
    }
  TcpHeader header;
  header.SetFlags (flags);
  header.SetSequenceNumber (seq);
  header.SetAckNumber (m_rxBuffer.NextRxSequence ());
  if (m_endPoint)
    {
      header.SetSourcePort (m_endPoint->GetLocalPort ());
      header.SetDestinationPort (m_endPoint->GetPeerPort ());
    }
  else
    {
      header.SetSourcePort (m_endPoint6->GetLocalPort ());
      header.SetDestinationPort (m_endPoint6->GetPeerPort ());
    }
  header.SetWindowSize (AdvertisedWindowSize ());
  AddOptions (header);
  if (m_retxEvent.IsExpired () )
    { // Schedule retransmit
      m_rto = m_rtt->RetransmitTimeout ();
      NS_LOG_LOGIC (this << " SendDataPacket Schedule ReTxTimeout at time " <<
                    Simulator::Now ().GetSeconds () << " to expire at time " <<
                    (Simulator::Now () + m_rto.Get ()).GetSeconds () );
      m_retxEvent = Simulator::Schedule (m_rto, &TcpSocketBase::ReTxTimeout, this);
    }
  NS_LOG_LOGIC ("Send packet via TcpL4Protocol with flags 0x" << std::hex << static_cast<uint32_t> (flags) << std::dec);
  if (m_endPoint)
    {
      m_tcp->SendPacket (p, header, m_endPoint->GetLocalAddress (),
                         m_endPoint->GetPeerAddress (), m_boundnetdevice);
    }
  else
    {
      m_tcp->SendPacket (p, header, m_endPoint6->GetLocalAddress (),
                         m_endPoint6->GetPeerAddress (), m_boundnetdevice);
    }
  m_rtt->SentSeq (seq, sz);       // notify the RTT
  // Notify the application of the data being sent unless this is a retransmit
  if (seq == m_nextTxSequence)
    {
      Simulator::ScheduleNow (&TcpSocketBase::NotifyDataSent, this, sz);
    }
  // Update highTxMark
  m_highTxMark = std::max (seq + sz, m_highTxMark.Get ());
  return sz;
}

/** Send as much pending data as possible according to the Tx window. Note that
 *  this function did not implement the PSH flag
 */
bool
TcpSocketBase::SendPendingData (bool withAck)
{
  NS_LOG_FUNCTION (this << withAck);
  if (m_txBuffer.Size () == 0)
    {
      return false;                           // Nothing to send

    }
  if (m_endPoint == 0 && m_endPoint6 == 0)
    {
      NS_LOG_INFO ("TcpSocketBase::SendPendingData: No endpoint; m_shutdownSend=" << m_shutdownSend);
      return false; // Is this the right way to handle this condition?
    }
  uint32_t nPacketsSent = 0;
  while (m_txBuffer.SizeFromSequence (m_nextTxSequence))
    {
      uint32_t w = AvailableWindow (); // Get available window size
      NS_LOG_LOGIC ("TcpSocketBase " << this << " SendPendingData" <<
                    " w " << w <<
                    " rxwin " << m_rWnd <<
                    " segsize " << m_segmentSize <<
                    " nextTxSeq " << m_nextTxSequence <<
                    " highestRxAck " << m_txBuffer.HeadSequence () <<
                    " pd->Size " << m_txBuffer.Size () <<
                    " pd->SFS " << m_txBuffer.SizeFromSequence (m_nextTxSequence));
      // Stop sending if we need to wait for a larger Tx window (prevent silly window syndrome)
      if (w < m_segmentSize && m_txBuffer.SizeFromSequence (m_nextTxSequence) > w)
        { // send in unit of segment.
          break; // No more
        }
      // Nagle's algorithm (RFC896): Hold off sending if there is unacked data
      // in the buffer and the amount of data to send is less than one segment
      if (!m_noDelay && UnAckDataCount () > 0
          && m_txBuffer.SizeFromSequence (m_nextTxSequence) < m_segmentSize)
        {
          NS_LOG_LOGIC ("Invoking Nagle's algorithm. Wait to send.");
          break;
        }
      uint32_t s = std::min (w, m_segmentSize);  // Send no more than window
      uint32_t sz = SendDataPacket (m_nextTxSequence, s, withAck);
      nPacketsSent++;                             // Count sent this loop
      m_nextTxSequence += sz;                     // Advance next tx sequence
    }
  NS_LOG_LOGIC ("SendPendingData sent " << nPacketsSent << " packets");
  return (nPacketsSent > 0);
}

uint32_t
TcpSocketBase::UnAckDataCount ()
{
  NS_LOG_FUNCTION (this);
  return m_nextTxSequence.Get () - m_txBuffer.HeadSequence ();
}

uint32_t
TcpSocketBase::BytesInFlight ()
{
  NS_LOG_FUNCTION (this);
  return m_highTxMark.Get () - m_txBuffer.HeadSequence ();
}

uint32_t
TcpSocketBase::Window () //xl, this function is overinvoked by TcpNewReno::Window (void)
{
  NS_LOG_FUNCTION (this);
  return m_rWnd;
}

uint32_t
TcpSocketBase::AvailableWindow ()
{
  NS_LOG_FUNCTION_NOARGS ();
  uint32_t unack = UnAckDataCount (); // Number of outstanding bytes
  uint32_t win = Window (); // Number of bytes allowed to be outstanding
  //using TcpNewReno::Window (void); by xl on Mon May  6 19:56:09 CST 2013
  NS_LOG_LOGIC ("UnAckCount=" << unack << ", Win=" << win);
  return (win < unack) ? 0 : (win - unack);
}

uint16_t
TcpSocketBase::AdvertisedWindowSize () //over revoked by TcpNewReno.
{
  return std::min (m_rxBuffer.MaxBufferSize () - m_rxBuffer.Size (), (uint32_t)m_maxWinSize);
}

// Receipt of new packet, put into Rx buffer
void
TcpSocketBase::ReceivedData (Ptr<Packet> p, const TcpHeader& tcpHeader, Ipv4Header::EcnType ipv4ecn)
{
  NS_LOG_FUNCTION (this << tcpHeader);
  NS_LOG_LOGIC ("receiveddata seq " << tcpHeader.GetSequenceNumber () <<
                ",receiveddata ack " << tcpHeader.GetAckNumber () <<
                ",receiveddata pkt size " << p->GetSize () );
  uint8_t flags = TcpHeader::NONE;
  if (ipv4ecn == Ipv4Header::ECN_CE)
  {
    //NS_LOG_DEBUG ("Get ipv4 ecn at receiver side.");
    flags = TcpHeader::ECE;
  }


  // Put into Rx buffer
  SequenceNumber32 expectedSeq = m_rxBuffer.NextRxSequence ();
  if (!m_rxBuffer.Add (p, tcpHeader))
    { // Insert failed: No data or RX buffer full
      NS_LOG_LOGIC ("expectedSeq = " << expectedSeq << ", add pkt failed! ");
      SendEmptyPacket (TcpHeader::ACK | flags);
      return;
    }
  // Now send a new ACK packet acknowledging all received and delivered data
  if (m_rxBuffer.Size () > m_rxBuffer.Available () || m_rxBuffer.NextRxSequence () > expectedSeq + p->GetSize ())
    { // A gap exists in the buffer, or we filled a gap: Always ACK
      NS_LOG_LOGIC ("expectedSeq = " << expectedSeq << ", m_rxBuffer.NextRxSequence () = "<<m_rxBuffer.NextRxSequence ()
			  << ", existing gap!");
      SendEmptyPacket (TcpHeader::ACK | flags);
    }
  /*else if (m_dctcp_mode)
  {//two state machine for delay ack.
	  if ( m_receiver_pre_state_is_ecn ==  true && ipv4ecn != Ipv4Header::ECN_CE )
	    {
		    m_receiver_pre_state_is_ecn = false;
            NS_LOG_LOGIC ("dctcp, pre ecn, current no ecn. " );
      		SendEmptyPacket (TcpHeader::ACK | TcpHeader::ECE);
	    }
	  else if ( m_receiver_pre_state_is_ecn == false && ipv4ecn == Ipv4Header::ECN_CE )
	    {
		    m_receiver_pre_state_is_ecn = true;
            NS_LOG_LOGIC ("dctcp, pre no ecn, current ecn. " );
      		SendEmptyPacket (TcpHeader::ACK);
	    }
	  else if (m_receiver_send_ack_times < m_delAckMaxCount)
        {
            NS_LOG_LOGIC ("dctcp, m_receiver_send_ack_times("<< m_receiver_send_ack_times
					<<") <  m_delAckMaxCount("<< m_delAckMaxCount<<")" );
          m_delAckEvent.Cancel ();
          m_delAckCount = 0;
          SendEmptyPacket (TcpHeader::ACK | flags);
        }
	  else if (++m_delAckCount >= m_delAckMaxCount)
        {
          m_delAckEvent.Cancel ();
          m_delAckCount = 0;
            NS_LOG_LOGIC ("dctcp, pre == current ecn. deltimes >= delmaxcount=" <<m_delAckMaxCount);
          SendEmptyPacket (TcpHeader::ACK | flags);
        }
      else if (m_delAckEvent.IsExpired ())
        {
            NS_LOG_LOGIC ("dctcp, pre == current ecn. deltimes < delmaxcount=" <<m_delAckMaxCount<<", delevent expired.");
          m_delAckEvent = Simulator::Schedule (m_delAckTimeout,
                                               &TcpSocketBase::DelAckTimeout, this);
          NS_LOG_LOGIC (this << " scheduled delayed ACK at " << (Simulator::Now () + Simulator::GetDelayLeft (m_delAckEvent)).GetSeconds ());
        }

  }*/
  else
    { // In-sequence packet: ACK if delayed ack count allows
	  if (m_receiver_send_ack_times < m_delAckMaxCount)
        {
          m_delAckEvent.Cancel ();
          m_delAckCount = 0;
          SendEmptyPacket (TcpHeader::ACK | flags);
        }
	  else if (++m_delAckCount >= m_delAckMaxCount)
        {
          m_delAckEvent.Cancel ();
          m_delAckCount = 0;
          SendEmptyPacket (TcpHeader::ACK | flags);
        }
      else if (m_delAckEvent.IsExpired ())
        {
          m_delAckEvent = Simulator::Schedule (m_delAckTimeout,
                                               &TcpSocketBase::DelAckTimeout, this);
          NS_LOG_LOGIC (this << " scheduled delayed ACK at " << (Simulator::Now () + Simulator::GetDelayLeft (m_delAckEvent)).GetSeconds ());
        }
    }
  // Notify app to receive if necessary
  if (expectedSeq < m_rxBuffer.NextRxSequence ())
    { // NextRxSeq advanced, we have something to send to the app
      if (!m_shutdownRecv)
        {
          NotifyDataRecv ();
        }
      // Handle exceptions
      if (m_closeNotified)
        {
          NS_LOG_WARN ("Why TCP " << this << " got data after close notification?");
        }
      // If we received FIN before and now completed all "holes" in rx buffer,
      // invoke peer close procedure
      if (m_rxBuffer.Finished () && (tcpHeader.GetFlags () & TcpHeader::FIN) == 0)
        {
          DoPeerClose ();
        }
    }
}

/** Called by ForwardUp() to estimate RTT */
void
TcpSocketBase::EstimateRtt (const TcpHeader& tcpHeader)
{
  // Use m_rtt for the estimation. Note, RTT of duplicated acknowledgement
  // (which should be ignored) is handled by m_rtt. Once timestamp option
  // is implemented, this function would be more elaborated.
  Time nextRtt =  m_rtt->AckSeq (tcpHeader.GetAckNumber () );

  //nextRtt will be zero for dup acks.  Don't want to update lastRtt in that case
  //but still needed to do list clearing that is done in AckSeq. 
  if(nextRtt != 0)
  {
    m_lastRtt = nextRtt;
    NS_LOG_FUNCTION(this << m_lastRtt);
  }
  
}

void
TcpSocketBase::NewAck (SequenceNumber32 const& ack)
{
  NS_LOG_FUNCTION (this << ack);

  if (m_state != SYN_RCVD)
    { // Set RTO unless the ACK is received in SYN_RCVD state
      NS_LOG_LOGIC (this << " Cancelled ReTxTimeout event which was set to expire at " <<
                    (Simulator::Now () + Simulator::GetDelayLeft (m_retxEvent)).GetSeconds ());
      m_retxEvent.Cancel ();
      // On recieving a "New" ack we restart retransmission timer .. RFC 2988
      m_rto = m_rtt->RetransmitTimeout ();
      NS_LOG_LOGIC (this << " Schedule ReTxTimeout at time " <<
                    Simulator::Now ().GetSeconds () << " to expire at time " <<
                    (Simulator::Now () + m_rto.Get ()).GetSeconds ());
      m_retxEvent = Simulator::Schedule (m_rto, &TcpSocketBase::ReTxTimeout, this);
    }
  if (m_rWnd.Get () == 0 && m_persistEvent.IsExpired ())
    { // Zero window: Enter persist state to send 1 byte to probe
      NS_LOG_LOGIC (this << "Enter zerowindow persist state");
      NS_LOG_LOGIC (this << "Cancelled ReTxTimeout event which was set to expire at " <<
                    (Simulator::Now () + Simulator::GetDelayLeft (m_retxEvent)).GetSeconds ());
      m_retxEvent.Cancel ();
      NS_LOG_LOGIC ("Schedule persist timeout at time " <<
                    Simulator::Now ().GetSeconds () << " to expire at time " <<
                    (Simulator::Now () + m_persistTimeout).GetSeconds ());
      m_persistEvent = Simulator::Schedule (m_persistTimeout, &TcpSocketBase::PersistTimeout, this);
      NS_ASSERT (m_persistTimeout == Simulator::GetDelayLeft (m_persistEvent));
    }
  // Note the highest ACK and tell app to send more
  NS_LOG_LOGIC ("TCP " << this << " NewAck " << ack <<
                " numberAck " << (ack - m_txBuffer.HeadSequence ())); // Number bytes ack'ed
  
  if (m_rdtcp_mode) m_sender_flowsize -= ack - m_txBuffer.HeadSequence ();
  m_txBuffer.DiscardUpTo (ack);
  
  if (GetTxAvailable () > 0)
    {
      NotifySend (GetTxAvailable ());
    }
  if (ack > m_nextTxSequence)
    {
      m_nextTxSequence = ack; // If advanced
    }
  if (m_txBuffer.Size () == 0 && m_state != FIN_WAIT_1 && m_state != CLOSING)
    { // No retransmit timer if no data to retransmit
      NS_LOG_LOGIC (this << " Cancelled ReTxTimeout event which was set to expire at " <<
                    (Simulator::Now () + Simulator::GetDelayLeft (m_retxEvent)).GetSeconds ());
      m_retxEvent.Cancel ();
    }
  // Try to send more data
  SendPendingData (m_connected);
}

// Called by the ReceivedAck() when new ACK received and by ProcessSynRcvd()
// when the three-way handshake completed. This cancels retransmission timer
// and advances Tx window
void
TcpSocketBase::NewAckECN (SequenceNumber32 const& ack)
{
  NS_LOG_FUNCTION (this << ack);

  if (m_state != SYN_RCVD)
    { // Set RTO unless the ACK is received in SYN_RCVD state
      NS_LOG_LOGIC (this << " Cancelled ReTxTimeout event which was set to expire at " <<
                    (Simulator::Now () + Simulator::GetDelayLeft (m_retxEvent)).GetSeconds ());
      m_retxEvent.Cancel ();
      // On recieving a "New" ack we restart retransmission timer .. RFC 2988
      m_rto = m_rtt->RetransmitTimeout ();
      NS_LOG_LOGIC (this << " Schedule ReTxTimeout at time " <<
                    Simulator::Now ().GetSeconds () << " to expire at time " <<
                    (Simulator::Now () + m_rto.Get ()).GetSeconds ());
      m_retxEvent = Simulator::Schedule (m_rto, &TcpSocketBase::ReTxTimeout, this);
    }
  if (m_rWnd.Get () == 0 && m_persistEvent.IsExpired ())
    { // Zero window: Enter persist state to send 1 byte to probe
      NS_LOG_LOGIC (this << "Enter zerowindow persist state");
      NS_LOG_LOGIC (this << "Cancelled ReTxTimeout event which was set to expire at " <<
                    (Simulator::Now () + Simulator::GetDelayLeft (m_retxEvent)).GetSeconds ());
      m_retxEvent.Cancel ();
      NS_LOG_LOGIC ("Schedule persist timeout at time " <<
                    Simulator::Now ().GetSeconds () << " to expire at time " <<
                    (Simulator::Now () + m_persistTimeout).GetSeconds ());
      m_persistEvent = Simulator::Schedule (m_persistTimeout, &TcpSocketBase::PersistTimeout, this);
      NS_ASSERT (m_persistTimeout == Simulator::GetDelayLeft (m_persistEvent));
    }
  // Note the highest ACK and tell app to send more
  NS_LOG_LOGIC ("TCP " << this << " NewAckECN " << ack <<
                " numberAck " << (ack - m_txBuffer.HeadSequence ())); // Number bytes ack'ed
  m_txBuffer.DiscardUpTo (ack);
  if (GetTxAvailable () > 0)
    {
      NotifySend (GetTxAvailable ());
    }
  if (ack > m_nextTxSequence)
    {
      m_nextTxSequence = ack; // If advanced
    }
  if (m_txBuffer.Size () == 0 && m_state != FIN_WAIT_1 && m_state != CLOSING)
    { // No retransmit timer if no data to retransmit
      NS_LOG_LOGIC (this << " Cancelled ReTxTimeout event which was set to expire at " <<
                    (Simulator::Now () + Simulator::GetDelayLeft (m_retxEvent)).GetSeconds ());
      m_retxEvent.Cancel ();
    }
  // Try to send more data
  SendPendingData (m_connected);
}

// Retransmit timeout
void
TcpSocketBase::ReTxTimeout ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC (this << " ReTxTimeout Expired at time " << Simulator::Now ().GetSeconds ());
  // If erroneous timeout in closed/timed-wait state, just return
  if (m_state == CLOSED || m_state == TIME_WAIT)
    {
      return;
    }
  // If all data are received (non-closing socket and nothing to send), just return
  if (m_state <= ESTABLISHED && m_txBuffer.HeadSequence () >= m_highTxMark)
    {
      return;
    }

  Retransmit ();
}

void
TcpSocketBase::DelAckTimeout (void)
{
  m_delAckCount = 0;
  SendEmptyPacket (TcpHeader::ACK);
}

void
TcpSocketBase::LastAckTimeout (void)
{
  NS_LOG_FUNCTION (this);

  m_lastAckEvent.Cancel ();
  if (m_state == LAST_ACK)
    {
      CloseAndNotify ();
    }
  if (!m_closeNotified)
    {
      m_closeNotified = true;
    }
}

// Send 1-byte data to probe for the window size at the receiver when
// the local knowledge tells that the receiver has zero window size
// C.f.: RFC793 p.42, RFC1112 sec.4.2.2.17
void
TcpSocketBase::PersistTimeout ()
{
  NS_LOG_LOGIC ("PersistTimeout expired at " << Simulator::Now ().GetSeconds ());
  m_persistTimeout = std::min (Seconds (60), Time (2 * m_persistTimeout)); // max persist timeout = 60s
  Ptr<Packet> p = m_txBuffer.CopyFromSequence (1, m_nextTxSequence);
  TcpHeader tcpHeader;
  tcpHeader.SetSequenceNumber (m_nextTxSequence);
  tcpHeader.SetAckNumber (m_rxBuffer.NextRxSequence ());
  tcpHeader.SetWindowSize (AdvertisedWindowSize ());
  if (m_endPoint != 0)
    {
      tcpHeader.SetSourcePort (m_endPoint->GetLocalPort ());
      tcpHeader.SetDestinationPort (m_endPoint->GetPeerPort ());
    }
  else
    {
      tcpHeader.SetSourcePort (m_endPoint6->GetLocalPort ());
      tcpHeader.SetDestinationPort (m_endPoint6->GetPeerPort ());
    }
  AddOptions (tcpHeader);

  if (m_endPoint != 0)
    {
      m_tcp->SendPacket (p, tcpHeader, m_endPoint->GetLocalAddress (),
                         m_endPoint->GetPeerAddress (), m_boundnetdevice);
    }
  else
    {
      m_tcp->SendPacket (p, tcpHeader, m_endPoint6->GetLocalAddress (),
                         m_endPoint6->GetPeerAddress (), m_boundnetdevice);
    }
  NS_LOG_LOGIC ("Schedule persist timeout at time "
                << Simulator::Now ().GetSeconds () << " to expire at time "
                << (Simulator::Now () + m_persistTimeout).GetSeconds ());
  m_persistEvent = Simulator::Schedule (m_persistTimeout, &TcpSocketBase::PersistTimeout, this);
}

void
TcpSocketBase::Retransmit ()
{
  m_nextTxSequence = m_txBuffer.HeadSequence (); // Start from highest Ack
  m_rtt->IncreaseMultiplier (); // Double the timeout value for next retx timer
  m_dupAckCount = 0;
  DoRetransmit (); // Retransmit the packet
}

void
TcpSocketBase::DoRetransmit ()
{
  NS_LOG_FUNCTION (this);
  // Retransmit SYN packet
  if (m_state == SYN_SENT)
    {
      if (m_cnCount > 0)
        {
          SendEmptyPacket (TcpHeader::SYN);
        }
      else
        {
          NotifyConnectionFailed ();
        }
      return;
    }
  // Retransmit non-data packet: Only if in FIN_WAIT_1 or CLOSING state
  if (m_txBuffer.Size () == 0)
    {
      if (m_state == FIN_WAIT_1 || m_state == CLOSING)
        { // Must have lost FIN, re-send
          SendEmptyPacket (TcpHeader::FIN);
        }
      return;
    }
  // Retransmit a data packet: Call SendDataPacket
  NS_LOG_LOGIC ("TcpSocketBase " << this << " retxing seq " << m_txBuffer.HeadSequence ());
  uint32_t sz = SendDataPacket (m_txBuffer.HeadSequence (), m_segmentSize, true);
  // In case of RTO, advance m_nextTxSequence
  m_nextTxSequence = std::max (m_nextTxSequence.Get (), m_txBuffer.HeadSequence () + sz);

}

void
TcpSocketBase::CancelAllTimers ()
{
  m_retxEvent.Cancel ();
  m_persistEvent.Cancel ();
  m_delAckEvent.Cancel ();
  m_lastAckEvent.Cancel ();
  m_timewaitEvent.Cancel ();
  
  m_dctcp_mod_markEvent.Cancel (); 
  m_retx_rEvent.Cancel ();
  m_sender_cal_markEvent.Cancel ();;  // xl
  m_receiver_cal_markEvent.Cancel ();
  
  m_rdctcp_mod_markEvent.Cancel (); 
  m_ictcp_measure_Event.Cancel (); 
  m_ictcp_global_update_Event.Cancel (); 
  m_delay_ack_Event.Cancel ();
  m_update_rdtcp_rtt_Event.Cancel ();
}

/** Move TCP to Time_Wait state and schedule a transition to Closed state */
void
TcpSocketBase::TimeWait ()
{
  NS_LOG_INFO (TcpStateName[m_state] << " -> TIME_WAIT");
  m_state = TIME_WAIT;
  CancelAllTimers ();
  // Move from TIME_WAIT to CLOSED after 2*MSL. Max segment lifetime is 2 min
  // according to RFC793, p.28
  m_timewaitEvent = Simulator::Schedule (Seconds (2 * m_msl),
                                         &TcpSocketBase::CloseAndNotify, this);
}

/** Below are the attribute get/set functions */

void
TcpSocketBase::SetSndBufSize (uint32_t size)
{
  m_txBuffer.SetMaxBufferSize (size);
}

uint32_t
TcpSocketBase::GetSndBufSize (void) const
{
  return m_txBuffer.MaxBufferSize ();
}

void
TcpSocketBase::SetRcvBufSize (uint32_t size)
{
  m_rxBuffer.SetMaxBufferSize (size);
}

uint32_t
TcpSocketBase::GetRcvBufSize (void) const
{
  return m_rxBuffer.MaxBufferSize ();
}

void
TcpSocketBase::SetSegSize (uint32_t size)
{
  m_segmentSize = size;
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "Cannot change segment size dynamically.");
}

uint32_t
TcpSocketBase::GetSegSize (void) const
{
  return m_segmentSize;
}

void
TcpSocketBase::SetConnTimeout (Time timeout)
{
  m_cnTimeout = timeout;
}

Time
TcpSocketBase::GetConnTimeout (void) const
{
  return m_cnTimeout;
}

void
TcpSocketBase::SetConnCount (uint32_t count)
{
  m_cnRetries = count;
}

uint32_t
TcpSocketBase::GetConnCount (void) const
{
  return m_cnRetries;
}

void
TcpSocketBase::SetDelAckTimeout (Time timeout)
{
  m_delAckTimeout = timeout;
}

Time
TcpSocketBase::GetDelAckTimeout (void) const
{
  return m_delAckTimeout;
}

void
TcpSocketBase::SetDelAckMaxCount (uint32_t count)
{
  m_delAckMaxCount = count;
}

uint32_t
TcpSocketBase::GetDelAckMaxCount (void) const
{
  return m_delAckMaxCount;
}

void
TcpSocketBase::SetTcpNoDelay (bool noDelay)
{
  m_noDelay = noDelay;
}

bool
TcpSocketBase::GetTcpNoDelay (void) const
{
  return m_noDelay;
}

void
TcpSocketBase::SetPersistTimeout (Time timeout)
{
  m_persistTimeout = timeout;
}

Time
TcpSocketBase::GetPersistTimeout (void) const
{
  return m_persistTimeout;
}

bool
TcpSocketBase::SetAllowBroadcast (bool allowBroadcast)
{
  // Broadcast is not implemented. Return true only if allowBroadcast==false
  return (!allowBroadcast);
}

bool
TcpSocketBase::GetAllowBroadcast (void) const
{
  return false;
}

/** Placeholder function for future extension that reads more from the TCP header */
void
TcpSocketBase::ReadOptions (const TcpHeader&)
{
}

/** Placeholder function for future extension that changes the TCP header */
void
TcpSocketBase::AddOptions (TcpHeader& tcpHeader)
{
	tcpHeader.SetDeadline(m_sender_deadline);
	tcpHeader.SetFlowId(m_sender_flowid);
	//if (m_rdtcp_mode)
  	tcpHeader.SetRemainFlowSize(m_sender_flowsize ); //for rdtcp controller. xl
}

} // namespace ns3
