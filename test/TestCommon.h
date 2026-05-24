#pragma once

#if defined(__clang__)
#   pragma clang diagnostic ignored "-Wc2y-extensions"
#endif

#include <catch2/catch_test_macros.hpp>

#include <promise/CVPromise.h>
#include <promise/MessageQueue.h>
#include <promise/Pool.h>
#include <promise/StatePromise.h>
#include <promise/promise.h>

#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;

struct TestError : std::runtime_error {
   using std::runtime_error::runtime_error;
};

struct CatchError : std::runtime_error {
   using std::runtime_error::runtime_error;
};

struct FinalThenError : std::runtime_error {
   using std::runtime_error::runtime_error;
};

struct FinallyError : std::runtime_error {
   using std::runtime_error::runtime_error;
};

template <class EXCEPTION>
void
RequireException(std::exception_ptr const& exception) {
   REQUIRE(exception != nullptr);
   REQUIRE_THROWS_AS(std::rethrow_exception(exception), EXCEPTION);
}

template <class FUN>
void
RunWithTimeout(std::chrono::milliseconds timeout, FUN&& fun) {
   std::promise<void> done;
   auto               future = done.get_future();
   std::exception_ptr test_exception{};

   std::thread worker([&] {
      try {
         std::invoke(std::forward<FUN>(fun));
      } catch (...) {
         test_exception = std::current_exception();
      }
      done.set_value();
   });

   if (future.wait_for(timeout) == std::future_status::timeout) {
      worker.detach();
      FAIL("Test timed out");
      return;
   }

   worker.join();
   if (test_exception) {
      std::rethrow_exception(test_exception);
   }
}

inline Promise<int>
CoroutineValue(int value) {
   co_return value;
}

inline Promise<void>
CoroutineVoid() {
   co_return;
}

namespace promise {
struct Test {
   template <class T, bool WITH_RESOLVER>
   using handle_type = typename details::Promise<T, WITH_RESOLVER>::handle_type;

   template <class T, bool WITH_RESOLVER>
   using promise_type = promise::details::Promise<T, WITH_RESOLVER>;

   template <class T, bool WITH_RESOLVER>
   static std::shared_ptr<promise::details::Promise<T, WITH_RESOLVER>> Create() {
      struct MakeUniqueFriend : details::Promise<T, WITH_RESOLVER> {
         MakeUniqueFriend(details::Promise<T, WITH_RESOLVER>::handle_type handle)
            : details::Promise<T, WITH_RESOLVER>(std::move(handle)) {}
      };

      auto promise = std::make_shared<MakeUniqueFriend>(handle_type<T, WITH_RESOLVER>{});

      auto [resolver, resolve, reject] = promise::Resolver<T>::Create();

      if constexpr (!WITH_RESOLVER) {
         resolver->resolved_ = true;
         if constexpr (std::is_void_v<T>) {
            resolver->value_is_set_ = true;
         } else {
            resolver->value_ = std::make_unique<T>();
         }
      }

      resolver->promise_ = promise;
      promise->resolver_ = std::move(resolver);

      return promise;
   }

   template <class T, bool WITH_RESOLVER>
   static std::shared_ptr<promise::details::Promise<T, WITH_RESOLVER>> CreateWithoutResolver() {
      struct MakeUniqueFriend : details::Promise<T, WITH_RESOLVER> {
         MakeUniqueFriend(details::Promise<T, WITH_RESOLVER>::handle_type handle)
            : details::Promise<T, WITH_RESOLVER>(std::move(handle)) {}
      };

      return std::make_shared<MakeUniqueFriend>(handle_type<T, WITH_RESOLVER>{});
   }

   template <class T, bool WITH_RESOLVER>
   static std::shared_ptr<promise::details::Promise<T, WITH_RESOLVER>> CreatePending() {
      auto promise                   = Create<T, WITH_RESOLVER>();
      promise->resolver_->resolved_  = false;
      promise->resolver_->exception_ = nullptr;

      if constexpr (std::is_void_v<T>) {
         promise->resolver_->value_is_set_ = false;
      } else {
         promise->resolver_->value_.reset();
      }

      return promise;
   }

   template <class T, bool WITH_RESOLVER>
   static std::size_t RegisterAwaitFunction(
     promise::details::Promise<T, WITH_RESOLVER>& promise,
     std::function<void()>                        fun
   ) {
      std::unique_lock lock{promise.mutex_};
      return promise.Await(std::move(fun), lock);
   }

   template <class T, bool WITH_RESOLVER>
   static bool
   UnregisterAwaitFunction(promise::details::Promise<T, WITH_RESOLVER>& promise, std::size_t id) {
      std::unique_lock lock{promise.mutex_};
      return promise.UnAwait(id, lock);
   }

   template <class T, bool WITH_RESOLVER>
   static std::size_t RegisterAwaitFunctionWithLockGuard(
     promise::details::Promise<T, WITH_RESOLVER>& promise,
     std::function<void()>                        fun
   ) {
      std::lock_guard lock{promise.mutex_};
      return promise.Await(std::move(fun), lock);
   }

