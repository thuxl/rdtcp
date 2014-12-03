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
#ifndef TCP_SOCKET_BASE_H
#define TCP_SOCKET_BASE_H

#include <stdint.h>
#include <list>
#include "ns3/callback.h"
#include "ns3/traced-value.h"
#include "ns3/tcp-socket.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv6-header.h"
#include "ns3/event-id.h"
#include "tcp-tx-buffer.h"
#include "tcp-rx-buffer.h"
#include "rtt-estimator.h"


//MAX_FLOWS_N is also defined both in net-device.h and tcp-socket-base.h
#define  MAX_FLOWS_N 500

namespace ns3 {

class Ipv4EndPoint;
class Ipv6EndPoint;
class Node;
class Packet;
class TcpL4Protocol;
class TcpHeader;
class NetDevice;

/**
 * \ingroup socket
 * \ingroup tcp
 *
 * \brief A base class for implementation of a stream socket using TCP.
 *
 * This class contains the essential components of TCP, as well as a sockets
 * interface for upper layers to call. This serves as a base for other TCP
 * functions where the sliding window mechanism is handled here. This class
 * provides connection orientation and sliding window flow control. Part of
 * this class is modified from the original NS-3 TCP socket implementation
 * (TcpSocketImpl) by Raj Bhattacharjea <raj.b@gatech.edu> of Georgia Tech.
 */
class TcpSocketBase : public TcpSocket
{
public:
  static TypeId GetTypeId (void);
  /**
   * Create an unbound TCP socket
   */
  TcpSocketBase (void);

  /**
   * Clone a TCP socket, for use upon receiving a connection request in LISTEN state
   */
  TcpSocketBase (const TcpSocketBase& sock);
  virtual ~TcpSocketBase (void);

  // Set associated Node, TcpL4Protocol, RttEstimator to this socket
  virtual void SetNode (Ptr<Node> node);
  virtual void SetTcp (Ptr<TcpL4Protocol> tcp);
  virtual void SetRtt (Ptr<RttEstimator> rtt);

  // Necessary implementations of null functions from ns3::Socket
  virtual enum SocketErrno GetErrno (void) const;    // returns m_errno
  virtual enum SocketType GetSocketType (void) const; // returns socket type
  virtual Ptr<Node> GetNode (void) const;            // returns m_node
  virtual int Bind (void);    // Bind a socket by setting up endpoint in TcpL4Protocol
  virtual int Bind6 (void);    // Bind a socket by setting up endpoint in TcpL4Protocol
  virtual int Bind (const Address &address);         // ... endpoint of specific addr or port
  virtual int Connect (const Address &address);      // Setup endpoint and call ProcessAction() to connect
  virtual int Connect (const Address &address, uint32_t flowsize, uint32_t deadline, uint32_t flowid);      // Setup endpoint and call ProcessAction() to connect
  virtual int Listen (void);  // Verify the socket is in a correct state and call ProcessAction() to listen
  virtual int Close (void);   // Close by app: Kill socket upon tx buffer emptied
  virtual int ShutdownSend (void);    // Assert the m_shutdownSend flag to prevent send to network
  virtual int ShutdownRecv (void);    // Assert the m_shutdownRecv flag to prevent forward to app
  virtual int Send (Ptr<Packet> p, uint32_t flags);  // Call by app to send data to network
  virtual int SendTo (Ptr<Packet> p, uint32_t flags, const Address &toAddress); // Same as Send(), toAddress is insignificant
  virtual Ptr<Packet> Recv (uint32_t maxSize, uint32_t flags); // Return a packet to be forwarded to app
  virtual Ptr<Packet> RecvFrom (uint32_t maxSize, uint32_t flags, Address &fromAddress); // ... and write the remote address at fromAddress
  virtual uint32_t GetTxAvailable (void) const; // Available Tx buffer size
  virtual uint32_t GetRxAvailable (void) const; // Available-to-read data size, i.e. value of m_rxAvailable
  virtual int GetSockName (Address &address) const; // Return local addr:port in address
  virtual void BindToNetDevice (Ptr<NetDevice> netdevice); // NetDevice with my m_endPoint

//protected: //del by xl.
public:
  // Implementing ns3::TcpSocket -- Attribute get/set
  virtual void     SetSndBufSize (uint32_t size);
  virtual uint32_t GetSndBufSize (void) const;
  virtual void     SetRcvBufSize (uint32_t size);
  virtual uint32_t GetRcvBufSize (void) const;
  virtual void     SetSegSize (uint32_t size);
  virtual uint32_t GetSegSize (void) const;
  virtual void     SetSSThresh (uint32_t threshold) = 0;
  virtual uint32_t GetSSThresh (void) const = 0;
  virtual void     SetInitialCwnd (uint32_t cwnd) = 0;
  virtual uint32_t GetInitialCwnd (void) const = 0;
  virtual void     SetConnTimeout (Time timeout);
  virtual Time     GetConnTimeout (void) const;
  virtual void     SetConnCount (uint32_t count);
  virtual uint32_t GetConnCount (void) const;
  virtual void     SetDelAckTimeout (Time timeout);
  virtual Time     GetDelAckTimeout (void) const;
  virtual void     SetDelAckMaxCount (uint32_t count);
  virtual uint32_t GetDelAckMaxCount (void) const;
  virtual void     SetTcpNoDelay (bool noDelay);
  virtual bool     GetTcpNoDelay (void) const;
  virtual void     SetPersistTimeout (Time timeout);
  virtual Time     GetPersistTimeout (void) const;
  virtual bool     SetAllowBroadcast (bool allowBroadcast);
  virtual bool     GetAllowBroadcast (void) const;

