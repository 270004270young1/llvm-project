// #include "tsan_local_read_map.h"
// #include "sanitizer_common/sanitizer_atomic.h"
// #include "tsan_rtl.h"

// namespace __tsan{

// template<unsigned MapSize, unsigned ShadowCnt>
// void LocalReadMap<MapSize,ShadowCnt>::Init(Sid sid){
//     sid_ = sid;
//     for (int i = 0; i < MapSize; i++) {
//       for (int j = 0; j < ShadowCnt; j++) {
//         StoreShadow(&localReadMap_[i][0][j],Shadow::kEmpty);
//         StoreShadow(&localReadMap_[i][1][j],Shadow::kEmpty);
//         atomic_store_relaxed(&addressMap_[i][0][j],0UL);
//         atomic_store_relaxed(&addressMap_[i][1][j],0UL);

//       }
//       atomic_store_release(&swapIndex_[i],false);
//     }
// }

// template<unsigned MapSize, unsigned ShadowCnt>
// bool LocalReadMap<MapSize,ShadowCnt>::Insert(uptr addr, RawShadow rawShadow){
//     const unsigned index = CalcHash<MapSize>(addr);
//     unsigned swapIndex = atomic_load_relaxed(&swapIndex_[index]);
//     uptr keys[ShadowCnt] = {0UL};
//     RawShadow shadows[ShadowCnt] = {Shadow::kEmpty};
//     for(unsigned i=0;i<ShadowCnt;i++){
//       keys[i] = atomic_load_relaxed(&addressMap_[index][swapIndex][i]);
//       shadows[i] = LoadShadow(&localReadMap_[index][swapIndex][i]);
//       if(keys[i]!=addr && keys[i]!=0UL){
//         continue;        
//       }

//       StoreShadow(&localReadMap_[index][swapIndex][i],rawShadow);
//       if(keys[i] == 0UL){
//         atomic_store_release(&addressMap_[index][swapIndex][i],addr);
//       }
//       return true;
//     }

//     bool isOutdated[ShadowCnt];
//     bool shouldGC = false;
//     for(unsigned i=0;i<ShadowCnt;i++){
//       isOutdated[i] = ctx->read_access_map.Contain(keys[i],sid_);
//       shouldGC |= isOutdated[i];
//     }

//     if(!shouldGC)
//       return false;

//     bool stored = false;
//     for(unsigned i=0;i<ShadowCnt;i++){

//       if(!isOutdated[i]){
//         StoreShadow(&localReadMap_[index][!swapIndex][i],shadows[i]);
//         atomic_store_release(&addressMap_[index][!swapIndex][i],keys[i]);
//       }else{
        
//         if(!stored){
//           StoreShadow(&localReadMap_[index][!swapIndex][i],rawShadow);
//           atomic_store_release(&addressMap_[index][!swapIndex][i],addr);
//           stored = true;
//         }else{
//           StoreShadow(&localReadMap_[index][!swapIndex][i],Shadow::kEmpty);
//           atomic_store_release(&addressMap_[index][!swapIndex][i],0UL);
//         }
        
//       }
      
//       StoreShadow(&localReadMap_[index][swapIndex][i],Shadow::FreedMarker());
//       atomic_store_release(&addressMap_[index][swapIndex][i],0UL);


//     }

//     atomic_store_release(&swapIndex_[index],!swapIndex);
//     return stored;
// }


// template<unsigned MapSize, unsigned ShadowCnt>
// RawShadow LocalReadMap<MapSize,ShadowCnt>::Get(uptr addr){
//     const unsigned index = CalcHash<MapSize>(addr);
//     bool expected = false;
//     //The purpose of this compare_exchange is to force the reader threads to read the latest value written by the thread of the map owner and build release-acquire relation with the latest update.
//     atomic_compare_exchange_strong(&swapIndex_[index],&expected,0,memory_order_acquire);
//     unsigned swapIndex = expected;

//     for(int i=0;i<ShadowCnt;i++){
//       if(atomic_load_relaxed(&addressMap_[index][swapIndex][i]) == addr){
//         RawShadow rawShadow = LoadShadow(&localReadMap_[index][swapIndex][i]);
//         if(rawShadow == Shadow::FreedMarker()){
//           rawShadow = atomic_load_relaxed(&addressMap_[index][!swapIndex][i]) == addr ? LoadShadow(&localReadMap_[index][!swapIndex][i]) : rawShadow;
//         }
//         return rawShadow == Shadow::FreedMarker() ? Shadow::kEmpty : rawShadow;
//       }
//     }
    
//     return Shadow::kEmpty;
// }
// template class LocalReadMap<1U,2U>;

// }