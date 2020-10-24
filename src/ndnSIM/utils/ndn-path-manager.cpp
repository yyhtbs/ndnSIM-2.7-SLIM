
#include <iostream>

#include "ndn-path-manager.hpp"
#include "ns3/simulator.h"
#include "ns3/double.h"
#include "ns3/integer.h"
#include "ns3/uinteger.h"

namespace ns3 {

namespace ndn {

PathManager::PathManager()
{
  m_lt = LtEstimator();
  m_bw = TibetEstimator();
  m_x = CreateObject<UniformRandomVariable> ();
  m_x->SetAttribute ("Min", DoubleValue (0));
  m_x->SetAttribute ("Max", DoubleValue (1));

}
  

void
PathManager::Update(uint16_t packetSize, uint64_t rtt, uint32_t tao)
{
  if (packetSize < 10) return; // It is DATA NACK
  
  packetSize = 1;

  m_lt.Update(rtt);

  m_bw.Update(packetSize, tao);
  
}

void
PathManager::UpdateRaaqm(uint64_t rtt)
{
  uint64_t now = Simulator::Now().GetNanoSeconds();

  // Add RTT
  m_raaqmMetric.push_back(std::pair<uint64_t, uint64_t> (now, rtt));
  
  if (rtt < m_minRtt) { m_minRtt = rtt; }
  
  // # Clean the expired entries
  auto it = m_raaqmMetric.begin();
  while (it != m_raaqmMetric.end()) {
    if (m_raaqmMetric.size() > 30) { // Remove the expried entries
    //~ if (now - it->first > 2 * m_minRtt) { // Remove the expried entries
      m_raaqmMetric.pop_front();
      it = m_raaqmMetric.begin();
    }
    else { break; } // Exit the while loop
  }
  //~ std::cout << "Current baseRTT is " << m_minRtt << std::endl;
  //~ std::cout << "Current RTT is " << rtt << std::endl;
  // # Update MAX and MIN
  // m_maxRtt = m_minRtt + 20e6;
  m_maxRtt = 0; for (auto rf : m_raaqmMetric) {m_maxRtt = rf.second > m_maxRtt ? rf.second : m_maxRtt; };
  // m_minRtt = UINT64_MAX; for (auto rf : m_raaqmMetric) {m_minRtt = rf.second < m_minRtt ? rf.second : m_minRtt; };
}

#define __PROB_MIN__ 1.0e-5
#define __PROB_MAX__ 0.5
#define __MIN_TH__ 5e6

bool
PathManager::CheckCongestion(uint64_t rtt) 
{
  if (rtt - m_minRtt < __MIN_TH__) return 0;
  if (rtt > m_maxRtt) return 1;
  
  double_t probCong = __PROB_MIN__ + (__PROB_MAX__ - __PROB_MIN__) * (rtt - (m_minRtt + __MIN_TH__)) / (m_maxRtt - (m_minRtt + __MIN_TH__));
  
  double_t value = m_x->GetValue ();

  if (value < probCong)
    return 1;
  else
    return 0;
  
}

double_t
PathManager::GetCurrentBdp()
{
  //~ std::cout << "RTT is " << (double_t)m_lt.GetMinEstimate() / (1.0e9) << std::endl;
  //~ std::cout << "BW is " << (double_t)m_bw.GetCurrentEstimate() << std::endl;

  return (double_t)m_lt.GetMinEstimate() / (1.0e9) * m_bw.GetCurrentEstimate();
}


void
PathManager::Reset()
{
  m_lt.Reset();
  m_bw.Reset();
  
}

} // namespace ndn
} // namespace ns3
