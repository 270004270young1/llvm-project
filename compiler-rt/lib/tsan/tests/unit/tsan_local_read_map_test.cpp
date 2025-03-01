#include "tsan_local_read_map.h"
#include "gtest/gtest.h"

namespace __tsan{

struct TestStruct{
    RawShadow rawShadow;
    uptr addr;
};

template<unsigned MapSize,unsigned ShadowCnt>
class TestLocalReadMap : public LocalReadMap<MapSize,ShadowCnt>{
    public:
        void GetInternalState(unsigned i, unsigned swapIndex, unsigned j, TestStruct* testStruct){
            testStruct->rawShadow = LoadShadow(&LocalReadMap<MapSize,ShadowCnt>::localReadMap_[i][swapIndex][j]);

            testStruct->addr = atomic_load_relaxed(&LocalReadMap<MapSize,ShadowCnt>::addressMap_[i][swapIndex][j]);
        }

};

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
    TestLocalReadMap<1U,2U> localReadMap;
    Sid sid = static_cast<Sid>(11);
    Epoch epoch = static_cast<Epoch>(22);
    uptr addr1 = static_cast<uptr>(200);
    uptr addr2 = static_cast<uptr>(310);
    uptr addr3 = static_cast<uptr>(320);
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

    TestStruct testStruct;
    localReadMap.GetInternalState(0,0,0,&testStruct);
    EXPECT_EQ(testStruct.addr,addr1);
    EXPECT_EQ(testStruct.rawShadow,shadow1.raw());

    localReadMap.GetInternalState(0,0,1,&testStruct);
    EXPECT_EQ(testStruct.addr,addr2);
    EXPECT_EQ(testStruct.rawShadow,shadow2.raw());

    localReadMap.GetInternalState(0,1,0,&testStruct);
    EXPECT_EQ(testStruct.addr,0UL);
    EXPECT_EQ(testStruct.rawShadow,Shadow::kEmpty);

    localReadMap.GetInternalState(0,1,1,&testStruct);
    EXPECT_EQ(testStruct.addr,0UL);
    EXPECT_EQ(testStruct.rawShadow,Shadow::kEmpty);

    EXPECT_EQ(localReadMap.Insert(addr3,shadow3.raw()),true);

    localReadMap.GetInternalState(0,0,0,&testStruct);
    EXPECT_EQ(testStruct.addr,0UL);
    EXPECT_EQ(testStruct.rawShadow,Shadow::FreedMarker());

    localReadMap.GetInternalState(0,0,1,&testStruct);
    EXPECT_EQ(testStruct.addr,0UL);
    EXPECT_EQ(testStruct.rawShadow,Shadow::FreedMarker());

    localReadMap.GetInternalState(0,1,0,&testStruct);
    EXPECT_EQ(testStruct.addr,addr3);
    EXPECT_EQ(testStruct.rawShadow,shadow3.raw());

    localReadMap.GetInternalState(0,1,1,&testStruct);
    EXPECT_EQ(testStruct.addr,0UL);
    EXPECT_EQ(testStruct.rawShadow,Shadow::kEmpty);

    EXPECT_EQ(localReadMap.Get(addr1),Shadow::kEmpty);
    EXPECT_EQ(localReadMap.Get(addr2),Shadow::kEmpty);
    EXPECT_EQ(localReadMap.Get(addr3),shadow3.raw());


}

}