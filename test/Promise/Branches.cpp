#include "../TestCommon.h"

using Matrix = test_types::Matrix;

TEMPLATE_LIST_TEST_CASE(
  "Catch pass-through preserves resolved values across return kinds",
  "[Promise][Branches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto resolved_int_with_void_catch =
        Promise<T>::Resolve(test_types::ValueFromInt<T>(33)).Catch([](TestError const&) {});

      REQUIRE(resolved_int_with_void_catch.Resolved());
      REQUIRE(resolved_int_with_void_catch.Value().has_value());
      REQUIRE(resolved_int_with_void_catch.Value().value() == test_types::ValueFromInt<T>(33));
   });
}

TEST_CASE(
  "Catch pass-through on resolved void promise ignores value handler",
  "[Promise][Branches]"
) {
   RunWithTimeout(2s, [&] {
      bool catch_called = false;
      auto resolved_void_with_value_catch =
        Promise<void>::Resolve().Catch([&catch_called](TestError const&) {
           catch_called = true;
           return 77;
        });

      REQUIRE(resolved_void_with_value_catch.Resolved());
      REQUIRE_FALSE(catch_called);
      REQUIRE_FALSE(resolved_void_with_value_catch.Value().has_value());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Catch async typed exception keeps exception alive across suspension",
  "[Promise][Branches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [gate, resolve, reject] = promise::Create<void>();

      auto recovered = [&]() {
         auto rejected = Promise<T>::template Reject<TestError>("typed async")
                           .Catch([&](TestError const& exception) -> Promise<Matrix::String> {
                              co_await gate;
                              co_return Matrix::String{exception.what()};
                           });

         if constexpr (std::is_same_v<T, Matrix::String>) {
            return std::move(rejected).Then([](Matrix::String const& value) { return value; });
         } else if constexpr (std::is_same_v<T, Matrix::VariantStringInt>) {
            return std::move(rejected).Then([](test_types::VariantWithString<T> const& value) {
               REQUIRE(std::holds_alternative<Matrix::String>(value));
               return std::get<Matrix::String>(value);
            });
         } else {
            return std::move(rejected).Then([](test_types::VariantWithString<T> const& value) {
               REQUIRE(std::holds_alternative<Matrix::String>(value));
               return std::get<Matrix::String>(value);
            });
         }
      }();

      REQUIRE_FALSE(recovered.Done());
      REQUIRE((*resolve)());
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

      REQUIRE(recovered.Resolved());
      REQUIRE(recovered.Value() == "typed async");
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Catch with exception_ptr async recovery resolves expected value",
  "[Promise][Branches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto recovered = Promise<T>::template Reject<TestError>("ptr async")
                         .Catch([](std::exception_ptr exception) -> Promise<T> {
                            RequireException<TestError>(exception);
                            co_return test_types::ValueFromInt<T>(909);
                         });

      REQUIRE(recovered.Resolved());
      REQUIRE(recovered.Value() == test_types::ValueFromInt<T>(909));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Finally async failure overrides previous rejection",
  "[Promise][Branches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p = Promise<T>::template Reject<TestError>("initial").Finally([]() -> Promise<void> {
         throw FinallyError("finally async fail");
         co_return;
      });

      REQUIRE(p.Rejected());
      RequireException<FinallyError>(p.Exception());
   });
}

TEST_CASE(
  "Then on rejected lvalue void promise with async continuation keeps rejection",
  "[Promise][Branches]"
) {
   RunWithTimeout(2s, [&] {
      auto rejected_source = Promise<void>::Reject<TestError>("then rejected lvalue");

      auto chained = rejected_source.Then([]() -> Promise<int> { co_return 1; });

      REQUIRE(chained.Rejected());
      RequireException<TestError>(chained.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Finally on resolved lvalue value promise with async continuation preserves value",
  "[Promise][Branches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto resolved_source = Promise<T>::Resolve(test_types::ValueFromInt<T>(55));

      auto chained = resolved_source.Finally([]() -> Promise<void> { co_return; });

      REQUIRE(chained.Resolved());
      REQUIRE(chained.Value() == test_types::ValueFromInt<T>(55));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Catch on rejected lvalue value promise with void handler resolves empty optional",
  "[Promise][Branches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto rejected_source = Promise<T>::template Reject<TestError>("catch void lvalue");

      auto recovered = rejected_source.Catch([](TestError const&) {});

      REQUIRE(recovered.Resolved());
      REQUIRE_FALSE(recovered.Value().has_value());
   });
}
