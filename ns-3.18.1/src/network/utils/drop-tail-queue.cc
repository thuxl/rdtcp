/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 University of Washington
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
 */

#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "drop-tail-queue.h"

#include "ns3/ecn-tag.h" // by xl on Mon May 13 20:35:42 CST 2013

NS_LOG_COMPONENT_DEFINE ("DropTailQueue");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (DropTailQueue);

TypeId DropTailQueue::GetTypeId (void) 
{
  static TypeId tid = TypeId ("ns3::DropTailQueue")
    .SetParent<Queue> ()
    .AddConstructor<DropTailQueue> ()
    .AddAttribute ("Mode", 
                   "Whether to use bytes (see MaxBytes) or packets (see MaxPackets) as the maximum queue size metric.",
                   EnumValue (QUEUE_MODE_PACKETS),
                   MakeEnumAccessor (&DropTailQueue::SetMode),
                   MakeEnumChecker (QUEUE_MODE_BYTES, "QUEUE_MODE_BYTES",
                                    QUEUE_MODE_PACKETS, "QUEUE_MODE_PACKETS"))
    .AddAttribute ("MaxPackets", 
                   "The maximum number of packets accepted by this DropTailQueue.",
                   UintegerValue (100),
                   MakeUintegerAccessor (&DropTailQueue::m_maxPackets),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxBytes", 
                   "The maximum number of bytes accepted by this DropTailQueue.",
                   UintegerValue (100 * 65535),
                   MakeUintegerAccessor (&DropTailQueue::m_maxBytes),
                   MakeUintegerChecker<uint32_t> ())
	//ECN XL
	.AddAttribute ("ECN", 
                   "The ECN mode switch for DropQueue in the device",
                   UintegerValue (0),
                   MakeUintegerAccessor (&DropTailQueue::m_ecn_mode),
                   MakeUintegerChecker<uint32_t> ())
	
	.AddAttribute ("ECN_Threshold", 
                   "The mark threshold for ECN mode ",
                   UintegerValue (40),
                   MakeUintegerAccessor (&DropTailQueue::m_ecn_threshold),
                   MakeUintegerChecker<uint32_t> ())

	.AddAttribute ("Offer_Load", 
                   "for offered load ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&DropTailQueue::m_offered_load),
                   MakeUintegerChecker<uint8_t> ())




  ;

  return tid;
}

DropTailQueue::DropTailQueue () :
  Queue (),
  m_packets (),
  m_bytesInQueue (0),
  m_droptimes(0),
  m_ecns(0),
  m_offered_load(0)
{
  NS_LOG_FUNCTION (this);
}

DropTailQueue::~DropTailQueue ()
{
  NS_LOG_FUNCTION (this);
}

void
DropTailQueue::SetMode (DropTailQueue::QueueMode mode)
{
  NS_LOG_FUNCTION (this << mode);
  m_mode = mode;
}

DropTailQueue::QueueMode
DropTailQueue::GetMode (void)
{
  NS_LOG_FUNCTION (this);
  return m_mode;
}

