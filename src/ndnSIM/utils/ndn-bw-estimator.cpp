#include <iostream>

#include "ndn-bw-estimator.hpp"
#include "ns3/simulator.h"
#include "ns3/double.h"
#include "ns3/integer.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"


namespace ns3 {

namespace ndn {

BwEstimator::BwEstimator()
{
  m_bandwidthEstimate = 0;
}

void
BwEstimator::Update(uint32_t packetSize, uint32_t windowMaxLen)
{
  uint64_t now = Simulator::Now().GetNanoSeconds();

  m_windowMaxLen = windowMaxLen;

  m_recvTimeWindow.push_back(now);
  if (m_recvTimeWindow.size() >= windowMaxLen) { // if recv list is fully filled, remove the element inside
    // Pop up all excessive elements
    while (m_recvTimeWindow.size() > windowMaxLen) {
      m_recvTimeWindow.pop_front();
    }
  }
  if (m_recvTimeWindow.size() <= 1) {
    m_bandwidthEstimate = 10;
    return;
  }
  m_bandwidthEstimate = (m_recvTimeWindow.size() - 1) * 1e9 / (m_recvTimeWindow.back() - m_recvTimeWindow.front());

}

uint64_t
BwEstimator::GetCurrentEstimate(void) const
{
  uint64_t now = Simulator::Now().GetNanoSeconds();

  //~ if (now - m_recvTimeWindow.back() > 1e9) // Measure Interval > 1 sec Interval
    //~ return (m_recvTimeWindow.size()) * 1e9 / (now - m_recvTimeWindow.front());
  //~ else
    return m_bandwidthEstimate;
}

void
BwEstimator::Reset()  // Reset to initial state
{
  m_bandwidthEstimate = 0;
}

} // namespace ndn
} // namespace ns3
