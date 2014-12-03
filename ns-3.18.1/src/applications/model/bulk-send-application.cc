/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Georgia Institute of Technology
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
 * Author: George F. Riley <riley@ece.gatech.edu>
 */

#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/tcp-socket-factory.h"
#include "bulk-send-application.h"

#include "ns3/mymacros.h"

#include <iostream>
using namespace std;

NS_LOG_COMPONENT_DEFINE ("BulkSendApplication");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (BulkSendApplication);

TypeId
BulkSendApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BulkSendApplication")
    .SetParent<Application> ()
    .AddConstructor<BulkSendApplication> ()
    .AddAttribute ("SendSize", "The amount of data to send each time.",
                   UintegerValue (512),
                   MakeUintegerAccessor (&BulkSendApplication::m_sendSize),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("Remote", "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&BulkSendApplication::m_peer),
                   MakeAddressChecker ())
    .AddAttribute ("MaxBytes",
                   "The total number of bytes to send. "
                   "Once these bytes are sent, "
                   "no data  is sent again. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BulkSendApplication::m_maxBytes),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Protocol", "The type of protocol to use.",
                   TypeIdValue (TcpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&BulkSendApplication::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&BulkSendApplication::m_txTrace))
    
	
	//for deadline and flow id, by xl
	.AddAttribute ("Deadline", 
			       "flow deadline, 0 means having no deadline, in ns.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BulkSendApplication::m_deadline),
                   MakeUintegerChecker<uint32_t> ())
   	.AddAttribute ("FlowId", 
			       "flow id.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BulkSendApplication::m_flowid),
                   MakeUintegerChecker<uint32_t> ())
 
	//new protocol, xl
	.AddAttribute ("DCTCP_MODE", 
                   "The DCTCP mode for datacenter ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BulkSendApplication::m_dctcp_mode),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("ICTCP_MODE", 
                   "The ICTCP mode for datacenter ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BulkSendApplication::m_ictcp_mode),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("RTCP_MODE", 
                   "The receiver TCP mode for datacenter ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BulkSendApplication::m_rtcp_mode),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("RDTCP_MODE", 
                   "The receiver DCTCP mode for datacenter ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BulkSendApplication::m_rdtcp_mode),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("RDTCP_MAXQUEUE_SIZE", 
                   "The max queue size for RDTCP controller ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BulkSendApplication::m_rdtcp_queue_maxsize),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("RDTCP_CONTROLLER_MODE", 
                   "The RDTCP controller is active when set 1.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BulkSendApplication::m_rdtcp_controller_mode),
                   MakeUintegerChecker<uint32_t> ())

	.AddAttribute ("RDTCP_no_priority", 
                   "RDTCP does not use flow priority.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BulkSendApplication::m_rdtcp_no_priority),
                   MakeUintegerChecker<uint32_t> ())


  

  ;
  return tid;
}


BulkSendApplication::BulkSendApplication ()
  : m_socket (0),
    m_connected (false),
    m_totBytes (0),
  
	m_dctcp_mode (0),  //xl
  m_ictcp_mode(0),  //xl
  m_rtcp_mode(0),  //xl
  m_rdtcp_mode(0),  //xl
  m_rdtcp_queue_maxsize(0),
  m_rdtcp_controller_mode(0),
  m_rdtcp_no_priority(0)

{
  NS_LOG_FUNCTION (this);
}

BulkSendApplication::~BulkSendApplication ()
{
  NS_LOG_FUNCTION (this);
}

void
BulkSendApplication::SetMaxBytes (uint32_t maxBytes)
{
  NS_LOG_FUNCTION (this << maxBytes);
  m_maxBytes = maxBytes;
}

Ptr<Socket>
BulkSendApplication::GetSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

void
BulkSendApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_socket = 0;
  // chain up
  Application::DoDispose ();
}

void BulkSendApplication::CwndChange1 (uint32_t oldCwnd, uint32_t newCwnd)
{// by xl on Thu May  9 23:36:12 CST 2013
	//if (GetNode ()->GetId() == 0)
	  PRINT_STUFF (newCwnd<<" :newcwnd: "<< oldCwnd<<": oldcwnd, sendbulk");
	  //PRINT_STATE ("-_- oldcwnd = "<<oldCwnd<<", newcwnd = " << newCwnd);
}

// Application Methods
void BulkSendApplication::StartApplication (void) // Called at time specified by Start
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

      // Fatal error if socket type is not NS3_SOCK_STREAM or NS3_SOCK_SEQPACKET
      if (m_socket->GetSocketType () != Socket::NS3_SOCK_STREAM &&
          m_socket->GetSocketType () != Socket::NS3_SOCK_SEQPACKET)
        {
          NS_FATAL_ERROR ("Using BulkSend with an incompatible socket type. "
                          "BulkSend requires SOCK_STREAM or SOCK_SEQPACKET. "
                          "In other words, use TCP instead of UDP.");
        }

      if (Inet6SocketAddress::IsMatchingType (m_peer))
        {
          m_socket->Bind6 ();
        }
      else if (InetSocketAddress::IsMatchingType (m_peer))
        {
          m_socket->Bind ();
        }


      m_socket->Connect (m_peer, m_sendSize, m_deadline, m_flowid);
      m_socket->ShutdownRecv ();
      m_socket->SetConnectCallback (
        MakeCallback (&BulkSendApplication::ConnectionSucceeded, this),
        MakeCallback (&BulkSendApplication::ConnectionFailed, this));
      m_socket->SetSendCallback (
        MakeCallback (&BulkSendApplication::DataSend, this));
	  m_socket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&BulkSendApplication::CwndChange1, Ptr<BulkSendApplication>(this))); // by xl
	  m_socket->SetAttribute ("RcvBufSize", UintegerValue (64*1024* 1024));
	  m_socket->SetAttribute ("SndBufSize", UintegerValue (64*1024* 1024));
  
    }
  if (m_connected)
    {
      SendData ();
    }
}

void BulkSendApplication::StopApplication (void) // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_connected = false;
    }
  else
    {
      NS_LOG_WARN ("BulkSendApplication found null socket to close in StopApplication");
    }
}


// Private helpers

void BulkSendApplication::SendData (void)
{
  NS_LOG_FUNCTION (this);

  while (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    { // Time to send more
      uint32_t toSend = m_sendSize;
      // Make sure we don't send too many
      if (m_maxBytes > 0)
        {
          toSend = std::min (m_sendSize, m_maxBytes - m_totBytes);
        }
      NS_LOG_LOGIC ("sending packet at " << Simulator::Now ());
      Ptr<Packet> packet = Create<Packet> (toSend);
      m_txTrace (packet);
      int actual = m_socket->Send (packet);
      if (actual > 0)
        {
          m_totBytes += actual;
        }
      // We exit this loop when actual < toSend as the send side
      // buffer is full. The "DataSent" callback will pop when
      // some buffer space has freed ip.
      if ((unsigned)actual != toSend)
        {
          break;
        }
    }
  // Check if time to close (all sent)
  if (m_totBytes == m_maxBytes && m_connected)
    {
      m_socket->Close ();
      m_connected = false;
    }
}

void BulkSendApplication::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("BulkSendApplication Connection succeeded");
  m_connected = true;
  SendData ();
}

void BulkSendApplication::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("BulkSendApplication, Connection Failed");
}

void BulkSendApplication::DataSend (Ptr<Socket>, uint32_t)
{
  NS_LOG_FUNCTION (this);

  if (m_connected)
    { // Only send new data if the connection has completed
      Simulator::ScheduleNow (&BulkSendApplication::SendData, this);
    }
}



} // Namespace ns3
