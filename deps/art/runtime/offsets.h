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

#ifndef ART_RUNTIME_OFFSETS_H_
#define ART_RUNTIME_OFFSETS_H_

#include <iosfwd>

#include "base/macros.h"
#include "base/pointer_size.h"
#include "runtime_globals.h"

namespace art HIDDEN {

// Allow the meaning of offsets to be strongly typed.
class Offset {
 public:
  constexpr explicit Offset(size_t val) : val_(val) {}
  constexpr int32_t Int32Value() const {
    return static_cast<int32_t>(val_);
  }
  constexpr uint32_t Uint32Value() const {
    return static_cast<uint32_t>(val_);
  }
  constexpr size_t SizeValue() const {
    return val_;
  }
  Offset& operator+=(const size_t rhs) {
    val_ += rhs;
    return *this;
  }
  constexpr bool operator==(Offset o) const {
    return SizeValue() == o.SizeValue();
  }
  constexpr bool operator!=(Offset o) const {
    return !(*this == o);
  }
  constexpr bool operator<(Offset o) const {
    return SizeValue() < o.SizeValue();
  }
  constexpr bool operator<=(Offset o) const {
    return !(*this > o);
  }
  constexpr bool operator>(Offset o) const {
    return o < *this;
  }
  constexpr bool operator>=(Offset o) const {
    return !(*this < o);
  }

 protected:
  size_t val_;
};
std::ostream& operator<<(std::ostream& os, const Offset& offs);

// Offsets relative to the current frame.
class FrameOffset : public Offset {
 public:
  constexpr explicit FrameOffset(size_t val) : Offset(val) {}
  bool operator>(FrameOffset other) const { return val_ > other.val_; }
  bool operator<(FrameOffset other) const { return val_ < other.val_; }
};

// Offsets relative to the current running thread.
template<PointerSize pointer_size>
class ThreadOffset : public Offset {
 public:
  constexpr explicit ThreadOffset(size_t val) : Offset(val) {}
};

using ThreadOffset32 = ThreadOffset<PointerSize::k32>;
using ThreadOffset64 = ThreadOffset<PointerSize::k64>;

// Offsets relative to an object.
class MemberOffset : public Offset {
 public:
  constexpr explicit MemberOffset(size_t val) : Offset(val) {}
};

}  // namespace art

#endif  // ART_RUNTIME_OFFSETS_H_
