// Copyright (c) 2014, Emmanuel Goossaert. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause License,
// that can be found in the LICENSE file.

#ifndef KINGDB_THREADPOOL_H_
#define KINGDB_THREADPOOL_H_

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>


namespace kdb {

class Task {
 public:
  Task() {}
  virtual ~Task() {}
  virtual void RunInLock(std::thread::id tid) = 0;
  virtual void Run(std::thread::id tid, uint64_t id) = 0;
};


class ThreadPool {
 // TODO: What if too many items are incoming? add_task()
 //       must return an error beyond a certain limit, or timeout
 // TODO: What if a run() method throws an exception? => force it to be noexcept?
 // TODO: Impose limit on number of items in queue -- for thread pool over
 //       sockets, the queue should be of size 0
 // TODO: Implement the stop method and make the worker listen to it and force
 //       them to stop when required.
 // TODO: Make sure all variables and methods follow the naming convention
 public:
  int num_threads_; 
  std::queue<Task*> queue_;
  std::condition_variable cv_;
  std::mutex mutex_;
  std::vector<std::thread> threads_;
  std::map<std::thread::id, uint64_t> tid_to_id;
  uint64_t seq_id;

  ThreadPool(int num_threads) {
    num_threads_ = num_threads;
    seq_id = 0;
  }

  ~ThreadPool() {
    for (auto& t: threads_) {
      t.join();
    }
  }

  void ProcessingLoop() {
    while (true) {
      std::unique_lock<std::mutex> lock(mutex_);
      if (queue_.size() == 0) {
        cv_.wait(lock);
      }
      auto task = queue_.front();
      queue_.pop();
      auto tid = std::this_thread::get_id();
      auto it_find = tid_to_id.find(tid);
      if (it_find == tid_to_id.end()) tid_to_id[tid] = seq_id++;
      task->RunInLock(tid);
      lock.unlock();
      task->Run(tid, tid_to_id[tid]);
      delete task;
    }
  }

  void AddTask(Task* task) {
      std::unique_lock<std::mutex> lock(mutex_);
      queue_.push(task);
      cv_.notify_one();
  }

  int Start() {
    // NOTE: Should each thread run the loop, or should the loop be running in a
    //       main thread that is then dispatching work by notifying other
    //       threads?
    for (auto i = 0; i < num_threads_; i++) {
      threads_.push_back(std::thread(&ThreadPool::ProcessingLoop, this));
    }
    return 0;
  }

  int Stop() {
    return 0;
  }

};

}

#endif // KINGDB_THREADPOOL_H_