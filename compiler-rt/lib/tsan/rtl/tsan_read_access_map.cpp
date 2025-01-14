#include "tsan_read_access_map.h"
#include "tsan_rtl.h"

namespace __tsan {
    

    bool ReadAccessMap::Add(uptr addr, Sid sid){

        int index = CalcHash<kReadAccessMapSize>(addr);

        Pair* pair = FindMatchedPair(index,addr);
        pair = pair == nullptr? FindEmptyPair(index) : pair;

        if(pair == nullptr)
            return false;

        u64 cell = pair->val[static_cast<u8>(sid) >> 6];
        u64 bits = 1ULL << (static_cast<u8>(sid) & 63ULL); 
        if(cell & bits){
            return true;
        }

        pair->key = addr;
        pair->val[static_cast<u8>(sid) >> 6]|= bits;
        return true;
    }

    Pair* ReadAccessMap::FindMatchedPair(int index, uptr addr){
        
        for(int i=0;i<kShadowCnt;i++){
            if(readAccessMap_[index][i].key == addr){
                return &readAccessMap_[index][i];
            }
        }
        
        return nullptr;
    }

    Pair* ReadAccessMap::FindEmptyPair(int index){
        for(int i=0;i<kShadowCnt;i++){
            bool found = true;
            for(int j=0;j<kReadAccessMapThreadCellSize;j++){
                if(readAccessMap_[index][i].val[j] != 0ULL){
                    found = false;
                    break;
                }
            }
            if(found){
                return &readAccessMap_[index][i];
            }
        }
        return nullptr;
    }


    void ReadAccessMap::Remove(uptr addr){

        int index = CalcHash<kReadAccessMapSize>(addr);
        for(int i=0;i<kShadowCnt;i++){
            if(readAccessMap_[index][i].key != addr)
                continue;

            readAccessMap_[index][i].key = 0UL;
            for(int j=0;j<kReadAccessMapThreadCellSize;j++){
                readAccessMap_[index][i].val[j] = 0ULL;
            }
        }
        

    }

} // namespace __tsan
