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

#ifndef ART_RUNTIME_GC_SPACE_SPACE_H_
#define ART_RUNTIME_GC_SPACE_SPACE_H_

#include <memory>
#include <string>

#include "base/atomic.h"
#include "base/locks.h"
#include "base/macros.h"
#include "base/mem_map.h"
#include "gc/accounting/space_bitmap.h"
#include "gc/collector/object_byte_pair.h"
#include "runtime_globals.h"

namespace art HIDDEN {
namespace mirror {
class Object;
}  // namespace mirror

namespace gc {

class Heap;

namespace space {

class AllocSpace;
class BumpPointerSpace;
class ContinuousMemMapAllocSpace;
class ContinuousSpace;
class DiscontinuousSpace;
class MallocSpace;
class DlMallocSpace;
class RosAllocSpace;
class ImageSpace;
class LargeObjectSpace;
class RegionSpace;
class ZygoteSpace;

static constexpr bool kDebugSpaces = kIsDebugBuild;

// See Space::GetGcRetentionPolicy.
enum GcRetentionPolicy {
  // Objects are retained forever with this policy for a space.
  kGcRetentionPolicyNeverCollect,
  // Every GC cycle will attempt to collect objects in this space.
  kGcRetentionPolicyAlwaysCollect,
  // Objects will be considered for collection only in "full" GC cycles, ie faster partial
  // collections won't scan these areas such as the Zygote.
  kGcRetentionPolicyFullCollect,
};
std::ostream& operator<<(std::ostream& os, GcRetentionPolicy policy);

enum SpaceType {
  kSpaceTypeImageSpace,
  kSpaceTypeMallocSpace,
  kSpaceTypeZygoteSpace,
  kSpaceTypeBumpPointerSpace,
  kSpaceTypeLargeObjectSpace,
  kSpaceTypeRegionSpace,
};
std::ostream& operator<<(std::ostream& os, SpaceType space_type);

// A space contains memory allocated for managed objects.
class EXPORT Space {
 public:
  // Dump space. Also key method for C++ vtables.
  virtual void Dump(std::ostream& os) const;

  // Name of the space. May vary, for example before/after the Zygote fork.
  const char* GetName() const {
    return name_.c_str();
  }

  // The policy of when objects are collected associated with this space.
  GcRetentionPolicy GetGcRetentionPolicy() const {
    return gc_retention_policy_;
  }

  // Is the given object contained within this space?
  virtual bool Contains(const mirror::Object* obj) const = 0;

  // The kind of space this: image, alloc, zygote, large object.
  virtual SpaceType GetType() const = 0;

  // Is this an image space, ie one backed by a memory mapped image file.
  bool IsImageSpace() const {
    return GetType() == kSpaceTypeImageSpace;
  }
  ImageSpace* AsImageSpace();

  // Is this a dlmalloc backed allocation space?
  bool IsMallocSpace() const {
    SpaceType type = GetType();
    return type == kSpaceTypeMallocSpace;
  }
  MallocSpace* AsMallocSpace();

  virtual bool IsDlMallocSpace() const {
    return false;
  }
  virtual DlMallocSpace* AsDlMallocSpace();

  virtual bool IsRosAllocSpace() const {
    return false;
  }
  virtual RosAllocSpace* AsRosAllocSpace();

  // Is this the space allocated into by the Zygote and no-longer in use for allocation?
  bool IsZygoteSpace() const {
    return GetType() == kSpaceTypeZygoteSpace;
  }
  virtual ZygoteSpace* AsZygoteSpace();

  // Is this space a bump pointer space?
  bool IsBumpPointerSpace() const {
    return GetType() == kSpaceTypeBumpPointerSpace;
  }
  virtual BumpPointerSpace* AsBumpPointerSpace();

  bool IsRegionSpace() const {
    return GetType() == kSpaceTypeRegionSpace;
  }
  virtual RegionSpace* AsRegionSpace();

