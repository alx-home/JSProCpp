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

#pragma once

#include "core/WPromise.h"
#include "core.h"

#include <shared_mutex>
#include <memory>

namespace promise {

/**
 * @brief Coroutine-friendly mutual exclusion primitive for async synchronization.
 *
 * Mutex allows coroutines to synchronize access to shared resources through async mutual exclusion.
 * - Lock() acquires the mutex for the current coroutine.
 * - operator*() provides awaitable handles for coroutines.
 *
 * Usage:
 *   - co_await Lock() or co_await *mutex to await the lock.
 *   - UnLock() to release the mutex.
 *   - OwnLock() to check if the mutex is currently owned.
 */
class Mutex {
public:
   /**
    * @brief Constructs a new Mutex with a fresh lock state.
    */
   Mutex() = default;

   Mutex(Mutex const&)                = delete;
   Mutex(Mutex&&) noexcept            = delete;
   Mutex& operator=(Mutex const&)     = delete;
   Mutex& operator=(Mutex&&) noexcept = delete;

   /**
    * @brief Destroys the mutex.
    */
   virtual ~Mutex() = default;

   /**
    * @brief Gets the underlying WPromise<void> for direct inspection or co_await.
    */
   [[nodiscard]] WPromise<void> operator*();

   /**
    * @brief Acquires the mutex for the current coroutine.
    */
   [[nodiscard]] WPromise<void> Lock();

   /**
    * @brief Releases the mutex, allowing other waiters to acquire it.
    */
   void UnLock();

   /**
    * @brief Checks if the mutex is currently owned (locked).
    */
   bool OwnLock() const;

private:
   /** @brief Mutex for synchronizing access to the lock state. */
   mutable std::shared_mutex mutex_;

   /** @brief The current promise representing the lock state. */
   WPromise<void> promise_{WPromise<void>::Resolve()};
   /** @brief Pointer to the resolve function for the current lock. */
   std::shared_ptr<Resolve<void>> resolve_{};
};

/**
 * @brief RAII-style guard for managing Mutex locks.
 *
 * LockGuard releases a lock upon destruction.
 * It supports move semantics but not copy semantics.
 *
 * @note The lock is not acquired upon construction; call co_await lock.Lock() to acquire the mutex.
 */
class LockGuard {
public:
   /** @brief Constructs a deferred lock from a shared lock on the Mutex's mutex. */
   explicit LockGuard(Mutex& mutex);

   LockGuard(LockGuard const&)            = delete;
   LockGuard& operator=(LockGuard const&) = delete;

   /** @brief Move constructor for LockGuard.
    * @param right The LockGuard to move from.
    *
    * @note If this LockGuard currently owns the lock, it will be released before acquiring the new
    * lock. After the move, the source LockGuard will no longer own any lock.
    */
   LockGuard(LockGuard&&) noexcept;
   /** @brief Move assignment operator for LockGuard.
    * @param right The LockGuard to move from.
    *
    * @return A reference to this LockGuard after the move.
    *
    * @note If this LockGuard currently owns the lock, it will be released before acquiring the new
    * lock. After the move, the source LockGuard will no longer own any lock.
    */
   LockGuard& operator=(LockGuard&&) noexcept;

   /** @brief Destroys the LockGuard, releasing the mutex if owned. */
   virtual ~LockGuard();

   /** @brief Acquires the mutex for the current coroutine.
    *
    * @return A promise that resolves once the mutex is acquired.
    */
   [[nodiscard]] WPromise<void> Lock();

   /** @brief Releases the mutex if owned.
    *
    * @note The lock must be acquired before calling this method, otherwise undefined behavior
    * occurs.
    */
   void Unlock();

   /** @brief Checks if the LockGuard currently owns the lock.
    *
    * @return True if the LockGuard owns the lock, false otherwise.
    */
   bool OwnLock() const;

private:
   /** @brief Indicates whether the LockGuard currently owns the lock. */
   bool own_lock_{false};
   /** @brief Pointer to the associated Mutex. */
   Mutex* mutex_;

   friend class Mutex;
};
}  // namespace promise