  // Helper functions: Connection set up
  int SetupCallback (void);        // Common part of the two Bind(), i.e. set callback and remembering local addr:port
  int DoConnect (void);            // Sending a SYN packet to make a connection if the state allows
  void ConnectionSucceeded (void); // Schedule-friendly wrapper for Socket::NotifyConnectionSucceeded()
  int SetupEndpoint (void);        // Configure m_endpoint for local addr for given remote addr
  int SetupEndpoint6 (void);       // Configure m_endpoint6 for local addr for given remote addr
  void CompleteFork (Ptr<Packet>, const TcpHeader&, const Address& fromAddress, const Address& toAdress);

  // Helper functions: Transfer operation
  void ForwardUp (Ptr<Packet> packet, Ipv4Header header, uint16_t port, Ptr<Ipv4Interface> incomingInterface);
  void ForwardUp6 (Ptr<Packet> packet, Ipv6Header header, uint16_t port);
  virtual void DoForwardUp (Ptr<Packet> packet, Ipv4Header header, uint16_t port, Ptr<Ipv4Interface> incomingInterface); //Get a pkt from L3
  virtual void DoForwardUp (Ptr<Packet> packet, Ipv6Header header, uint16_t port); // Ipv6 version
  void ForwardIcmp (Ipv4Address icmpSource, uint8_t icmpTtl, uint8_t icmpType, uint8_t icmpCode, uint32_t icmpInfo);
  void ForwardIcmp6 (Ipv6Address icmpSource, uint8_t icmpTtl, uint8_t icmpType, uint8_t icmpCode, uint32_t icmpInfo);  
  bool SendPendingData (bool withAck = false); // Send as much as the window allows
  uint32_t SendDataPacket (SequenceNumber32 seq, uint32_t maxSize, bool withAck); // Send a data packet
  void SendEmptyPacket (uint8_t flags); // Send a empty packet that carries a flag, e.g. ACK
  void SendRST (void); // Send reset and tear down this socket
  bool OutOfRange (SequenceNumber32 head, SequenceNumber32 tail) const; // Check if a sequence number range is within the rx window

