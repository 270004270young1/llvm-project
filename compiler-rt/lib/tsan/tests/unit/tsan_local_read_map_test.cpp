#include "tsan_local_read_map.h"
#include "gtest/gtest.h"

namespace __tsan{

TEST(LocalReadMap, BasicGetInsert){
    LocalReadMap<1U,2U> localReadMap;
    Sid sid = static_cast<Sid>(11);
    Epoch epoch = static_cast<Epoch>(22);
    uptr addr = static_cast<uptr>(20);
    u32 size = static_cast<u32>(4);
    localReadMap.Init(sid);
    
    FastState fs;
    fs.SetSid(sid);
    fs.SetEpoch(epoch);
    Shadow shadow(fs,addr,size,kAccessRead);
    EXPECT_EQ(localReadMap.Insert(addr,shadow.raw()),true);
    // u32 expected = static_cast<u32>(shadow.raw());
    EXPECT_EQ(localReadMap.Get(addr),shadow.raw());
}

TEST(LocalReadMap, Overwrite){
    LocalReadMap<1U,2U> localReadMap;
    Sid sid = static_cast<Sid>(11);
    Epoch epoch = static_cast<Epoch>(22);
    uptr addr1 = static_cast<uptr>(20);
    uptr addr2 = static_cast<uptr>(31);
    uptr addr3 = static_cast<uptr>(32);
    u32 size = static_cast<u32>(4);
    localReadMap.Init(sid);
    
    FastState fs;
    fs.SetSid(sid);
    fs.SetEpoch(epoch);
    Shadow shadow1(fs,addr1,size,kAccessRead);
    Shadow shadow2(fs,addr2,size,kAccessRead);
    Shadow shadow3(fs,addr3,size,kAccessRead);
    
    EXPECT_EQ(localReadMap.Insert(addr1,shadow1.raw()),true);
    EXPECT_EQ(localReadMap.Insert(addr2,shadow2.raw()),true);
    EXPECT_EQ(localReadMap.Insert(addr3,shadow3.raw()),true);

    EXPECT_EQ(localReadMap.Get(addr1),Shadow::kEmpty);
    EXPECT_EQ(localReadMap.Get(addr2),shadow2.raw());
    EXPECT_EQ(localReadMap.Get(addr3),shadow3.raw());


}

}