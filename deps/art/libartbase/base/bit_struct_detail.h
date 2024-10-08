/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_BIT_STRUCT_DETAIL_H_
#define ART_LIBARTBASE_BASE_BIT_STRUCT_DETAIL_H_

#include "bit_utils.h"
#include "globals.h"

#include <type_traits>

// Implementation details for bit_struct.h
// Not intended to be used stand-alone.

namespace art {

template <typename T>
static constexpr size_t BitStructSizeOf();

namespace detail {
// Select the smallest uintX_t that will fit kBitSize bits.
template <size_t kBitSize>
struct MinimumTypeUnsignedHelper {
  using type =
    std::conditional_t<kBitSize == 0, void,       // NOLINT [whitespace/operators] [3]
    std::conditional_t<kBitSize <= 8, uint8_t,    // NOLINT [whitespace/operators] [3]
    std::conditional_t<kBitSize <= 16, uint16_t,  // NOLINT [whitespace/operators] [3]
    std::conditional_t<kBitSize <= 32, uint32_t,
    std::conditional_t<kBitSize <= 64, uint64_t,
    std::conditional_t<kBitSize <= BitSizeOf<uintmax_t>(), uintmax_t, void>>>>>>;
};

// Select the smallest [u]intX_t that will fit kBitSize bits.
// Automatically picks intX_t or uintX_t based on the sign-ness of T.
template <typename T, size_t kBitSize>
struct MinimumTypeHelper {
  using type_unsigned = typename MinimumTypeUnsignedHelper<kBitSize>::type;

  using type =
    std::conditional_t</* if */   std::is_signed_v<T>,
                       /* then */ std::make_signed_t<type_unsigned>,
                       /* else */ type_unsigned>;
};

// Helper for converting to and from T to an integral type.
template <typename T>
union ValueConverter {
  using StorageType = typename MinimumTypeHelper<T, sizeof(T) * kBitsPerByte>::type;

  static constexpr StorageType ToUnderlyingStorage(T value) {
    ValueConverter converter;
    converter.value_.val_ = value;
    return converter.storage_.val_;
  }

  static constexpr T FromUnderlyingStorage(StorageType storage) {
    ValueConverter converter;
    converter.storage_.val_ = storage;
    return converter.value_.val_;
  }

  // Underlying values must be wrapped in separate standard-layout structs.
  // See below for more details.
  struct StorageWrapper {
    StorageType val_;
  };
  struct ValueWrapper {
    T val_;
  };

  // Safely alias storage_ and value_ together.
  //
  // See C++ 9.5.1 [class.union]:
  // If a standard-layout union contains several standard-layout structs that share a common
  // initial sequence ... it is permitted to inspect the common initial sequence of any of
  // standard-layout struct members.
  StorageWrapper storage_;
  ValueWrapper value_;
#if defined(__cpp_lib_is_layout_compatible)
#error "When upgrading to a libc++ with this functionality, remove this error and check that this is OK for all use cases."
  static_assert(std::is_layout_compatible_v<StorageWrapper, ValueWrapper>);
#endif

  // Future work: In theory almost non-standard layout can be supported here,
  // assuming they don't rely on the address of (this).
  // We just have to use memcpy since the union-aliasing would not work.
};

// Denotes the beginning of a bit struct.
//
// This marker is required by the C++ standard in order to
// have a "common initial sequence".
//
// See C++ 9.5.1 [class.union]:
// If a standard-layout union contains several standard-layout structs that share a common
// initial sequence ... it is permitted to inspect the common initial sequence of any of
// standard-layout struct members.
template <size_t kSize>
struct DefineBitStructSize {
 private:
  typename MinimumTypeUnsignedHelper<kSize>::type _;
};

// Check if type "T" has a member called _ in it.
template <typename T>
struct HasUnderscoreField {
 private:
  using TrueT = std::bool_constant<true>::type;
  using FalseT = std::bool_constant<false>::type;

  template <typename C>
  static constexpr auto Test(void*) -> decltype(std::declval<C>()._, TrueT{});

  template <typename>
  static constexpr FalseT Test(...);

 public:
  static constexpr bool value = decltype(Test<T>(nullptr))::value;
};

// Infer the type of the member of &T::M.
template <typename T, typename M>
M GetMemberType(M T:: *);

// Ensure the minimal type storage for 'T' matches its declared BitStructSizeOf.
// Nominally used by the BITSTRUCT_DEFINE_END macro.
template <typename T>
static constexpr bool ValidateBitStructSize() {
  static_assert(std::is_union_v<T>, "T must be union");
  static_assert(std::is_standard_layout_v<T>, "T must be standard-layout");
  static_assert(HasUnderscoreField<T>::value, "T must have the _ DefineBitStructSize");

  const size_t kBitStructSizeOf = BitStructSizeOf<T>();
  static_assert(std::is_same_v<decltype(GetMemberType(&T::_)),
                               DefineBitStructSize<kBitStructSizeOf>>,
                "T::_ must be a DefineBitStructSize of the same size");

  const size_t kExpectedSize = (BitStructSizeOf<T>() < kBitsPerByte)
                                   ? kBitsPerByte
                                   : RoundUpToPowerOfTwo(kBitStructSizeOf);

  // Ensure no extra fields were added in between START/END.
  const size_t kActualSize = sizeof(T) * kBitsPerByte;
  return kExpectedSize == kActualSize;
}
}  // namespace detail
}  // namespace art

#endif  // ART_LIBARTBASE_BASE_BIT_STRUCT_DETAIL_H_
