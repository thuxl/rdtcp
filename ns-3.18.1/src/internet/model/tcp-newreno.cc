/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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

#include "tcp-newreno.h"
#include "ns3/log.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/simulator.h"
#include "ns3/abort.h"
#include "ns3/node.h"
#include "ns3/packet.h"

NS_LOG_COMPONENT_DEFINE ("TcpNewReno");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (TcpNewReno);

TypeId
TcpNewReno::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpNewReno")
    .SetParent<TcpSocketBase> ()
    .AddConstructor<TcpNewReno> ()
    .AddAttribute ("ReTxThreshold", "Threshold for fast retransmit",
                    UintegerValue (3),
                    MakeUintegerAccessor (&TcpNewReno::m_retxThresh),
                    MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("LimitedTransmit", "Enable limited transmit",
		    BooleanValue (false),
		    MakeBooleanAccessor (&TcpNewReno::m_limitedTx),
		    MakeBooleanChecker ())
    .AddTraceSource ("CongestionWindow",
                     "The TCP connection's congestion window",
                     MakeTraceSourceAccessor (&TcpNewReno::m_cWnd))
	
	//receive socket is created by fork(), and it can not be traced
    .AddTraceSource ("ReceiverWindow",
                     "The TCP receiver congestion window",
                     MakeTraceSourceAccessor (&TcpNewReno::m_receiver_cWnd))
  ;
  return tid;
}

TcpNewReno::TcpNewReno (void)
  : m_retxThresh (3), // mute valgrind, actual value set by the attribute system
    m_inFastRec (false),
    m_limitedTx (false), // mute valgrind, actual value set by the attribute system
    
    m_tot_acks(0),	//  xl
    m_ecn_acks(0),
    m_alpha_last(0.0),
    m_alpha(0.0),

    m_dupacks(0),
    m_rec_tot_acks(0),	//  xl
    m_rec_ecn_acks(0),
    m_rec_alpha_last(0.0),
    m_rec_alpha(0.0),
    m_ictcp_state(SlowStart),
    m_ictcp_condition2_times(0)
 
{
  NS_LOG_FUNCTION (this);
}

TcpNewReno::TcpNewReno (const TcpNewReno& sock)
  : TcpSocketBase (sock),
    m_cWnd (sock.m_cWnd),
    m_ssThresh (sock.m_ssThresh),
    m_initialCWnd (sock.m_initialCWnd),
    m_retxThresh (sock.m_retxThresh),
    m_inFastRec (false),
    m_limitedTx (sock.m_limitedTx),

    m_tot_acks(0),	//  xl
    m_ecn_acks(0),
    m_alpha_last(0.0),
    m_alpha(0.0),
    m_dupacks(0),
    m_receiver_cWnd (sock.m_receiver_cWnd),
    m_receiver_ssThresh(sock.m_receiver_ssThresh),
    m_rec_tot_acks(0),	//  xl
    m_rec_ecn_acks(0),
    m_rec_alpha_last(0.0),
    m_rec_alpha(0.0),
    m_ictcp_state(SlowStart),
    m_ictcp_condition2_times(0)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("Invoked the tcpnewreno copy constructor");
}

TcpNewReno::~TcpNewReno (void)
{
}

/** We initialize m_cWnd from this function, after attributes initialized */
int
TcpNewReno::Listen (void)
{
  NS_LOG_FUNCTION (this);
  InitializeCwnd ();
  return TcpSocketBase::Listen ();
}

/** We initialize m_cWnd from this function, after attributes initialized */
int
TcpNewReno::Connect (const Address & address)
{
  NS_LOG_FUNCTION (this << address);
  InitializeCwnd ();
  return TcpSocketBase::Connect (address);
}
/** We initialize m_cWnd from this function, after attributes initialized */
int
TcpNewReno::Connect (const Address & address, uint32_t flowsize, uint32_t deadline, uint32_t flowid)
{
  NS_LOG_FUNCTION (this << address);
  InitializeCwnd ();
  return TcpSocketBase::Connect (address, flowsize, deadline, flowid);
}


/** Limit the size of in-flight data by cwnd and receiver's rxwin */
uint32_t
TcpNewReno::Window (void)
{
  NS_LOG_FUNCTION (this);
  
  if (m_rtcp_mode == 1) 
      return m_rWnd; //mod by xl 
  NS_LOG_DEBUG ("rwnd="<< m_rWnd <<", cwnd="<< m_cWnd <<".");
  return std::min (m_rWnd.Get (), m_cWnd.Get ());
}

Ptr<TcpSocketBase>
TcpNewReno::Fork (void)
{
  return CopyObject<TcpNewReno> (this);
}

	uint16_t
TcpNewReno::AdvertisedWindowSize ()
{
	  /*if (m_rdtcp_mode == 1  && m_rdtcp_controller_mode == 1 && m_receive_device && m_state == ESTABLISHED ) {
        ReceiverIterator it = m_receive_device->m_rec_infos.find ((uint64_t)this );
        if (it !=  m_receive_device->m_rec_infos.end () && it->second.m_rec_get_flowsize > 0 ) 
	      m_receiver_cWnd = m_receive_device->m_rec_infos[(uint64_t)this ].m_rdtcp_max_win * m_segmentSize;
		else
	      m_receiver_cWnd = 0;
	  }*/
	uint32_t tmp = std::min (m_rxBuffer.MaxBufferSize () - m_rxBuffer.Size (), m_receiver_cWnd.Get()), tmp2 = 65535;
  return std::min (tmp, tmp2);
}


void
TcpNewReno::SetAlpha_DCTCP ()
{
  //NS_LOG_DEBUG ("totack="<<m_tot_acks<< ", ecnack="<<m_ecn_acks <<", at the start of function.");
	if (m_ecn_acks == 0) return;
  m_alpha_last = (double)m_ecn_acks/(double)m_tot_acks; 
  m_alpha = ((1 - ALPHA_WEIGHT) * m_alpha) + (ALPHA_WEIGHT * m_alpha_last); //ALPHA_WEIGHT=1/16 defined in tcp-socket-base.h by xl on Thu Apr  4 23:04:06 CST 2013
  

  int value = floor( ((double) m_cWnd * (1.0 - m_alpha / 2.0)) + 0.5);
  
  NS_LOG_DEBUG ("totack="<<m_tot_acks<< ", ecnack="<<m_ecn_acks <<", alpha : " << m_alpha 
		  << " , Congestion Window modified from " << m_cWnd << " to " << value 
		  << ", rtt= "<<m_rtt->GetCurrentEstimate() );
  m_cWnd    = value;
  if (m_cWnd < m_segmentSize)m_cWnd = m_segmentSize;

  // reset the counts for the next window!
  m_tot_acks = 0; // to avoid division by 0
  m_ecn_acks = 0;
}

