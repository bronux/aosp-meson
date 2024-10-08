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

package com.android.ahat.heapdump;

import java.util.Comparator;
import java.util.Iterator;
import java.util.List;
import java.util.function.Predicate;

/**
 * A collection of instances that can be searched for by id.
 */
class Instances<T extends AhatInstance> implements Iterable<T> {

  private final List<T> mInstances;

  /**
   * Create a collection of instances that can be looked up by id.
   * Note: this takes ownership of the given list of instances.
   */
  public Instances(List<T> instances) {
    mInstances = instances;

    // Sort the instances by id so we can use binary search to lookup
    // instances by id.
    instances.sort(new Comparator<AhatInstance>() {
      @Override
      public int compare(AhatInstance a, AhatInstance b) {
        return Long.compare(a.getId(), b.getId());
      }
    });

    // Ensure there is a one-to-one mapping between ids and instances by
    // removing instances that have the same id as a previous instance. The
    // heap dump really ought not to include multiple instances with the same
    // id, but this happens on some older versions of ART and in some versions
    // of the RI.
    Predicate<T> isDuplicate = new Predicate<T>() {
      private long previous = -1;

      @Override
      public boolean test(T x) {
        if (x.getId() == previous) {
          return true;
        }
        previous = x.getId();
        return false;
      }
    };
    mInstances.removeIf(isDuplicate);
  }

  /**
   * Look up an instance by id.
   * Returns null if no instance with the given id is found.
   */
  public T get(long id) {
    // Binary search over the sorted instances.
    int start = 0;
    int end = mInstances.size();
    while (start < end) {
      int mid = start + ((end - start) / 2);
      T midInst = mInstances.get(mid);
      long midId = midInst.getId();
      if (id == midId) {
        return midInst;
      } else if (id < midId) {
        end = mid;
      } else {
        start = mid + 1;
      }
    }
    return null;
  }

  public void removeIf(Predicate<T> predicate) {
    mInstances.removeIf(predicate);
  }

  public int size() {
    return mInstances.size();
  }

  @Override
  public Iterator<T> iterator() {
    return mInstances.iterator();
  }
}

