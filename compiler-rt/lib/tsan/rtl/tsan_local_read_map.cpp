#include "tsan_local_read_map.h"

#include "tsan_platform.h"
#include "tsan_rtl.h"
#include "sanitizer_common/sanitizer_atomic.h"

namespace __tsan {

LocalReadMap::LocalReadMap(){
    for (int i = 0; i < kThreadSlotCount; i++) {
      for (int j = 0; j < kShadowCnt; j++) {
        // localReadMap_[i][j].key = 0UL;
        // localReadMap_[i][j].val = Shadow::kEmpty;
        localReadMap_[i][j] = Shadow::kEmpty;

      }
    }
}

bool LocalReadMap::Add(uptr addr, RawShadow rawShadow) {
  int index = CalcHash<kLocalReadMapSize>(addr);

  int pos = FindMatchedOrEmptySlot(index,addr);
  if(pos == -1)
    return false;
    
  StoreShadow(&localReadMap_[index][pos],rawShadow);
  return true;
}

void LocalReadMap::Remove(uptr addr) {
  int index = CalcHash<kLocalReadMapSize>(addr);
  int pos = FindMatchedOrEmptySlot(index,addr);
  StoreShadow(&localReadMap_[index][pos],Shadow::kEmpty);

  // for (int i = 0; i < kShadowCnt; i++) {
  //   RawShadow oldShadow = LoadShadow(&localReadMap_[index][i]);
  //   if (ShadowToMem(&oldShadow) == addr) {
  //     int lastPos = FindEmptySlot(index);
  //     if (UNLIKELY(lastPos == -1)) {
  //       return;
  //     }

  //     if (i != lastPos) {
  //       localReadMap_[index][i] = localReadMap_[index][lastPos];
  //     }
  //     localReadMap_[index][lastPos].key = 0;
  //     localReadMap_[index][lastPos].val = Shadow::kEmpty;
  //   }
  // }
}

RawShadow LocalReadMap::Get(uptr addr){

  int index = CalcHash<kLocalReadMapSize>(addr);
  for(int i=0;i<kShadowCnt;i++){
    if(static_cast<uptr>(atomic_load_relaxed(&addressMap_[index][i])) == addr){
      return LoadShadow(&localReadMap_[index][i]);
    }
  }
  
  return Shadow::kEmpty;
}

int LocalReadMap::FindEmptySlot(int index) {
  for (int i = 0; i < kShadowCnt; i++) {
    RawShadow oldShadow = LoadShadow(&localReadMap_[index][i]);
    if (oldShadow == Shadow::kEmpty) {
      return i;
    }
  }

  return -1;
}

int LocalReadMap::FindMatchedSlot(int index, uptr addr){
  for (int i = 0; i < kShadowCnt; i++) {
    if (static_cast<uptr>(atomic_load_relaxed(&addressMap_[index][i])) == addr) {
      return i;
    }
  }

  return -1;
}

int LocalReadMap::FindMatchedOrEmptySlot(int index, uptr addr){

  RawShadow oldShadows[kShadowCnt];
  for(int i=0;i<kShadowCnt;i++){
    if(static_cast<uptr>(atomic_load_relaxed(&addressMap_[index][i])) == addr){
      return i;
    }
    oldShadows[i] = LoadShadow(&localReadMap_[index][i]);
  }

  for(int i=0;i<kShadowCnt;i++){
    if(oldShadows[i] == Shadow::kEmpty)
      return i;
  }

  return -1;
}


}  // namespace __tsan