void
TcpNewReno::SetAlpha_RDCTCP ()
{
	if (m_rec_ecn_acks == 0) return;
  m_rec_alpha_last = (double)m_rec_ecn_acks/(double)m_rec_tot_acks; 
  m_rec_alpha = ((1 - ALPHA_WEIGHT) * m_rec_alpha) + (ALPHA_WEIGHT * m_rec_alpha_last); //ALPHA_WEIGHT=1/16 defined in tcp-socket-base.h by xl on Thu Apr  4 23:04:06 CST 2013
  

  int value = floor( ((double) m_receiver_cWnd * (1.0 - m_rec_alpha / 2.0)) + 0.5);
  NS_LOG_DEBUG ("connectN:"<<m_receiver_order <<",rec_totack="<<m_rec_tot_acks<< ", rec_ecnack="<<m_rec_ecn_acks 
		  <<", rec_alpha : " << m_rec_alpha << " , Receiver Congestion Window modified from " << m_receiver_cWnd 
		  << " to " << value << ", rtt_r="<< m_rtt->GetCurrentEstimate_r());
  m_receiver_cWnd = value;
  if (m_receiver_cWnd < m_segmentSize)m_receiver_cWnd = m_segmentSize;

  // reset the counts for the next window!
  m_rec_tot_acks = 0; // to avoid division by 0
  m_rec_ecn_acks = 0;
}


void 
TcpNewReno::Modify_Received_Cwnd (bool seqlargerack, uint8_t tcpecnflags, bool immediat_ack )
{
  uint32_t rxsize = m_rxBuffer.Size();
  uint32_t rxavailable = m_rxBuffer.Available();
  //SequenceNumber32 seq = tcpHeader.GetSequenceNumber (); by xl on Sat May 25 02:19:10 CST 2013

  //ecn counter

  //PRINT_STUFF ("Modify_Received_Cwnd, after rxb.add(), m_receiver_cWnd ="<<  m_receiver_cWnd  <<" ,m_rxBuffer: Size() ="<< m_rxBuffer.Size() <<", Available() ="<< m_rxBuffer.Available() <<", MaxBufferSize() ="<< m_rxBuffer.MaxBufferSize() <<", rxb.m_nextRxSeq ="<< m_rxBuffer.m_nextRxSeq  <<", rxb.m_finSeq ="<< m_rxBuffer.m_finSeq << ", rxb.m_gotFin ="<< m_rxBuffer.m_gotFin<<", m_receiver_inFastRec ="<<m_receiver_inFastRec << ", get ecn="<<m_rec_ecn_acks<<", tot="<< m_rec_tot_acks);
  if (tcpecnflags == TcpHeader::ECE) 
      m_receiver_ssThresh = std::max (2 * m_segmentSize, m_receiver_cWnd.Get () / 2);
 
  if (seqlargerack && !m_receiver_inFastRec )
  {
	if (++m_dupacks == m_retxThresh) //no need for 3 times ????xl 
	{//enter fast recovery
		m_receiver_inFastRec = true;
        m_receiver_ssThresh = std::max (2 * m_segmentSize, m_receiver_cWnd.Get () / 2);
        m_receiver_cWnd = m_receiver_ssThresh + 3 * m_segmentSize;
        
		//xPRINT_STUFF ("Modify_Received_Cwnd, Triple dupack. Enter fast recovery mode. Reset m_receiver_cWnd to " << m_receiver_cWnd << ", ssthresh to " << m_receiver_ssThresh << ", expect ack = "<<m_rxBuffer.NextRxSequence ());
	}
	//xPRINT_STUFF ("Modify_Received_Cwnd, lost packet, m_dupacks="<<m_dupacks <<", m_receiver_cWnd to " << m_receiver_cWnd << ", ssthresh to " << m_receiver_ssThresh << ", expect ack = "<<m_rxBuffer.NextRxSequence ());
	//mod by xl, if not in fast recovery, and receive unexpected seq, do not change window.
	NS_LOG_DEBUG ("larger seq>ack("<< m_rxBuffer.NextRxSequence ()<<"), dup times="<<m_dupacks<<",dup thresh="<<m_retxThresh
			<< ", m_receiver_inFastRec ="<<m_receiver_inFastRec );
	return ;

  }
  else if (m_receiver_inFastRec && seqlargerack && rxavailable == 0 )
  {
	  if (m_rdtcp_mode && tcpecnflags == TcpHeader::ECE) return;
      m_receiver_cWnd += m_segmentSize;
      //xPRINT_STUFF ("Modify_Received_Cwnd, Dupack in fast recovery mode. Increase cwnd to " << m_receiver_cWnd <<" ,m_retxThresh = "<< m_retxThresh);
	  return ;
  }
  else if (m_receiver_inFastRec && rxsize > rxavailable && rxavailable > 0)
  {
	  if (m_receiver_cWnd < rxavailable)m_receiver_cWnd = 0;
	  else m_receiver_cWnd -= rxavailable;
	  if (m_rdtcp_mode && tcpecnflags == TcpHeader::ECE && m_receiver_cWnd >= m_segmentSize) return;
	  m_receiver_cWnd += m_segmentSize;
	  //xPRINT_STUFF("Modify_Received_Cwnd, Partial ACK in fast recovery: cwnd set to =[" << m_receiver_cWnd << "]B,ssthresh=" << m_receiver_ssThresh <<",segment="<<m_segmentSize);
	  return ;
 
  }
  else if (m_receiver_inFastRec && rxsize == rxavailable)
  {
	  m_receiver_inFastRec = false;
	  m_dupacks = 0;
      m_receiver_cWnd = std::min (m_receiver_ssThresh , rxsize + m_segmentSize);
	  //xPRINT_STUFF("Modify_Received_Cwnd, Received full ACK. Leaving fast recovery with cwnd set to [" << m_receiver_cWnd <<"]B,ssthresh=" << m_receiver_ssThresh <<",segment="<<m_segmentSize);
 
  }

  if (tcpecnflags == TcpHeader::ECE) 
  {
      //m_receiver_ssThresh = std::max (2 * m_segmentSize, m_receiver_cWnd.Get () / 2);
      //m_delAckCount = m_delAckMaxCount;
	  //xPRINT_STUFF(" Modify_Received_Cwnd ,ece return! ");
	  return; //ece not increase win 
  }

  if (m_receiver_cWnd < m_receiver_ssThresh )
    { // Slow start mode, add one segSize to cWnd. Default m_receiver_ssThresh is 65535. (RFC2001, sec.1)
      //if (m_rxBuffer.Size () > m_rxBuffer.Available () || m_rxBuffer.NextRxSequence () > expectedSeq + p->GetSize ()) by xl on Fri Jun 21 02:24:06 CST 2013
      if (m_delAckCount >= m_delAckMaxCount - 1 || immediat_ack ) //ready to send
          m_receiver_cWnd += m_segmentSize;
	  //xPRINT_STUFF("Modify_Received_Cwnd, In SlowStart, updated to m_receiver_cWnd =[" << m_receiver_cWnd <<"]B, ssthresh=" << m_receiver_ssThresh <<",segment="<<m_segmentSize<<", m_delAckCount = "<<m_delAckCount <<", m_delAckMaxCount = "<<m_delAckMaxCount );
    }
  else
    { // Congestion avoidance mode, increase by (segSize*segSize)/cwnd. (RFC2581, sec.3.1)
      // To increase cwnd for one segSize per RTT, it should be (ackBytes*segSize)/cwnd
      double adder = static_cast<double> (m_segmentSize * m_segmentSize) / m_receiver_cWnd.Get ();
      adder = std::max (1.0, adder);
      m_receiver_cWnd += static_cast<uint32_t> (adder);
	  //xPRINT_STUFF("Modify_Received_Cwnd, In CongAvoid, updated to m_receiver_cWnd =[" << m_receiver_cWnd <<"]B,adder="<<adder<<"B,ssthresh=" << m_receiver_ssThresh <<",segment="<<m_segmentSize); // by xl on Sun May  5 21:04:38 CST 2013
    }

}

