#include "../TestCommon.h"

using Matrix = test_types::Matrix;

TEMPLATE_LIST_TEST_CASE(
  "Race covers void and optional branches",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [pending_value, resolve_value, reject_value] = promise::Create<T>();
      auto raced_optional = promise::Race(Promise<void>::Resolve(), pending_value);
      REQUIRE(raced_optional.Resolved());
      REQUIRE_FALSE(raced_optional.Value().has_value());
      REQUIRE((*resolve_value)(test_types::ValueFromInt<T>(15)));
      REQUIRE_FALSE((*reject_value)(std::make_exception_ptr(TestError{"ignored"})));

      auto [pending_void, resolve_void, reject_void] = promise::Create<void>();
      auto raced_void = promise::Race(Promise<void>::Resolve(), pending_void);
      REQUIRE(raced_void.Resolved());
      REQUIRE((*resolve_void)());
      REQUIRE_FALSE((*reject_void)(std::make_exception_ptr(TestError{"ignored"})));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Catch with promise returning handlers preserves passthrough",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto void_source =
        Promise<void>::Resolve().Catch([](TestError const&) -> Promise<void> { co_return; });
      REQUIRE(void_source.Resolved());

      auto value_source = Promise<T>::Resolve(test_types::ValueFromInt<T>(66))
                            .Catch([](TestError const&) -> Promise<void> { co_return; });
      REQUIRE(value_source.Resolved());
      REQUIRE(value_source.Value().has_value());
      REQUIRE(value_source.Value().value() == test_types::ValueFromInt<T>(66));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Finally with promise returning handlers preserves values and exceptions",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto resolved_value =
        Promise<T>::Resolve(test_types::ValueFromInt<T>(5)).Finally([]() -> Promise<void> {
           co_return;
        });
      REQUIRE(resolved_value.Resolved());
      REQUIRE(resolved_value.Value() == test_types::ValueFromInt<T>(5));

      auto resolved_void = Promise<void>::Resolve().Finally([]() -> Promise<void> { co_return; });
      REQUIRE(resolved_void.Resolved());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Catch rejected paths produce expected optional shapes",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto rejected_value_to_void =
        Promise<T>::template Reject<TestError>("drop value").Catch([](TestError const&) {});
      REQUIRE(rejected_value_to_void.Resolved());
      REQUIRE_FALSE(rejected_value_to_void.Value().has_value());

      auto rejected_void_to_value =
        Promise<void>::Reject<TestError>("supply value").Catch([](TestError const&) {
           return test_types::ValueFromInt<T>(303);
        });
      REQUIRE(rejected_void_to_value.Resolved());
      REQUIRE(rejected_void_to_value.Value().has_value());
      REQUIRE(rejected_void_to_value.Value().value() == test_types::ValueFromInt<T>(303));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "WPromise create and v detach type erased paths",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [created, resolve, reject] = WPromise<T>::Create();
      REQUIRE_FALSE(created.Done());
      REQUIRE((*resolve)(test_types::ValueFromInt<T>(404)));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(created.Resolved());
      REQUIRE(created.Value() == test_types::ValueFromInt<T>(404));

      auto [pending, resolve_pending, reject_pending] = WPromise<T>::Create();
      auto erased = std::move(pending).template ToPointer<promise::VPromise>();
      std::move(*erased).VDetach();
      REQUIRE((*resolve_pending)(test_types::ValueFromInt<T>(505)));
      REQUIRE_FALSE((*reject_pending)(std::make_exception_ptr(TestError{"ignored"})));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Member race covers void target and rejection forwarding",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [void_race, resolve_void_race, reject_void_race] = promise::Create<void>();
      auto source_value = Promise<T>::Resolve(test_types::ValueFromInt<T>(1));
      auto race_void_result =
        source_value.Race(std::move(void_race), resolve_void_race, reject_void_race);
      REQUIRE(race_void_result.Resolved());
      REQUIRE_FALSE((*resolve_void_race)());
      REQUIRE_FALSE((*reject_void_race)(std::make_exception_ptr(TestError{"ignored"})));

      auto [pending_race, resolve_pending_race, reject_pending_race] = promise::Create<T>();
      auto source_rejected = Promise<T>::template Reject<TestError>("race reject");
      auto race_rejected_result =
        source_rejected.Race(std::move(pending_race), resolve_pending_race, reject_pending_race);
      REQUIRE(race_rejected_result.Rejected());
      RequireException<TestError>(race_rejected_result.Exception());
      REQUIRE_FALSE((*resolve_pending_race)(test_types::ValueFromInt<T>(66)));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Pending then promise continuation propagates exception",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [source, resolve, reject] = promise::Create<T>();
      auto chained                   = source.Then([](T) -> Promise<T> {
         throw CatchError("then async failure");
         co_return test_types::ValueFromInt<T>(0);
      });
      source.WaitAwaited(0);
      REQUIRE((*resolve)(test_types::ValueFromInt<T>(7)));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(chained.Rejected());
      RequireException<CatchError>(chained.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Pending then sync continuation keeps rejection from source",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [source, resolve, reject] = promise::Create<T>();
      auto chained = source.Then([](T const& value) { return test_types::BumpValue(value, 1); });
      source.WaitAwaited(0);
      REQUIRE((*reject)(std::make_exception_ptr(TestError{"pending then reject"})));
      REQUIRE_FALSE((*resolve)(test_types::ValueFromInt<T>(10)));
      REQUIRE(chained.Rejected());
      RequireException<TestError>(chained.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Pending catch handler throwing turns into rejection",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [source, resolve, reject] = promise::Create<T>();
      auto recovered =
        source.Catch([](TestError const&) -> T { throw CatchError("catch sync failure"); });
      source.WaitAwaited(0);
      REQUIRE((*reject)(std::make_exception_ptr(TestError{"trigger"})));
      REQUIRE_FALSE((*resolve)(test_types::ValueFromInt<T>(12)));
      REQUIRE(recovered.Rejected());
      RequireException<CatchError>(recovered.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Pending catch async handler rejection propagates",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [source, resolve, reject] = promise::Create<T>();
      auto recovered                 = source.Catch([](TestError const&) -> Promise<T> {
         throw CatchError("catch async failure");
         co_return test_types::ValueFromInt<T>(0);
      });
      source.WaitAwaited(0);
      REQUIRE((*reject)(std::make_exception_ptr(TestError{"trigger async"})));
      REQUIRE_FALSE((*resolve)(test_types::ValueFromInt<T>(42)));
      REQUIRE(recovered.Rejected());
      RequireException<CatchError>(recovered.Exception());
   });
}
