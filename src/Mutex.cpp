/*
MIT License

Copyright (c) 2025 Alexandre GARCIN

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "Mutex.h"

#include "promise.h"

#include <cassert>
#include <tuple>

namespace promise {

/** @brief Dereferences to the underlying promise.
 *
 * @return A copy of the underlying WPromise<void>.
 */
WPromise<void>
Mutex::operator*() {
   return Lock();
}

/**
 * @brief Acquires the mutex for the current coroutine.
 */
WPromise<void>
Mutex::Lock() {
   return [this] -> Promise<void> {
      auto [promise, resolve, reject] = WPromise<void>::Create();
      (void)reject;

      std::unique_lock lock{mutex_};
      auto             current_promise = promise_;
      promise_                         = std::move(promise);
      lock.unlock();

      co_await current_promise;

      lock.lock();
      assert(!resolve_);
      resolve_ = std::move(resolve);
   };
}

/**
 * @brief Releases the mutex, allowing other waiters to acquire it.
 */
void
Mutex::UnLock() {
   std::unique_lock lock{mutex_};
   assert(resolve_);
   auto resolve = std::move(resolve_);
   resolve_     = nullptr;

   lock.unlock();
   (*resolve)();
}

/**
 * @brief Checks if the mutex is currently owned (locked).
 */
bool
Mutex::OwnsLock() const {
   std::shared_lock lock{mutex_};
   return resolve_ != nullptr;
}

/** @brief Constructs a deferred lock from a shared lock on the Mutex's mutex. */
LockGuard::LockGuard(Mutex& mutex)
   : mutex_(&mutex) {}

/** @brief Move constructor for LockGuard.
 * @param right The LockGuard to move from.
 *
 * @note If this LockGuard currently owns the lock, it will be released before acquiring the new
 * lock. After the move, the source LockGuard will no longer own any lock.
 */
LockGuard::LockGuard(LockGuard&& right) noexcept
   : own_lock_(right.own_lock_)
   , mutex_(right.mutex_) {
   right.own_lock_ = false;
   right.mutex_    = nullptr;
}

/** @brief Move assignment operator for LockGuard.
 * @param right The LockGuard to move from.
 *
 * @return A reference to this LockGuard after the move.
 *
 * @note If this LockGuard currently owns the lock, it will be released before acquiring the new
 * lock. After the move, the source LockGuard will no longer own any lock.
 */
LockGuard&
LockGuard::operator=(LockGuard&& right) noexcept {
   if (this != &right) {
      if (own_lock_) {
         assert(mutex_);
         mutex_->UnLock();
      }

      own_lock_       = right.own_lock_;
      mutex_          = right.mutex_;
      right.own_lock_ = false;
      right.mutex_    = nullptr;
   }

   return *this;
}

/** @brief Destroys the LockGuard, releasing the mutex if owned. */
LockGuard::~LockGuard() {
   if (own_lock_) {
      assert(mutex_);
      mutex_->UnLock();
   }
}

/** @brief Releases the mutex if owned.
 *
 * @note The lock must be acquired before calling this method, otherwise undefined behavior occurs.
 */
void
LockGuard::Unlock() {
   assert(own_lock_);
   assert(mutex_);
   own_lock_ = false;
   mutex_->UnLock();
}

/** @brief Checks if the LockGuard currently owns the lock.
 *
 * @return True if the LockGuard owns the lock, false otherwise.
 */
bool
LockGuard::OwnsLock() const {
   return own_lock_;
}

/** @brief Acquires the mutex for the current coroutine.
 *
 * @return A promise that resolves once the mutex is acquired.
 */
WPromise<void>
LockGuard::Lock() {
   assert(mutex_);
   assert(!own_lock_);
   own_lock_ = true;
   return mutex_->Lock();
}

}  // namespace promise
