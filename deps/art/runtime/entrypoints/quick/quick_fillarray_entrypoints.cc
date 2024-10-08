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

#include "art_method-inl.h"
#include "callee_save_frame.h"
#include "entrypoints/entrypoint_utils.h"
#include "mirror/array.h"

namespace art HIDDEN {

/*
 * Handle fill array data by copying appropriate part of dex file into array.
 */
extern "C" int artHandleFillArrayDataFromCode(const Instruction::ArrayDataPayload* payload,
                                              mirror::Array* array,
                                              Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  bool success = FillArrayData(array, payload);
  return success ? 0 : -1;
}

}  // namespace art
