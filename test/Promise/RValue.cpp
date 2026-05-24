#include "../TestCommon.h"

using Matrix = test_types::Matrix;

TEMPLATE_LIST_TEST_CASE(
  "Rvalue Then Catch Finally paths are exercised",
  "[Promise][RValue]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto then_rvalue =
        Promise<T>::Resolve(test_types::ValueFromInt<T>(5)).Then([](T const& value) {
           return test_types::BumpValue<T>(value, 1);
        });
      REQUIRE(then_rvalue.Resolved());
      REQUIRE(then_rvalue.Value() == test_types::BumpValue<T>(test_types::ValueFromInt<T>(5), 1));

      auto catch_rvalue =
        Promise<T>::template Reject<TestError>("rvalue catch").Catch([](TestError const&) {
           return test_types::ValueFromInt<T>(77);
        });
      REQUIRE(catch_rvalue.Resolved());
      REQUIRE(catch_rvalue.Value() == test_types::ValueFromInt<T>(77));

      bool finally_called = false;
      auto finally_rvalue =
        Promise<T>::Resolve(test_types::ValueFromInt<T>(8)).Finally([&finally_called]() {
           finally_called = true;
        });
      REQUIRE(finally_rvalue.Resolved());
      REQUIRE(finally_rvalue.Value() == test_types::ValueFromInt<T>(8));
      REQUIRE(finally_called);
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Member Race handles done race promise and done source promise",
  "[Promise][RValue]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [already_done_race, resolve_done_race, reject_done_race] = promise::Create<T>();
      REQUIRE((*resolve_done_race)(test_types::ValueFromInt<T>(41)));
      REQUIRE_FALSE((*reject_done_race)(std::make_exception_ptr(TestError{"ignored"})));

      auto source_done = Promise<T>::Resolve(test_types::ValueFromInt<T>(99));
      auto winner1 =
        source_done.Race(std::move(already_done_race), resolve_done_race, reject_done_race);
      REQUIRE(winner1.Resolved());
      REQUIRE(winner1.Value() == test_types::ValueFromInt<T>(41));

      auto [pending_race, resolve_pending_race, reject_pending_race] = promise::Create<T>();
      auto source_done_2 = Promise<T>::Resolve(test_types::ValueFromInt<T>(123));
      auto winner2 =
        source_done_2.Race(std::move(pending_race), resolve_pending_race, reject_pending_race);

      REQUIRE(winner2.Resolved());
      REQUIRE(winner2.Value() == test_types::ValueFromInt<T>(123));
      REQUIRE_FALSE((*resolve_pending_race)(test_types::ValueFromInt<T>(77)));
      REQUIRE_FALSE((*reject_pending_race)(std::make_exception_ptr(TestError{"ignored"})));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Member Race unregisters awaiter when race promise completes first",
  "[Promise][RValue]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [pending_source, resolve_source, reject_source] = promise::Create<T>();
      auto [pending_race, resolve_race, reject_race]       = promise::Create<T>();

      auto winner = pending_source.Race(std::move(pending_race), resolve_race, reject_race);

      REQUIRE((*resolve_race)(test_types::ValueFromInt<T>(555)));
      REQUIRE_FALSE((*reject_race)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(winner.Resolved());
      REQUIRE(winner.Value() == test_types::ValueFromInt<T>(555));

      REQUIRE((*resolve_source)(test_types::ValueFromInt<T>(10)));
      REQUIRE_FALSE((*reject_source)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(winner.Resolved());
      REQUIRE(winner.Value() == test_types::ValueFromInt<T>(555));
   });
}
