
#ifndef NDN_PATH_MANAGER_H
#define NDN_PATH_MANAGER_H

#include <deque>
#include "ns3/nstime.h"
#include "ndn-lt-estimator.hpp"
#include "ndn-bw-estimator.hpp"
#include "ndn-tibet-estimator.hpp"
#include "ns3/random-variable-stream.h"
namespace ns3 {

namespace ndn {

class PathManager{
public:

  PathManager();

  void
  Update(uint16_t packetSize, uint64_t rtt, uint32_t tao);
  
  void
  UpdateRaaqm(uint64_t rtt);
  
  bool
  CheckCongestion(uint64_t rtt);

  double_t
  GetCurrentBdp();

  void
  Reset();

protected:
  LtEstimator m_lt; // Current estimate

  TibetEstimator m_bw;
  
  std::list<std::pair<uint64_t, uint64_t> > m_raaqmMetric;
  uint64_t m_baseRtt = UINT64_MAX;
  uint64_t m_minRtt;
  uint64_t m_maxRtt;
  Ptr<UniformRandomVariable> m_x;
};

} // namespace ndn

} // namespace ns3

#endif /* RTT_ESTIMATOR_H */
