#ifndef MYMACROS_H
#define MYMACROS_H

// Balajee
#define PRINT_STATE(msg)                                                                                                    \
  cout << Simulator::Now ().GetSeconds () << ":this:"<<this <<": " << msg <<"("<<__FILE__<<"),("<<__LINE__<<")"<< endl                 \

#define PRINT_STUFF(msg)                                                                                                    \
  cout << Simulator::Now ().GetSeconds () << ":Node " << m_node->GetId() << ":this:"<< this <<":" << msg <<"("<<__FILE__<<"),("<<__LINE__<<")"<< endl                            \

#define PRINT_SIMPLE(msg)                                                                                                    \
  cout << Simulator::Now ().GetSeconds () << ": " << msg << endl                                                             \

#endif
