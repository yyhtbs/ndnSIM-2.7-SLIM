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

#include "ndn-consumer-westwood.hpp"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include <limits>

NS_LOG_COMPONENT_DEFINE("ndn.ConsumerWestwood");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(ConsumerWestwood);

TypeId
ConsumerWestwood::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::ConsumerWestwood")
      .SetGroupName("Ndn")
      .SetParent<Consumer>()
      .AddConstructor<ConsumerWestwood>()

      .AddAttribute("Window", "Initial size of the window", StringValue("1"),
                    MakeUintegerAccessor(&ConsumerWestwood::GetWindow, &ConsumerWestwood::SetWindow),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("PayloadSize",
                    "Average size of content object size (to calculate interest generation rate)",
                    UintegerValue(1040), MakeUintegerAccessor(&ConsumerWestwood::GetPayloadSize,
                                                              &ConsumerWestwood::SetPayloadSize),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("Size", "Amount of data in megabytes to request, relying on PayloadSize "
                            "parameter (alternative to MaxSeq attribute)",
                    DoubleValue(-1), // don't impose limit by default
                    MakeDoubleAccessor(&ConsumerWestwood::GetMaxSize, &ConsumerWestwood::SetMaxSize),
                    MakeDoubleChecker<double>())

      .AddAttribute("MaxSeq", "Maximum sequence number to request (alternative to Size attribute, "
                              "would activate only if Size is -1). "
                              "The parameter is activated only if Size negative (not set)",
                    IntegerValue(std::numeric_limits<uint32_t>::max()),
                    MakeUintegerAccessor(&ConsumerWestwood::GetSeqMax, &ConsumerWestwood::SetSeqMax),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("InitialWindowOnTimeout", "Set window to initial value when timeout occurs",
                    BooleanValue(true),
                    MakeBooleanAccessor(&ConsumerWestwood::m_setInitialWindowOnTimeout),
                    MakeBooleanChecker())

      .AddTraceSource("WindowTrace",
                      "Window that controls how many outstanding interests are allowed",
                      MakeTraceSourceAccessor(&ConsumerWestwood::m_window),
                      "ns3::ndn::ConsumerWestwood::WindowTraceCallback")
      .AddTraceSource("InFlight", "Current number of outstanding interests",
                      MakeTraceSourceAccessor(&ConsumerWestwood::m_inFlight),
                      "ns3::ndn::ConsumerWestwood::WindowTraceCallback");

  return tid;
}

ConsumerWestwood::ConsumerWestwood()
  : m_payloadSize(1040)
  , m_inFlight(0)
{
}

void
ConsumerWestwood::SetWindow(uint32_t window)
{
  m_initialWindow = window;
  m_window = m_initialWindow;
}

uint32_t
ConsumerWestwood::GetWindow() const
{
  return m_initialWindow;
}

uint32_t
ConsumerWestwood::GetPayloadSize() const
{
  return m_payloadSize;
}

void
ConsumerWestwood::SetPayloadSize(uint32_t payload)
{
  m_payloadSize = payload;
}

double
ConsumerWestwood::GetMaxSize() const
{
  if (m_seqMax == 0)
    return -1.0;

  return m_maxSize;
}

void
ConsumerWestwood::SetMaxSize(double size)
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
ConsumerWestwood::GetSeqMax() const
{
  return m_seqMax;
}

void
ConsumerWestwood::SetSeqMax(uint32_t seqMax)
{
  if (m_maxSize < 0)
    m_seqMax = seqMax;

  // ignore otherwise
}

void
ConsumerWestwood::ScheduleNextPacket()
{
  if (m_window == static_cast<uint32_t>(0)) {
    Simulator::Remove(m_sendEvent);

    NS_LOG_DEBUG(
      "Next event in " << (std::min<double>(0.5, m_rtt->RetransmitTimeout().ToDouble(Time::S)))
                       << " sec");
    m_sendEvent =
      Simulator::Schedule(Seconds(
                            std::min<double>(0.5, m_rtt->RetransmitTimeout().ToDouble(Time::S))),
                          &Consumer::SendPacket, this);
  }
  else if (m_inFlight >= m_window) {
    // simply do nothing
  }
  else {
    if (m_sendEvent.IsRunning()) {
      Simulator::Remove(m_sendEvent);
    }

    m_sendEvent = Simulator::ScheduleNow(&Consumer::SendPacket, this);
  }
}

///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
ConsumerWestwood::OnData(shared_ptr<const Data> contentObject)
{
  Consumer::OnData(contentObject);

  // Create a Path Manager for each Path
  uint64_t pathId = contentObject->getPathId();

  if (m_pathManagerMap.find(pathId) == m_pathManagerMap.end()) {
    m_pathManagerMap.insert(std::pair<uint64_t, PathManager>(pathId, PathManager()));
  }
  
  m_prevTime = Simulator::Now().GetMicroSeconds();
  m_pathManagerMap[pathId].Update(contentObject->getContent().size(), 
                        m_rtt->GetCurrentEstimate().GetNanoSeconds(), m_window + 1);



  if (m_isSupress == 0)
    m_window = (double_t)(m_window + 1.0 / m_window);

  if (m_inFlight > static_cast<uint32_t>(0))
    m_inFlight--;
  
  //~ if (GetNode()->GetId() == 1) {
    //~ std::cout << "*****\nNode" << GetNode()->GetId() << ": Current Estimation: " << m_pathManagerMap[pathId].GetCurrentBdp() << std::endl;
    //~ std::cout << "Node" << GetNode()->GetId() << ": Window: " << m_window << ", InFlight: " << m_inFlight << std::endl;
  //~ } else {
    //~ std::cout << "\t\t\t\t\t*****\n\t\t\t\t\tNode" << GetNode()->GetId() << ": Current Estimation: " << m_pathManagerMap[pathId].GetCurrentBdp() << std::endl;
    //~ std::cout << "\t\t\t\t\tNode" << GetNode()->GetId() << ": Window: " << m_window << ", InFlight: " << m_inFlight << std::endl;
  //~ }
  // Wait for a RTT
  if (m_isSupress == 2 && (Simulator::Now() - m_supressTime > m_rtt->GetCurrentEstimate())) {
    m_isSupress = 0;
  }

  if (m_isSupress == 0 && contentObject->getContentType()== 6) { // It means an NACK
    

    m_window = m_pathManagerMap[pathId].GetCurrentBdp() + 1;
    m_isSupress = 2;
    m_supressTime = Simulator::Now();
  }

  ScheduleNextPacket();
}

void
ConsumerWestwood::OnTimeout(uint32_t sequenceNumber)
{
  if (m_inFlight > static_cast<uint32_t>(0))
    m_inFlight--;

  if (m_setInitialWindowOnTimeout) {
    // m_window = std::max<uint32_t> (0, m_window - 1);
    m_window = m_initialWindow;
  }

  NS_LOG_DEBUG("Window: " << m_window << ", InFlight: " << m_inFlight);
  Consumer::OnTimeout(sequenceNumber);
}

void
ConsumerWestwood::WillSendOutInterest(uint32_t sequenceNumber)
{
  m_inFlight++;
  Consumer::WillSendOutInterest(sequenceNumber);
}

} // namespace ndn
} // namespace ns3
