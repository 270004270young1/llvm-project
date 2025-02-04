#ifndef TSAN_READ_ACCESS_MAP_H
#define TSAN_READ_ACCESS_MAP_H

#include "tsan_defs.h"
namespace __tsan {

template <typename Key, typename Val>
struct KeyValPair {
  Key key;
  Val val;
};

typedef KeyValPair<atomic_uintptr_t, atomic_uint64_t> Pair;

class ReadAccessMap {
 public:

  bool Insert(uptr addr, Sid sid);
  void Remove(uptr addr);
  bool Contain(uptr addr, Sid sid);
  u64 Get(uptr addr);

  ReadAccessMap();
  ReadAccessMap(const ReadAccessMap&) = delete;
  ReadAccessMap(ReadAccessMap&&) = delete;
  ReadAccessMap& operator=(const ReadAccessMap&) = delete;
  ReadAccessMap& operator=(ReadAccessMap&&) = delete;

  static const u64 EMPTY_STATE = ((1ULL << 63) - 1ULL);
  static const u64 DELETE_STATE = (1ULL << 63) - 1ULL | (1ULL << 63);

 private:
  Pair* ReadAccessMap::FindMatchedPair(int index, uptr addr, u8 swapIndex);
//   Pair* ReadAccessMap::GetEmptyPair(int index, uptr addr);
  bool UpdateCell(Pair* pair, u64 cell, Sid sid);

  Pair readAccessMap_[kReadAccessMapSize][2][kShadowCnt];
  atomic_uint8_t gcTracker_[kReadAccessMapSize];
};

}  // namespace __tsan

#endif