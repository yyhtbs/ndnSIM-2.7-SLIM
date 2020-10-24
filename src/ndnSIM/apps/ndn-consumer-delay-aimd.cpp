/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "ndn-consumer-delay-aimd.hpp"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include <limits>

NS_LOG_COMPONENT_DEFINE("ndn.ConsumerDelayAimd");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(ConsumerDelayAimd);

TypeId
ConsumerDelayAimd::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::ConsumerDelayAimd")
      .SetGroupName("Ndn")
      .SetParent<Consumer>()
      .AddConstructor<ConsumerDelayAimd>()

      .AddAttribute("Window", "Initial size of the window", StringValue("1"),
                    MakeUintegerAccessor(&ConsumerDelayAimd::GetWindow, &ConsumerDelayAimd::SetWindow),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("PayloadSize",
                    "Average size of content object size (to calculate interest generation rate)",
                    UintegerValue(1040), MakeUintegerAccessor(&ConsumerDelayAimd::GetPayloadSize,
                                                              &ConsumerDelayAimd::SetPayloadSize),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("Size", "Amount of data in megabytes to request, relying on PayloadSize "
                            "parameter (alternative to MaxSeq attribute)",
                    DoubleValue(-1), // don't impose limit by default
                    MakeDoubleAccessor(&ConsumerDelayAimd::GetMaxSize, &ConsumerDelayAimd::SetMaxSize),
                    MakeDoubleChecker<double>())

      .AddAttribute("MaxSeq", "Maximum sequence number to request (alternative to Size attribute, "
                              "would activate only if Size is -1). "
                              "The parameter is activated only if Size negative (not set)",
                    IntegerValue(std::numeric_limits<uint32_t>::max()),
                    MakeUintegerAccessor(&ConsumerDelayAimd::GetSeqMax, &ConsumerDelayAimd::SetSeqMax),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("InitialWindowOnTimeout", "Set window to initial value when timeout occurs",
                    BooleanValue(true),
                    MakeBooleanAccessor(&ConsumerDelayAimd::m_setInitialWindowOnTimeout),
                    MakeBooleanChecker())

      .AddTraceSource("WindowTrace",
                      "Window that controls how many outstanding interests are allowed",
                      MakeTraceSourceAccessor(&ConsumerDelayAimd::m_window),
                      "ns3::ndn::ConsumerDelayAimd::WindowTraceCallback")
      .AddTraceSource("InFlight", "Current number of outstanding interests",
                      MakeTraceSourceAccessor(&ConsumerDelayAimd::m_inFlight),
                      "ns3::ndn::ConsumerDelayAimd::WindowTraceCallback");

  return tid;
}

ConsumerDelayAimd::ConsumerDelayAimd()
  : m_payloadSize(1040)
  , m_inFlight(0)
{
}

void
ConsumerDelayAimd::SetWindow(uint32_t window)
{
  m_initialWindow = window;
  m_window = m_initialWindow;
}

uint32_t
ConsumerDelayAimd::GetWindow() const
{
  return m_initialWindow;
}

uint32_t
ConsumerDelayAimd::GetPayloadSize() const
{
  return m_payloadSize;
}

void
ConsumerDelayAimd::SetPayloadSize(uint32_t payload)
{
  m_payloadSize = payload;
}

double
ConsumerDelayAimd::GetMaxSize() const
{
  if (m_seqMax == 0)
    return -1.0;

  return m_maxSize;
}

void
ConsumerDelayAimd::SetMaxSize(double size)
{
  m_maxSize = size;
  if (m_maxSize < 0) {
    m_seqMax = std::numeric_limits<uint32_t>::max();
    return;
  }

  m_seqMax = floor(1.0 + m_maxSize * 1024.0 * 1024.0 / m_payloadSize);
  NS_LOG_DEBUG("MaxSeqNo: " << m_seqMax);
  // std::cout << "MaxSeqNo: " << m_seqMax << "\n";
}

uint32_t
ConsumerDelayAimd::GetSeqMax() const
{
  return m_seqMax;
}

void
ConsumerDelayAimd::SetSeqMax(uint32_t seqMax)
{
  if (m_maxSize < 0)
    m_seqMax = seqMax;

  // ignore otherwise
}

void
ConsumerDelayAimd::ScheduleNextPacket()
{
  if (m_window == static_cast<uint32_t>(0)) {
    Simulator::Remove(m_sendEvent);

    m_sendEvent =
      Simulator::Schedule(Seconds(
                            std::min<double>(0.5, m_rtt->RetransmitTimeout().ToDouble(Time::S))),
                          &Consumer::SendPacket, this);
  }
  else if (m_inFlight >= m_window) {
    // simply do nothing
  }
  else {
    if (m_sendEvent.IsRunning()) {  Simulator::Remove(m_sendEvent); }

    m_sendEvent = Simulator::ScheduleNow(&ConsumerDelayAimd::SendPacket, this);
  }
}


void
ConsumerDelayAimd::SendPacket()
{
  if (!m_active)
    return;

  NS_LOG_FUNCTION_NOARGS();

  uint32_t seq = std::numeric_limits<uint32_t>::max(); // invalid

  while (m_retxSeqs.size()) {
    seq = *m_retxSeqs.begin();
    m_retxSeqs.erase(m_retxSeqs.begin());
    break;
  }

  if (seq == std::numeric_limits<uint32_t>::max()) {
    if (m_seqMax != std::numeric_limits<uint32_t>::max()) {
      if (m_seq >= m_seqMax) {
        return; // we are totally done
      }
    }

    seq = m_seq++;
  }

  //
  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);
  nameWithSequence->appendSequenceNumber(seq);
  //

  // shared_ptr<Interest> interest = make_shared<Interest> ();
  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*nameWithSequence);
  interest->setCanBePrefix(false);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);

  // NS_LOG_INFO ("Requesting Interest: \n" << *interest);
  NS_LOG_INFO("> Interest for " << seq);

  m_interestOutTime[seq] = Simulator::Now().GetNanoSeconds();

  WillSendOutInterest(seq);

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);

  ScheduleNextPacket();
}
///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

