#ifndef TSAN_READ_ACCESS_MAP_H
#define TSAN_READ_ACCESS_MAP_H

#include "tsan_defs.h"
#include "sanitizer_common/sanitizer_hash.h"

namespace __tsan {



template <typename Key, typename Val>
struct KeyValPair {
  Key key;
  Val val;
};

typedef KeyValPair<atomic_uintptr_t, atomic_uint64_t> Pair;

template <unsigned MapSize = 0U, unsigned ShadowCnt = 0U>
class ReadAccessMap {
 public:

  ReadAccessMap(const ReadAccessMap&) = delete;
  ReadAccessMap(ReadAccessMap&&) = delete;
  ReadAccessMap& operator=(const ReadAccessMap&) = delete;
  ReadAccessMap& operator=(ReadAccessMap&&) = delete;
  
  ReadAccessMap(){
    for (unsigned i = 0; i < MapSize; i++) {
      for (unsigned j = 0; j < ShadowCnt; j++) {
        atomic_store_relaxed(&readAccessMap_[i][0][j].key, 0UL);
        atomic_store_relaxed(&readAccessMap_[i][1][j].key, 0UL);

        atomic_store_relaxed(&readAccessMap_[i][0][j].val, EMPTY_STATE);
        atomic_store_relaxed(&readAccessMap_[i][1][j].val, EMPTY_STATE);
      }
      atomic_store_release(&gcTracker_[i], 0U);
    }
  }

  bool Insert(uptr addr, Sid sid){
    const unsigned index = CalcHash(addr);
    u8 gcTracker = 0U;
    //Force this thread to acquire the latest value of gcTracker_[index]
    atomic_compare_exchange_strong(&gcTracker_[index],&gcTracker,0U,memory_order_acquire);
    const u8 swapIndex = gcTracker & 2U ? 1 : 0;

    Pair* pairs = readAccessMap_[index][swapIndex];
    for (int i = 0; i < ShadowCnt; i++) {
      const u64 cell = atomic_load_acquire(&pairs[i].val);
      const uptr key = atomic_load_relaxed(&pairs[i].key);

      if(key == addr && cell == DELETE_STATE)
        return false;

      if(key == addr){
        return UpdateCell(&pairs[i], cell, sid);
      }else if(key == 0UL){
        uptr expected = 0UL;
        if (atomic_compare_exchange_strong(&pairs[i].key, &expected, addr,
                                          memory_order_acq_rel)) {
          return UpdateCell(&pairs[i], cell, sid);
        }

        if (expected == addr) {
          return UpdateCell(&pairs[i], cell, sid);
        }
      }

      // uptr zero = 0UL;
      // return atomic_compare_exchange_strong(&pairs[i].key, &zero, addr,
      //                                    memory_order_acq_rel);
    }

    return false;
  }

  void Remove(uptr addr){
    const unsigned index = CalcHash(addr);
    u8 gcTracker = 0U;
    //Force this thread to acquire the latest value of gcTracker_[index]
    atomic_compare_exchange_strong(&gcTracker_[index],&gcTracker,0U,memory_order_acquire);
    // gcTracker = atomic_load_acquire(&gcTracker_[index]);
    const u8 swapIndex = gcTracker & 2U ? 1 : 0;
    const bool gcInProgress = gcTracker & 1U;
    Pair* curPair = readAccessMap_[index][swapIndex];
    Pair* swapPair = readAccessMap_[index][!swapIndex];
    // for (unsigned i = 0; i < ShadowCnt; i++) {
    //   if (static_cast<uptr>(atomic_load_acquire(&curPair[i].key)) == addr && atomic_load_acquire(&curPair[i].val) != DELETE_STATE){
    //     atomic_store_release(&curPair[i].val, DELETE_STATE);
    //     break;
    //   }
    // }
    uptr curKeys[ShadowCnt] = {0UL};
    u64 curVals[ShadowCnt] = {0ULL};
    bool deleted = false;
    for (unsigned i = 0; i < ShadowCnt; i++) {
      curVals[i] = atomic_load_acquire(&curPair[i].val);
      curKeys[i] = atomic_load_relaxed(&curPair[i].key);
      if (!deleted && curKeys[i] == addr){
        curVals[i] = DELETE_STATE;
        atomic_store_release(&curPair[i].val, DELETE_STATE);
        deleted = true;
      }
    }

    // for (unsigned i = 0; i < ShadowCnt; i++) {
    //   if (static_cast<uptr>(atomic_load_acquire(&curPair[i].key)) == 0UL)
    //     return;
    // }
    bool hasDeletedState = deleted;
    for (unsigned i = 0; i < ShadowCnt; i++) {
      if (curKeys[i] == 0UL)
        return;
      hasDeletedState |= curVals[i] == DELETE_STATE;
    }

    if(!hasDeletedState)
      return;

    if (gcInProgress ||
        !atomic_compare_exchange_strong(&gcTracker_[index], &gcTracker,
                                        gcTracker | 1U, memory_order_acq_rel))
      return;

    //Start GC
    for (unsigned i = 0; i < ShadowCnt; i++) {
      u64 curVal = curVals[i];
      uptr curKey = curKeys[i];
      if (curVal == DELETE_STATE) {
        atomic_store_relaxed(&swapPair[i].key, 0UL);
        atomic_store_release(&swapPair[i].val, EMPTY_STATE);
        continue;
      }
      // u64 oldVal = atomic_load_acquire(&swapPair[i].val);

      // if (!atomic_compare_exchange_strong(&swapPair[i].val, &oldVal, curVal,
      //                                     memory_order_relaxed)) {
      //   atomic_store_release(&gcTracker_[index], gcTracker);
      //   return atomic_load_acquire(&gcTracker_[index]);
      // }
      atomic_store_relaxed(&swapPair[i].key,curKey);
      atomic_store_release(&swapPair[i].val,curVal);

      // memory_order_release is required to ensure the previous CAS won't get
      // reordered after this CAS.
      if (!atomic_compare_exchange_strong(&curPair[i].val, &curVal,
                                          DELETE_STATE, memory_order_acq_rel)) {
        atomic_store_release(&gcTracker_[index], gcTracker);
        return;
      }
    }

    atomic_store_release(&gcTracker_[index], !swapIndex << 1);
  }

