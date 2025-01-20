#include "tsan_read_access_map.h"

#include "tsan_rtl.h"

namespace __tsan {

bool ReadAccessMap::Add(uptr addr, Sid sid) {
  int index = CalcHash<kReadAccessMapSize>(addr);

  Pair* pair = FindMatchedPair(index, addr);
  bool matched = pair != nullptr;
  pair = pair == nullptr ? FindEmptyPair(index) : pair;

  if (pair == nullptr)
    return false;

  u64 cell = atomic_load_acquire(&pair->val[static_cast<u8>(sid) >> 6]);
  u64 bits = 1ULL << (static_cast<u8>(sid) & 63ULL);
  if (cell & bits) {
    return true;
  }
  if (!atomic_compare_exchange_strong(&pair->key, matched ? &addr : 0UL, addr,
                                      memory_order_relaxed))
    return false;

  atomic_store_release(&pair->val[static_cast<u8>(sid) >> 6], cell | bits);
  return true;
}

Pair* ReadAccessMap::FindMatchedPair(int index, uptr addr) {
  for (int i = 0; i < kShadowCnt; i++) {
    if (atomic_load_relaxed(&readAccessMap_[index][i].key) == addr) {
      return &readAccessMap_[index][i];
    }
  }

  return nullptr;
}

Pair* ReadAccessMap::FindEmptyPair(int index) {
  for (int i = 0; i < kShadowCnt; i++) {
    bool found = true;
    for (int j = 0; j < kReadAccessMapThreadCellSize; j++) {
      if (atomic_load_relaxed(&readAccessMap_[index][i].val[j]) != 0ULL) {
        found = false;
        break;
      }
    }
    if (found) {
      return &readAccessMap_[index][i];
    }
  }
  return nullptr;
}

void ReadAccessMap::Remove(uptr addr) {
  int index = CalcHash<kReadAccessMapSize>(addr);
  for (int i = 0; i < kShadowCnt; i++) {
    if (atomic_load_relaxed(&readAccessMap_[index][i].key) != addr)
      continue;

    for (int j = 0; j < kReadAccessMapThreadCellSize; j++) {
      atomic_store_relaxed(&readAccessMap_[index][i].val[j], 0ULL);
    }
    atomic_store_release(&readAccessMap_[index][i].key, 0UL);
  }
}

}  // namespace __tsan
