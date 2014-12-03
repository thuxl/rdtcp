/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
//
// Copyright (c) 2006 Georgia Tech Research Corporation
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Author: Rajib Bhattacharjea<raj.b@gatech.edu>
//


// Ported from:
// Georgia Tech Network Simulator - Round Trip Time Estimation Class
// George F. Riley.  Georgia Tech, Spring 2002

// Implements several variations of round trip time estimators

#include <iostream>

#include "rtt-estimator.h"
#include "ns3/simulator.h"
#include "ns3/double.h"
#include "ns3/integer.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("RttEstimator");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (RttEstimator);

TypeId 
RttEstimator::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RttEstimator")
    .SetParent<Object> ()
    .AddAttribute ("MaxMultiplier", 
                   "Maximum RTO Multiplier",
                   UintegerValue (64),
                   MakeUintegerAccessor (&RttEstimator::m_maxMultiplier),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("InitialEstimation", 
                   "Initial RTT estimation",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&RttEstimator::m_initialEstimatedRtt),
                   MakeTimeChecker ())
    .AddAttribute ("MinRTO", 
                   "Minimum retransmit timeout value",
                   TimeValue (Seconds (0.2)), // RFC2988 says min RTO=1 sec, but Linux uses 200ms. See http://www.postel.org/pipermail/end2end-interest/2004-November/004402.html
                   MakeTimeAccessor (&RttEstimator::SetMinRto,
                                     &RttEstimator::GetMinRto),
                   MakeTimeChecker ())
  ;
  return tid;
}

void 
RttEstimator::SetMinRto (Time minRto)
{
  NS_LOG_FUNCTION (this << minRto);
  m_minRto = minRto;
}
Time 
RttEstimator::GetMinRto (void) const
{
  return m_minRto;
}
void 
RttEstimator::SetCurrentEstimate (Time estimate)
{
  NS_LOG_FUNCTION (this << estimate);
  m_currentEstimatedRtt = estimate;
}
Time 
RttEstimator::GetCurrentEstimate (void) const
{
  return m_currentEstimatedRtt;
}

Time 
RttEstimator::GetCurrentEstimate_r (void) const
{//rtcp
  return m_currentEstimatedRtt_r;
}



//RttHistory methods
RttHistory::RttHistory (SequenceNumber32 s, uint32_t c, Time t)
  : seq (s), count (c), time (t), retx (false)
{
  NS_LOG_FUNCTION (this);
}

RttHistory::RttHistory (const RttHistory& h)
  : seq (h.seq), count (h.count), time (h.time), retx (h.retx)
{
  NS_LOG_FUNCTION (this);
}

// Base class methods

RttEstimator::RttEstimator ()
  : m_next (1), m_history (),
    m_nSamples (0),
    m_multiplier (1),
    
	m_multiplier_r (1), //xl
    m_last_ack (0),       //xl
    m_history_r (),
    m_nSamples_r (0)
{ 
  NS_LOG_FUNCTION (this);
  //note next=1 everywhere since first segment will have sequence 1
  
  // We need attributes initialized here, not later, so use the 
  // ConstructSelf() technique documented in the manual
  ObjectBase::ConstructSelf (AttributeConstructionList ());
  m_currentEstimatedRtt = m_initialEstimatedRtt;
  m_currentEstimatedRtt_r = m_initialEstimatedRtt; //xl
  NS_LOG_DEBUG ("Initialize m_currentEstimatedRtt to " << m_currentEstimatedRtt.GetSeconds () << " sec.");
}

RttEstimator::RttEstimator (const RttEstimator& c)
  : Object (c), m_next (c.m_next), m_history (c.m_history), 
    m_maxMultiplier (c.m_maxMultiplier), 
    m_initialEstimatedRtt (c.m_initialEstimatedRtt),
    m_currentEstimatedRtt (c.m_currentEstimatedRtt), m_minRto (c.m_minRto),
    m_nSamples (c.m_nSamples), m_multiplier (c.m_multiplier),
	
	m_multiplier_r (c.m_multiplier_r), //xl
    m_last_ack (c.m_last_ack),     //xl
    m_history_r (c.m_history_r), 
    m_nSamples_r (c.m_nSamples_r),
    m_currentEstimatedRtt_r (c.m_currentEstimatedRtt_r)
{
  NS_LOG_FUNCTION (this);
}

