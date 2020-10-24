#include "ndn-consumer-icp.hpp"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include <limits>

NS_LOG_COMPONENT_DEFINE("ndn.ConsumerIcp");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(ConsumerIcp);

TypeId
ConsumerIcp::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::ConsumerIcp")
      .SetGroupName("Ndn")
      .SetParent<Consumer>()
      .AddConstructor<ConsumerIcp>()

      .AddAttribute("Window", "Initial size of the window", StringValue("1"),
                    MakeUintegerAccessor(&ConsumerIcp::GetWindow, &ConsumerIcp::SetWindow),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("PayloadSize",
                    "Average size of content object size (to calculate interest generation rate)",
                    UintegerValue(1040), MakeUintegerAccessor(&ConsumerIcp::GetPayloadSize,
                                                              &ConsumerIcp::SetPayloadSize),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("Size", "Amount of data in megabytes to request, relying on PayloadSize "
                            "parameter (alternative to MaxSeq attribute)",
                    DoubleValue(-1), // don't impose limit by default
                    MakeDoubleAccessor(&ConsumerIcp::GetMaxSize, &ConsumerIcp::SetMaxSize),
                    MakeDoubleChecker<double>())

      .AddAttribute("MaxSeq", "Maximum sequence number to request (alternative to Size attribute, "
                              "would activate only if Size is -1). "
                              "The parameter is activated only if Size negative (not set)",
                    IntegerValue(std::numeric_limits<uint32_t>::max()),
                    MakeUintegerAccessor(&ConsumerIcp::GetSeqMax, &ConsumerIcp::SetSeqMax),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("InitialWindowOnTimeout", "Set window to initial value when timeout occurs",
                    BooleanValue(true),
                    MakeBooleanAccessor(&ConsumerIcp::m_setInitialWindowOnTimeout),
                    MakeBooleanChecker())

      .AddTraceSource("WindowTrace",
                      "Window that controls how many outstanding interests are allowed",
                      MakeTraceSourceAccessor(&ConsumerIcp::m_window),
                      "ns3::ndn::ConsumerIcp::WindowTraceCallback")
      .AddTraceSource("InFlight", "Current number of outstanding interests",
                      MakeTraceSourceAccessor(&ConsumerIcp::m_inFlight),
                      "ns3::ndn::ConsumerIcp::WindowTraceCallback");

  return tid;
}

ConsumerIcp::ConsumerIcp()
  : m_payloadSize(1040)
  , m_inFlight(0)
{
}

void
ConsumerIcp::SetWindow(uint32_t window)
{
  m_initialWindow = window;
  m_window = m_initialWindow;
}

uint32_t
ConsumerIcp::GetWindow() const
{
  return m_initialWindow;
}

uint32_t
ConsumerIcp::GetPayloadSize() const
{
  return m_payloadSize;
}

void
ConsumerIcp::SetPayloadSize(uint32_t payload)
{
  m_payloadSize = payload;
}

double
ConsumerIcp::GetMaxSize() const
{
  if (m_seqMax == 0)
    return -1.0;

  return m_maxSize;
}

void
ConsumerIcp::SetMaxSize(double size)
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
ConsumerIcp::GetSeqMax() const
{
  return m_seqMax;
}

void
ConsumerIcp::SetSeqMax(uint32_t seqMax)
{
  if (m_maxSize < 0)
    m_seqMax = seqMax;

  // ignore otherwise
}

void
ConsumerIcp::ScheduleNextPacket()
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
ConsumerIcp::OnData(shared_ptr<const Data> contentObject)
{
  Consumer::OnData(contentObject);

  m_window = m_window + 1 / m_window;

  if (m_inFlight > static_cast<uint32_t>(0))
    m_inFlight--;
  NS_LOG_DEBUG("Window: " << m_window << ", InFlight: " << m_inFlight);

  if (m_count > 0) m_count--;
  ScheduleNextPacket();
}

void
ConsumerIcp::OnTimeout(uint32_t sequenceNumber)
{
  if (m_inFlight > static_cast<uint32_t>(0))
    m_inFlight--;

  if (m_count > 0) { Consumer::OnTimeout(sequenceNumber); m_count--; return; }
  if (m_setInitialWindowOnTimeout) {
    // m_window = std::max<uint32_t> (0, m_window - 1);
    m_count = m_window * 2;
    m_window = std::max<double> (1, m_window * 0.75);
    std::cout << "Reduce Window" << std::endl;
  }

  NS_LOG_DEBUG("Window: " << m_window << ", InFlight: " << m_inFlight);
  Consumer::OnTimeout(sequenceNumber);
}

void
ConsumerIcp::WillSendOutInterest(uint32_t sequenceNumber)
{
  m_inFlight++;
  Consumer::WillSendOutInterest(sequenceNumber);
}

} // namespace ndn
} // namespace ns3
