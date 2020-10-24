
#ifndef NDN_LT_ESTIMATOR_H
#define NDN_LT_ESTIMATOR_H

namespace ns3 {

namespace ndn {

class LtEstimator {
public:

  LtEstimator();

  void
  Update(uint64_t currentRtt);

  uint64_t
  GetMinEstimate(void) const;

  virtual void
  Reset();

private:
  uint64_t m_minRtt;   // The Minimum RTT Estimation
  
  uint64_t m_avgRtt;   // The Average RTT Estimation
  
  uint16_t m_exampleAttribute;

};

} // namespace ndn

} // namespace ns3

#endif /* RTT_ESTIMATOR_H */
