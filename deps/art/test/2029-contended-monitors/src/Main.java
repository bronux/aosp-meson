/*
 * Copyright (C) 2019 The Android Open Source Project
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
import java.util.concurrent.atomic.AtomicInteger;

public class Main {

  private final boolean PRINT_TIMES = false;  // False for use as run test.

  // Number of increments done by each thread.  Must be multiple of largest hold time below,
  // times any possible thread count. We reduce this below if PRINT_TIMES is not set, and
  // even more in debuggable mode.
  private final int UNSCALED_TOTAL_ITERS  = 16_000_000;
  private final int UNSCALED_MAX_HOLD_TIME = 2_000_000;
  private int totalIters;
  private int maxHoldTime;

  private int counter;

  private AtomicInteger atomicCounter = new AtomicInteger();

  private Object lock;

  private int currentThreadCount = 0;

  private static boolean debuggable = false;  // Running in a debuggable ART environment.

  // A function such that if we repeatedly apply it to -1, the value oscillates
  // between -1 and 3. Thus the average value is 1.
  // This is designed to make it hard for the compiler to predict the values in
  // the sequence.
  private int nextInt(int x) {
    if (x < 0) {
      return x * x + 2;
    } else {
      return x - 4;
    }
  }

  // Increment counter by n, holding lock for time roughly propertional to n.
  // N must be even.
  private void holdFor(Object lock, int n) {
    synchronized(lock) {
      int y = -1;
      for (int i = 0; i < n; ++i) {
        counter += y;
        y = nextInt(y);
      }
    }
  }

  // Increment local by an even number n in a way that takes time roughly proportional
  // to n.
  private void spinFor(int n) {
    int y = -1;
    int local_counter = 0;
    for (int i = 0; i < n; ++i) {
      local_counter += y;
      y = nextInt(y);
    }
    if (local_counter != n) {
      throw new Error();
    }
  }

  private class RepeatedLockHolder implements Runnable {
    RepeatedLockHolder(boolean shared, int n /* even */) {
      sharedLock = shared;
      holdTime = n;
    }
    @Override
    public void run() {
      Object myLock = sharedLock ? lock : new Object();
      int nIters = totalIters / currentThreadCount / holdTime;
      for (int i = 0; i < nIters; ++i) {
        holdFor(myLock, holdTime);
      }
    }
    private boolean sharedLock;
    private int holdTime;
  }

  private class RepeatedIntermittentLockHolder implements Runnable {
    RepeatedIntermittentLockHolder(boolean shared, int n /* even */) {
      sharedLock = shared;
      holdTime = n;
    }
    @Override
    public void run() {
      Object myLock = sharedLock ? lock : new Object();
      int iter_divisor = 10 * currentThreadCount * holdTime;
      int nIters = totalIters / iter_divisor;
      if (totalIters % iter_divisor != 0 || holdTime % 2 == 1) {
        System.err.println("Misconfigured: totalIters = " + totalIters
            + " iter_divisor = "  + iter_divisor);
      }
      for (int i = 0; i < nIters; ++i) {
        spinFor(9 * holdTime);
        holdFor(myLock, holdTime);
      }
    }
    private boolean sharedLock;
    private int holdTime;
  }

  private class SleepyLockHolder implements Runnable {
    SleepyLockHolder(boolean shared) {
      sharedLock = shared;
    }
    @Override
    public void run() {
      Object myLock = sharedLock ? lock : new Object();
      int nIters = totalIters / currentThreadCount / 10_000;
      for (int i = 0; i < nIters; ++i) {
        synchronized(myLock) {
          try {
            Thread.sleep(2);
          } catch(InterruptedException e) {
            throw new AssertionError("Unexpected interrupt");
          }
          counter += 10_000;
        }
      }
    }
    private boolean sharedLock;
  }

  // Increment atomicCounter n times, on average by 1 each time.
  private class RepeatedIncrementer implements Runnable {
    @Override
    public void run() {
      int y = -1;
      int nIters = totalIters / currentThreadCount;
      for (int i = 0; i < nIters; ++i) {
        atomicCounter.addAndGet(y);
        y = nextInt(y);
      }
    }
  }

  // Run n threads doing work. Return the elapsed time this took, in milliseconds.
  private long runMultiple(int n, Runnable work) {
    Thread[] threads = new Thread[n];
    // Replace lock, so that we start with a clean, uninflated lock each time.
    lock = new Object();
    for (int i = 0; i < n; ++i) {
      threads[i] = new Thread(work);
    }
    long startTime = System.currentTimeMillis();
    for (int i = 0; i < n; ++i) {
      threads[i].start();
    }
    for (int i = 0; i < n; ++i) {
      try {
        threads[i].join();
      } catch(InterruptedException e) {
        throw new AssertionError("Unexpected interrupt");
      }
    }
    return System.currentTimeMillis() - startTime;
  }

  // Run on different numbers of threads.
  private void runAll(Runnable work, Runnable init, Runnable checker) {
    for (int i = 1; i <= 8; i *= 2) {
      currentThreadCount = i;
      init.run();
      long time = runMultiple(i, work);
      if (PRINT_TIMES) {
        System.out.print(time + (i == 8 ? "\n" : "\t"));
      }
      checker.run();
    }
  }

  private class CheckAtomicCounter implements Runnable {
    @Override
    public void run() {
      if (atomicCounter.get() != totalIters) {
        throw new AssertionError("Failed atomicCounter postcondition check for "
            + currentThreadCount + " threads");
      }
    }
  }

  private class CheckCounter implements Runnable {
    private final int expected;
    public CheckCounter(int e) {
      expected = e;
    }
    @Override
    public void run() {
      if (counter != expected) {
        throw new AssertionError("Failed counter postcondition check for "
            + currentThreadCount + " threads, expected " + expected + " got " + counter);
      }
    }
  }

  private void run() {
    int scale = PRINT_TIMES ? 1 : (debuggable ? 20 : 10);
    totalIters = UNSCALED_TOTAL_ITERS / scale;
    maxHoldTime = UNSCALED_MAX_HOLD_TIME / scale;
    if (PRINT_TIMES) {
      System.out.println("All times in milliseconds for 1, 2, 4 and 8 threads");
    }
    System.out.println("Atomic increments");
    runAll(new RepeatedIncrementer(), () -> { atomicCounter.set(0); }, new CheckAtomicCounter());
    for (int i = 2; i <= UNSCALED_MAX_HOLD_TIME / 100; i *= 10) {
      // i * 8 (max thread count) divides totalIters
      System.out.println("Hold time " + i + ", shared lock");
      runAll(new RepeatedLockHolder(true, i), () -> { counter = 0; },
          new CheckCounter(totalIters));
    }
    for (int i = 2; i <= UNSCALED_MAX_HOLD_TIME / 1000; i *= 10) {
      // i * 8 (max thread count) divides totalIters
      System.out.println("Hold time " + i + ", pause time " + (9 * i) + ", shared lock");
      runAll(new RepeatedIntermittentLockHolder(true, i), () -> { counter = 0; },
          new CheckCounter(totalIters / 10));
    }
    if (PRINT_TIMES) {
      for (int i = 2; i <= maxHoldTime; i *= 1000) {
        // i divides totalIters
        System.out.println("Hold time " + i + ", private lock");
        // Since there is no mutual exclusion final counter value is unpredictable.
        runAll(new RepeatedLockHolder(false, i), () -> { counter = 0; }, () -> {});
      }
    }
    System.out.println("Hold for 2 msecs while sleeping, shared lock");
    runAll(new SleepyLockHolder(true), () -> { counter = 0; }, new CheckCounter(totalIters));
    System.out.println("Hold for 2 msecs while sleeping, private lock");
    runAll(new SleepyLockHolder(false), () -> { counter = 0; }, () -> {});
  }

  public static void main(String[] args) {
    if (System.getProperty("java.vm.name").equalsIgnoreCase("Dalvik")) {
      try {
        System.loadLibrary(args[0]);
        debuggable = isDebuggable();
      } catch (Throwable t) {
        // Conservatively assume we might be debuggable, and pretend we loaded the library.
        // As of June 2024, we seem to get here with atest.
        debuggable = true;
        System.out.println("JNI_OnLoad called");
      }
    } else {
      System.out.println("JNI_OnLoad called");  // Not really, but match expected output.
    }
    System.out.println("Starting");
    new Main().run();
  }

  private static native boolean isDebuggable();
}
