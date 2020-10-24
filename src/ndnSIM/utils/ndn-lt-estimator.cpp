#include <iostream>

#include "ndn-lt-estimator.hpp"
#include "ns3/simulator.h"
#include "ns3/double.h"
#include "ns3/integer.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"


namespace ns3 {

namespace ndn {


LtEstimator::LtEstimator()
{
  m_minRtt = UINT64_MAX;
  m_avgRtt = 0;
}

void
LtEstimator::Update(uint64_t currentRtt)
{
  // Update minimum RTT
  if (currentRtt < m_minRtt)
    m_minRtt = currentRtt;
  // Other functions, needed to be implemented later
}

uint64_t
LtEstimator::GetMinEstimate(void) const
{
  return m_minRtt;
}

void
LtEstimator::Reset()  // Reset to initial state
{
  m_minRtt = UINT64_MAX;
  m_avgRtt = 0;
}

} // namespace ndn
} // namespace ns3
