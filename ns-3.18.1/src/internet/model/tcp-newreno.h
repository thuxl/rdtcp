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

#ifndef TCP_NEWRENO_H
#define TCP_NEWRENO_H

#include "tcp-socket-base.h"

//for dctcp
#define  ALPHA_WEIGHT (1.0/16.0)
//for d2tcp
#define  GAMMA_WEIGHT (1.0/4.0) 

namespace ns3 {

class Packet;

/**
 * \ingroup socket
 * \ingroup tcp
 *
 * \brief An implementation of a stream socket using TCP.
 *
 * This class contains the NewReno implementation of TCP, as of \RFC{2582}.
 */
class TcpNewReno : public TcpSocketBase
{
public:
  static TypeId GetTypeId (void);
  /**
   * Create an unbound tcp socket.
   */
  TcpNewReno (void);
  TcpNewReno (const TcpNewReno& sock);
  virtual ~TcpNewReno (void);

  // From TcpSocketBase
  virtual int Connect (const Address &address);
  virtual int Listen (void);

protected:
  virtual uint32_t Window (void); // Return the max possible number of unacked bytes
  virtual Ptr<TcpSocketBase> Fork (void); // Call CopyObject<TcpNewReno> to clone me
  virtual void NewAck (SequenceNumber32 const& seq); // Inc cwnd and call NewAck() of parent
  virtual void DupAck (const TcpHeader& t, uint32_t count);  // Halving cwnd and reset nextTxSequence
  virtual void Retransmit (void); // Exit fast recovery upon retransmit timeout

  // Implementing ns3::TcpSocket -- Attribute get/set
  virtual void     SetSegSize (uint32_t size);
  virtual void     SetSSThresh (uint32_t threshold);
  virtual uint32_t GetSSThresh (void) const;
  virtual void     SetInitialCwnd (uint32_t cwnd);
  virtual uint32_t GetInitialCwnd (void) const;
private:
  void InitializeCwnd (void);            // set m_cWnd when connection starts

protected:
  TracedValue<uint32_t>  m_cWnd;         //< Congestion window
  uint32_t               m_ssThresh;     //< Slow Start Threshold
  uint32_t               m_initialCWnd;  //< Initial cWnd value
  SequenceNumber32       m_recover;      //< Previous highest Tx seqnum for fast recovery
  uint32_t               m_retxThresh;   //< Fast Retransmit threshold
  bool                   m_inFastRec;    //< currently in fast recovery
  bool                   m_limitedTx;    //< perform limited transmit
  
  // DCTCP stuff
  uint32_t               m_tot_acks;
  uint32_t               m_ecn_acks;
  double                 m_alpha_last;
  double                 m_alpha;
  virtual void NewAckECN (SequenceNumber32 const& seq); // Inc cwnd and call NewAck() of parent
  virtual void SetAlpha_DCTCP (); //Balajee // by xl on Tue Mar  5 20:48:04 CST 2013

  //rtcp
  virtual void 			 Retransmit_r (uint8_t flags); // Halving cwnd and call DoRetransmit() , xl
  void                   Modify_Received_Cwnd (bool seqlargerack, uint8_t ecnflags, bool immediat_ack ); 

  uint32_t               m_dupacks;
  TracedValue<uint32_t>  m_receiver_cWnd;                 //xl < receiver Congestion window 
  uint32_t               m_receiver_ssThresh;     //< Slow Start Threshold
  virtual uint16_t       AdvertisedWindowSize (void);     //xl The amount of Rx window announced to the peer
  virtual void           ReceivedData (Ptr<Packet>, const TcpHeader&, Ipv4Header::EcnType ipv4ecn); //xl
  
  
  //rdtcp
  uint32_t               m_rec_tot_acks;
  uint32_t               m_rec_ecn_acks;
  virtual void SetAlpha_RDCTCP (); //by xl on Tue Mar  5 20:48:04 CST 2013
  double                 m_rec_alpha_last;
  double                 m_rec_alpha;

  //ictcp
  void ICTCP_receiveddData (Ptr<Packet> p, const TcpHeader& tcpHeader, Ipv4Header::EcnType ecn); //xl
  virtual double ModifyCwnd_ICTCP (double bwa, uint8_t slot_mode); //xl
  virtual void  ICTCP_share_decrease (uint16_t	connection_count); 
  uint8_t                m_ictcp_state; //0 for slow start, 1 for congestion avoidance.
  uint8_t                m_ictcp_condition2_times; 
 
  typedef enum {
    SlowStart,       // 0
    CongAvoid,       // 1
    NON			   // 2
  } Ictcpstates_t;

  //rdtcp controller
  virtual int Connect (const Address &address, uint32_t flowsize, uint32_t deadline, uint32_t flowid);      // Setup endpoint and call ProcessAction() to connect

};

} // namespace ns3

#endif /* TCP_NEWRENO_H */