  // Does this space hold large objects and implement the large object space abstraction?
  bool IsLargeObjectSpace() const {
    return GetType() == kSpaceTypeLargeObjectSpace;
  }
  LargeObjectSpace* AsLargeObjectSpace();

  virtual bool IsContinuousSpace() const {
    return false;
  }
  ContinuousSpace* AsContinuousSpace();

  virtual bool IsDiscontinuousSpace() const {
    return false;
  }
  DiscontinuousSpace* AsDiscontinuousSpace();

  virtual bool IsAllocSpace() const {
    return false;
  }
  virtual AllocSpace* AsAllocSpace();

  virtual bool IsContinuousMemMapAllocSpace() const {
    return false;
  }
  virtual ContinuousMemMapAllocSpace* AsContinuousMemMapAllocSpace();

  // Returns true if objects in the space are movable.
  virtual bool CanMoveObjects() const = 0;

  virtual ~Space() {}

 protected:
  Space(const std::string& name, GcRetentionPolicy gc_retention_policy);

  void SetGcRetentionPolicy(GcRetentionPolicy gc_retention_policy) {
    gc_retention_policy_ = gc_retention_policy;
  }

  // Name of the space that may vary due to the Zygote fork.
  std::string name_;

 protected:
  // When should objects within this space be reclaimed? Not constant as we vary it in the case
  // of Zygote forking.
  GcRetentionPolicy gc_retention_policy_;

 private:
  friend class art::gc::Heap;
  DISALLOW_IMPLICIT_CONSTRUCTORS(Space);
};
std::ostream& operator<<(std::ostream& os, const Space& space);

// AllocSpace interface.
class AllocSpace {
 public:
  // Number of bytes currently allocated.
  virtual uint64_t GetBytesAllocated() = 0;
  // Number of objects currently allocated.
  virtual uint64_t GetObjectsAllocated() = 0;

  // Allocate num_bytes without allowing growth. If the allocation
  // succeeds, the output parameter bytes_allocated will be set to the
  // actually allocated bytes which is >= num_bytes.
  // Alloc can be called from multiple threads at the same time and must be thread-safe.
  //
  // bytes_tl_bulk_allocated - bytes allocated in bulk ahead of time for a thread local allocation,
  // if applicable. It is
  // 1) equal to bytes_allocated if it's not a thread local allocation,
  // 2) greater than bytes_allocated if it's a thread local
  //    allocation that required a new buffer, or
  // 3) zero if it's a thread local allocation in an existing
  //    buffer.
  // This is what is to be added to Heap::num_bytes_allocated_.
  virtual mirror::Object* Alloc(Thread* self, size_t num_bytes, size_t* bytes_allocated,
                                size_t* usable_size, size_t* bytes_tl_bulk_allocated) = 0;