RttEstimator::~RttEstimator ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
RttEstimator::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void RttEstimator::SentSeq (SequenceNumber32 seq, uint32_t size)
{ 
  NS_LOG_FUNCTION (this << seq << size);
  // Note that a particular sequence has been sent
  if (seq == m_next)
    { // This is the next expected one, just log at end
      m_history.push_back (RttHistory (seq, size, Simulator::Now () ));
      m_next = seq + SequenceNumber32 (size); // Update next expected
    }
  else
    { // This is a retransmit, find in list and mark as re-tx
      for (RttHistory_t::iterator i = m_history.begin (); i != m_history.end (); ++i)
        {
          if ((seq >= i->seq) && (seq < (i->seq + SequenceNumber32 (i->count))))
            { // Found it
              i->retx = true;
              // One final test..be sure this re-tx does not extend "next"
              if ((seq + SequenceNumber32 (size)) > m_next)
                {
                  m_next = seq + SequenceNumber32 (size);
                  i->count = ((seq + SequenceNumber32 (size)) - i->seq); // And update count in hist
                }
              break;
            }
        }
    }
}

void RttEstimator::SentAck (SequenceNumber32 ack, uint32_t distance, bool fin_sign)
{ //xl for receiver to estimate RTT
	
  /*for (RttHistory_t::iterator i = m_history_r.begin (); i != m_history_r.end (); ++i)
	  NS_LOG_DEBUG ("sentack:  before sentack ack = "<< ack.GetValue()<<", preack= "<< m_last_ack <<", iterator i->ack = "<<i->seq<<", i->expect seq = "<< i->count<<", i->time = "<<i->time);*/


  if (ack > m_last_ack || (fin_sign && ack == m_last_ack))
    { 
      //if (ack == m_last_ack) {m_history_r.pop_back(); PRINT_STATE ("sentack, rtt for notify, duplication ack = "<< ack);} 
      m_last_ack = ack; 
      m_history_r.push_back (RttHistory (ack, ack.GetValue()+distance, Simulator::Now () ));
    }
  else if (ack == m_last_ack){
	   //dis = 0 or ack == last_ack means retransmit, we don't estimate retransmit RTT
      std::deque<uint32_t> delete_r;  uint32_t k =0;
	  for (RttHistory_t::iterator i = m_history_r.begin (); i != m_history_r.end (); ++i, ++k)
	  {
          if (ack == i->seq /*&& ack + SequenceNumber32 (distance) == SequenceNumber32 (i->count)*/)
	      {
	          //PRINT_STATE ("sentack:  erase 1  i->ack = "<< i->seq<<", i->expect seq = "<< i->count<<", i->time = "<<i->time);
			  delete_r.push_back(k);
              //m_history_r.erase (i);
	    	  //return ;
	      }
		  else if (ack + SequenceNumber32 (distance) <= SequenceNumber32(i->count))
		  { 
			  /*if (i == m_history_r.end()){
	                 PRINT_STATE ("sentack:  erase  i->ack = "<< i->seq<<", i->expect seq = "<< i->count<<", i->time = "<<i->time);
				  m_history_r.erase (i);  
				  break ;
			  }*/
	          //PRINT_STATE ("sentack:  erase 2 i->ack = "<< i->seq<<", i->expect seq = "<< i->count<<", i->time = "<<i->time);
			  delete_r.push_back(k);
		  } 
	  }
	  for (std::deque<uint32_t>::iterator i = delete_r.begin (); i != delete_r.end (); ++i)
		  m_history_r.erase (m_history_r.begin () + *i);
  }


  /*for (RttHistory_t::iterator i = m_history_r.begin (); i != m_history_r.end (); ++i)
	  NS_LOG_DEBUG ("sentack: after sentack, ack = "<< ack<<", preack= "<< m_last_ack <<", iterator i->ack = "<<i->seq<<", i->expect seq = "<< i->count<<", i->time = "<<i->time);*/
  
}
Time RttEstimator::AckSeq (SequenceNumber32 ackSeq)
{ 
  NS_LOG_FUNCTION (this << ackSeq);
  // An ack has been received, calculate rtt and log this measurement
  // Note we use a linear search (O(n)) for this since for the common
  // case the ack'ed packet will be at the head of the list
  Time m = Seconds (0.0);
  if (m_history.size () == 0) return (m);    // No pending history, just exit
  
  /*RttHistory& h = m_history.front ();
  if (!h.retx && ackSeq >= (h.seq + SequenceNumber32 (h.count)))
    { // Ok to use this sample
      m = Simulator::Now () - h.time; // Elapsed time
      Measurement (m);                // Log the measurement
      ResetMultiplier ();             // Reset multiplier on valid measurement
    } */ //del by xl, some bugs.

  int32_t k = 0;
  for (RttHistory_t::iterator i = m_history.begin (); i != m_history.end (); ++i, ++k)
  {
      if (!i->retx && ackSeq >= (i->seq + SequenceNumber32 (i->count)))
		  continue;
	  else break;
  }

  //PRINT_STATE ("onack, k="<<k);
  if (k - 1 >= 0)
  {
	  RttHistory& h = m_history.at(k - 1);
      m = Simulator::Now () - h.time; // Elapsed time
      //PRINT_STATE ("ackseq, find, h.retx = "<< h.retx<<", ack = "<<ackSeq<<", h.count = "<<h.count<<" + h.seq = "<<h.seq<<", h.time = "<<h.time <<", m = "<<m);
      Measurement (m);                // Log the measurement
      ResetMultiplier ();             // Reset multiplier on valid measurement
  }

  // Now delete all ack history with seq <= ack
  while(m_history.size () > 0)
    {
      RttHistory& h = m_history.front ();
      if ((h.seq + SequenceNumber32 (h.count)) > ackSeq) break;               // Done removing
      m_history.pop_front (); // Remove
    }
  return m;
}

