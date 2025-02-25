#include "tsan_read_access_map.h"
#include "gtest/gtest.h"

namespace __tsan {

const unsigned size = sizeof(u64) / sizeof(Sid);
const u64 DELETE_STATE = ReadAccessMap<1U,2U>::DELETE_STATE;
const u64 EMPTY_STATE = ReadAccessMap<1U,2U>::EMPTY_STATE;

struct TestPair{
  uptr key;
  u64 val;
};

template<unsigned MapSize, unsigned ShadowCnt>
class TestReadAccessMap: public ReadAccessMap<MapSize,ShadowCnt>{

public:
  void GetInternalState(unsigned i, unsigned swap, unsigned j, TestPair* testPair){
    testPair->key = atomic_load_acquire(&ReadAccessMap<MapSize,ShadowCnt>::readAccessMap_[i][swap][j].key);
    testPair->val = atomic_load_acquire(&ReadAccessMap<MapSize,ShadowCnt>::readAccessMap_[i][swap][j].val);
    
  }
};

unsigned GetSids(u64 sidCell, Sid* sids) {
  unsigned bitShifted = 0U;
  unsigned idx = 0U;
  const unsigned EMPTY = (1U<<8) - 1U;
  for (u64 curSlot = (1ULL << 8) - 1ULL; curSlot > 0;
       curSlot <<= sizeof(Sid) * 8, bitShifted += sizeof(Sid) * 8) {
    unsigned slotSid = static_cast<unsigned>((sidCell & curSlot) >> bitShifted);

    if(slotSid == EMPTY){
        return idx;
    }
    sids[idx++] = static_cast<Sid>(slotSid);
  }
  return idx;
}

TEST(ReadAccessMap, BasicGetInsert) {
  ReadAccessMap<1U, 2U> readAccessMap;
  uptr addr = 31UL;
  Sid sid = static_cast<Sid>(13);

  EXPECT_EQ(readAccessMap.Get(addr),EMPTY_STATE);
  EXPECT_EQ(readAccessMap.Insert(addr, sid), true);

  Sid sids[size];
  u64 expected = readAccessMap.Get(addr);
  unsigned idx = GetSids(expected,sids);
  
  EXPECT_EQ(idx,1U);
  EXPECT_EQ(sids[0],sid);
}

TEST(ReadAccessMap, TestInsertOrder) {
  ReadAccessMap<1U, 2U> readAccessMap;
  uptr addr = 31UL;
  Sid sids[size];
  for (int i = 0; i < size; i++) {
    sids[i] = static_cast<Sid>(i);
    EXPECT_EQ(readAccessMap.Insert(addr, sids[i]), true);
  }

  Sid expectedSids[size];
  u64 expected = readAccessMap.Get(addr);
  unsigned idx = GetSids(expected,expectedSids);

  EXPECT_EQ(idx,size);
  for(int i=0;i<size;i++){
    EXPECT_EQ(sids[i],expectedSids[i]);
  }
}

TEST(ReadAccessMap, TestContain){
  ReadAccessMap<1U, 2U> readAccessMap;
  uptr addr = 31UL;
  Sid sids[size];
  for (int i = 0; i < size; i++) {
    sids[i] = static_cast<Sid>(i);
    if(i<size/2){
      EXPECT_EQ(readAccessMap.Insert(addr, sids[i]), true);
    }
  }

  for(int i=0;i<size;i++){
    if(i<size/2){
      EXPECT_EQ(readAccessMap.Contain(addr,sids[i]),true);
    }else{
      EXPECT_EQ(readAccessMap.Contain(addr,sids[i]),false);
    }
  }
}

TEST(ReadAccessMap, TestRemove){
  ReadAccessMap<1U, 3U> readAccessMap;
  uptr addr1 = 31UL;
  uptr addr2 = 32UL;
  Sid sid1 = static_cast<Sid>(1);
  Sid sid2 = static_cast<Sid>(2);
  Sid sids[size];
  unsigned idx;

  EXPECT_EQ(readAccessMap.Insert(addr1,sid1),true);
  EXPECT_EQ(readAccessMap.Insert(addr2,sid2),true);

  readAccessMap.Remove(addr1);

  EXPECT_EQ(readAccessMap.Get(addr1),EMPTY_STATE);
  idx = GetSids(readAccessMap.Get(addr2),sids);
  EXPECT_EQ(idx,1U);
  EXPECT_EQ(sids[0],sid2);

}

TEST(ReadAccessMap, TestGC){
  TestReadAccessMap<1U, 2U> readAccessMap;
  uptr addr1 = 31UL;
  uptr addr2 = 32UL;
  Sid sid1 = static_cast<Sid>(1);
  Sid sid2 = static_cast<Sid>(2);
  Sid sids[size];
  unsigned idx;

  EXPECT_EQ(readAccessMap.Insert(addr1,sid1),true);
  EXPECT_EQ(readAccessMap.Insert(addr2,sid2),true);

  idx = GetSids(readAccessMap.Get(addr1),sids);
  EXPECT_EQ(idx,1U);
  EXPECT_EQ(sids[0],sid1);

  idx = GetSids(readAccessMap.Get(addr2),sids);
  EXPECT_EQ(idx,1U);
  EXPECT_EQ(sids[0],sid2);

  readAccessMap.Remove(addr1);

  TestPair pair; 
  readAccessMap.GetInternalState(0,0,0,&pair);
  EXPECT_EQ(pair.key,addr1);
  EXPECT_EQ(pair.val,DELETE_STATE);

  readAccessMap.GetInternalState(0,0,1,&pair);
  EXPECT_EQ(pair.key,addr2);
  EXPECT_EQ(pair.val,DELETE_STATE);
  
  readAccessMap.GetInternalState(0,1,0,&pair);
  EXPECT_EQ(pair.key,0UL);
  EXPECT_EQ(pair.val,EMPTY_STATE);
  
  readAccessMap.GetInternalState(0,1,1,&pair);
  EXPECT_EQ(pair.key,addr2);
  EXPECT_NE(pair.val,EMPTY_STATE);

  EXPECT_EQ(readAccessMap.Get(addr1),EMPTY_STATE);
  EXPECT_NE(readAccessMap.Get(addr2),EMPTY_STATE);

  idx = GetSids(readAccessMap.Get(addr2),sids);
  EXPECT_EQ(idx,1U);
  EXPECT_EQ(sids[0],sid2);


}

TEST(ReadAccessMap, TestExceedShadowCnt){
  TestReadAccessMap<1U, 1U> readAccessMap;
  uptr addr1 = 31UL;
  uptr addr2 = 32UL;
  Sid sid1 = static_cast<Sid>(1);

  EXPECT_EQ(readAccessMap.Insert(addr1,sid1),true);
  EXPECT_EQ(readAccessMap.Insert(addr2,sid1),false);

}

TEST(ReadAccessMap, TestExceedSidSlot){
  TestReadAccessMap<1U, 1U> readAccessMap;
  uptr addr1 = 31UL;
  for(int i=0;i<size;i++){
    EXPECT_EQ(readAccessMap.Insert(addr1,static_cast<Sid>(i)),true);
  }

  for(int i=0;i<size;i++){
    EXPECT_EQ(readAccessMap.Insert(addr1,static_cast<Sid>(i)),true);
  }
  EXPECT_EQ(readAccessMap.Insert(addr1,static_cast<Sid>(size+1)),false);

}

}  // namespace __tsan