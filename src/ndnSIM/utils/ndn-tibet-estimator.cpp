#include <iostream>

#include "ndn-tibet-estimator.hpp"
#include "ns3/simulator.h"
#include "ns3/double.h"
#include "ns3/integer.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"


namespace ns3 {

namespace ndn {

TibetEstimator::TibetEstimator()
{
  m_bandwidthEstimate = 0;
}

void
TibetEstimator::Update(uint32_t packetSize, uint32_t windowMaxLen)
{
  uint64_t now = Simulator::Now().GetNanoSeconds();

#define __ALPHA__ 0.99
#define __MAX_INTERVAL__ 2.0e9

  m_avgWindowLength = __ALPHA__ * (double_t)m_avgWindowLength + (1.0 - __ALPHA__) * 1.0;

  double_t timeInterval = std::min(__MAX_INTERVAL__, (double_t)(now - m_prevArrivalTime));

  m_avgInterval = __ALPHA__ * (double_t)m_avgInterval + (1.0 - __ALPHA__) * timeInterval;

  m_prevArrivalTime = now;

  m_bandwidthEstimate = m_avgWindowLength * 1.0e9 / m_avgInterval;

}

uint64_t
TibetEstimator::GetCurrentEstimate(void) const
{
  return m_bandwidthEstimate;
}

void
TibetEstimator::Reset()  // Reset to initial state
{
  m_bandwidthEstimate = 0;
}

} // namespace ndn
} // namespace ns3
