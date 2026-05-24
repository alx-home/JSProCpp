#include "../TestCommon.h"

using Matrix    = test_types::Matrix;
using VariantSI = Matrix::VariantStringInt;
using TupleInt  = Matrix::TupleInt;

TEMPLATE_LIST_TEST_CASE(
  "Tuple<int> promise branches are exercised",
  "[Promise][ComplexCoverage]",
  Matrix::PromiseValueTypes
) {
   RunWithTimeout(2s, [&] {
      auto done = Promise<TupleInt>::Resolve(TupleInt{1});
      REQUIRE(done.Resolved());

      auto then_done = done.Then([](TupleInt const& value) { return std::get<0>(value) + 2; });
      REQUIRE(then_done.Resolved());
      REQUIRE(then_done.Value() == 3);

      auto finally_done = done.Finally([] {});
      REQUIRE(finally_done.Resolved());

      auto [pending, resolve, reject] = promise::Create<TupleInt>();
      auto pending_then =
        pending.Then([](TupleInt const& value) { return std::get<0>(value) + 4; });

      pending.WaitAwaited(0);
      REQUIRE((*resolve)(TupleInt{5}));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

      REQUIRE(pending_then.Resolved());
      REQUIRE(pending_then.Value() == 9);

      auto [pending_reject, resolve_reject, reject_reject] = promise::Create<TupleInt>();
      auto recovered = pending_reject.Catch([](TestError const&) { return TupleInt{7}; });
      pending_reject.WaitAwaited(0);
      REQUIRE((*reject_reject)(std::make_exception_ptr(TestError{"tuple reject"})));
      REQUIRE_FALSE((*resolve_reject)(TupleInt{0}));
      REQUIRE(recovered.Resolved());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Variant string-int promise branches are exercised",
  "[Promise][ComplexCoverage]",
  Matrix::PromiseValueTypes
) {
   RunWithTimeout(2s, [&] {
      auto done_value = Promise<VariantSI>::Resolve(VariantSI{7});
      REQUIRE(done_value.Resolved());

      auto then_value = done_value.Then([](VariantSI const& value) {
         return std::holds_alternative<int>(value) ? std::get<int>(value) + 1 : 0;
      });
      REQUIRE(then_value.Resolved());
      REQUIRE(then_value.Value() == 8);

      auto reject_recover =
        Promise<VariantSI>::Reject<TestError>("variant reject").Catch([](TestError const&) {
           return VariantSI{std::string{"recovered"}};
        });
      REQUIRE(reject_recover.Resolved());
      REQUIRE(std::holds_alternative<std::string>(reject_recover.Value()));
      REQUIRE(std::get<std::string>(reject_recover.Value()) == "recovered");

      auto [pending, resolve, reject] = promise::Create<VariantSI>();
      auto pending_then               = pending.Then([](VariantSI const& value) {
         if (std::holds_alternative<std::string>(value)) {
            return std::get<std::string>(value) + "-ok";
         }
         return std::to_string(std::get<int>(value));
      });
      auto pending_finally            = pending.Finally([] {});

      pending.WaitAwaited(0);
      REQUIRE((*resolve)(VariantSI{std::string{"hello"}}));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

      REQUIRE(pending_then.Resolved());
      REQUIRE(pending_then.Value() == "hello-ok");
      REQUIRE(pending_finally.Resolved());

      auto [pending2, resolve2, reject2] = promise::Create<VariantSI>();
      auto pending_catch =
        pending2.Catch([](TestError const& err) { return VariantSI{std::string{err.what()}}; });

      pending2.WaitAwaited(0);
      REQUIRE((*reject2)(std::make_exception_ptr(TestError{"variant pending reject"})));
      REQUIRE_FALSE((*resolve2)(VariantSI{0}));
      REQUIRE(pending_catch.Resolved());
      REQUIRE(std::holds_alternative<std::string>(pending_catch.Value()));
      REQUIRE(std::get<std::string>(pending_catch.Value()) == "variant pending reject");
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Resolver-backed variant and tuple promises",
  "[Promise][ComplexCoverage]",
  Matrix::PromiseValueTypes
) {
   RunWithTimeout(2s, [&] {
      auto resolver_variant = MakePromise(
        [](Resolve<VariantSI> const& resolve, Reject const&) -> Promise<VariantSI, true> {
           REQUIRE(resolve(VariantSI{std::string{"resolver variant"}}));
           co_return;
        }
      );
      REQUIRE(resolver_variant.Resolved());
      REQUIRE(std::holds_alternative<std::string>(resolver_variant.Value()));

      auto resolver_tuple =
        MakePromise([](Resolve<TupleInt> const& resolve, Reject const&) -> Promise<TupleInt, true> {
           REQUIRE(resolve(TupleInt{42}));
           co_return;
        });
      REQUIRE(resolver_tuple.Resolved());

      auto resolver_reject = MakePromise(
        [](Resolve<VariantSI> const&, Reject const& reject) -> Promise<VariantSI, true> {
           reject.Apply<TestError>("resolver complex reject");
           co_return;
        }
      );
      REQUIRE(resolver_reject.Rejected());
      RequireException<TestError>(resolver_reject.Exception());
   });
}
