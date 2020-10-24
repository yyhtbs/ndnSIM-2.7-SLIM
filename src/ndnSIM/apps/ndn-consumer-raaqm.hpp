/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2018  Regents of the University of California.
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

#ifndef NDN_CONSUMER_RAAQM_H
#define NDN_CONSUMER_RAAQM_H

#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ndn-consumer.hpp"
#include "ns3/traced-value.h"
#include <ndn-cxx/lp/tags.hpp>

namespace ns3 {
namespace ndn {

class ConsumerRaaqm : public Consumer {
public:
  static TypeId
  GetTypeId();

  /**
   * \brief Default constructor
   */
  ConsumerRaaqm();

  // From App
  virtual void
  OnData(shared_ptr<const Data> contentObject);

  virtual void
  OnTimeout(uint32_t sequenceNumber);

  virtual void
  WillSendOutInterest(uint32_t sequenceNumber);

public:
  typedef std::function<void(double)> WindowTraceCallback;

protected:
  /**
   * \brief Constructs the Interest packet and sends it using a callback to the underlying NDN
   * protocol
   */
  virtual void
  ScheduleNextPacket();
  
  virtual void
  SendPacket();
  
private:
  virtual void
  SetWindow(uint32_t window);

  uint32_t
  GetWindow() const;

  virtual void
  SetPayloadSize(uint32_t payload);

  uint32_t
  GetPayloadSize() const;

  double
  GetMaxSize() const;

  void
  SetMaxSize(double size);

  uint32_t
  GetSeqMax() const;

  void
  SetSeqMax(uint32_t seqMax);


protected:


  class PathManager {

  public:
    PathManager();
    void
    Update(uint64_t rtt);
    bool
    CheckCongestion(uint64_t rtt);
    void
    Reset();

  protected:
    std::list<std::pair<uint64_t, uint64_t> > m_raaqmMetric;
    uint64_t m_baseRtt = UINT64_MAX;
    uint64_t m_minRtt;
    uint64_t m_maxRtt;
    Ptr<UniformRandomVariable> m_rand;
  public:
    uint64_t m_counter = 0;
  };

  uint32_t m_payloadSize; // expected payload size
  double m_maxSize;       // max size to request

  // uint32_t m_counter = 0;

  uint32_t m_initialWindow;
  bool m_setInitialWindowOnTimeout;

  TracedValue<double> m_window;
  TracedValue<uint32_t> m_inFlight;
  std::unordered_map<uint64_t, PathManager> m_pathManagerMap;
  //~ uint64_t m_prevTime = 0;
  
  uint64_t m_totalRtx = 0;
  
  bool m_ss = 1;
  double_t t_totalRTT = 0;
  uint64_t t_totalPkg = 0;

  std::unordered_map<uint32_t, uint64_t> m_interestOutTime;

};

} // namespace ndn
} // namespace ns3

#endif // NDN_CONSUMER_WINDOW_H