  // Helper functions: Connection close
  int DoClose (void); // Close a socket by sending RST, FIN, or FIN+ACK, depend on the current state
  void CloseAndNotify (void); // To CLOSED state, notify upper layer, and deallocate end point
  void Destroy (void); // Kill this socket by zeroing its attributes
  void Destroy6 (void); // Kill this socket by zeroing its attributes
  void DeallocateEndPoint (void); // Deallocate m_endPoint
  void PeerClose (Ptr<Packet>, const TcpHeader&); // Received a FIN from peer, notify rx buffer
  void DoPeerClose (void); // FIN is in sequence, notify app and respond with a FIN
  void CancelAllTimers (void); // Cancel all timer when endpoint is deleted
  void TimeWait (void);  // Move from CLOSING or FIN_WAIT_2 to TIME_WAIT state

  // State transition functions
  //void ProcessEstablished (Ptr<Packet>, const TcpHeader&); // Received a packet upon ESTABLISHED state
  void ProcessEstablished (Ptr<Packet> packet, const TcpHeader& tcpHeader, Ipv4Header::EcnType ipv4ecn);
  void ProcessListen (Ptr<Packet>, const TcpHeader&, const Address&, const Address&); // Process the newly received ACK
  void ProcessSynSent (Ptr<Packet>, const TcpHeader&); // Received a packet upon SYN_SENT
  void ProcessSynRcvd (Ptr<Packet>, const TcpHeader&, const Address&, const Address&); // Received a packet upon SYN_RCVD
  void ProcessWait (Ptr<Packet>, const TcpHeader&); // Received a packet upon CLOSE_WAIT, FIN_WAIT_1, FIN_WAIT_2
  void ProcessClosing (Ptr<Packet>, const TcpHeader&); // Received a packet upon CLOSING
  void ProcessLastAck (Ptr<Packet>, const TcpHeader&); // Received a packet upon LAST_ACK

  // Window management
  virtual uint32_t UnAckDataCount (void);       // Return count of number of unacked bytes
  virtual uint32_t BytesInFlight (void);        // Return total bytes in flight
  virtual uint32_t Window (void);               // Return the max possible number of unacked bytes
  virtual uint32_t AvailableWindow (void);      // Return unfilled portion of window
  virtual uint16_t AdvertisedWindowSize (void); // The amount of Rx window announced to the peer

  // Manage data tx/rx
  virtual Ptr<TcpSocketBase> Fork (void) = 0; // Call CopyObject<> to clone me
  //virtual void ReceivedAck (Ptr<Packet>, const TcpHeader&); // Received an ACK packet
  //virtual void ReceivedData (Ptr<Packet>, const TcpHeader&); // Recv of a data, put into buffer, call L7 to get it if necessary
  virtual void ReceivedAck (Ptr<Packet>, const TcpHeader&, Ipv4Header::EcnType ); // Received an ACK packet
  virtual void ReceivedData (Ptr<Packet>, const TcpHeader&, Ipv4Header::EcnType ); // Recv of a data, put into buffer, call L7 to get it if necessary
  virtual void EstimateRtt (const TcpHeader&); // RTT accounting
  virtual void NewAck (SequenceNumber32 const& seq); // Update buffers w.r.t. ACK
  virtual void NewAckECN (SequenceNumber32 const& seq); // Update buffers w.r.t. ACK// by xl on Thu Mar  7 20:38:10 CST 2013
  virtual void DupAck (const TcpHeader& t, uint32_t count) = 0; // Received dupack
  virtual void ReTxTimeout (void); // Call Retransmit() upon RTO event
  virtual void Retransmit (void); // Halving cwnd and call DoRetransmit()
  virtual void DelAckTimeout (void);  // Action upon delay ACK timeout, i.e. send an ACK
  virtual void LastAckTimeout (void); // Timeout at LAST_ACK, close the connection
  virtual void PersistTimeout (void); // Send 1 byte probe to get an updated window size
  virtual void DoRetransmit (void); // Retransmit the oldest packet
  virtual void ReadOptions (const TcpHeader&); // Read option from incoming packets
  virtual void AddOptions (TcpHeader&); // Add option to outgoing packets

//protected: del by xl.
public:
  // Counters and events
  EventId           m_retxEvent;       //< Retransmission event
  EventId           m_lastAckEvent;    //< Last ACK timeout event
  EventId           m_delAckEvent;     //< Delayed ACK timeout event
  EventId           m_persistEvent;    //< Persist event: Send 1 byte to probe for a non-zero Rx window
  EventId           m_timewaitEvent;   //< TIME_WAIT expiration event: Move this socket to CLOSED state
  uint32_t          m_dupAckCount;     //< Dupack counter
  uint32_t          m_delAckCount;     //< Delayed ACK counter
  uint32_t          m_delAckMaxCount;  //< Number of packet to fire an ACK before delay timeout
  bool              m_noDelay;         //< Set to true to disable Nagle's algorithm
  uint32_t          m_cnCount;         //< Count of remaining connection retries
  uint32_t          m_cnRetries;       //< Number of connection retries before giving up
  TracedValue<Time> m_rto;             //< Retransmit timeout
  TracedValue<Time> m_lastRtt;         //< Last RTT sample collected
  Time              m_delAckTimeout;   //< Time to delay an ACK
  Time              m_persistTimeout;  //< Time between sending 1-byte probes
  Time              m_cnTimeout;       //< Timeout for connection retry

