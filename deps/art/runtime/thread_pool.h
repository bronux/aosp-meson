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

#ifndef ART_RUNTIME_THREAD_POOL_H_
#define ART_RUNTIME_THREAD_POOL_H_

#include <deque>
#include <functional>
#include <vector>

#include "barrier.h"
#include "base/macros.h"
#include "base/mem_map.h"
#include "base/mutex.h"

namespace art HIDDEN {

class AbstractThreadPool;

class Closure {
 public:
  virtual ~Closure() { }
  virtual void Run(Thread* self) = 0;
};

class FunctionClosure : public Closure {
 public:
  explicit FunctionClosure(std::function<void(Thread*)>&& f) : func_(std::move(f)) {}
  void Run(Thread* self) override {
    func_(self);
  }

 private:
  std::function<void(Thread*)> func_;
};

class Task : public Closure {
 public:
  // Called after Closure::Run has been called.
  virtual void Finalize() { }
};

class SelfDeletingTask : public Task {
 public:
  virtual ~SelfDeletingTask() { }
  virtual void Finalize() {
    delete this;
  }
};

class FunctionTask : public SelfDeletingTask {
 public:
  explicit FunctionTask(std::function<void(Thread*)>&& func) : func_(std::move(func)) {}

  void Run(Thread* self) override {
    func_(self);
  }

 private:
  std::function<void(Thread*)> func_;
};

class ThreadPoolWorker {
 public:
  static const size_t kDefaultStackSize = 1 * MB;

  size_t GetStackSize() const {
    DCHECK(stack_.IsValid());
    return stack_.Size();
  }

  virtual ~ThreadPoolWorker();

  // Set the "nice" priority for this worker.
  void SetPthreadPriority(int priority);

  // Get the "nice" priority for this worker.
  int GetPthreadPriority();

  Thread* GetThread() const { return thread_; }

 protected:
  ThreadPoolWorker(AbstractThreadPool* thread_pool, const std::string& name, size_t stack_size);
  static void* Callback(void* arg) REQUIRES(!Locks::mutator_lock_);
  virtual void Run();

  AbstractThreadPool* const thread_pool_;
  const std::string name_;
  MemMap stack_;
  pthread_t pthread_;
  Thread* thread_;

 private:
  friend class AbstractThreadPool;
  DISALLOW_COPY_AND_ASSIGN(ThreadPoolWorker);
};

// Note that thread pool workers will set Thread#setCanCallIntoJava to false.
class AbstractThreadPool {
 public:
  // Returns the number of threads in the thread pool.
  size_t GetThreadCount() const {
    return threads_.size();
  }

  EXPORT const std::vector<ThreadPoolWorker*>& GetWorkers();

  // Broadcast to the workers and tell them to empty out the work queue.
  EXPORT void StartWorkers(Thread* self) REQUIRES(!task_queue_lock_);

  // Do not allow workers to grab any new tasks.
  EXPORT void StopWorkers(Thread* self) REQUIRES(!task_queue_lock_);

  // Returns if the thread pool has started.
  bool HasStarted(Thread* self) REQUIRES(!task_queue_lock_);

  // Add a new task, the first available started worker will process it. Does not delete the task
  // after running it, it is the caller's responsibility.
  virtual void AddTask(Thread* self, Task* task) REQUIRES(!task_queue_lock_) = 0;

  // Remove all tasks in the queue.
  virtual void RemoveAllTasks(Thread* self) REQUIRES(!task_queue_lock_) = 0;

  virtual size_t GetTaskCount(Thread* self) REQUIRES(!task_queue_lock_) = 0;

  // Create the threads of this pool.
  EXPORT void CreateThreads();

  // Stops and deletes all threads in this pool.
  void DeleteThreads();

  // Wait for all tasks currently on queue to get completed. If the pool has been stopped, only
  // wait till all already running tasks are done.
  // When the pool was created with peers for workers, do_work must not be true (see ThreadPool()).
  EXPORT void Wait(Thread* self, bool do_work, bool may_hold_locks) REQUIRES(!task_queue_lock_);

  // Returns the total amount of workers waited for tasks.
  uint64_t GetWaitTime() const {
    return total_wait_time_;
  }

