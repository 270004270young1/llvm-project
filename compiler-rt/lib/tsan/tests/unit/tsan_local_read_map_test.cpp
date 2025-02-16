#include "tsan_local_read_map.h"
#include "gtest/gtest.h"

namespace __tsan{

TEST(LocalReadMap,BasicGetInsert){
    LocalReadMap localReadMap;
    Sid sid = static_cast<Sid>(11);
    Epoch epoch = static_cast<Epoch>(22);
    uptr addr = static_cast<uptr>(20);
    u32 size = static_cast<u32>(4);
    localReadMap.Init(sid);
    
    FastState fs;
    fs.SetSid(sid);
    fs.SetEpoch(epoch);
    Shadow shadow(fs,addr,size,kAccessRead);
    localReadMap.Insert(addr,shadow.raw());
    u32 expected = static_cast<u32>(shadow.raw());
    EXPECT_EQ(static_cast<u32>(localReadMap.Get(addr)),0U);
}

}