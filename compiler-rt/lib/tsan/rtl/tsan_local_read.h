
#ifndef TSAN_LOCAL_READ_H
#define TSAN_LOCAL_READ_H

#include <unordered_map>

#include "tsan_defs.h"
#include "tsan_shadow.h"

namespace __tsan {

class LocalRead {
 public:
  LocalRead(){
    for (int i = 0; i < kThreadSlotCount; i++) {
      for (int j = 0; j < kShadowCnt; j++) {
        localReadMap_[i][j] = Shadow::kEmpty;
      }
    }
  };

//   void Add(RawShadow shadow);

  LocalRead(const LocalRead& other) = delete;
  LocalRead(LocalRead&& other) = delete;
  LocalRead& operator=(const LocalRead& other) = delete;
  LocalRead& operator=(LocalRead&& other) = delete;

 private:
  //     int count;
  VECTOR_ALIGNED RawShadow localReadMap_[kThreadSlotCount][kShadowCnt];
  //   VECTOR_ALIGNED int localReadMap_[kThreadSlotCount];
};

}  // namespace __tsan
#endif