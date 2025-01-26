#include "tsan_read_access_map.h"

#include "tsan_rtl.h"

namespace __tsan {

ReadAccessMap::ReadAccessMap() {
  for (unsigned i = 0; i < kReadAccessMapSize; i++) {
    for (unsigned j = 0; j < kShadowCnt; j++) {
      atomic_store_relaxed(&readAccessMap_[i][j].key, 0UL);
      for (unsigned index = 0; index < kReadAccessMapThreadCellSize; index++) {
        atomic_store_relaxed(&readAccessMap_[i][j].val[index], 0ULL);
      }
    }
  }
}

bool ReadAccessMap::Add(uptr addr, Sid sid) {
  int index = CalcHash<kReadAccessMapSize>(addr);

  if (Pair* pair = FindMatchedPair(index, addr)) {
    u64 cell = atomic_load_acquire(&pair->val[static_cast<u8>(sid) >> 6]);
    u64 bits = 1ULL << (static_cast<u8>(sid) & 63ULL);
    if (cell & bits) {
      return true;
    }
    UpdateCell(pair, sid);
    return true;
  }

  for (int i = 0; i < kShadowCnt; i++) {
    uptr key = atomic_load_acquire(&readAccessMap_[index][i].key);
    if (key != 0UL || key != addr)
      continue;

    u64 bits = 1ULL << (static_cast<u8>(sid) & 63ULL);
    if (key == addr) {
      u64 cell = atomic_load_acquire(
          &readAccessMap_[index][i].val[static_cast<u8>(sid) >> 6]);
      if (cell & bits) {
        return true;
      }
      UpdateCell(&readAccessMap_[index][i], sid);
      return true;
    }

    if (atomic_compare_exchange_strong(&readAccessMap_[index][i].key, 0UL, addr,
                                       memory_order_acq_rel)) {
      UpdateCell(&readAccessMap_[index][i], sid);
      return true;
    }

    if (atomic_load_acquire(&readAccessMap_[index][i].key) == addr) {
      UpdateCell(&readAccessMap_[index][i], sid);
      return true;
    }
  }

  return false;
  // bool matched = pair != nullptr;
  // pair = pair == nullptr ? FindEmptyPair(index) : pair;

  // if (pair == nullptr)
  //   return false;

  // u64 cell = atomic_load_acquire(&pair->val[static_cast<u8>(sid) >> 6]);
  // u64 bits = 1ULL << (static_cast<u8>(sid) & 63ULL);
  // if (cell & bits) {
  //   return true;
  // }
  // if (atomic_load_relaxed(&pair->key) == 0UL &&
  // !atomic_compare_exchange_strong(&pair->key, 0UL, addr,
  //                                     memory_order_relaxed))
  //   return false;

  // do{
  //   cell = atomic_load_relaxed(&pair->val[static_cast<u8>(sid) >> 6]);
  // }
  // while(!atomic_compare_exchange_weak(&pair->val[static_cast<u8>(sid) >>
  // 6],&cell,cell|bits,memory_order_acq_rel));
  // // atomic_store_release(&pair->val[static_cast<u8>(sid) >> 6], cell |
  // bits); return true;
}

Pair* ReadAccessMap::FindMatchedPair(int index, uptr addr) {
  for (int i = 0; i < kShadowCnt; i++) {
    if (atomic_load_acquire(&readAccessMap_[index][i].key) == addr) {
      return &readAccessMap_[index][i];
    }
  }

  return nullptr;
}

Pair* ReadAccessMap::GetEmptyPair(int index, uptr addr) {
  for (int i = 0; i < kShadowCnt; i++) {
    uptr key = atomic_load_acquire(&readAccessMap_[index][i].key);
    if (key != 0UL || key != addr)
      continue;

    if (key == addr)
      return &readAccessMap_[index][i];

    if (atomic_compare_exchange_strong(&readAccessMap_[index][i].key, 0UL, addr,
                                       memory_order_relaxed)) {
      return &readAccessMap_[index][i];
    }

    if (atomic_load_relaxed(&readAccessMap_[index][i].key) == addr) {
      return &readAccessMap_[index][i];
    }
  }
  return nullptr;
}

void ReadAccessMap::Remove(uptr addr) {
  int index = CalcHash<kReadAccessMapSize>(addr);
  for (int i = 0; i < kShadowCnt; i++) {
    if (atomic_load_acquire(&readAccessMap_[index][i].key) != addr)
      continue;

    for (int j = 0; j < kReadAccessMapThreadCellSize; j++) {
      atomic_store_relaxed(&readAccessMap_[index][i].val[j], 0ULL);
    }
    atomic_store_release(&readAccessMap_[index][i].key, 0UL);
  }
}

bool ReadAccessMap::Contain(uptr addr, Sid sid){

  int index = CalcHash<kReadAccessMapSize>(addr);
  u64 bits = 1ULL << (static_cast<u8>(sid) & 63ULL);
  for(unsigned i=0;i<kShadowCnt;i++){
    if(atomic_load_acquire(&readAccessMap_[index][i].key) == addr){
      
      u64 cell = atomic_load_acquire(&readAccessMap_[index][i].val[static_cast<u8>(sid) >> 6]);

      return cell & bits;
    }
  }
  return false;

}

void UpdateCell(Pair* pair, Sid sid) {
  u64 cell;
  u64 bits = 1ULL << (static_cast<u8>(sid) & 63ULL);
  do {
    cell = atomic_load_acquire(&pair->val[static_cast<u8>(sid) >> 6]);
  } while (!atomic_compare_exchange_weak(&pair->val[static_cast<u8>(sid) >> 6],
                                         &cell, cell | bits,
                                         memory_order_release));
}

}  // namespace __tsan
