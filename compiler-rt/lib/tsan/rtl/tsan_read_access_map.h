#ifndef TSAN_READ_ACCESS_MAP_H
#define TSAN_READ_ACCESS_MAP_H

#include "tsan_defs.h"
namespace __tsan {

template <typename Key, typename Val>
struct KeyValPair{
    Key key;
    Val val;
};

typedef KeyValPair<atomic_uintptr_t,atomic_uint64_t[kReadAccessMapThreadCellSize]> Pair;

class ReadAccessMap{

    public:
        ReadAccessMap();

        bool Add(uptr addr, Sid sid);
        void Remove(uptr addr);
        bool Contain(uptr addr, Sid sid);

        ReadAccessMap(const ReadAccessMap&) = delete;
        ReadAccessMap(ReadAccessMap&&) = delete;
        ReadAccessMap& operator=(const ReadAccessMap&) = delete;
        ReadAccessMap& operator=(ReadAccessMap&&) = delete;

    private:
        Pair* ReadAccessMap::FindMatchedPair(int index, uptr addr);
        Pair* ReadAccessMap::GetEmptyPair(int index, uptr addr);

        KeyValPair<atomic_uintptr_t,atomic_uint64_t[kReadAccessMapThreadCellSize]> readAccessMap_[kReadAccessMapSize][kShadowCnt];
};


} // namespace __tsan


#endif