bool RttEstimator::OnSeq (SequenceNumber32 Seq)
{//xl for receiver to estimate RTT

  Time m = Seconds (0.0);
  bool ret = false;
 //NS_LOG_DEBUG ("rtt for onseq, seq = "<<Seq<<", m_history_r.size ()="<<m_history_r.size ());
  if (m_history_r.size () == 0) return false;    // No pending history, just exit
 
  /*for (RttHistory_t::iterator i = m_history_r.begin (); i != m_history_r.end (); ++i)
	  NS_LOG_DEBUG ("onseq:before onseq, for seq = "<< Seq<<", iterator i->ack = "<<i->seq<<", i->expect seq = "<< i->count<<", i->time = "<<i->time);*/


  std::deque<uint32_t> delete_r;  uint32_t k =0;
  for (RttHistory_t::iterator i = m_history_r.begin (); i != m_history_r.end (); ++i, ++k)
  {
	  //PRINT_STATE ("onseq: for seq = "<< Seq<<", iterator i->ack = "<<i->seq<<", i->time = "<<i->time<<", i->dis = "<< i->count);
      if (Seq == SequenceNumber32 (i->count))
      { // Found it
          m = Simulator::Now () - i->time; // Elapsed time
          Measurement_r (m);                // Log the measurement
	      //NS_LOG_DEBUG ("onseq:m="<< m<<", find i->ack = "<<i->seq<<", i->expect seq = "<< i->count<<", i->time = "<<i->time);
          //m_history_r.erase (i);
		  delete_r.push_back(k);  
          ResetMultiplier_r ();             //????after 3 times, then reset?  Reset multiplier on valid measurement
		  ret = true;
      }
	  else if (Seq > SequenceNumber32 (i->count))
	  {
		  delete_r.push_back(k);  //some packets are dropped or lost. //xl
          //m_history_r.erase (i);
	  }
  }
 
  for (std::deque<uint32_t>::iterator i = delete_r.begin (); i != delete_r.end (); ++i)
	  m_history_r.erase (m_history_r.begin () + *i);

  //for (RttHistory_t::iterator i = m_history_r.begin (); i != m_history_r.end (); ++i) 
	  //NS_LOG_DEBUG ("onseq: after onseq2, for seq = "<< Seq<<", iterator i->ack = "<<i->seq<<", i->expect seq = "<< i->count<<", i->time = "<<i->time);
  return ret;
}

