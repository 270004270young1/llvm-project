
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
        StoreShadow(&localReadMap_[i][0][j],Shadow::kEmpty);
        StoreShadow(&localReadMap_[i][1][j],Shadow::kEmpty);
        atomic_store_relaxed(&addressMap_[i][0][j],0UL);
        atomic_store_relaxed(&addressMap_[i][1][j],0UL);

      }
      atomic_store_released(&swapIndex_[i],false);
    }
  }

  bool Insert(uptr addr, RawShadow rawShadow){
    const unsigned index = CalcHash<MapSize>(addr);
    unsigned swapIndex = atomic_load_relaxed(&swapIndex_[index]);
    uptr keys[ShadowCnt] = {0UL};
    RawShadow shadows[ShadowCnt] = {Shadow::kEmpty};
    for(unsigned i=0;i<ShadowCnt;i++){
      keys[i] = atomic_load_acquire(&addressMap_[index][swapIndex][i]);
      shadows[i] = atomic_load_relaxed(&localReadMap_[index][swapIndex][i]);
      if(key!=addr || key!=0UL){
        continue;        
      }

      StoreShadow(&localReadMap_[index][swapIndex][pos],rawShadow);
      if(key == 0UL){
        atomic_store_release(&addressMap_[index][swapIndex][pos],addr);
      }
      return true;
    }

    bool isOutdated[ShadowCnt];
    bool shouldGC = false;
    for(unsigned i=0;i<ShadowCnt;i++){
      isOutdated[i] = ctx->read_access_map.Contain(keys[i],sid_);
      shouldGC |= isOutdated[i];
    }

    if(!shouldGC)
      return false;

    bool stored = false;
    for(unsigned i=0;i<ShadowCnt;i++){

      if(!isOutdated[i]){
        atomic_store_relaxed(&localReadMap_[index][!swapIndex][i],shadows[i]);
        atomic_store_released(&addressMap_[index][!swapIndex][i],keys[i]);
      }else{
        
        if(!stored){
          atomic_store_relaxed(&localReadMap_[index][!swapIndex][i],rawShadow);
          atomic_store_released(&addressMap_[index][!swapIndex][i],addr);
          stored = true;
        }else{
          atomic_store_relaxed(&localReadMap_[index][!swapIndex][i],Shadow::kEmpty);
          atomic_store_released(&addressMap_[index][!swapIndex][i],0UL);
        }
        
      }
      
      atomic_store_relaxed(&localReadMap_[index][swapIndex][i],Shadow::FreedMarker);
      atomic_store_released(&addressMap_[index][swapIndex][i],0UL);


    }

    atomic_store_release(&swapIndex_[index],!swapIndex);
    return stored;
  }
  
  RawShadow Get(uptr addr){
    const unsigned index = CalcHash<MapSize>(addr);
    bool expected = false;
    //The purpose of this compare_exchange is to force the reader threads to read the latest value written by the thread of the map owner and build release-acquire relation with the latest update.
    atomic_compare_exchange_strong(&swapIndex_[index],&expected,0,memory_order_acquire);
    unsigned swapIndex = expected;

    for(int i=0;i<ShadowCnt;i++){
      if(static_cast<uptr>(atomic_load_relaxed(&addressMap_[index][swapIndex][i])) == addr){
        RawShadow rawShadow = LoadShadow(&localReadMap_[index][swapIndex][i]);
        if(rawShadow == Shadow::FreedMarker){
          rawShadow = atomic_load_relaxed(addressMap_[index][!swapIndex][i]) == addr ? atomic_load_relaxed(&localReadMap_[index][!swapIndex][i]) : rawShadow;
        }
        return rawShadow == Shadow::FreedMarker ? Shadow::kEmpty : rawShadow;
      }
    }
    
    return Shadow::kEmpty;
  }


 private:
  
  // int FindEmptySlot(int index){
  //   for (int i = 0; i < ShadowCnt; i++) {
  //     RawShadow oldShadow = LoadShadow(&localReadMap_[index][i]);
  //     if (oldShadow == Shadow::kEmpty) {
  //       return i;
  //     }
  //   }

  //   return -1;
  // }
 
  // int FindMatchedSlot(int index, uptr addr){
  //     for (int i = 0; i < ShadowCnt; i++) {
  //       if (static_cast<uptr>(atomic_load_relaxed(&addressMap_[index][i])) == addr) {
  //         return i;
  //       }
  //     }

  //     return -1;
  // }
  
  // int FindMatchedOrEmptySlot(unsigned index, unsigned swapIndex, uptr addr){
  //   uptr keys[ShadowCnt];
  //   for(int i=0;i<ShadowCnt;i++){
  //     keys[i] = static_cast<uptr>(atomic_load_relaxed(&addressMap_[index][swapIndex][i]));
  //     if(keys[i] == addr || keys[i] == 0UL){
  //       return i;
  //     }
  //   }

  //   for(int i=0;i<ShadowCnt;i++){
  //     uptr loadedAddr = keys[i];

  //     if(ctx->read_access_map.Contain(loadedAddr,sid_)){
  //       continue;
  //     }
  //     // atomic_compare_exchange_strong(&addressMap_[index][swapIndex][i],&loadedAddr,0UL,memory_order_acquire);

  //     return i;
  //   }

  //   return -1;
  // }

  Sid sid_;
  VECTOR_ALIGNED RawShadow localReadMap_[MapSize][2][ShadowCnt];
  atomic_uintptr_t addressMap_[MapSize][2][ShadowCnt];
  atomic_bool_t swapIndex_[MapSize];
  

  // VECTOR_ALIGNED KeyValPair<uptr,RawShadow> localReadMap_[kLocalReadMapSize][kShadowCnt];
  //   VECTOR_ALIGNED int localReadMap_[kThreadSlotCount];
};

}  // namespace __tsan
#endif