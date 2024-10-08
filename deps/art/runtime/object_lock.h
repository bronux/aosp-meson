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

#ifndef ART_RUNTIME_OBJECT_LOCK_H_
#define ART_RUNTIME_OBJECT_LOCK_H_

#include "base/locks.h"
#include "base/macros.h"
#include "handle.h"

namespace art HIDDEN {

class Thread;

template <typename T>
class ObjectLock {
 public:
  EXPORT ObjectLock(Thread* self, Handle<T> object) REQUIRES_SHARED(Locks::mutator_lock_);

  EXPORT ~ObjectLock() REQUIRES_SHARED(Locks::mutator_lock_);

  void WaitIgnoringInterrupts() REQUIRES_SHARED(Locks::mutator_lock_);

  void Notify() REQUIRES_SHARED(Locks::mutator_lock_);

  void NotifyAll() REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  Thread* const self_;
  Handle<T> const obj_;

  DISALLOW_COPY_AND_ASSIGN(ObjectLock);
};

template <typename T>
class ObjectTryLock {
 public:
  ObjectTryLock(Thread* self, Handle<T> object) REQUIRES_SHARED(Locks::mutator_lock_);

  ~ObjectTryLock() REQUIRES_SHARED(Locks::mutator_lock_);

  bool Acquired() const {
    return acquired_;
  }

 private:
  Thread* const self_;
  Handle<T> const obj_;
  bool acquired_;


  DISALLOW_COPY_AND_ASSIGN(ObjectTryLock);
};

}  // namespace art

#endif  // ART_RUNTIME_OBJECT_LOCK_H_