double TcpNewReno::ModifyCwnd_ICTCP (double bwa, uint8_t slot_mode)
{
  NS_LOG_FUNCTION (this);
	double r1=0.1, r2=0.5;
	m_be = std::max (m_bm , m_receiver_cWnd.Get ()*8/m_rtt->GetCurrentEstimate_r().GetSeconds ()/1048576 ); //Mbps
    m_d = (m_be-m_bm) /m_be;
	/*NS_LOG_DEBUG (
			"m_receiver_cWnd=" <<m_receiver_cWnd
			<<",m_receive_device->m_dev_ictcp_receiver_win["<< m_receiver_order_on_dev
			<<"]="<<m_receive_device->m_dev_ictcp_receiver_win[m_receiver_order_on_dev] 
			<<", m_be="<<m_be<<"Mbps.m_bm="<< m_bm  
			<<"Mbps,rtt="<< m_rtt->GetCurrentEstimate_r() <<", m_d="<<m_d);*/

	if (m_receiver_cWnd != m_receive_device->m_dev_ictcp_receiver_win[m_receiver_order_on_dev])
        NS_LOG_DEBUG ("ModifyCwnd_ICTCP, different, m_receiver_cWnd("<< m_receiver_cWnd << ") !=dev_ictcp_receiver_win["<< m_receiver_order_on_dev<< "]("<< m_receive_device->m_dev_ictcp_receiver_win[m_receiver_order_on_dev]<< ")");
    m_receiver_cWnd = m_receive_device->m_dev_ictcp_receiver_win[m_receiver_order_on_dev];
	if (m_d <= r1 || m_d <= (double) m_segmentSize / m_receiver_cWnd.Get ()) 
	{
	    Time tmp2RTT = Time::FromDouble (m_receiver_pre_rtt.GetNanoSeconds () * 2, Time::NS);
		double increment = (double) m_segmentSize *8/m_rtt->GetCurrentEstimate_r().GetSeconds ()/1048576;
		if (slot_mode == 2 && Simulator::Now() - m_receiver_last_mod_time >= tmp2RTT){
			if (bwa >= increment) {
			    m_ictcp_state = SlowStart;
			    bwa -= increment;
                m_receiver_cWnd += m_segmentSize;
			    m_receiver_last_mod_time = Simulator::Now();
			    //xPRINT_STUFF ("condition1, slowstart, rw="<<m_receiver_cWnd );
			}
			else {
              double adder = static_cast<double> (m_segmentSize * m_segmentSize) / m_receiver_cWnd.Get ();
              adder = std::max (1.0, adder);
      		  double increment2 = (double) adder*8/m_rtt->GetCurrentEstimate_r().GetSeconds ()/1048576;
			  m_ictcp_state = CongAvoid;
			  if (bwa >= increment2){
			        bwa -= increment2;
                    m_receiver_cWnd += static_cast<uint32_t> (adder);
			        m_receiver_last_mod_time  = Simulator::Now();
			        //xPRINT_STUFF ("condition1, CongAvoid, rw="<<m_receiver_cWnd );
			  }
			}
		}
		m_ictcp_condition2_times = 0;
	}
	else if (m_d > r2){
		if (++m_ictcp_condition2_times == 3)
		{
		    m_ictcp_condition2_times = 0;
            m_receiver_cWnd -= m_segmentSize;
			if (m_receiver_cWnd < 2*m_segmentSize )m_receiver_cWnd = 2 * m_segmentSize;
			m_receiver_last_mod_time  = Simulator::Now();
			//xPRINT_STUFF ("condition2, SlowStart, rw="<<m_receiver_cWnd );
		}
	    m_ictcp_state = SlowStart;
	}
	else{
		m_ictcp_condition2_times = 0;
	    m_ictcp_state = SlowStart;
		//xPRINT_STUFF ("condition3, SlowStart, rw="<<m_receiver_cWnd );
	}

    m_receive_device->m_dev_ictcp_receiver_win[m_receiver_order_on_dev] = m_receiver_cWnd;

    //SendEmptyPacket (TcpHeader::ACK);
	return bwa;
}

void  TcpNewReno::ICTCP_share_decrease (uint16_t	connection_count)
{
  NS_LOG_FUNCTION (this);
	uint64_t avg_recWin = 0, closed_win = 0;
	//PRINT_STUFF ("ICTCP_share_decrease, connection_count="<<connection_count);
    for (uint16_t i=0; i<connection_count; i++) {
	    avg_recWin += m_receive_device->m_dev_ictcp_receiver_win[i];
		if (m_receive_device->m_dev_ictcp_receiver_win[i] == 0) closed_win++; 
	}
	avg_recWin = (double) avg_recWin / (connection_count - closed_win);
    
	for (uint16_t i=0; i<connection_count; i++)
	    if (m_receive_device->m_dev_ictcp_receiver_win[i] > avg_recWin ){
            m_receive_device->m_dev_ictcp_receiver_win[i] = std::max (m_receive_device->m_dev_ictcp_receiver_win[i] - m_segmentSize, 2 * m_segmentSize);
	        //xPRINT_STUFF ("ICTCP_share_decrease, connection_count = "<< connection_count <<",decrease m_receive_device->m_dev_ictcp_receiver_win["<<i<<"] ="<<m_receive_device->m_dev_ictcp_receiver_win[i] );
		}
}

