#ifndef NX_PRIORITIZER_H
#define NX_PRIORITIZER_H

#include "traversal.h"

namespace nx {

class Prioritizer: public MetricTraversal {
 public:
  uint32_t current_frame;

  Prioritizer(): bias(1.0f), prefetch(200), prefetched(10) {}

  void prioritize(Nexus *nx, uint32_t frame);

  void setBias(float _bias) { bias = _bias; }
  //number of nodes traversed after the termination condition is met.
  void setPrefetch(uint32_t _prefetch) { prefetch = _prefetch; }

 protected:
  Action expand(HeapNode h);

  float bias;         //give priority to one model over another.
  uint32_t prefetch;   //patch prefetched
  uint32_t prefetched;
};

} //namespace

#endif // PRIORITIZER_H
