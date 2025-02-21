// #include "tsan_read_access_map.h"

// #include "tsan_rtl.h"

// namespace __tsan {

// ReadAccessMap::ReadAccessMap() {
//   for (unsigned i = 0; i < kReadAccessMapSize; i++) {
//     for (unsigned j = 0; j < kShadowCnt; j++) {
//       atomic_store_relaxed(&readAccessMap_[i][0][j].key, 0UL);
//       atomic_store_relaxed(&readAccessMap_[i][1][j].key, 0UL);

//       atomic_store_relaxed(&readAccessMap_[i][0][j].val, EMPTY_STATE);
//       atomic_store_relaxed(&readAccessMap_[i][1][j].val, EMPTY_STATE);
//     }
//     atomic_store_relaxed(&gcTracker_[i], 0U);
//   }
// }

// bool ReadAccessMap::Insert(uptr addr, Sid sid) {
//   const int index = CalcHash<kReadAccessMapSize>(addr);
//   const u8 swapIndex = atomic_load_acquire(&gcTracker_[index]) & 2U ? 1 : 0;

//   Pair* pairs = readAccessMap_[index][swapIndex];
//   for (int i = 0; i < kShadowCnt; i++) {
//     const uptr key = static_cast<uptr>(atomic_load_acquire(&pairs[i].key));
//     const u64 cell = static_cast<u64>(atomic_load_acquire(&pairs[i].val));
//     if ((key != 0UL && key != addr) || cell == DELETE_STATE)
//       continue;

//     if (key == addr) {
//       return UpdateCell(&pairs[i], cell, sid);
//     }

//     uptr zero = 0UL;
//     if (atomic_compare_exchange_strong(&pairs[i].key, &zero, addr,
//                                        memory_order_acq_rel)) {
//       return UpdateCell(&pairs[i], cell, sid);
//     }

//     if (static_cast<uptr>(atomic_load_acquire(&pairs[i].key)) == addr) {
//       return UpdateCell(&pairs[i], cell, sid);
//     }
//     // uptr zero = 0UL;
//     // return atomic_compare_exchange_strong(&pairs[i].key, &zero, addr,
//     //                                    memory_order_acq_rel);
//   }

//   return false;
// }

// Pair* ReadAccessMap::FindMatchedPair(int index, uptr addr, u8 swapIndex) {
//   for (int i = 0; i < kShadowCnt; i++) {
//     if (static_cast<uptr>(atomic_load_acquire(
//             &readAccessMap_[index][swapIndex][i].key)) == addr &&
//         atomic_load_acquire(&readAccessMap_[index][swapIndex][i].val) !=
//             DELETE_STATE) {
//       return &readAccessMap_[index][swapIndex][i];
//     }
//   }

//   return nullptr;
// }

// // Pair* ReadAccessMap::GetEmptyPair(int index, uptr addr) {
// //   for (int i = 0; i < kShadowCnt; i++) {
// //     const uptr key =
// //     static_cast<uptr>(atomic_load_acquire(&readAccessMap_[index][i].key)); if
// //     (key != 0UL || key != addr)
// //       continue;

// //     if (key == addr)
// //       return &readAccessMap_[index][i];

// //     uptr zero = 0UL;
// //     if (atomic_compare_exchange_strong(&readAccessMap_[index][i].key, &zero,
// //     addr,
// //                                        memory_order_relaxed)) {
// //       return &readAccessMap_[index][i];
// //     }

// //     if (static_cast<uptr>(atomic_load_relaxed(&readAccessMap_[index][i].key))
// //     == addr) {
// //       return &readAccessMap_[index][i];
// //     }
// //   }
// //   return nullptr;
// // }

// void ReadAccessMap::Remove(uptr addr) {
//   const int index = CalcHash<kReadAccessMapSize>(addr);
//   u8 gcTracker = atomic_load_acquire(&gcTracker_[index]);
//   const u8 swapIndex = gcTracker & 2U ? 1 : 0;
//   const bool gcInProgress = gcTracker & 1U;
//   Pair* curPair = readAccessMap_[index][swapIndex];
//   Pair* swapPair = readAccessMap_[index][!swapIndex];
//   for (unsigned i = 0; i < kShadowCnt; i++) {
//     if (static_cast<uptr>(atomic_load_acquire(&curPair[i].key)) != addr)
//       continue;

//     atomic_store_release(&curPair[i].val, DELETE_STATE);

//     for (unsigned j = 0; j < kShadowCnt; j++) {
//       if (static_cast<uptr>(atomic_load_acquire(&curPair[j].key)) == 0UL)
//         return;
//     }

//     if (gcInProgress ||
//         !atomic_compare_exchange_strong(&gcTracker_[index], &gcTracker,
//                                         gcTracker | 1U, memory_order_acquire))
//       return;

//     for (unsigned j = 0; j < kShadowCnt; j++) {
//       u64 curVal = atomic_load_acquire(&curPair[j].val);
//       if (curVal == DELETE_STATE) {
//         atomic_store_relaxed(&swapPair[j].key, 0UL);
//         atomic_store_release(&swapPair[j].val, EMPTY_STATE);
//         continue;
//       }
//       u64 oldVal = atomic_load_acquire(&swapPair[j].val);

