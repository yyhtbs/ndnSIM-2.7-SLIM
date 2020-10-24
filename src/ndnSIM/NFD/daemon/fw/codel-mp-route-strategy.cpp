#include "codel-mp-route-strategy.hpp"
#include "algorithm.hpp"
#include "core/logger.hpp"

namespace nfd {
namespace fw {

NFD_LOG_INIT(CodelMpRouteStrategy);
NFD_REGISTER_STRATEGY(CodelMpRouteStrategy);

const time::milliseconds CodelMpRouteStrategy::RETX_SUPPRESSION_INITIAL(10);
const time::milliseconds CodelMpRouteStrategy::RETX_SUPPRESSION_MAX(250);

CodelMpRouteStrategy::CodelMpRouteStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
  , ProcessNackTraits(this)
  , m_retxSuppression(RETX_SUPPRESSION_INITIAL,
                      RetxSuppressionExponential::DEFAULT_MULTIPLIER,
                      RETX_SUPPRESSION_MAX)
{
  ParsedInstanceName parsed = parseInstanceName(name);
  if (!parsed.parameters.empty()) {
    BOOST_THROW_EXCEPTION(std::invalid_argument("CodelMpRouteStrategy does not accept parameters"));
  }
  if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
    BOOST_THROW_EXCEPTION(std::invalid_argument(
      "CodelMpRouteStrategy does not support version " + to_string(*parsed.version)));
  }
  this->setInstanceName(makeInstanceName(name, getStrategyName()));

}

const Name&
CodelMpRouteStrategy::getStrategyName()
{
  static Name strategyName("/localhost/nfd/strategy/codel-mp-route/%FD%01");
  return strategyName;
}

uint16_t 
faceSelectionCodel(std::vector<double_t> input) 
{
  double_t k = 0;
  uint16_t idx = 0;
  for (uint16_t i = 0; i < input.size(); i++) {
    if (input[i] > k) {
      idx = i;
      k = input[i];
    }
  }
  return idx;
}

void
CodelMpRouteStrategy::afterReceiveInterest(const Face& inFace, const Interest& interest,
                                         const shared_ptr<pit::Entry>& pitEntry)
{
  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();
  
  if (nexthops.begin() == nexthops.end()) {
    lp::NackHeader nackHeader;
    nackHeader.setReason(lp::NackReason::NO_ROUTE);
    this->sendNack(pitEntry, inFace, nackHeader);

    this->rejectPendingInterest(pitEntry);
    return;
  }
  
  std::vector<double_t> metricArr;
  
  auto & rt = m_metric[fibEntry.getPrefix()];
  for (auto it = nexthops.begin(); it != nexthops.end(); ++it) {
    
    // std::cout << "Face: " << it->getFace().getId() << " === #PI: " << rt[it->getFace().getId()] << std::endl;
    
    if (rt.find(it->getFace().getId()) == rt.end())
      metricArr.push_back(0.5);    
    else  // Record the Metric
      metricArr.push_back(1.0 / (1.0 + (double_t)(rt[it->getFace().getId()])));    
  }

  auto found = nexthops.begin();
  advance(found, faceSelectionCodel(metricArr));
  
  Face& outFace = found->getFace();

  this->sendInterest(pitEntry, outFace, interest);
  
}

void
CodelMpRouteStrategy::afterReceiveNack(const Face& inFace, const lp::Nack& nack,
                                     const shared_ptr<pit::Entry>& pitEntry)
{
  this->processNack(inFace, nack, pitEntry);
}

void
CodelMpRouteStrategy::afterReceiveData(const shared_ptr<pit::Entry>& pitEntry,
                                       const Face& inFace, const Data& data)
{
  /// ******* Added by Yuhang + START
  Name prefix = Strategy::lookupFib(*pitEntry).getPrefix();
  uint32_t faceId = inFace.getId();

  if (m_metric.find(prefix) == m_metric.end() || m_metric.at(prefix).find(faceId) == m_metric.at(prefix).end())
    std::cout << "Error in rfa-route-strategy.cpp -> afterReceiveData(): The metric is missing for the given prefix" << std::endl;
  // Remove PI from the ALL Interface
  for (auto it = pitEntry->out_begin(); it != pitEntry->out_end(); ++it) {
    uint32_t faceId = it->getFace().getId();
    m_metric[prefix][faceId]--;
  }  
  
  Data* dataPtr = const_cast<Data*>(&data);
  Face* inFacePtr = const_cast<Face*>(&inFace);
  dataPtr->wireDecode(data.m_wire);

  uint64_t rtt = (time::steady_clock::now() - pitEntry->getOutRecord(*inFacePtr)->getLastRenewed()).count();

  uint64_t hopRtt = rtt - dataPtr->getNextHopRtt();

  dataPtr->setNextHopRtt(rtt);
  dataPtr->setPathId((dataPtr->getPathId() << 6) + (inFacePtr->getId() - 250) % 64);

  // # Set Congestion Level to the Face
  // ## set baseHopRtt
  if (hopRtt < inFacePtr->m_baseHopRtt)
    inFacePtr->m_baseHopRtt = hopRtt;

  #define __ALPHA__ 0.5
    // ## set measured queuig delay
  inFacePtr->m_queueDelay = __ALPHA__ * (hopRtt - inFacePtr->m_baseHopRtt) 
                                            + (1 - __ALPHA__) * inFacePtr->m_queueDelay;

  // # CONSTANT DECLARATION
  #define __TARGET_SOJOURN_TIME__ 5e6
  #define __MAX_SOJOURN_TIME__ 20e6

  // # Check whether the sojourn time is above the target

  if (hopRtt - inFacePtr->m_baseHopRtt >= 100e6) {
    inFacePtr->m_firstAboveTime = time::steady_clock::now();
    inFacePtr->m_isChecking = 1;
    dataPtr->setContentType(6);
    inFacePtr->m_totalLoss = inFacePtr->m_totalLoss + 1;
    inFacePtr->m_reactTimeWindow = inFacePtr->m_reactTimeWindow / sqrt(inFacePtr->m_totalLoss);
    
  }
  else {
    if (hopRtt - inFacePtr->m_baseHopRtt >= __TARGET_SOJOURN_TIME__) { // 
      // Check if the Face needs to enter the Dropping State
      if (inFacePtr->m_isChecking == 0) { // Not in a checking status, enter Drop-Check State
        inFacePtr->m_firstAboveTime = time::steady_clock::now();
        inFacePtr->m_isChecking = 1;
      }
      else { 
        if ((time::steady_clock::now() - inFacePtr->m_firstAboveTime).count() > inFacePtr->m_reactTimeWindow) {  
          // The Queue does not converge during Drop-Checking State.
          dataPtr->setContentType(6);

          inFacePtr->m_totalLoss = inFacePtr->m_totalLoss + 1;
          inFacePtr->m_firstAboveTime = time::steady_clock::now();
          inFacePtr->m_reactTimeWindow = inFacePtr->m_reactTimeWindow / sqrt(inFacePtr->m_totalLoss);

        } else {;} // 
      }
    }
    else if (hopRtt - inFacePtr->m_baseHopRtt < __TARGET_SOJOURN_TIME__) {
      inFacePtr->m_isChecking = 0;
      inFacePtr->m_totalLoss = 0;
      inFacePtr->m_reactTimeWindow = __MAX_SOJOURN_TIME__;
    }
  }

  dataPtr->wireEncode();


  Strategy::afterReceiveData(pitEntry, inFace, data);

  /// ******* Added by Yuhang - END}
}

void
CodelMpRouteStrategy::afterSendInterest(const shared_ptr<pit::Entry>& pitEntry, Face& outFace, const Interest& interest)
{
  Name prefix = Strategy::lookupFib(*pitEntry).getPrefix();
  uint32_t faceId = outFace.getId();

  if (m_metric.find(prefix) == m_metric.end()) {
    
    std::unordered_map<uint32_t, int32_t> value = {{faceId, 0}};
    m_metric.insert({prefix, value});
  }
  if (m_metric.at(prefix).find(faceId) == m_metric.at(prefix).end()) {
    // Create a entry in the map
    m_metric.at(prefix).insert({faceId, 0});
  }
  m_metric[prefix][faceId]++;
}

void
CodelMpRouteStrategy::afterInterestFinalize(const shared_ptr<pit::Entry>& pitEntry)
{
  Name prefix = Strategy::lookupFib(*pitEntry).getPrefix();

// Remove the Number of Pending Interest
  for (auto it = pitEntry->out_begin(); it != pitEntry->out_end(); ++it) {
    uint32_t faceId = it->getFace().getId();
    m_metric[prefix][faceId]--;
  }
    
}
} // namespace fw
} // namespace nfd
