
#ifndef NFD_DAEMON_FW_RFA_ROUTE_STRATEGY_HPP
#define NFD_DAEMON_FW_RFA_ROUTE_STRATEGY_HPP

#include "strategy.hpp"
#include "process-nack-traits.hpp"
#include "retx-suppression-exponential.hpp"

namespace nfd {
namespace fw {

class RfaRouteStrategy : public Strategy
                         , public ProcessNackTraits<RfaRouteStrategy>
{
public:
  explicit
  RfaRouteStrategy(Forwarder& forwarder, const Name& name = getStrategyName());

  static const Name&
  getStrategyName();

  void
  afterReceiveInterest(const Face& inFace, const Interest& interest,
                       const shared_ptr<pit::Entry>& pitEntry) override;

  void
  afterReceiveData(const shared_ptr<pit::Entry>& pitEntry,
                       const Face& inFace, const Data& data);

  void
  afterReceiveNack(const Face& inFace, const lp::Nack& nack,
                   const shared_ptr<pit::Entry>& pitEntry) override;

  void
  afterSendInterest(const shared_ptr<pit::Entry>& pitEntry, Face& outFace,
                   const Interest& interest) override;

  void
  afterInterestFinalize(const shared_ptr<pit::Entry>& pitEntry) override; 
  
  std::unordered_map<Name, std::unordered_map<uint32_t, int32_t> > m_metric;
  
  friend ProcessNackTraits<RfaRouteStrategy>;
};

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_BEST_ROUTE_STRATEGY2_HPP