//       if (!atomic_compare_exchange_strong(&swapPair[j].val, &oldVal, curVal,
//                                           memory_order_relaxed)) {
//         atomic_store_release(&gcTracker_[index], gcTracker);
//         return;
//       }

//       // memory_order_release is required to ensure the previous CAS won't get
//       // reordered after this CAS.
//       if (!atomic_compare_exchange_strong(&curPair[j].val, &curVal,
//                                           DELETE_STATE, memory_order_release)) {
//         atomic_store_release(&gcTracker_[index], gcTracker);
//         return;
//       }
//     }

//     atomic_store_release(&gcTracker_[index], swapIndex << 1);
//   }
// }

// bool ReadAccessMap::Contain(uptr addr, Sid sid) {
//   const int index = CalcHash<kReadAccessMapSize>(addr);
//   const u8 swapIndex = atomic_load_acquire(&gcTracker_[index]) & 2U ? 1 : 0;

//   if (Pair* pair = FindMatchedPair(index, addr, swapIndex)) {
//     const u64 cell = atomic_load_acquire(&pair->val);
//     if (cell == DELETE_STATE || cell == EMPTY_STATE)
//       return false;

//     for (u64 curSlot = (1ULL << 8) - 1ULL, sidSlot = static_cast<u64>(sid);
//          curSlot > 0ULL;
//          curSlot <<= sizeof(Sid) * 8, sidSlot <<= sizeof(Sid) * 8) {
//       if ((cell & curSlot) == sidSlot)
//         return true;
//     }
//   }
//   return true;
// }

// u64 ReadAccessMap::Get(uptr addr) {
//   const int index = CalcHash<kReadAccessMapSize>(addr);
//   const u8 swapIndex = atomic_load_acquire(&gcTracker_[index]) & 2U ? 1 : 0;

//   for (unsigned i = 0; i < kShadowCnt; i++) {
//     if (static_cast<uptr>(atomic_load_acquire(
//             &readAccessMap_[index][swapIndex][i].key)) != addr ||
//         atomic_load_acquire(&readAccessMap_[index][swapIndex][i].val) ==
//             DELETE_STATE) {
//       continue;
//     }

//     u64 cell = atomic_load_acquire(&readAccessMap_[index][swapIndex][i].val);
//     if (cell != DELETE_STATE) {
//       return cell;
//     }

//     if (atomic_load_acquire(&readAccessMap_[index][!swapIndex][i].key) ==
//         addr) {
//       cell = atomic_load_acquire(&readAccessMap_[index][!swapIndex][i].val);
//       return cell == DELETE_STATE ? EMPTY_STATE : cell;
//     }
//   }
//   return EMPTY_STATE;

//   //------------------------------------------------------------

//   // const uptr tracePos = atomic_load_relaxed(&thr->trace_pos);

//   // const unsigned startCellBit = static_cast<unsigned>(tracePos /
//   // sizeof(Event)) % (sizeof(u64)*8); const unsigned startCellIndex =
//   // static_cast<unsigned>(tracePos / sizeof(Event) % kShadowCnt); u64 curBit =
//   // 1ULL<<startCellBit; u64 cell =
//   // atomic_load_acquire(&pair->val[startCellIndex]); unsigned curCellIndex =
//   // startCellIndex; unsigned sidIndex = 0U; do{

//   //   if(cell & curBit){
//   //     sids[sidIndex++] =
//   //     static_cast<Sid>(64U*curCellIndex+__builtin_ffsll(curBit)-1U);
//   //   }
//   //   if(curBit==1ULL){
//   //     curCellIndex = (curCellIndex+1)%kShadowCnt;
//   //     cell = atomic_load_acquire(&pair->val[curCellIndex]);
//   //     curBit = 1ULL<<63;
//   //   }else{
//   //     curBit>>=1;
//   //   }
//   // }while(sidIndex<kShadowCnt && !(curBit==(1ULL<<startCellBit) &&
//   // curCellIndex==startCellIndex)); return true;
// }

// // void UpdateCell(Pair* pair, Sid sid) {
// //   u64 cell =
// //   static_cast<u64>(atomic_load_acquire(&pair->val[static_cast<u8>(sid) >>
// //   6])); const u64 bits = 1ULL << (static_cast<u8>(sid) & 63ULL); while
// //   (!atomic_compare_exchange_weak(&pair->val[static_cast<u8>(sid) >> 6],
// //                                          &cell, cell | bits,
// //                                          memory_order_release));
// // }

// bool ReadAccessMap::UpdateCell(Pair* pair, u64 cell, Sid sid) {
//   for (u64 curSlot = (1ULL << 8) - 1ULL, sidSlot = static_cast<u64>(sid);
//        curSlot > 0ULL;
//        curSlot <<= sizeof(Sid) * 8, sidSlot <<= sizeof(Sid) * 8) {
//     if ((cell & curSlot) == sidSlot)
//       return true;
//     if ((cell & curSlot) == curSlot) {
//       curSlot = cell & ~curSlot;
//       if (cell == EMPTY_STATE) {
//         curSlot |= 1ULL << 63;
//       }
//       curSlot |= sidSlot;
//       return atomic_compare_exchange_strong(&pair->val, &cell, curSlot,
//                                             memory_order_release);
//       // return atomic_compare_exchange_strong(&pair->val, &cell, curSlot,
//       //                                       memory_order_release);
//     }
//   }
//   return false;
// }

// }  // namespace __tsan