bool 
DropTailQueue::DoEnqueue (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);
  uint32_t orig_ecn_thresh ;

  if (m_mode == QUEUE_MODE_PACKETS) {
    uint16_t reduce_packet_size = (double)m_offered_load/100 * m_maxPackets;
	m_maxPackets -= reduce_packet_size ;
	if (m_ecn_threshold < reduce_packet_size) { 
		orig_ecn_thresh = m_ecn_threshold ;
		m_ecn_threshold = 0;
	}
	else
	    m_ecn_threshold -= reduce_packet_size ;
	if (/*m_packets.size () >= m_maxPackets && */ m_bytesInQueue >= m_maxPackets * 1500 )
    {
      //NS_LOG_LOGIC ("Queue full (at max packets) -- droppping pkt");
      NS_LOG_LOGIC ("Queue full (at max packets["<< m_maxPackets << "]) -- droppping pkt for "<< m_droptimes <<" times" );
      NS_LOG_DEBUG ("Queue full size("<< m_packets.size ()<<") (at max packets["<< m_maxPackets 
			  << "]) -- droppping pkt for "<< m_droptimes <<" times" 
			  << ", p->GetSize ()="<<p->GetSize ()<< ", m_bytesInQueue("<<m_bytesInQueue<<") >("<< m_maxPackets * 1500 <<") " );
      Drop (p);
	  m_droptimes++;
	  m_maxPackets += reduce_packet_size ;
	  if (m_ecn_threshold == 0) m_ecn_threshold = orig_ecn_thresh ;
	  else m_ecn_threshold += reduce_packet_size ;
      return false;
    }
	else if (/*m_packets.size () >= m_ecn_threshold*/m_bytesInQueue >= m_ecn_threshold * 1500 ) {
	  if (m_ecn_mode) {
      	  NS_LOG_DEBUG ("Marking ecn in droptail queue, packet mode. m_packets.size ()["
				<< m_packets.size ()<<"] >= m_ecn_threshold["<< m_ecn_threshold <<"]. "
				<< ", m_bytesInQueue( " << m_bytesInQueue <<") >= m_ecn_threshold * 1500 ("
				<< m_ecn_threshold * 1500 << ")"
				);
	      Mark_ECN (p); 
		}
        m_ecns++;
	 }
	
	m_maxPackets += reduce_packet_size ;
	if (m_ecn_threshold == 0) m_ecn_threshold = orig_ecn_thresh ;
	else m_ecn_threshold += reduce_packet_size ;
  }

  if (m_mode == QUEUE_MODE_BYTES) {
    uint16_t reduce_byte_size = (double)m_offered_load/100 * m_maxBytes;
	m_maxBytes -= reduce_byte_size ;
	if (m_ecn_threshold < reduce_byte_size ) { 
		orig_ecn_thresh = m_ecn_threshold ;
		m_ecn_threshold = 0;
	}
	else
	    m_ecn_threshold -= reduce_byte_size ;
	if (m_bytesInQueue + p->GetSize () >= m_maxBytes)
    {
      NS_LOG_LOGIC ("Queue full (packet would exceed max bytes) -- droppping pkt");
      NS_LOG_DEBUG ("Queue full (packet would exceed max bytes["<<m_maxBytes<<"]) -- droppping pkt for "<< m_droptimes <<" times" );
      Drop (p);
	  m_droptimes++;
	  m_maxBytes += reduce_byte_size ;
	  if (m_ecn_threshold == 0) m_ecn_threshold = orig_ecn_thresh ;
	  else m_ecn_threshold += reduce_byte_size ;
      return false;
    }
	  else if (m_ecn_mode && m_bytesInQueue + p->GetSize () >= m_ecn_threshold) {
      	NS_LOG_DEBUG ("Marking ecn in droptail queue, bytes mode. m_bytesInQueue + p->GetSize ()["
				<< m_bytesInQueue + p->GetSize () <<"] >= m_ecn_threshold["<< m_ecn_threshold << "]." );
	    Mark_ECN (p); 
        m_ecns++;
	  }
	m_maxBytes += reduce_byte_size ;
	if (m_ecn_threshold == 0) m_ecn_threshold = orig_ecn_thresh ;
	else m_ecn_threshold += reduce_byte_size ;
  }


  m_bytesInQueue += p->GetSize ();
  m_packets.push (p);

  NS_LOG_LOGIC ("Number packets " << m_packets.size ());
  NS_LOG_LOGIC ("Number bytes " << m_bytesInQueue);

  return true;
}

Ptr<Packet>
DropTailQueue::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  if (m_packets.empty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }

  Ptr<Packet> p = m_packets.front ();
  m_packets.pop ();
  m_bytesInQueue -= p->GetSize ();

  NS_LOG_LOGIC ("Popped " << p);

  NS_LOG_LOGIC ("Number packets " << m_packets.size ());
  NS_LOG_LOGIC ("Number bytes " << m_bytesInQueue);

  return p;
}

Ptr<const Packet>
DropTailQueue::DoPeek (void) const
{
  NS_LOG_FUNCTION (this);

  if (m_packets.empty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }

  Ptr<Packet> p = m_packets.front ();

  NS_LOG_LOGIC ("Number packets " << m_packets.size ());
  NS_LOG_LOGIC ("Number bytes " << m_bytesInQueue);

  return p;
}
	
void
DropTailQueue::Mark_ECN (Ptr<Packet> p)
{
          EcnTag ecntag;
          p->AddPacketTag(ecntag);
}

} // namespace ns3

