/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_COLLECTOR_PARTIAL_MARK_SWEEP_H_
#define ART_RUNTIME_GC_COLLECTOR_PARTIAL_MARK_SWEEP_H_

#include "mark_sweep.h"

#include "base/macros.h"

namespace art HIDDEN {
namespace gc {
namespace collector {

class PartialMarkSweep : public MarkSweep {
 public:
  // Virtual as overridden by StickyMarkSweep.
  GcType GetGcType() const override {
    return kGcTypePartial;
  }

  PartialMarkSweep(Heap* heap, bool is_concurrent, const std::string& name_prefix = "");
  ~PartialMarkSweep() {}

 protected:
  // Bind the live bits to the mark bits of bitmaps for spaces that aren't collected for partial
  // collections, ie the Zygote space. Also mark this space is immune. Virtual as overridden by
  // StickyMarkSweep.
  void BindBitmaps() override REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PartialMarkSweep);
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_PARTIAL_MARK_SWEEP_H_
