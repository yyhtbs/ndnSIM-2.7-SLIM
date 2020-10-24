
#ifndef NDN_TIBET_ESTIMATOR_H
#define NDN_TIBET_ESTIMATOR_H

#include "ns3/nstime.h"
#include <list>

namespace ns3 {

namespace ndn {

class TibetEstimator {
public:

  TibetEstimator();


  virtual void
  Reset();
  
  void
  Update(uint32_t packetSize, uint32_t windowMaxLen);

  uint64_t
  GetCurrentEstimate(void) const;

private:
  double_t m_bandwidthEstimate;  

  uint64_t m_prevArrivalTime = 0;
  double_t m_avgWindowLength = 0;
  double_t m_avgInterval = 10e6;

};

} // namespace ndn

} // namespace ns3

#endif /* TIBET_ESTIMATOR_H */
