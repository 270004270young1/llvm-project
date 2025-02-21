
#ifndef TSAN_LOCAL_READ_MAP_H
#define TSAN_LOCAL_READ_MAP_H

#include "tsan_defs.h"
#include "tsan_shadow.h"
#include "tsan_rtl.h"

class ReadAccessMap;

// struct KeyValPair;

namespace __tsan {

template<unsigned MapSize = 0U, unsigned ShadowCnt = 0U>
class LocalReadMap {
 public:

  LocalReadMap() = default;
  LocalReadMap(const LocalReadMap&) = delete;
  LocalReadMap(LocalReadMap&&) = delete;
  LocalReadMap& operator=(const LocalReadMap&) = delete;
  LocalReadMap& operator=(LocalReadMap&&) = delete;

  void Init(Sid sid){
    sid_ = sid;
    for (int i = 0; i < MapSize; i++) {
      for (int j = 0; j < ShadowCnt; j++) {
        StoreShadow(&localReadMap_[i][j],Shadow::kEmpty);
        atomic_store_relaxed(&addressMap_[i][j],0UL);
      }
    }
  }

  bool Insert(uptr addr, RawShadow rawShadow){
    const unsigned index = CalcHash<MapSize>(addr);
    // Printf("This is address: %lu\n",addr);
    const int pos = FindMatchedOrEmptySlot(index,addr);
    if(pos == -1)
      return false;
    StoreShadow(&localReadMap_[index][pos],rawShadow);
    atomic_store_release(&addressMap_[index][pos],addr);

    return true;
  }
  
  RawShadow Get(uptr addr){
    const unsigned index = CalcHash<MapSize>(addr);
    for(int i=0;i<ShadowCnt;i++){
      if(static_cast<uptr>(atomic_load_acquire(&addressMap_[index][i])) == addr){
        return LoadShadow(&localReadMap_[index][i]);
      }
    }
    
    return Shadow::kEmpty;
  }


 private:
  
  int FindEmptySlot(int index){
    for (int i = 0; i < ShadowCnt; i++) {
      RawShadow oldShadow = LoadShadow(&localReadMap_[index][i]);
      if (oldShadow == Shadow::kEmpty) {
        return i;
      }
    }

    return -1;
  }
 
  int FindMatchedSlot(int index, uptr addr){
      for (int i = 0; i < ShadowCnt; i++) {
        if (static_cast<uptr>(atomic_load_relaxed(&addressMap_[index][i])) == addr) {
          return i;
        }
      }

      return -1;
  }
  
  int FindMatchedOrEmptySlot(int index, uptr addr){
    uptr keys[ShadowCnt];
    for(int i=0;i<ShadowCnt;i++){
      keys[i] = static_cast<uptr>(atomic_load_relaxed(&addressMap_[index][i]));
      if(keys[i] == addr || keys[i] == 0UL){
        return i;
      }
    }

    for(int i=0;i<ShadowCnt;i++){
      uptr loadedAddr = keys[i];

      if(ctx->read_access_map.Contain(loadedAddr,sid_)){
        continue;
      }
      atomic_compare_exchange_strong(&addressMap_[index][i],&loadedAddr,0UL,memory_order_acquire);


      return i;
    }

    return -1;
  }

  Sid sid_;
  VECTOR_ALIGNED RawShadow localReadMap_[MapSize][ShadowCnt];
  VECTOR_ALIGNED atomic_uintptr_t addressMap_[MapSize][ShadowCnt];
  

  // VECTOR_ALIGNED KeyValPair<uptr,RawShadow> localReadMap_[kLocalReadMapSize][kShadowCnt];
  //   VECTOR_ALIGNED int localReadMap_[kThreadSlotCount];
};

}  // namespace __tsan
#endif