#define __DELTA__ 0.85

void
ConsumerDelayAimd::OnData(shared_ptr<const Data> contentObject)
{
  Consumer::OnData(contentObject);


  if (m_inFlight > static_cast<uint32_t>(0))
    m_inFlight--;

  // Create a Path Manager for each Path
  uint64_t pathId = contentObject->getPathId();
  
  uint32_t seq = contentObject->getName().at(-1).toSequenceNumber();

  /// Create Path Manager for New Path
  if (m_pathManagerMap.find(pathId) == m_pathManagerMap.end()) {
    m_pathManagerMap.insert(std::pair<uint64_t, PathManager>(pathId, PathManager()));
  }

  /// Calculate the RTT
  uint64_t rtt = (Simulator::Now().GetNanoSeconds() - m_interestOutTime[seq]);

  m_pathManagerMap[pathId].Update(rtt);
  m_interestOutTime.erase(seq);

  m_pathManagerMap[pathId].m_counter = m_pathManagerMap[pathId].m_counter >= 1 ? m_pathManagerMap[pathId].m_counter - 1: 0;

  /// Check if Congestion
  if (m_pathManagerMap[pathId].CheckCongestion(rtt) && m_pathManagerMap[pathId].m_counter == 0) {
    m_pathManagerMap[pathId].m_counter = m_window * 2;
    m_window = m_window * __DELTA__ * (double_t)m_pathManagerMap[pathId].GetBaseRtt() / (double_t)rtt;          // Window Back-off

    if (m_window < 2.0) m_window = 2.0;
    
    std::cout << "Window Size = " << m_window << std::endl;

    m_ss = 0;                             // Stop Slow Start   
  }
  else {
    if (m_ss == 1)
      m_window = m_window + 1;            // Slow Start
    else
      m_window = m_window + 1 / m_window; // Linear Probe

    std::cout << "Window Size = " << m_window << std::endl;

  }

  ScheduleNextPacket();
}

void
ConsumerDelayAimd::OnTimeout(uint32_t sequenceNumber)
{
  if (m_inFlight > static_cast<uint32_t>(0))
    m_inFlight--;

  if (m_setInitialWindowOnTimeout) {
    m_window = m_initialWindow;
  }
  m_totalRtx++;

  std::cout << "Node " << GetNode()->GetId() << ": Total RTX " << m_totalRtx << std::endl;
  NS_LOG_DEBUG("Window: " << m_window << ", InFlight: " << m_inFlight);
  Consumer::OnTimeout(sequenceNumber);
}

void
ConsumerDelayAimd::WillSendOutInterest(uint32_t sequenceNumber)
{
  m_inFlight++;
  Consumer::WillSendOutInterest(sequenceNumber);
}

ConsumerDelayAimd::PathManager::PathManager()
{
  m_rand = CreateObject<UniformRandomVariable> ();
  m_rand->SetAttribute ("Min", DoubleValue (0));
  m_rand->SetAttribute ("Max", DoubleValue (1));
}

void
ConsumerDelayAimd::PathManager::Update(uint64_t rtt)
{
  uint64_t now = Simulator::Now().GetNanoSeconds();

  // Add RTT samples
  m_rttSamples.push_back(std::pair<uint64_t, uint64_t> (now, rtt));
  
  if (rtt < m_baseRtt) { m_baseRtt = rtt; }

  // # Clean the expired entries
  while (m_rttSamples.size() > 30) {
    m_rttSamples.pop_front();
    // std::cout << "m_rttSamples size = " << m_rttSamples.size() << std::endl;
  }
    // std::cout << "Out" << std::endl;

}

#define __PROB_MIN__ 1.0e-5
#define __PROB_MAX__ 0.5
#define __MIN_TH__ 20e6 // 20ms

bool
ConsumerDelayAimd::PathManager::CheckCongestion(uint64_t rtt) 
{
    return rtt < (m_baseRtt + __MIN_TH__)? 0 : 1;    // To tolerate RTT noise
}

uint64_t
ConsumerDelayAimd::PathManager::GetBaseRtt()
{
    return m_baseRtt;
}
} // namespace ndn
} // namespace ns3