void
TcpNewReno::NewAckECN (const SequenceNumber32& seq)
{
  NS_LOG_FUNCTION (this << seq);
  m_ssThresh = std::max (2 * m_segmentSize, BytesInFlight () / 2);
  /*NS_LOG_DEBUG ("TcpNewReno receieved ACK+ECN for seq " << seq <<
                " cwnd " << m_cWnd <<
				", m_rWnd " <<m_rWnd <<
                " ssthresh " << m_ssThresh);*/

  //for dctcp 
  if (m_txBuffer.HeadSequence ().GetValue() == 1)
  {//first ack is for only one data packet.
    m_tot_acks++; 
    m_ecn_acks++;
	//NS_LOG_DEBUG ("TcpNewReno, newackecn, first ack, tot ack="<< m_tot_acks <<",ecn ack="<<m_ecn_acks );
  }
  else
  {
    uint32_t acked_count = ceil ( (double)((seq.GetValue() - m_txBuffer.HeadSequence ().GetValue()) / m_segmentSize) );
    m_tot_acks += acked_count;
    m_ecn_acks += acked_count;
	//NS_LOG_DEBUG ("TcpNewReno, newackecn, m_txBuffer.HeadSequence ()="<< m_txBuffer.HeadSequence ()
	//		<< ", acked count = "<< acked_count<<",tot ack="<< m_tot_acks <<",ecn ack="<<m_ecn_acks );
  }
  m_sender_pre_state_is_ecn = true;

  // Check for exit condition of fast recovery
  if (m_inFastRec && seq < m_recover)
    { // Partial ACK, partial window deflation (RFC2582 sec.3 bullet #5 paragraph 3)
      m_cWnd -= seq - m_txBuffer.HeadSequence ();
      if (!m_dctcp_mode) 
		  m_cWnd += m_segmentSize;  // increase cwnd by xl on Sat Jun 15 15:31:36 CST 2013
      NS_LOG_DEBUG ("Partial ACK in fast recovery: cwnd set to " << m_cWnd);
      TcpSocketBase::NewAck (seq); // update m_nextTxSequence and send new data if allowed by window
      DoRetransmit (); // Assume the next seq is lost. Retransmit lost packet
      return;
    }
  else if (m_inFastRec && seq >= m_recover)
    { // Full ACK (RFC2582 sec.3 bullet #5 paragraph 2, option 1)
      m_cWnd = std::min (m_ssThresh, BytesInFlight () + m_segmentSize);
      m_inFastRec = false;
      NS_LOG_DEBUG ("Received full ACK. Leaving fast recovery with cwnd set to " << m_cWnd);
    }

  //when face ecn,do not increase window. by xl
  // Increase of cwnd based on current phase (slow start or congestion avoidance)
  /*if (m_cWnd < m_ssThresh)
    { // Slow start mode, add one segSize to cWnd. Default m_ssThresh is 65535. (RFC2001, sec.1)
      if (!m_dctcp_mode) m_cWnd += m_segmentSize;
      NS_LOG_INFO ("In SlowStart, updated to cwnd " << m_cWnd << " ssthresh " << m_ssThresh);
    }
  else
    { // Congestion avoidance mode, increase by (segSize*segSize)/cwnd. (RFC2581, sec.3.1)
      // To increase cwnd for one segSize per RTT, it should be (ackBytes*segSize)/cwnd
      double adder = static_cast<double> (m_segmentSize * m_segmentSize) / m_cWnd.Get ();
      adder = std::max (1.0, adder);
      if (!m_dctcp_mode) m_cWnd += static_cast<uint32_t> (adder);
      NS_LOG_INFO ("In CongAvoid, updated to cwnd " << m_cWnd << " ssthresh " << m_ssThresh);
    }*/

  // Complete newAck processing
  TcpSocketBase::NewAck (seq);
}


/** New ACK (up to seqnum seq) received. Increase cwnd and call TcpSocketBase::NewAck() */
void
TcpNewReno::NewAck (const SequenceNumber32& seq)
{
  NS_LOG_FUNCTION (this << seq);
  /*NS_LOG_DEBUG ("TcpNewReno receieved ACK for seq " << seq <<
                " cwnd " << m_cWnd <<
				", m_rWnd " <<m_rWnd <<
                " ssthresh " << m_ssThresh);*/

  //for dctcp 
  if (m_txBuffer.HeadSequence ().GetValue() == 1)
  {//first ack is for only one data packet.
    m_tot_acks++;
	//NS_LOG_DEBUG ("TcpNewReno, newack, first ack, tot ack="<< m_tot_acks <<",ecn ack="<<m_ecn_acks );
  }
  else
  {
    uint32_t acked_count = ceil ( (double)((seq.GetValue() - m_txBuffer.HeadSequence ().GetValue()) / m_segmentSize) );
    m_tot_acks += acked_count;
	//NS_LOG_DEBUG ("TcpNewReno, newack, acked count = "<< acked_count<<",tot ack="<< m_tot_acks <<",ecn ack="<<m_ecn_acks );
  }
  m_sender_pre_state_is_ecn = false;


  // Check for exit condition of fast recovery
  if (m_inFastRec && seq < m_recover)
    { // Partial ACK, partial window deflation (RFC2582 sec.3 bullet #5 paragraph 3)
      m_cWnd -= seq - m_txBuffer.HeadSequence ();
      m_cWnd += m_segmentSize;  // increase cwnd
      NS_LOG_INFO ("Partial ACK in fast recovery: cwnd set to " << m_cWnd);
      TcpSocketBase::NewAck (seq); // update m_nextTxSequence and send new data if allowed by window
      DoRetransmit (); // Assume the next seq is lost. Retransmit lost packet
      return;
    }
  else if (m_inFastRec && seq >= m_recover)
    { // Full ACK (RFC2582 sec.3 bullet #5 paragraph 2, option 1)
      m_cWnd = std::min (m_ssThresh, BytesInFlight () + m_segmentSize);
      m_inFastRec = false;
      NS_LOG_INFO ("Received full ACK. Leaving fast recovery with cwnd set to " << m_cWnd);
    }

  // Increase of cwnd based on current phase (slow start or congestion avoidance)
  if (m_cWnd < m_ssThresh)
    { // Slow start mode, add one segSize to cWnd. Default m_ssThresh is 65535. (RFC2001, sec.1)
      m_cWnd += m_segmentSize;
      NS_LOG_INFO ("In SlowStart, updated to cwnd " << m_cWnd << " ssthresh " << m_ssThresh);
    }
  else
    { // Congestion avoidance mode, increase by (segSize*segSize)/cwnd. (RFC2581, sec.3.1)
      // To increase cwnd for one segSize per RTT, it should be (ackBytes*segSize)/cwnd
      double adder = static_cast<double> (m_segmentSize * m_segmentSize) / m_cWnd.Get ();
      adder = std::max (1.0, adder);
      m_cWnd += static_cast<uint32_t> (adder);
      NS_LOG_INFO ("In CongAvoid, updated to cwnd " << m_cWnd << " ssthresh " << m_ssThresh);
    }

  // Complete newAck processing
  TcpSocketBase::NewAck (seq);
}


