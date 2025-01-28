#include "tsan_local_read_map.h"

#include "tsan_platform.h"
#include "tsan_rtl.h"
#include "sanitizer_common/sanitizer_atomic.h"

//Constraint: Allow multiple reader but write and remove operation can only be performed by the thread which owns this LocalReadMap. Add() and Remove() operations are thread-safe under this constraint.

namespace __tsan {

LocalReadMap::LocalReadMap(Sid sid):sid_(sid){
    for (int i = 0; i < kThreadSlotCount; i++) {
      for (int j = 0; j < kShadowCnt; j++) {
        // localReadMap_[i][j].key = 0UL;
        // localReadMap_[i][j].val = Shadow::kEmpty;
        StoreShadow(&localReadMap_[i][j],Shadow::kEmpty);
        atomic_store_relaxed(&addressMap_[i][j],0UL);
      }
    }
}

// We only have one producer here and multiple consumer so we don't need 
// to worry about the multiple threads write to the same addressMap and
// localReadMap
bool LocalReadMap::AddOrUpdate(uptr addr, RawShadow rawShadow) {
  const int index = CalcHash<kLocalReadMapSize>(addr);

  const int pos = FindMatchedOrEmptySlot(index,addr);
  if(pos == -1)
    return false;
  StoreShadow(&localReadMap_[index][pos],rawShadow);
  atomic_store_release(&addressMap_[index][pos],addr);

  return true;
}

void LocalReadMap::Remove(uptr addr) {
  const int index = CalcHash<kLocalReadMapSize>(addr);
  const int pos = FindMatchedSlot(index,addr);
  if(pos == -1)
    return;

  StoreShadow(&localReadMap_[index][pos],Shadow::kEmpty);
  atomic_store_release(&addressMap_[index][pos],0UL);
}

RawShadow LocalReadMap::Get(uptr addr){

  const int index = CalcHash<kLocalReadMapSize>(addr);
  for(int i=0;i<kShadowCnt;i++){
    if(static_cast<uptr>(atomic_load_acquire(&addressMap_[index][i])) == addr){
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

  for(int i=0;i<kShadowCnt;i++){
    if(static_cast<uptr>(atomic_load_relaxed(&addressMap_[index][i])) == addr){
      return i;
    }
  }

  for(int i=0;i<kShadowCnt;i++){
    uptr loadedAddr = static_cast<uptr>(atomic_load_relaxed(&addressMap_[index][i]));
    if(loadedAddr != 0UL){

      if(ctx->read_access_map.Contain(loadedAddr,sid_)){
        continue;
      }
      atomic_compare_exchange_strong(&addressMap_[index][i],&loadedAddr,0UL,memory_order_acquire);

    }

    return i;
  }

  return -1;
}


}  // namespace __tsan
