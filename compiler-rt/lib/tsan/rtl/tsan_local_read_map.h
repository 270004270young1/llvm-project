
#ifndef TSAN_LOCAL_READ_MAP_H
#define TSAN_LOCAL_READ_MAP_H

#include "tsan_defs.h"
#include "tsan_shadow.h"

// struct KeyValPair;

namespace __tsan {

class LocalReadMap {
 public:

  bool Insert(uptr addr, RawShadow shadow);
  RawShadow Get(uptr addr);
  void Init(Sid sid);

  LocalReadMap() = default;
  LocalReadMap(const LocalReadMap&) = delete;
  LocalReadMap(LocalReadMap&&) = delete;
  LocalReadMap& operator=(const LocalReadMap&) = default;
  LocalReadMap& operator=(LocalReadMap&&) = delete;

 private:
  int FindEmptySlot(int index);
  int FindMatchedSlot(int index, uptr addr);
  int FindMatchedOrEmptySlot(int index, uptr addr);

  Sid sid_;
  VECTOR_ALIGNED RawShadow localReadMap_[kLocalReadMapSize][kShadowCnt];
  VECTOR_ALIGNED atomic_uintptr_t addressMap_[kLocalReadMapSize][kShadowCnt];
  

  // VECTOR_ALIGNED KeyValPair<uptr,RawShadow> localReadMap_[kLocalReadMapSize][kShadowCnt];
  //   VECTOR_ALIGNED int localReadMap_[kThreadSlotCount];
};

}  // namespace __tsan
#endif