void RttEstimator::ClearSent ()
{ 
  NS_LOG_FUNCTION (this);
  // Clear all history entries
  m_next = 1;
  m_history.clear ();
}

void RttEstimator::IncreaseMultiplier ()
{
  NS_LOG_FUNCTION (this);
  m_multiplier = (m_multiplier*2 < m_maxMultiplier) ? m_multiplier*2 : m_maxMultiplier;
  NS_LOG_DEBUG ("Multiplier increased to " << m_multiplier);
}

void RttEstimator::ResetMultiplier ()
{
  NS_LOG_FUNCTION (this);
  m_multiplier = 1;
}

void RttEstimator::IncreaseMultiplier_r ()
{
  NS_LOG_FUNCTION (this);
  m_multiplier_r = (m_multiplier_r*2 < m_maxMultiplier) ? m_multiplier_r*2 : m_maxMultiplier;
  NS_LOG_DEBUG ("Multiplier_r increased to " << m_multiplier_r);
}

void RttEstimator::ResetMultiplier_r ()
{
  NS_LOG_FUNCTION (this);
  m_multiplier_r = 1;
}


void RttEstimator::Reset ()
{ 
  NS_LOG_FUNCTION (this);
  // Reset to initial state
  m_next = 1;
  m_currentEstimatedRtt = m_initialEstimatedRtt;
  m_history.clear ();         // Remove all info from the history
  m_nSamples = 0;
  ResetMultiplier ();

  //ICTCP or RTCP RTT
  ResetMultiplier_r ();
  m_last_ack = 0;
  m_history_r.clear ();
  m_nSamples_r = 0;
  m_currentEstimatedRtt_r = m_initialEstimatedRtt;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Mean-Deviation Estimator

NS_OBJECT_ENSURE_REGISTERED (RttMeanDeviation);

TypeId 
RttMeanDeviation::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RttMeanDeviation")
    .SetParent<RttEstimator> ()
    .AddConstructor<RttMeanDeviation> ()
    .AddAttribute ("Gain",
                   "Gain used in estimating the RTT, must be 0 < Gain < 1",
                   DoubleValue (0.1),
                   MakeDoubleAccessor (&RttMeanDeviation::m_gain),
                   MakeDoubleChecker<double> ())
  ;
  return tid;
}

RttMeanDeviation::RttMeanDeviation() :
  m_variance (0) , m_variance_r (0)
{ 
  NS_LOG_FUNCTION (this);
}

RttMeanDeviation::RttMeanDeviation (const RttMeanDeviation& c)
  : RttEstimator (c), m_gain (c.m_gain), m_variance (c.m_variance), m_variance_r (c.m_variance_r)
{
  NS_LOG_FUNCTION (this);
}

TypeId
RttMeanDeviation::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void RttMeanDeviation::Measurement (Time m)
{
  NS_LOG_FUNCTION (this << m);
  if (m_nSamples)
    { // Not first
      Time err (m - m_currentEstimatedRtt);
      double gErr = err.ToDouble (Time::S) * m_gain;
      m_currentEstimatedRtt += Time::FromDouble (gErr, Time::S);
      Time difference = Abs (err) - m_variance;
      NS_LOG_DEBUG ("m_variance += " << Time::FromDouble (difference.ToDouble (Time::S) * m_gain, Time::S));
      m_variance += Time::FromDouble (difference.ToDouble (Time::S) * m_gain, Time::S);
    }
  else
    { // First sample
      m_currentEstimatedRtt = m;             // Set estimate to current
      //variance = sample / 2;               // And variance to current / 2
      m_variance = m; // try this
      NS_LOG_DEBUG ("(first sample) m_variance += " << m);
    }
  m_nSamples++;
}

