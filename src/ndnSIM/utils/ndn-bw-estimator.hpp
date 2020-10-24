
#ifndef NDN_BW_ESTIMATOR_H
#define NDN_BW_ESTIMATOR_H

#include "ns3/nstime.h"
#include <list>

namespace ns3 {

namespace ndn {

class BwEstimator {
public:

  BwEstimator();


  virtual void
  Reset();
  
  void
  Update(uint32_t packetSize, uint32_t windowMaxLen);

  uint64_t
  GetCurrentEstimate(void) const;

private:
  double_t m_bandwidthEstimate;  

  uint64_t m_totalNumberOfPacket = 0;
  std::list<uint64_t> m_recvTimeWindow;
  uint32_t m_windowMaxLen;
};

} // namespace ndn

} // namespace ns3

#endif /* RTT_ESTIMATOR_H */