// Receipt of new packet, put into Rx buffer. xl for receiver window
void
TcpNewReno::ReceivedData (Ptr<Packet> p, const TcpHeader& tcpHeader, Ipv4Header::EcnType ipv4ecn)
{
  NS_LOG_FUNCTION (this << tcpHeader);
  NS_LOG_DEBUG ("seq " << tcpHeader.GetSequenceNumber () <<
                " ack " << tcpHeader.GetAckNumber () <<
                " pkt size " << p->GetSize () << 
				" ecn = "<< (ipv4ecn == Ipv4Header::ECN_CE));
  //Balajee ECE xl !!!!!!!!!!!!!!!!!!!!!!1
	  //PRINT_STUFF ("tcpnewreno::receiveddata, showing get ec set in ipheader="<<ipv4ecn<<", m_rtcp_mode = "<<m_rtcp_mode );
  /*if (m_rtt->OnSeq (tcpHeader.GetSequenceNumber () ) && m_rdtcp_mode == 1){
      PRINT_STUFF (m_rtt->GetCurrentEstimate_r() <<" : receiver RTT 2, seq="<< tcpHeader.GetSequenceNumber ()<<", ReceivedData, ipv4ecn: "<< (ipv4ecn == Ipv4Header::CE));
      //SetAlpha_RDCTCP ();
  }*/
  if (m_ictcp_mode == 1){
	  ICTCP_receiveddData (p, tcpHeader, ipv4ecn);
	  return; 
  }
  if (m_rtcp_mode == 1) 
      m_retx_rEvent.Cancel ();
  //else if (m_rdtcp_mode == 1)
	  //m_receive_device->m_rec_infos[(uint64_t)this ].m_rec_get_flowsize = tcpHeader.GetRemainFlowSize();
  
	  
  m_rec_tot_acks++;	//  xl for rdctcp
  uint8_t tcpflags = TcpHeader::NONE;
  if (ipv4ecn == Ipv4Header::ECN_CE)
  {
    tcpflags = TcpHeader::ECE;
    m_rec_ecn_acks++;
	//PRINT_STUFF("receiveddata, get ipv4ecn="<<m_rec_ecn_acks<<", tot="<< m_rec_tot_acks);
  }


  // Put into Rx buffer
  SequenceNumber32 expectedSeq = m_rxBuffer.NextRxSequence ();
  if (!m_rxBuffer.Add (p, tcpHeader))
    { // Insert failed: No data or RX buffer full
	 NS_LOG_DEBUG ("add fail. pre ack = "<<expectedSeq <<", current ack = "<<m_rxBuffer.NextRxSequence () <<", received seq = "<<tcpHeader.GetSequenceNumber ());

	  if (m_rdtcp_mode == 1  && m_rdtcp_controller_mode == 1 ) {
          //m_delay_ack_Event = Simulator::Schedule ( 
		//	      GetDelayTime(), 
		//		  &TcpSocketBase::SendEmptyPacket, this, TcpHeader::ACK | tcpflags );
	      SignalSendEmptyPacket (TcpHeader::ACK | tcpflags );
	  }
	  else
          SendEmptyPacket (TcpHeader::ACK | tcpflags);
      return;
    }
  
  //PRINT_STUFF("ReceivedData, pre ack = "<<expectedSeq <<", current ack = "<<m_rxBuffer.NextRxSequence () <<", received seq = "<<tcpHeader.GetSequenceNumber ());
  m_received_dataN += p->GetSize ();
  if (m_rdtcp_mode == 1) {
	  m_receive_device->m_rec_infos[(uint64_t)this ].m_rec_get_flowsize -= p->GetSize ();
	  if (m_receive_device->m_rec_infos[(uint64_t)this ].m_rec_get_flowsize > 0)
  	    m_receive_device->m_rec_infos[(uint64_t)this ].m_rec_get_flowsize_Reciprocal = 
		      (double)1/m_receive_device->m_rec_infos[(uint64_t)this ].m_rec_get_flowsize ;
	  else { 
		  m_receive_device->m_rec_infos[(uint64_t)this ].m_rec_get_flowsize_Reciprocal = 0;
		  NS_LOG_DEBUG ("afterrec,flowsize=0,reciprocal=0!");
	  }

  //NS_LOG_DEBUG ("receiveddata rec num:"<< m_receiver_order << ", received data size="<< p->GetSize () <<", flowsize="
//		  << m_receive_device->m_rec_infos[(uint64_t)this ].m_rec_get_flowsize 
//		  <<", my reciprocal="
//		  << m_receive_device->m_rec_infos[(uint64_t)this ].m_rec_get_flowsize_Reciprocal
//		  <<", m_receive_device->m_rec_infos.size()="<< m_receive_device->m_rec_infos.size() ) ;



		/*if (m_node->GetId () == 872)
     	NS_LOG_DEBUG ("before, receiveddata in win = "<<ceil ((double)p->GetSize () / m_segmentSize)
			<<", receiveddata in byte = " <<p->GetSize ()
			<<", m_incre_bytes =" <<m_incre_bytes 
			<<", rec upper=" <<m_recCwnd_upper 
			<<", header seq="<<tcpHeader.GetSequenceNumber ().GetValue()
			<<", upper - seq="<<ceil ((double)( m_recCwnd_upper - tcpHeader.GetSequenceNumber ().GetValue() ) / m_segmentSize)
			<<", m_flight_wins = "<< m_receive_device->m_flight_wins 
			<<", m_incre_bytes =" << m_incre_bytes 
			);   */
      if (m_receive_device && m_rdtcp_mode == 1  && m_rdtcp_controller_mode == 1 ) {
		if (m_receive_device->m_rec_infos[(uint64_t)this ].m_rec_get_flowsize_Reciprocal == 0){
    	    //m_receive_device->m_flight_wins    -= ceil ((double)( m_recCwnd_upper - tcpHeader.GetSequenceNumber ().GetValue() ) / m_segmentSize);
			//if (p->GetSize () <= m_incre_bytes ) 
				//;//not take into account.
     	        /*NS_LOG_DEBUG ("problem. p->GetSize ():"<< p->GetSize () 
						<<" <= m_incre_bytes:"<<m_incre_bytes
			            <<", m_flight_wins = "<< m_receive_device->m_flight_wins 
			            <<", rec upper=" <<m_recCwnd_upper 
            			<<", header seq="<<tcpHeader.GetSequenceNumber ().GetValue()
	            		<<",floor: upper - seq="<<floor ((double)( m_recCwnd_upper - tcpHeader.GetSequenceNumber ().GetValue() ) / m_segmentSize)
						); */
			//else //use a new segment window, so substract a window
				m_receive_device->m_flight_wins -= floor ((double)( m_recCwnd_upper - tcpHeader.GetSequenceNumber ().GetValue() ) / m_segmentSize);
		
			/*if (m_node->GetId () == 872)
			NS_LOG_DEBUG ("fin. receiveddata in win = "<<ceil ((double)p->GetSize () / m_segmentSize)
			<<", receiveddata in byte = " <<p->GetSize ()
			<<", m_incre_bytes =" <<m_incre_bytes 
			<<", rec upper=" <<m_recCwnd_upper 
			<<", header seq="<<tcpHeader.GetSequenceNumber ().GetValue()
			<<", upper - seq="<<ceil ((double)( m_recCwnd_upper - tcpHeader.GetSequenceNumber ().GetValue() ) / m_segmentSize)
			<<", m_flight_wins = "<< m_receive_device->m_flight_wins 
			);*/

		}
		else
    	    m_receive_device->m_flight_wins    -= ceil ((double)p->GetSize () / m_segmentSize);
    	if (m_receive_device->m_flight_wins < 0) m_receive_device->m_flight_wins = 0;
  
		/*if (m_node->GetId () == 872)
    	NS_LOG_DEBUG ("after, receiveddata in win = "<<ceil ((double)p->GetSize () / m_segmentSize)
			<<", receiveddata in byte = " <<p->GetSize ()
			<<", rec upper=" <<m_recCwnd_upper 
			<<", header seq="<<tcpHeader.GetSequenceNumber ().GetValue()
			<<", upper - seq(byte)="<< m_recCwnd_upper - tcpHeader.GetSequenceNumber ().GetValue()
			<<", upper - seq(win)="<<ceil ((double)( m_recCwnd_upper - tcpHeader.GetSequenceNumber ().GetValue() ) / m_segmentSize)
			<<", m_flight_wins = "<< m_receive_device->m_flight_wins );
			*/
      }
  }

  if (m_rtcp_mode == 1) {
	  //for receiver congestion window
      Modify_Received_Cwnd ( tcpHeader.GetSequenceNumber () > expectedSeq , tcpflags, 
			  (m_rxBuffer.Size () > m_rxBuffer.Available () || m_rxBuffer.NextRxSequence () > expectedSeq + p->GetSize ())); 
	  //for rdtcp controller
	  //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	  if (m_rdtcp_mode == 1  && m_rdtcp_controller_mode == 1 ) {
		  /*NS_LOG_DEBUG("m_receive_device->m_rec_infos[(uint64_t)this ].m_rdtcp_max_win="
				  <<m_receive_device->m_rec_infos[(uint64_t)this ].m_rdtcp_max_win
				  <<", m_receive_device->m_rdtcp_closed_count ="<<m_receive_device->m_rdtcp_closed_count 
				  <<", m_receive_device->m_dev_socket_cout="<<m_receive_device->m_dev_socket_cout
				  );*/
	      //m_receiver_cWnd = m_receive_device->m_rec_infos[(uint64_t)this ].m_rdtcp_max_win * m_segmentSize;
	      m_receiver_cWnd = std::min (m_receive_device->m_rec_infos[(uint64_t)this ].m_rdtcp_max_win * m_segmentSize,
				  m_receiver_cWnd.Get() );
	  }
  }
  // Now send a new ACK packet acknowledging all received and delivered data
  if (m_rxBuffer.Size () > m_rxBuffer.Available () || m_rxBuffer.NextRxSequence () > expectedSeq + p->GetSize ())
    { // A gap exists in the buffer, or we filled a gap: Always ACK
      //m_retxEvent = Simulator::Schedule (m_rto, &TcpSocketBase::SendEmptyPacket, this, tcpflags);
      //NS_LOG_DEBUG ("m_rxBuffer.Size ()="<< m_rxBuffer.Size()<<", m_rxBuffer.Available ()="<< m_rxBuffer.Available ()<<",m_rxBuffer.NextRxSequence ()="<<m_rxBuffer.NextRxSequence ()<<", expectedSeq["<< expectedSeq<<"] + p->GetSize ()["<<  p->GetSize () <<"]="<<expectedSeq + p->GetSize () );
	  if (m_rdtcp_mode == 1  && m_rdtcp_controller_mode == 1 ) {
          //m_delay_ack_Event = Simulator::Schedule ( 
		//	      GetDelayTime(), 
		//		  &TcpSocketBase::SendEmptyPacket, this, TcpHeader::ACK | tcpflags );
	      SignalSendEmptyPacket (TcpHeader::ACK | tcpflags);
	  }
	  else
          SendEmptyPacket (TcpHeader::ACK | tcpflags);
    }
  /*else if (m_dctcp_mode)
  {//two state machine for delay ack.
	  if ( m_receiver_pre_state_is_ecn ==  true && ipv4ecn != Ipv4Header::ECN_CE )
	    {
            NS_LOG_DEBUG ("dctcp, pre ecn, current no ecn. " );
		    m_receiver_pre_state_is_ecn = false;
          m_delAckEvent.Cancel ();
          m_delAckCount = 0;
      		SendEmptyPacket (TcpHeader::ACK | TcpHeader::ECE);
	    }
	  else if ( m_receiver_pre_state_is_ecn == false && ipv4ecn == Ipv4Header::ECN_CE )
	    {
            NS_LOG_DEBUG ("dctcp, pre no ecn, current ecn. " );
		    m_receiver_pre_state_is_ecn = true;
          m_delAckEvent.Cancel ();
          m_delAckCount = 0;
      		SendEmptyPacket (TcpHeader::ACK);
	    }
	  else if (m_receiver_send_ack_times < m_delAckMaxCount)
        {
            NS_LOG_DEBUG ("dctcp, pre == current ecn("<< ipv4ecn <<"). m_receiver_send_ack_times("<< m_receiver_send_ack_times
					<<") <  m_delAckMaxCount("<< m_delAckMaxCount<<")" );
          m_delAckEvent.Cancel ();
          m_delAckCount = 0;
          SendEmptyPacket (TcpHeader::ACK | tcpflags);
        }
	  else if (++m_delAckCount >= m_delAckMaxCount)
        {
            NS_LOG_DEBUG ("dctcp, pre == current ecn("<< ipv4ecn <<"). deltimes("<<m_delAckCount  <<") >= delmaxcount(" <<m_delAckMaxCount<<")");
          m_delAckEvent.Cancel ();
          m_delAckCount = 0;
          SendEmptyPacket (TcpHeader::ACK | tcpflags);
        }
      else if (m_delAckEvent.IsExpired ())
        {
            NS_LOG_DEBUG ("dctcp, pre == current ecn("<< ipv4ecn <<"). deltimes("<<m_delAckCount  <<") < delmaxcount(" <<m_delAckMaxCount
					<<"), , delevent expired.");
          m_delAckEvent = Simulator::Schedule (m_delAckTimeout,
                                               &TcpSocketBase::DelAckTimeout, this);
          NS_LOG_DEBUG (this << " scheduled delayed ACK at " << (Simulator::Now () + Simulator::GetDelayLeft (m_delAckEvent)).GetSeconds ());
        }

  }*/
  else
    { // In-sequence packet: ACK if delayed ack count allows
     if (m_receiver_send_ack_times < m_delAckMaxCount)
        {//for correct delay ack.
            //NS_LOG_DEBUG ("newrenotcp, receiver send ack("<<m_receiver_send_ack_times  <<") < delmaxcount("<< m_delAckMaxCount<<"). " );
          m_delAckEvent.Cancel ();
          m_delAckCount = 0;
	      if (m_rdtcp_mode == 1  && m_rdtcp_controller_mode == 1 ) {
              //m_delay_ack_Event = Simulator::Schedule ( 
			   //   GetDelayTime(), 
		    	//  &TcpSocketBase::SendEmptyPacket, this, TcpHeader::ACK | tcpflags );
	          SignalSendEmptyPacket (TcpHeader::ACK | tcpflags);
	      }
    	  else
              SendEmptyPacket (TcpHeader::ACK | tcpflags);
        }
	  else if (++m_delAckCount >= m_delAckMaxCount)
        {
	      //NS_LOG_DEBUG("newrenotcp, m_delAckCount ="<<m_delAckCount <<", m_delAckMaxCount = "<<m_delAckMaxCount <<",m_receiver_cWnd="<<m_receiver_cWnd);
          m_delAckEvent.Cancel ();
          m_delAckCount = 0;

	      if (m_rdtcp_mode == 1  && m_rdtcp_controller_mode == 1 ) {
              //m_delay_ack_Event = Simulator::Schedule ( 
			   //   GetDelayTime(), 
		    	// &TcpSocketBase::SendEmptyPacket, this, TcpHeader::ACK | tcpflags );
	          SignalSendEmptyPacket  (TcpHeader::ACK | tcpflags);
	      }
    	  else
              SendEmptyPacket (TcpHeader::ACK | tcpflags);
        }
      else if (m_delAckEvent.IsExpired ())
        {//XL ?
		  /*if (m_rtcp_mode) 
		      m_delAckTimeout = m_rtt->RetransmitTimeout_r (1);*/ //add by xl
	      //xPRINT_STUFF("8888888888888888888, m_delAckCount = "<<m_delAckCount <<", m_delAckMaxCount = "<<m_delAckMaxCount <<",m_delAckTimeout="<<m_delAckTimeout<<", m_receiver_cWnd= "<<m_receiver_cWnd);
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
	      //if (m_rdtcp_mode == 1  && m_rdtcp_controller_mode == 1 ) {
              //m_delay_ack_Event = Simulator::Schedule ( 
			   //   GetDelayTime(), 
		   		//  &TcpSocketBase::DoPeerClose , this );
	      //}
    	  //else
		  //close no need delay! xl.
              DoPeerClose ();
        }
    }
}