void RttMeanDeviation::Measurement_r (Time m)
{
  NS_LOG_FUNCTION (this << m);
  if (m_nSamples_r)
    { // Not first
      Time err (m - m_currentEstimatedRtt_r);
      double gErr = err.ToDouble (Time::S) * m_gain;
      m_currentEstimatedRtt_r += Time::FromDouble (gErr, Time::S);
      Time difference = Abs (err) - m_variance_r;
      NS_LOG_DEBUG ("m_variance_r += " << Time::FromDouble (difference.ToDouble (Time::S) * m_gain, Time::S));
      m_variance_r += Time::FromDouble (difference.ToDouble (Time::S) * m_gain, Time::S);
      //PRINT_SIMPLE ("measurement_r,  m_variance_r = " <<m_variance_r  );
    }
  else
    { // First sample
      m_currentEstimatedRtt_r = m;             // Set estimate to current
      //variance = sample / 2;               // And variance to current / 2
      m_variance_r = m; // try this
      //PRINT_SIMPLE ("(first sample) m_variance_r = " << m);
    }
      //PRINT_SIMPLE (m_currentEstimatedRtt_r <<" = RTT for receiver, Measurement_r." );
  m_nSamples_r++;
}


Time RttMeanDeviation::RetransmitTimeout ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("RetransmitTimeout:  var " << m_variance.GetSeconds () << " est " << m_currentEstimatedRtt.GetSeconds () << " multiplier " << m_multiplier);
  // RTO = srtt + 4* rttvar
  int64_t temp = m_currentEstimatedRtt.ToInteger (Time::MS) + 4 * m_variance.ToInteger (Time::MS);
  if (temp < m_minRto.ToInteger (Time::MS))
    {
      temp = m_minRto.ToInteger (Time::MS);
    } 
  temp = temp * m_multiplier; // Apply backoff
  Time retval = Time::FromInteger (temp, Time::MS);
  NS_LOG_DEBUG ("RetransmitTimeout:  return " << retval.GetSeconds ());
  return (retval);  
}

Time RttMeanDeviation::RetransmitTimeout_r (uint32_t type)
{//linux core only suppor MS measurement of RTO, so if the rtt < 1ms, the rto will be 0.
 //type =0 for ack timeout, type=1 for delack timeout
  NS_LOG_FUNCTION (this);
  // RTO = srtt + 4* rttvar
  int64_t temp = m_currentEstimatedRtt_r.ToInteger (Time::MS) + 4 * m_variance_r.ToInteger (Time::MS);
  if (temp < m_minRto.ToInteger (Time::MS) )
  {
      temp = m_minRto.ToInteger (Time::MS) ;
  } 
  //if (type == 1) temp = temp / 2; 
  if (temp == 0) temp = 1;
  temp = temp * m_multiplier_r; // Apply backoff
  Time retval = Time::FromInteger (temp, Time::MS);
  /*PRINT_STATE ("RetransmitTimeout_r:  var_r " << m_variance_r.GetSeconds () << " est_r " << m_currentEstimatedRtt_r.GetSeconds () << " multiplier_r " << m_multiplier_r << ", rto=est+4*var="<<retval);*/
  //PRINT_SIMPLE ("RetransmitTimeout_r:  return " << retval.GetSeconds ());
  return (retval);  
}
Ptr<RttEstimator> RttMeanDeviation::Copy () const
{
  NS_LOG_FUNCTION (this);
  return CopyObject<RttMeanDeviation> (this);
}

void RttMeanDeviation::Reset ()
{ 
  NS_LOG_FUNCTION (this);
  // Reset to initial state
  m_variance = Seconds (0);
  RttEstimator::Reset ();
}
void RttMeanDeviation::Gain (double g)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT_MSG( (g > 0) && (g < 1), "RttMeanDeviation: Gain must be less than 1 and greater than 0" );
  m_gain = g;
}




} //namespace ns3
