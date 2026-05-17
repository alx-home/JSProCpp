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
   static bool await_ready(promise::details::Promise<T, WITH_RESOLVER>& promise) {
      return promise.await_ready();
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
};
}  // namespace promise

using TestHelper = promise::Test;
