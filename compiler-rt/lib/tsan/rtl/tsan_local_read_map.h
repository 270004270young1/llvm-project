
#ifndef TSAN_LOCAL_READ_MAP_H
#define TSAN_LOCAL_READ_MAP_H

#include "tsan_defs.h"
#include "tsan_shadow.h"

// struct KeyValPair;

namespace __tsan {

class LocalReadMap {
 public:
  LocalReadMap();

  bool AddOrUpdate(uptr addr, RawShadow shadow);
  void Remove(uptr addr);
  RawShadow Get(uptr addr);

  LocalReadMap(const LocalReadMap&) = delete;
  LocalReadMap(LocalReadMap&&) = delete;
  LocalReadMap& operator=(const LocalReadMap&) = default;
  LocalReadMap& operator=(LocalReadMap&&) = delete;

 private:
  int FindEmptySlot(int index);
  int FindMatchedSlot(int index, uptr addr);
  int GetMatchedOrEmptySlot(int index, uptr addr);


  VECTOR_ALIGNED RawShadow localReadMap_[kLocalReadMapSize][kShadowCnt];
  VECTOR_ALIGNED atomic_uintptr_t addressMap_[kLocalReadMapSize][kShadowCnt];
  

  // VECTOR_ALIGNED KeyValPair<uptr,RawShadow> localReadMap_[kLocalReadMapSize][kShadowCnt];
  //   VECTOR_ALIGNED int localReadMap_[kThreadSlotCount];
};

}  // namespace __tsan
#endif