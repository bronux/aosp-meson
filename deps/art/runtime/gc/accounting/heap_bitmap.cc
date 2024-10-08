/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "heap_bitmap.h"

#include "gc/accounting/space_bitmap-inl.h"
#include "gc/space/space.h"

namespace art HIDDEN {
namespace gc {
namespace accounting {

void HeapBitmap::AddContinuousSpaceBitmap(accounting::ContinuousSpaceBitmap* bitmap) {
  DCHECK(bitmap != nullptr);
  // Check that there is no bitmap overlap.
  for (const auto& cur_bitmap : continuous_space_bitmaps_) {
    CHECK(bitmap->HeapBegin() >= cur_bitmap->HeapLimit() ||
          bitmap->HeapLimit() <= cur_bitmap->HeapBegin())
              << "Bitmap " << bitmap->Dump() << " overlaps with existing bitmap "
              << cur_bitmap->Dump();
  }
  continuous_space_bitmaps_.push_back(bitmap);
}

void HeapBitmap::RemoveContinuousSpaceBitmap(accounting::ContinuousSpaceBitmap* bitmap) {
  DCHECK(bitmap != nullptr);
  auto it = std::find(continuous_space_bitmaps_.begin(), continuous_space_bitmaps_.end(), bitmap);
  DCHECK(it != continuous_space_bitmaps_.end());
  continuous_space_bitmaps_.erase(it);
}

void HeapBitmap::AddLargeObjectBitmap(LargeObjectBitmap* bitmap) {
  DCHECK(bitmap != nullptr);
  large_object_bitmaps_.push_back(bitmap);
}

void HeapBitmap::RemoveLargeObjectBitmap(LargeObjectBitmap* bitmap) {
  DCHECK(bitmap != nullptr);
  auto it = std::find(large_object_bitmaps_.begin(), large_object_bitmaps_.end(), bitmap);
  DCHECK(it != large_object_bitmaps_.end());
  large_object_bitmaps_.erase(it);
}

}  // namespace accounting
}  // namespace gc
}  // namespace art
