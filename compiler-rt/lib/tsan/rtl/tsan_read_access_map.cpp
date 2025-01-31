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
  const int index = CalcHash<kReadAccessMapSize>(addr);

  if (Pair* pair = FindMatchedPair(index, addr)) {
    const u64 cell = static_cast<u64>(atomic_load_acquire(&pair->val[static_cast<u8>(sid) >> 6]));
    const u64 bits = 1ULL << (static_cast<u8>(sid) & 63ULL);
    if (cell & bits) {
      return true;
    }
    UpdateCell(pair, sid);
    return true;
  }

  for (int i = 0; i < kShadowCnt; i++) {
    const uptr key = static_cast<uptr>(atomic_load_acquire(&readAccessMap_[index][i].key));
    if (key != 0UL || key != addr)
      continue;

    const u64 bits = 1ULL << (static_cast<u8>(sid) & 63ULL);
    if (key == addr) {
      const u64 cell = static_cast<u64>(atomic_load_acquire(
          &readAccessMap_[index][i].val[static_cast<u8>(sid) >> 6]));
      if (cell & bits) {
        return true;
      }
      UpdateCell(&readAccessMap_[index][i], sid);
      return true;
    }

    uptr zero = 0UL;
    if (atomic_compare_exchange_strong(&readAccessMap_[index][i].key, &zero, addr,
                                       memory_order_acq_rel)) {
      UpdateCell(&readAccessMap_[index][i], sid);
      return true;
    }

    if (static_cast<uptr>(atomic_load_acquire(&readAccessMap_[index][i].key)) == addr) {
      UpdateCell(&readAccessMap_[index][i], sid);
      return true;
    }
  }

  return false;
}

Pair* ReadAccessMap::FindMatchedPair(int index, uptr addr) {
  for (int i = 0; i < kShadowCnt; i++) {
    if (static_cast<uptr>(atomic_load_acquire(&readAccessMap_[index][i].key)) == addr) {
      return &readAccessMap_[index][i];
    }
  }

  return nullptr;
}

Pair* ReadAccessMap::GetEmptyPair(int index, uptr addr) {
  for (int i = 0; i < kShadowCnt; i++) {
    const uptr key = static_cast<uptr>(atomic_load_acquire(&readAccessMap_[index][i].key));
    if (key != 0UL || key != addr)
      continue;

    if (key == addr)
      return &readAccessMap_[index][i];

    uptr zero = 0UL;
    if (atomic_compare_exchange_strong(&readAccessMap_[index][i].key, &zero, addr,
                                       memory_order_relaxed)) {
      return &readAccessMap_[index][i];
    }

    if (static_cast<uptr>(atomic_load_relaxed(&readAccessMap_[index][i].key)) == addr) {
      return &readAccessMap_[index][i];
    }
  }
  return nullptr;
}

void ReadAccessMap::Remove(uptr addr) {
  const int index = CalcHash<kReadAccessMapSize>(addr);
  for (int i = 0; i < kShadowCnt; i++) {
    if (static_cast<uptr>(atomic_load_acquire(&readAccessMap_[index][i].key)) != addr)
      continue;

    atomic_store_release(&readAccessMap_[index][i].val[kReadAccessMapThreadCellSize-1],1ULL<<63);

    atomic_thread_fence(memory_order_release);

    for (int j = 0; j < kReadAccessMapThreadCellSize-1; j++) {
      atomic_store_relaxed(&readAccessMap_[index][i].val[j], 0ULL);
    }


    atomic_store_relaxed(&readAccessMap_[index][i].key, 0UL);


    atomic_store_release(&readAccessMap_[index][i].val[kReadAccessMapThreadCellSize-1],0ULL);
  }
}

bool ReadAccessMap::Contain(uptr addr, Sid sid){

  const int index = CalcHash<kReadAccessMapSize>(addr);
  const u64 bits = 1ULL << (static_cast<u8>(sid) & 63ULL);
  for(unsigned i=0;i<kShadowCnt;i++){
    if(static_cast<uptr>(atomic_load_acquire(&readAccessMap_[index][i].key)) == addr){
      
      u64 cell = static_cast<u64>(atomic_load_acquire(&readAccessMap_[index][i].val[static_cast<u8>(sid) >> 6]));

      return cell & bits;
    }
  }
  return false;

}

bool ReadAccessMap::Get(uptr addr, ThreadState* thr, Sid* sids){

  const int index = CalcHash<kReadAccessMapSize>(addr);
  Pair* pair = FindMatchedPair(index,addr);
  if(pair == nullptr)
    return false;
  const uptr tracePos = atomic_load_relaxed(&thr->trace_pos);
  
  const unsigned startCellBit = static_cast<unsigned>(tracePos / sizeof(Event)) % (sizeof(u64)*8);
  const unsigned startCellIndex = static_cast<unsigned>(tracePos / sizeof(Event) % kShadowCnt);
  u64 curBit = 1ULL<<startCellBit;
  u64 cell = atomic_load_acquire(&pair->val[startCellIndex]);
  unsigned curCellIndex = startCellIndex;
  unsigned sidIndex = 0U;
  do{

    if(cell & curBit){      
      sids[sidIndex++] = static_cast<Sid>(64U*curCellIndex+__builtin_ffsll(curBit)-1U);
    }
    if(curBit==1ULL){
      curCellIndex = (curCellIndex+1)%kShadowCnt;
      cell = atomic_load_acquire(&pair->val[curCellIndex]);
      curBit = 1ULL<<63;
    }else{
      curBit>>=1;
    }
  }while(sidIndex<kShadowCnt && !(curBit==(1ULL<<startCellBit) && curCellIndex==startCellIndex));
  return true;

}

void UpdateCell(Pair* pair, Sid sid) {
  u64 cell = static_cast<u64>(atomic_load_acquire(&pair->val[static_cast<u8>(sid) >> 6]));
  const u64 bits = 1ULL << (static_cast<u8>(sid) & 63ULL);
  while (!atomic_compare_exchange_weak(&pair->val[static_cast<u8>(sid) >> 6],
                                         &cell, cell | bits,
                                         memory_order_release));
}

}  // namespace __tsan