  bool Contain(uptr addr, Sid sid){
    const unsigned index = CalcHash(addr);
    const u8 swapIndex = atomic_load_acquire(&gcTracker_[index]) & 2U ? 1 : 0;

    if (Pair* pair = FindMatchedPair(index, addr, swapIndex)) {
      const u64 cell = atomic_load_acquire(&pair->val);
      if (cell == DELETE_STATE || cell == EMPTY_STATE)
        return false;

      for (u64 curSlot = (1ULL << 8) - 1ULL, sidSlot = static_cast<u64>(sid);
          curSlot > 0ULL;
          curSlot <<= sizeof(Sid) * 8, sidSlot <<= sizeof(Sid) * 8) {
        if ((cell & curSlot) == sidSlot)
          return true;
      }
    }
    return false;
  }

  unsigned Get(uptr addr, Sid* sids){
    const unsigned index = CalcHash(addr);
    const u8 swapIndex = atomic_load_acquire(&gcTracker_[index]) & 2U ? 1 : 0;

    for (unsigned i = 0; i < ShadowCnt; i++) {
      uptr key = atomic_load_acquire(&readAccessMap_[index][swapIndex][i].key);
      if (key != addr) {
        continue;
      }

      u64 cell = atomic_load_acquire(&readAccessMap_[index][swapIndex]
      [i].val);
      if (cell != DELETE_STATE) {
        return ExtractCell(cell,sids);
      }

      if (atomic_load_acquire(&readAccessMap_[index][!swapIndex][i].key) ==
          addr) {
        cell = atomic_load_acquire(&readAccessMap_[index][!swapIndex][i].val);
        return cell == DELETE_STATE ? 0U : ExtractCell(cell,sids);
      }
    }
    return 0U;
  }


  static const u64 EMPTY_STATE = ((1ULL << 63) - 1ULL);
  static const u64 DELETE_STATE = (1ULL << 63) - 1ULL | (1ULL << 63);

 protected:

  Pair* FindMatchedPair(int index, uptr addr, u8 swapIndex){
    for (int i = 0; i < kShadowCnt; i++) {
      if (static_cast<uptr>(atomic_load_acquire(
              &readAccessMap_[index][swapIndex][i].key)) == addr &&
          atomic_load_acquire(&readAccessMap_[index][swapIndex][i].val) !=
              DELETE_STATE) {
        return &readAccessMap_[index][swapIndex][i];
      }
    }

    return nullptr;
  }
  
//   Pair* ReadAccessMap::GetEmptyPair(int index, uptr addr);
  bool UpdateCell(Pair* pair, u64 cell, Sid sid){
    for (u64 curSlot = (1ULL << 8) - 1ULL, sidSlot = static_cast<u64>(sid);
        curSlot > 0ULL;
        curSlot <<= sizeof(Sid) * 8, sidSlot <<= sizeof(Sid) * 8) {
      if ((cell & curSlot) == sidSlot)
        return true;
      if ((cell & curSlot) == curSlot) {
        curSlot = cell & ~curSlot;
        if (cell == EMPTY_STATE) {
          curSlot |= 1ULL << 63;
        }
        curSlot |= sidSlot;
        return atomic_compare_exchange_strong(&pair->val, &cell, curSlot,
                                              memory_order_release);
        // return atomic_compare_exchange_strong(&pair->val, &cell, curSlot,
        //                                       memory_order_release);
      }
    }
    return false;
  }

  ALWAYS_INLINE
  unsigned ExtractCell(u64 cell, Sid* sids){
    if(cell == EMPTY_STATE)
      return 0;

    unsigned idx = 0;
    const u64 EMPTY = (1<<8)-1U;
    for (;cell>0;cell>>=8) {
      
      if((cell & EMPTY) == EMPTY)
        return idx;
      
      sids[idx++] = static_cast<Sid>(cell & EMPTY);

    }
    return idx;
  }

  unsigned CalcHash(uptr addr){
    MurMur2Hash64Builder hasher;
    hasher.add(static_cast<u64>(addr));
    return static_cast<uptr>(hasher.get()) % MapSize;
  }

  Pair readAccessMap_[MapSize][2][ShadowCnt];
  atomic_uint8_t gcTracker_[MapSize];
};

extern ReadAccessMap<kReadAccessMapSize, kShadowCnt>* read_access_map;


}  // namespace __tsan

#endif