  // Thread-unsafe allocation for when mutators are suspended, used by the semispace collector.
  virtual mirror::Object* AllocThreadUnsafe(Thread* self, size_t num_bytes, size_t* bytes_allocated,
                                            size_t* usable_size,
                                            size_t* bytes_tl_bulk_allocated)
      REQUIRES(Locks::mutator_lock_) {
    return Alloc(self, num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
  }

  // Return the storage space required by obj.
  virtual size_t AllocationSize(mirror::Object* obj, size_t* usable_size) = 0;

  // Returns how many bytes were freed.
  virtual size_t Free(Thread* self, mirror::Object* ptr) = 0;

  // Free (deallocate) all objects in a list, and return the number of bytes freed.
  virtual size_t FreeList(Thread* self, size_t num_ptrs, mirror::Object** ptrs) = 0;

  // Revoke any sort of thread-local buffers that are used to speed up allocations for the given
  // thread, if the alloc space implementation uses any.
  // Returns the total free bytes in the revoked thread local runs that's to be subtracted
  // from Heap::num_bytes_allocated_ or zero if unnecessary.
  virtual size_t RevokeThreadLocalBuffers(Thread* thread) = 0;

  // Revoke any sort of thread-local buffers that are used to speed up allocations for all the
  // threads, if the alloc space implementation uses any.
  // Returns the total free bytes in the revoked thread local runs that's to be subtracted
  // from Heap::num_bytes_allocated_ or zero if unnecessary.
  virtual size_t RevokeAllThreadLocalBuffers() = 0;

  // Compute largest free contiguous chunk of memory available in the space and
  // log it if it's smaller than failed_alloc_bytes and return true.
  // Otherwise leave os untouched and return false.
  virtual bool LogFragmentationAllocFailure(std::ostream& os, size_t failed_alloc_bytes) = 0;

 protected:
  struct SweepCallbackContext {
    SweepCallbackContext(bool swap_bitmaps, space::Space* space);
    const bool swap_bitmaps;
    space::Space* const space;
    Thread* const self;
    collector::ObjectBytePair freed;
  };

  AllocSpace() {}
  virtual ~AllocSpace() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AllocSpace);
};

// Continuous spaces have bitmaps, and an address range. Although not required, objects within
// continuous spaces can be marked in the card table.
class ContinuousSpace : public Space {
 public:
  // Address at which the space begins.
  uint8_t* Begin() const {
    return begin_;
  }

  // Current address at which the space ends, which may vary as the space is filled.
  uint8_t* End() const {
    return end_.load(std::memory_order_relaxed);
  }

  // The end of the address range covered by the space.
  uint8_t* Limit() const {
    return limit_;
  }

  // Change the end of the space. Be careful with use since changing the end of a space to an
  // invalid value may break the GC.
  void SetEnd(uint8_t* end) {
    end_.store(end, std::memory_order_relaxed);
  }

  void SetLimit(uint8_t* limit) {
    limit_ = limit;
  }

  // Current size of space
  size_t Size() const {
    return End() - Begin();
  }

  virtual accounting::ContinuousSpaceBitmap* GetLiveBitmap() = 0;
  virtual accounting::ContinuousSpaceBitmap* GetMarkBitmap() = 0;

  // Maximum which the mapped space can grow to.
  virtual size_t Capacity() const {
    return Limit() - Begin();
  }

  // Is object within this space? We check to see if the pointer is beyond the end first as
  // continuous spaces are iterated over from low to high.
  bool HasAddress(const mirror::Object* obj) const {
    const uint8_t* byte_ptr = reinterpret_cast<const uint8_t*>(obj);
    return byte_ptr >= Begin() && byte_ptr < Limit();
  }

  bool Contains(const mirror::Object* obj) const {
    return HasAddress(obj);
  }

  virtual bool IsContinuousSpace() const {
    return true;
  }

  bool HasBoundBitmaps() REQUIRES(Locks::heap_bitmap_lock_);

  virtual ~ContinuousSpace() {}

 protected:
  ContinuousSpace(const std::string& name, GcRetentionPolicy gc_retention_policy,
                  uint8_t* begin, uint8_t* end, uint8_t* limit) :
      Space(name, gc_retention_policy), begin_(begin), end_(end), limit_(limit) {
  }

  // The beginning of the storage for fast access.
  uint8_t* begin_;

  // Current end of the space.
  Atomic<uint8_t*> end_;

  // Limit of the space.
  uint8_t* limit_;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ContinuousSpace);
};

// A space where objects may be allocated higgledy-piggledy throughout virtual memory. Currently
// the card table can't cover these objects and so the write barrier shouldn't be triggered. This
// is suitable for use for large primitive arrays.
class DiscontinuousSpace : public Space {
 public:
  accounting::LargeObjectBitmap* GetLiveBitmap() {
    return &live_bitmap_;
  }

  accounting::LargeObjectBitmap* GetMarkBitmap() {
    return &mark_bitmap_;
  }