void TcpNewReno::ICTCP_receiveddData (Ptr<Packet> p, const TcpHeader& tcpHeader, Ipv4Header::EcnType ipv4ecn) //xl
{//just receive data, not return ack, except fin packet.
  NS_LOG_FUNCTION (this);
	  
  // Put into Rx buffer
  SequenceNumber32 expectedSeq = m_rxBuffer.NextRxSequence ();
  if (!m_rxBuffer.Add (p, tcpHeader))
    { // Insert failed: No data or RX buffer full
	 //xPRINT_STUFF("ReceivedData, add fail. pre ack = "<<expectedSeq <<", current ack = "<<m_rxBuffer.NextRxSequence () <<", received seq = "<<tcpHeader.GetSequenceNumber ());
      SendEmptyPacket (TcpHeader::ACK);
      return;
    }
  
  //PRINT_STUFF("ReceivedData, pre ack = "<<expectedSeq <<", current ack = "<<m_rxBuffer.NextRxSequence () <<", received seq = "<<tcpHeader.GetSequenceNumber ());
  m_received_dataN += p->GetSize ();

  // Now send a new ACK packet acknowledging all received and delivered data
  if (m_rxBuffer.Size () > m_rxBuffer.Available () || m_rxBuffer.NextRxSequence () > expectedSeq + p->GetSize ())
    { // A gap exists in the buffer, or we filled a gap: Always ACK
      //PRINT_STUFF (" m_rxBuffer.Size ()="<< m_rxBuffer.Size()<<", m_rxBuffer.Available ()="<< m_rxBuffer.Available ()<<",m_rxBuffer.NextRxSequence ()="<<m_rxBuffer.NextRxSequence ()<<", expectedSeq["<< expectedSeq<<"] + p->GetSize ()["<<  p->GetSize () <<"]="<<expectedSeq + p->GetSize () );
      SendEmptyPacket (TcpHeader::ACK);
    }
  else
    { // In-sequence packet: ACK if delayed ack count allows
	      //xPRINT_STUFF("7777777777777777777, m_delAckCount ="<<m_delAckCount <<", m_delAckMaxCount = "<<m_delAckMaxCount <<",m_receiver_cWnd="<<m_receiver_cWnd);
		  //checkRwnd (); //xl
          SendEmptyPacket (TcpHeader::ACK);
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


/** Cut cwnd and enter fast recovery mode upon triple dupack */
void
TcpNewReno::DupAck (const TcpHeader& t, uint32_t count)
{
  NS_LOG_FUNCTION (this << count);
  
  //for dctcp, m_dctcp_mode  
  if (t.GetFlags() & TcpHeader::ECE)
  {//duplicate ack means just one packet.
    m_tot_acks++; 
    m_ecn_acks++;
  }
  else
    m_tot_acks++; 

  if (count == m_retxThresh && !m_inFastRec)
    { // triple duplicate ack triggers fast retransmit (RFC2582 sec.3 bullet #1)
      m_ssThresh = std::max (2 * m_segmentSize, BytesInFlight () / 2);
      m_cWnd = m_ssThresh + 3 * m_segmentSize;
      m_recover = m_highTxMark;
      m_inFastRec = true;
      NS_LOG_DEBUG ("Triple dupack. Enter fast recovery mode. Reset cwnd to " << m_cWnd <<
                   ", ssthresh to " << m_ssThresh << " at fast recovery seqnum " << m_recover);
      DoRetransmit ();
    }
  else if (m_inFastRec)
    { // Increase cwnd for every additional dupack (RFC2582, sec.3 bullet #3)
	  if (!(m_dctcp_mode && (t.GetFlags() & TcpHeader::ECE)))
        m_cWnd += m_segmentSize;
      NS_LOG_DEBUG ("Dupack in fast recovery mode. Increase cwnd to " << m_cWnd<<", count="<<count);
      SendPendingData (m_connected);
    }
  else if (!m_inFastRec && m_limitedTx && m_txBuffer.SizeFromSequence (m_nextTxSequence) > 0)
    { // RFC3042 Limited transmit: Send a new packet for each duplicated ACK before fast retransmit
      NS_LOG_DEBUG ("Limited transmit");
      uint32_t sz = SendDataPacket (m_nextTxSequence, m_segmentSize, true);
      m_nextTxSequence += sz;                    // Advance next tx sequence
    };
}


/** Retransmit timeout */
void
TcpNewReno::Retransmit (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC (this << " ReTxTimeout Expired at time " << Simulator::Now ().GetSeconds ());
  m_inFastRec = false;

  // If erroneous timeout in closed/timed-wait state, just return
  if (m_state == CLOSED || m_state == TIME_WAIT) return;
  // If all data are received (non-closing socket and nothing to send), just return
  if (m_state <= ESTABLISHED && m_txBuffer.HeadSequence () >= m_highTxMark) return;

  // According to RFC2581 sec.3.1, upon RTO, ssthresh is set to half of flight
  // size and cwnd is set to 1*MSS, then the lost packet is retransmitted and
  // TCP back to slow start
  m_ssThresh = std::max (2 * m_segmentSize, BytesInFlight () / 2);
  m_cWnd = m_segmentSize;
  m_nextTxSequence = m_txBuffer.HeadSequence (); // Restart from highest Ack
  NS_LOG_INFO ("RTO. Reset cwnd to " << m_cWnd <<
               ", ssthresh to " << m_ssThresh << ", restart from seqnum " << m_nextTxSequence);
  m_rtt->IncreaseMultiplier ();             // Double the next RTO
  DoRetransmit ();                          // Retransmit the packet
}

void
TcpNewReno::Retransmit_r (uint8_t flags)
{
  NS_LOG_FUNCTION (this);
  m_receiver_inFastRec = false;
  m_dupacks =0;//because sender has no problem of delack, so it does no care m_dupacks, but receiver has to. xl

  // If erroneous timeout in closed/timed-wait state, just return
  if (m_state == CLOSED || m_state == TIME_WAIT) return;
  // If all data are received (non-closing socket and nothing to send), just return
  //if (m_state <= ESTABLISHED && m_txBuffer.HeadSequence () >= m_highTxMark) return;

  // According to RFC2581 sec.3.1, upon RTO, ssthresh is set to half of flight
  // size and cwnd is set to 1*MSS, then the lost packet is retransmitted and
  // TCP back to slow start
  /*NS_LOG_DEBUG("TcpNewReno,retransmit_r ,m_receiver_order_on_dev="<<m_receiver_order_on_dev
		  <<", m_receive_device="<<m_receive_device 
		  );*/
  m_receiver_ssThresh = std::max (2 * m_segmentSize, m_receiver_cWnd.Get () / 2);
  if (m_ictcp_mode){
      m_receiver_cWnd = 2 * m_segmentSize;
      m_receive_device->m_dev_ictcp_receiver_win[m_receiver_order_on_dev] = m_receiver_cWnd;
  }
  else
      m_receiver_cWnd = m_segmentSize;
  m_delAckCount = m_delAckMaxCount - 1; //win = seg,sender can just send only one packet, receiver need respond immediately
  
  NS_LOG_DEBUG("TcpNewReno,retransmit_r ,ack = "<<  m_rxBuffer.NextRxSequence ()<<", receiver cwnd=" << m_receiver_cWnd << "B,ssthresh=" << m_receiver_ssThresh <<",segment="<<m_segmentSize<<", m_retxEvent = "<< &m_retxEvent<<", isexpire="<<m_retxEvent.IsExpired () );
  m_rtt->IncreaseMultiplier_r ();             // Double the next RTO
  //DoRetransmit ();                          // Retransmit the packet by xl on Tue Jun  4 19:13:10 CST 2013
  if (m_rdtcp_mode == 1  && m_rdtcp_controller_mode == 1 ) {
	  Time random_delay = NanoSeconds ( (Simulator::Now().GetNanoSeconds() % 10) * m_rtt->GetCurrentEstimate_r().GetNanoSeconds() );
      m_delay_ack_Event = Simulator::Schedule ( 
			      random_delay , 
				  &TcpSocketBase::SignalSendEmptyPacket, this, flags | TcpHeader::PSH );
	  //SignalSendEmptyPacket (flags | TcpHeader::PSH);
  }
  else
     SendEmptyPacket (flags | TcpHeader::PSH);
      //SendEmptyPacket (flags);
}

void
TcpNewReno::SetSegSize (uint32_t size)
{
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "TcpNewReno::SetSegSize() cannot change segment size after connection started.");
  m_segmentSize = size;
}

void
TcpNewReno::SetSSThresh (uint32_t threshold)
{
  m_ssThresh = threshold;
  m_receiver_ssThresh = threshold;
}

uint32_t
TcpNewReno::GetSSThresh (void) const
{
  return m_ssThresh;
}

void
TcpNewReno::SetInitialCwnd (uint32_t cwnd)
{
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "TcpNewReno::SetInitialCwnd() cannot change initial cwnd after connection started.");
  m_initialCWnd = cwnd;
}

