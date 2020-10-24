

#include "rfa-route-strategy.hpp"
#include "algorithm.hpp"
#include "core/logger.hpp"

namespace nfd {
namespace fw {

NFD_LOG_INIT(RfaRouteStrategy);
NFD_REGISTER_STRATEGY(RfaRouteStrategy);

RfaRouteStrategy::RfaRouteStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
  , ProcessNackTraits(this)
{
  ParsedInstanceName parsed = parseInstanceName(name);
  if (!parsed.parameters.empty()) {
    BOOST_THROW_EXCEPTION(std::invalid_argument("RfaRouteStrategy does not accept parameters"));
  }
  if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
    BOOST_THROW_EXCEPTION(std::invalid_argument(
      "RfaRouteStrategy does not support version " + to_string(*parsed.version)));
  }
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
}

const Name&
RfaRouteStrategy::getStrategyName()
{
  static Name strategyName("/localhost/nfd/strategy/rfa-route/%FD%05");
  return strategyName;
}

uint16_t 
faceSelection(std::vector<double_t> input) 
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
RfaRouteStrategy::afterReceiveInterest(const Face& inFace, const Interest& interest,
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
  advance(found, faceSelection(metricArr));
  
  Face& outFace = found->getFace();

  this->sendInterest(pitEntry, outFace, interest);
  
  
  //~ if (hasPendingOutRecords(*pitEntry)) {
    //~ // not a new Interest, don't forward
    //~ return;
  //~ }
  //~ const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  //~ for (const auto& nexthop : fibEntry.getNextHops()) {
    //~ Face& outFace = nexthop.getFace();
    //~ if (!wouldViolateScope(inFace, interest, outFace) &&
        //~ canForwardToLegacy(*pitEntry, outFace)) {
      //~ this->sendInterest(pitEntry, outFace, interest);
      //~ return;
    //~ }
  //~ }
  //~ this->rejectPendingInterest(pitEntry);

}
void
RfaRouteStrategy::afterReceiveData(const shared_ptr<pit::Entry>& pitEntry,
                                       const Face& inFace, const Data& data)
{
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
  dataPtr->setPathId((dataPtr->getPathId() << 6) + (inFacePtr->getId() - 250) % 64);
  dataPtr->wireEncode();

  
  // Call the default  Strategy
  Strategy::afterReceiveData(pitEntry, inFace, data);
  
}
void
RfaRouteStrategy::afterReceiveNack(const Face& inFace, const lp::Nack& nack,
                                     const shared_ptr<pit::Entry>& pitEntry)
{
  // In forwarder.cpp the afterReceiveNack will also trigger ExpiryTimer->InterestFinalize.
  // this function must be disabled, otherwise it will be overlap with InterestFinalize
  //~ Name prefix = Strategy::lookupFib(*pitEntry).getPrefix();
  //~ uint32_t faceId = inFace.getId();

  //~ if (m_metric.find(prefix) == m_metric.end() || m_metric.at(prefix).find(faceId) == m_metric.at(prefix).end())
    //~ std::cout << "Error in rfa-route-strategy.cpp -> afterReceiveNack(): The metric is missing for the given prefix" << std::endl;
  //~ // Remove PI from the Interface
  //~ m_metric[prefix][faceId]--;
  // ************************************
  this->processNack(inFace, nack, pitEntry);
}
void
RfaRouteStrategy::afterSendInterest(const shared_ptr<pit::Entry>& pitEntry, Face& outFace, const Interest& interest)
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
RfaRouteStrategy::afterInterestFinalize(const shared_ptr<pit::Entry>& pitEntry)
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