  // Connections to other layers of TCP/IP
  Ipv4EndPoint*       m_endPoint;
  Ipv6EndPoint*       m_endPoint6;
  Ptr<Node>           m_node;
  Ptr<TcpL4Protocol>  m_tcp;
  Callback<void, Ipv4Address,uint8_t,uint8_t,uint8_t,uint32_t> m_icmpCallback;
  Callback<void, Ipv6Address,uint8_t,uint8_t,uint8_t,uint32_t> m_icmpCallback6;

  // Round trip time estimation
  Ptr<RttEstimator> m_rtt;

  // Rx and Tx buffer management
  TracedValue<SequenceNumber32> m_nextTxSequence; //< Next seqnum to be sent (SND.NXT), ReTx pushes it back
  TracedValue<SequenceNumber32> m_highTxMark;     //< Highest seqno ever sent, regardless of ReTx
  TcpRxBuffer                   m_rxBuffer;       //< Rx buffer (reordering buffer)
  TcpTxBuffer                   m_txBuffer;       //< Tx buffer

  // State-related attributes
  TracedValue<TcpStates_t> m_state;         //< TCP state
  enum SocketErrno         m_errno;         //< Socket error code
  bool                     m_closeNotified; //< Told app to close socket
  bool                     m_closeOnEmpty;  //< Close socket upon tx buffer emptied
  bool                     m_shutdownSend;  //< Send no longer allowed
  bool                     m_shutdownRecv;  //< Receive no longer allowed
  bool                     m_connected;     //< Connection established
  double                   m_msl;           //< Max segment lifetime

  // Window management
  uint32_t              m_segmentSize; //< Segment size
  uint16_t              m_maxWinSize;  //< Maximum window size to advertise
  TracedValue<uint32_t> m_rWnd;        //< Flow control window at remote side



//new protocols, xl
public:
  uint16_t                m_dctcp_mode;  //xl
  uint16_t		          m_ictcp_mode;  //xl
  uint16_t		          m_rtcp_mode;  //xl
  uint16_t		          m_rdtcp_mode;  //xl
//dctcp
  bool                    m_first_ack_on_established;
  void          ModifyCwnd_DCTCP (); //xl
  virtual void  SetAlpha_DCTCP (); //Balajee// by xl on Thu Mar  7 20:38:16 CST 2013
  EventId                 m_dctcp_mod_markEvent;    // when to update the ECN marking percentage// by xl on Thu Mar  7 20:40:25 CST 2013

  uint32_t                m_receiver_order;  //xl
  bool 					  m_receiver_pre_state_is_ecn; //for dctcp two state ack machine at receiver side.
  bool 					  m_sender_pre_state_is_ecn; //for dctcp two state ack machine at receiver side.