uint32_t
TcpNewReno::GetInitialCwnd (void) const
{
  return m_initialCWnd;
}

void 
TcpNewReno::InitializeCwnd (void)
{
  /*
   * Initialize congestion window, default to 1 MSS (RFC2001, sec.1) and must
   * not be larger than 2 MSS (RFC2581, sec.3.1). Both m_initiaCWnd and
   * m_segmentSize are set by the attribute system in ns3::TcpSocket.
   */
  m_cWnd = m_initialCWnd * m_segmentSize;
  if (m_rtcp_mode == 1){
	  if (m_ictcp_mode == 0)
         m_receiver_cWnd = m_initialCWnd * m_segmentSize; //xl for receiver, this must be 2, because ReceivedData has delayed ack for severai times
	  else if(m_ictcp_mode == 1){
		 //xPRINT_STUFF ("InitializeCwnd, m_receive_device->m_dev_ictcp_receiver_win["<< m_receiver_order<<"]="<<m_receive_device->m_dev_ictcp_receiver_win[m_receiver_order]);
         m_receiver_cWnd = 2 * m_segmentSize;
	  }
  }
  else 
     m_receiver_cWnd = m_maxWinSize; //xl for receiver

  NS_LOG_DEBUG("after initial cwnd="<<m_cWnd<<", m_receiver_cWnd ="<<m_receiver_cWnd );


}

} // namespace ns3
