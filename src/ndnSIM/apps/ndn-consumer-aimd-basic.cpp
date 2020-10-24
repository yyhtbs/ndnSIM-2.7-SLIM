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

#include "ndn-consumer-aimd-basic.hpp"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include <limits>

NS_LOG_COMPONENT_DEFINE("ndn.ConsumerAimdBasic");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(ConsumerAimdBasic);

TypeId
ConsumerAimdBasic::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::ConsumerAimdBasic")
      .SetGroupName("Ndn")
      .SetParent<Consumer>()
      .AddConstructor<ConsumerAimdBasic>()

      .AddAttribute("Window", "Initial size of the window", StringValue("5"),
                    MakeUintegerAccessor(&ConsumerAimdBasic::GetWindow, &ConsumerAimdBasic::SetWindow),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("PayloadSize",
                    "Average size of content object size (to calculate interest generation rate)",
                    UintegerValue(1040), MakeUintegerAccessor(&ConsumerAimdBasic::GetPayloadSize,
                                                              &ConsumerAimdBasic::SetPayloadSize),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("Size", "Amount of data in megabytes to request, relying on PayloadSize "
                            "parameter (alternative to MaxSeq attribute)",
                    DoubleValue(-1), // don't impose limit by default
                    MakeDoubleAccessor(&ConsumerAimdBasic::GetMaxSize, &ConsumerAimdBasic::SetMaxSize),
                    MakeDoubleChecker<double>())

      .AddAttribute("MaxSeq", "Maximum sequence number to request (alternative to Size attribute, "
                              "would activate only if Size is -1). "
                              "The parameter is activated only if Size negative (not set)",
                    IntegerValue(std::numeric_limits<uint32_t>::max()),
                    MakeUintegerAccessor(&ConsumerAimdBasic::GetSeqMax, &ConsumerAimdBasic::SetSeqMax),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("InitialWindowOnTimeout", "Set window to initial value when timeout occurs",
                    BooleanValue(true),
                    MakeBooleanAccessor(&ConsumerAimdBasic::m_setInitialWindowOnTimeout),
                    MakeBooleanChecker())

      .AddTraceSource("WindowTrace",
                      "Window that controls how many outstanding interests are allowed",
                      MakeTraceSourceAccessor(&ConsumerAimdBasic::m_window),
                      "ns3::ndn::ConsumerAimdBasic::WindowTraceCallback")
      .AddTraceSource("InFlight", "Current number of outstanding interests",
                      MakeTraceSourceAccessor(&ConsumerAimdBasic::m_inFlight),
                      "ns3::ndn::ConsumerAimdBasic::WindowTraceCallback");

  return tid;
}

ConsumerAimdBasic::ConsumerAimdBasic()
  : m_payloadSize(1040)
  , m_inFlight(0)
{
}

void
ConsumerAimdBasic::SetWindow(uint32_t window)
{
  m_initialWindow = window;
  m_window = m_initialWindow;
}

uint32_t
ConsumerAimdBasic::GetWindow() const
{
  return m_initialWindow;
}

uint32_t
ConsumerAimdBasic::GetPayloadSize() const
{
  return m_payloadSize;
}

void
ConsumerAimdBasic::SetPayloadSize(uint32_t payload)
{
  m_payloadSize = payload;
}

double
ConsumerAimdBasic::GetMaxSize() const
{
  if (m_seqMax == 0)
    return -1.0;

  return m_maxSize;
}

void
ConsumerAimdBasic::SetMaxSize(double size)
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
ConsumerAimdBasic::GetSeqMax() const
{
  return m_seqMax;
}

void
ConsumerAimdBasic::SetSeqMax(uint32_t seqMax)
{
  if (m_maxSize < 0)
    m_seqMax = seqMax;

  // ignore otherwise
}

void
ConsumerAimdBasic::ScheduleNextPacket()
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

    m_sendEvent = Simulator::ScheduleNow(&ConsumerAimdBasic::SendPacket, this);
  }
}
void
ConsumerAimdBasic::SendPacket()
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
  //~ std::cout << "Time: " <<   Simulator::Now().GetMilliSeconds()  << "node(" << GetNode()->GetId() << ") requesting Interest: " << interest->getName() << std::endl;

  m_interestOutTime[seq] = Simulator::Now().GetMicroSeconds();

  WillSendOutInterest(seq);

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);

  ScheduleNextPacket();
}
///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
ConsumerAimdBasic::OnData(shared_ptr<const Data> contentObject)
{
  Consumer::OnData(contentObject);


  if (m_inFlight > static_cast<uint32_t>(0))
    m_inFlight--;
  NS_LOG_DEBUG("Window: " << m_window << ", InFlight: " << m_inFlight);


  m_counter = m_counter >= 1 ? m_counter - 1 : 0;
  
  uint32_t seq = contentObject->getName().at(-1).toSequenceNumber();

  uint64_t rtt = (Simulator::Now().GetMicroSeconds() - m_interestOutTime[seq]) / 1.0e3;

  t_totalRTT += (double_t)rtt;
  t_totalPkg += 1;
  
  //~ std::cout << "Current  RTT is " << rtt << std::endl;

  //~ std::cout << "Current Mean RTT is " << t_totalRTT / t_totalPkg << std::endl;

  if (contentObject->getContentType() == 6 && m_counter == 0) {
    std::cout << "Node " << GetNode()->GetId() << "Congestion!" << std::endl;
    m_counter = m_window * 2;
    m_window = ((double_t)m_window * 0.875 > 1.0) ? (double_t)m_window * 0.875 : 1.0;
    m_ss = 0;
    
  }
  else {
    if (m_ss == 1)
      m_window = m_window + 0.5;
      //~ m_window = m_window + 2 / m_window;
    else
      m_window = m_window + 2 / m_window;
  }

  ScheduleNextPacket();
}

void
ConsumerAimdBasic::OnTimeout(uint32_t sequenceNumber)
{
  if (m_inFlight > static_cast<uint32_t>(0))
    m_inFlight--;

  if (m_setInitialWindowOnTimeout) {
    // m_window = std::max<uint32_t> (0, m_window - 1);
    // m_window = m_initialWindow;
  }

  m_totalRtx++;

  // std::cout << "Node " << GetNode()->GetId() << ": Total RTX " << m_totalRtx << std::endl;

  NS_LOG_DEBUG("Window: " << m_window << ", InFlight: " << m_inFlight);
  Consumer::OnTimeout(sequenceNumber);
}

void
ConsumerAimdBasic::WillSendOutInterest(uint32_t sequenceNumber)
{
  m_inFlight++;
  Consumer::WillSendOutInterest(sequenceNumber);
}

} // namespace ndn
} // namespace ns3