  bool IsDiscontinuousSpace() const override {
    return true;
  }

  virtual ~DiscontinuousSpace() {}

 protected:
  DiscontinuousSpace(const std::string& name, GcRetentionPolicy gc_retention_policy);

  accounting::LargeObjectBitmap live_bitmap_;
  accounting::LargeObjectBitmap mark_bitmap_;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(DiscontinuousSpace);
};

class MemMapSpace : public ContinuousSpace {
 public:
  // Size of the space without a limit on its growth. By default this is just the Capacity, but
  // for the allocation space we support starting with a small heap and then extending it.
  virtual size_t NonGrowthLimitCapacity() const {
    return Capacity();
  }

  MemMap* GetMemMap() {
    return &mem_map_;
  }

  const MemMap* GetMemMap() const {
    return &mem_map_;
  }

  MemMap ReleaseMemMap() {
    return std::move(mem_map_);
  }

 protected:
  MemMapSpace(const std::string& name,
              MemMap&& mem_map,
              uint8_t* begin,
              uint8_t* end,
              uint8_t* limit,
              GcRetentionPolicy gc_retention_policy)
      : ContinuousSpace(name, gc_retention_policy, begin, end, limit),
        mem_map_(std::move(mem_map)) {
  }

  // Underlying storage of the space
  MemMap mem_map_;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MemMapSpace);
};

// Used by the heap compaction interface to enable copying from one type of alloc space to another.
class ContinuousMemMapAllocSpace : public MemMapSpace, public AllocSpace {
 public:
  bool IsAllocSpace() const override {
    return true;
  }
  AllocSpace* AsAllocSpace() override {
    return this;
  }

  bool IsContinuousMemMapAllocSpace() const override {
    return true;
  }
  ContinuousMemMapAllocSpace* AsContinuousMemMapAllocSpace() override {
    return this;
  }

  // Make the mark bitmap an alias of the live bitmap. Save the current mark bitmap into
  // `temp_bitmap_`, so that we can restore it later in ContinuousMemMapAllocSpace::UnBindBitmaps.
  void BindLiveToMarkBitmap() REQUIRES(Locks::heap_bitmap_lock_);
  // Unalias the mark bitmap from the live bitmap and restore the old mark bitmap.
  void UnBindBitmaps() REQUIRES(Locks::heap_bitmap_lock_);
  // Swap the live and mark bitmaps of this space. This is used by the GC for concurrent sweeping.
  void SwapBitmaps() REQUIRES(Locks::heap_bitmap_lock_);

  // Clear the space back to an empty space.
  virtual void Clear() = 0;

  accounting::ContinuousSpaceBitmap* GetLiveBitmap() override {
    return &live_bitmap_;
  }

  accounting::ContinuousSpaceBitmap* GetMarkBitmap() override {
    return &mark_bitmap_;
  }

  accounting::ContinuousSpaceBitmap* GetTempBitmap() {
    return &temp_bitmap_;
  }

  collector::ObjectBytePair Sweep(bool swap_bitmaps);
  virtual accounting::ContinuousSpaceBitmap::SweepCallback* GetSweepCallback() = 0;

 protected:
  accounting::ContinuousSpaceBitmap live_bitmap_;
  accounting::ContinuousSpaceBitmap mark_bitmap_;
  accounting::ContinuousSpaceBitmap temp_bitmap_;

  ContinuousMemMapAllocSpace(const std::string& name,
                             MemMap&& mem_map,
                             uint8_t* begin,
                             uint8_t* end,
                             uint8_t* limit,
                             GcRetentionPolicy gc_retention_policy)
      : MemMapSpace(name, std::move(mem_map), begin, end, limit, gc_retention_policy) {
  }

 private:
  friend class gc::Heap;
  DISALLOW_IMPLICIT_CONSTRUCTORS(ContinuousMemMapAllocSpace);
};

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_SPACE_H_