   template <class T, bool WITH_RESOLVER>
   static bool UnregisterAwaitFunctionWithLockGuard(
     promise::details::Promise<T, WITH_RESOLVER>& promise,
     std::size_t                                  id
   ) {
      std::lock_guard lock{promise.mutex_};
      return promise.UnAwait(id, lock);
   }

   template <class T, bool WITH_RESOLVER>
   static void AwaitSuspendWithLockGuard(
     promise::details::Promise<T, WITH_RESOLVER>& promise,
     std::coroutine_handle<>                      h
   ) {
      std::lock_guard lock{promise.mutex_};
      promise.Await(h, lock);
   }

   template <class T, bool WITH_RESOLVER>
   static std::size_t Awaiters(promise::details::Promise<T, WITH_RESOLVER> const& promise) {
      return promise.Awaiters();
   }

   template <class T, bool WITH_RESOLVER>
   static std::size_t UseCount(promise::details::Promise<T, WITH_RESOLVER> const& promise) {
      return promise.UseCount();
   }

   template <class T, bool WITH_RESOLVER>
   static bool IsResolved(promise::details::Promise<T, WITH_RESOLVER> const& promise) {
      std::shared_lock lock{promise.mutex_};
      return promise.IsResolved(lock);
   }

   template <class T, bool WITH_RESOLVER, class VALUE>
      requires(!std::is_void_v<T>)
   static bool Resolve(promise::details::Promise<T, WITH_RESOLVER>& promise, VALUE&& value) {
      return promise.resolver_->Resolve(std::forward<VALUE>(value));
   }

   template <class T, bool WITH_RESOLVER>
      requires(std::is_void_v<T>)
   static bool Resolve(promise::details::Promise<T, WITH_RESOLVER>& promise) {
      return promise.resolver_->Resolve();
   }

   template <class T, bool WITH_RESOLVER>
   static bool
   Reject(promise::details::Promise<T, WITH_RESOLVER>& promise, std::exception_ptr exception) {
      return promise.resolver_->Reject(std::move(exception));
   }

   template <class T, bool WITH_RESOLVER>
   static bool await_ready(promise::details::Promise<T, WITH_RESOLVER>& promise) {
      return promise.await_ready();
   }

   template <class T, bool WITH_RESOLVER>
   static void InitSuspendAwaitResume(promise::details::Promise<T, WITH_RESOLVER>& promise) {
      using InitSuspend = typename promise_type<T, WITH_RESOLVER>::PromiseType::InitSuspend;
      InitSuspend init{.self_ = promise};
      init.await_resume();
   }

   template <class T, bool WITH_RESOLVER>
   static auto GetParentFromPromiseType(promise::details::Promise<T, WITH_RESOLVER>& promise) ->
     typename promise_type<T, WITH_RESOLVER>::PromiseType::Parent* {
      auto* promise_type = promise.handle_ ? &promise.handle_.promise() : nullptr;
      return promise_type ? promise_type->GetParent() : nullptr;
   }

   template <class T, bool WITH_RESOLVER>
   static bool InitSuspendAwaitReady(promise::details::Promise<T, WITH_RESOLVER>& promise) {
      using InitSuspend = typename promise_type<T, WITH_RESOLVER>::PromiseType::InitSuspend;
      InitSuspend init{.self_ = promise};
      return init.await_ready();
   }

   template <class T, bool WITH_RESOLVER>
   static void InitSuspendAwaitSuspend(
     promise::details::Promise<T, WITH_RESOLVER>& promise,
     std::coroutine_handle<>                      h
   ) {
      using InitSuspend = typename promise_type<T, WITH_RESOLVER>::PromiseType::InitSuspend;
      InitSuspend init{.self_ = promise};
      init.await_suspend(h);
   }

   template <class T, bool WITH_RESOLVER>
   static bool
   await_suspend(promise::details::Promise<T, WITH_RESOLVER>& promise, std::coroutine_handle<> h) {
      return promise.await_suspend(h);
   }

   template <class T, bool WITH_RESOLVER>
   static auto await_resume(promise::details::Promise<T, WITH_RESOLVER>& promise) {
      return promise.await_resume();
   }

   template <class T, bool WITH_RESOLVER, class FUN, class... ARGS>
   static auto
   Then(promise::details::Promise<T, WITH_RESOLVER>& promise, FUN&& func, ARGS&&... args) {
      return promise.Then(std::function{std::forward<FUN>(func)}, std::forward<ARGS>(args)...);
   }

   template <class T, bool WITH_RESOLVER, class FUN, class... ARGS>
   static auto
   Catch(promise::details::Promise<T, WITH_RESOLVER>& promise, FUN&& func, ARGS&&... args) {
      return promise.Catch(std::function{std::forward<FUN>(func)}, std::forward<ARGS>(args)...);
   }

   template <class T, bool WITH_RESOLVER, class FUN>
   static auto Finally(promise::details::Promise<T, WITH_RESOLVER>& promise, FUN&& func) {
      return promise.Finally(std::function{std::forward<FUN>(func)});
   }
};
}  // namespace promise

using TestHelper = promise::Test;