  uint32_t 				  m_receiver_send_ack_times;  //for correct delay ack .
  void          Calculate_Sender_Goodput (); //for sender goodput
  void          CwndChange_rec (uint32_t oldCwnd, uint32_t newCwnd); // by xl 
  
 //rtcp 
  bool                    m_receiver_inFastRec;                     //< currently in fast recovery 
  virtual void  Retransmit_r (uint8_t flags){}   // Halving cwnd and call DoRetransmit() , xl
  SequenceNumber32        m_pre_ack; //xl for rto_r and m_retx_rEvent
  uint32_t                m_overRTO_r_times;
  EventId                 m_retx_rEvent;       //< Retransmission event, xl for receiver
  TracedValue<Time>       m_rto_r;             //< Retransmit timeout, for receiver, xl
  void          Calculate_Receiver_Goodput(); //for sender goodput

  //rdtcp
  virtual void  SetAlpha_RDCTCP (); //Balajee// by xl on Thu Mar  7 20:38:16 CST 2013
  EventId                 m_rdctcp_mod_markEvent;    // when to update the ECN marking percentage// by xl on Thu Mar  7 20:40:25 CST 2013
  void          ModifyCwnd_RDCTCP (); //xl

  //ictcp
  Ptr<NetDevice>          m_receive_device; //just test for only one netdevice
  uint32_t                m_received_dataN;  //xl
  
  EventId                 m_sender_cal_markEvent;  // xl
  EventId                 m_receiver_cal_markEvent;  // xl
  
  uint32_t                m_receiver_order_on_dev;  //xl
  EventId                 m_ictcp_global_update_Event;       //< Retransmission event
  EventId                 m_ictcp_measure_Event;       //< Retransmission event
  void           Calculate_ICTCP_Receiver_Goodput(); //for sender goodput
  void           ICTCP_UPDATE(); //xl
  virtual void   ICTCP_share_decrease (uint16_t	connection_count); 
  virtual double ModifyCwnd_ICTCP (double bwa, uint8_t slot_mode); //xl
  bool                    m_firstStart_cal_ictcp; //for every connetion
  Time					  m_receiver_pre_rtt;
  double				  m_bm;   //for ictcp measured bandwidth, Mbps
  double 				  m_be;   //for ictcp expected bandwidth, Mbps
  double 				  m_d;   //for ictcp (m_be-m_bm) /m_be;
  Time					  m_receiver_last_mod_time; //for every connection

  //rdtcp controller
  typedef std::map<uint64_t, NetDevice::rdtcp_info>::iterator ReceiverIterator;
  uint32_t                m_sender_flowsize;
  uint32_t                m_rdtcp_queue_maxsize;
  uint32_t                m_rdtcp_controller_mode;
  virtual void            Update_rdtcp_MaxWin_RttDelay ();
  virtual void            Update_rdtcp_GlobalRtt ();
  EventId                 m_delay_ack_Event;       //< Retransmission event, xl for receiver
  EventId                 m_update_rdtcp_rtt_Event;       //< Retransmission event, xl for receiver
  Time					  GetDelayTime();

  //list controller
  //struct item_t {
//	  Ptr<Object> socket_ptr;
//	  uint8_t flags ;
  //};
  typedef std::list<NetDevice::item_t>::iterator            ReceiverListIterator;
  typedef std::list<NetDevice::item_t>::reverse_iterator    Reverse_ReceiverListIterator;
  void SignalSendEmptyPacket (uint8_t flags);
  uint32_t										    m_recCwnd_upper;
  
  //for calculate flow completion time and missed rate w.r.t. deadline
  uint32_t 											m_sender_deadline; //in ns.
  uint32_t 											m_sender_flowid; //in ns.

  uint32_t 											m_rec_deadline; //in ns.
  uint32_t 											m_rec_flowid; //in ns.
  uint32_t 											m_rec_flowsize; //in ns.
  uint32_t											m_incre_bytes;

  //for all protocols
  uint32_t										    m_all_rec_num;

  //for rdtcp
  uint32_t											m_rdtcp_no_priority;
};

} // namespace ns3

#endif /* TCP_SOCKET_BASE_H */