  // Provides a way to bound the maximum number of worker threads, threads must be less the the
  // thread count of the thread pool.
  void SetMaxActiveWorkers(size_t threads) REQUIRES(!task_queue_lock_);

  // Set the "nice" priority for threads in the pool.
  void SetPthreadPriority(int priority);

  // CHECK that the "nice" priority of threads in the pool is the given
  // `priority`.
  void CheckPthreadPriority(int priority);

  // Wait for workers to be created.
  void WaitForWorkersToBeCreated();

  virtual ~AbstractThreadPool() {}

 protected:
  // get a task to run, blocks if there are no tasks left
  Task* GetTask(Thread* self) REQUIRES(!task_queue_lock_);

  // Try to get a task, returning null if there is none available.
  Task* TryGetTask(Thread* self) REQUIRES(!task_queue_lock_);
  virtual Task* TryGetTaskLocked() REQUIRES(task_queue_lock_) = 0;

  // Are we shutting down?
  bool IsShuttingDown() const REQUIRES(task_queue_lock_) {
    return shutting_down_;
  }

  virtual bool HasOutstandingTasks() const REQUIRES(task_queue_lock_) = 0;

  EXPORT AbstractThreadPool(const char* name,
                            size_t num_threads,
                            bool create_peers,
                            size_t worker_stack_size);

  const std::string name_;
  Mutex task_queue_lock_;
  ConditionVariable task_queue_condition_ GUARDED_BY(task_queue_lock_);
  ConditionVariable completion_condition_ GUARDED_BY(task_queue_lock_);
  bool started_ GUARDED_BY(task_queue_lock_);
  bool shutting_down_ GUARDED_BY(task_queue_lock_);
  // How many worker threads are waiting on the condition.
  size_t waiting_count_ GUARDED_BY(task_queue_lock_);
  std::vector<ThreadPoolWorker*> threads_;
  // Work balance detection.
  uint64_t start_time_ GUARDED_BY(task_queue_lock_);
  uint64_t total_wait_time_;
  Barrier creation_barier_;
  size_t max_active_workers_ GUARDED_BY(task_queue_lock_);
  const bool create_peers_;
  const size_t worker_stack_size_;

 private:
  friend class ThreadPoolWorker;
  friend class WorkStealingWorker;
  DISALLOW_COPY_AND_ASSIGN(AbstractThreadPool);
};

class EXPORT ThreadPool : public AbstractThreadPool {
 public:
  // Create a named thread pool with the given number of threads.
  //
  // If create_peers is true, all worker threads will have a Java peer object. Note that if the
  // pool is asked to do work on the current thread (see Wait), a peer may not be available. Wait
  // will conservatively abort if create_peers and do_work are true.
  static ThreadPool* Create(const char* name,
                            size_t num_threads,
                            bool create_peers = false,
                            size_t worker_stack_size = ThreadPoolWorker::kDefaultStackSize) {
    ThreadPool* pool = new ThreadPool(name, num_threads, create_peers, worker_stack_size);
    pool->CreateThreads();
    return pool;
  }

  void AddTask(Thread* self, Task* task) REQUIRES(!task_queue_lock_) override;
  size_t GetTaskCount(Thread* self) REQUIRES(!task_queue_lock_) override;
  void RemoveAllTasks(Thread* self) REQUIRES(!task_queue_lock_) override;
  ~ThreadPool() override;

 protected:
  Task* TryGetTaskLocked() REQUIRES(task_queue_lock_) override;

  bool HasOutstandingTasks() const REQUIRES(task_queue_lock_) override {
    return started_ && !tasks_.empty();
  }

  ThreadPool(const char* name,
             size_t num_threads,
             bool create_peers,
             size_t worker_stack_size)
      : AbstractThreadPool(name, num_threads, create_peers, worker_stack_size) {}

 private:
  std::deque<Task*> tasks_ GUARDED_BY(task_queue_lock_);

  DISALLOW_COPY_AND_ASSIGN(ThreadPool);
};

}  // namespace art

#endif  // ART_RUNTIME_THREAD_POOL_H_
