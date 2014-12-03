#include "ecn-tag.h"

namespace ns3 {

// TODO Balajee Need to check this
TypeId 
EcnTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::EcnTag")
    .SetParent<Tag> ()
    .AddConstructor<EcnTag> ()
    .AddAttribute ("Ecn",
                   "A simple value",
                   EmptyAttributeValue (),
                   MakeUintegerAccessor (&EcnTag::GetEcn),
                   MakeUintegerChecker<uint8_t> ())
  ;
  return tid;
}
TypeId 
EcnTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
uint32_t 
EcnTag::GetSerializedSize (void) const
{
  return 1;
}
void 
EcnTag::Serialize (TagBuffer i) const
{
  i.WriteU8 (m_ecn);
}
void 
EcnTag::Deserialize (TagBuffer i)
{
  m_ecn = i.ReadU8 ();
}
void 
EcnTag::Print (std::ostream &os) const
{
  os << "v=" << (uint32_t)m_ecn;
}
void 
EcnTag::SetEcn (uint8_t value)
{
  m_ecn = value;
}
uint8_t 
EcnTag::GetEcn (void) const
{
  return m_ecn;
}  

}
