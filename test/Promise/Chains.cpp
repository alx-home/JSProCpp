#include "../TestCommon.h"

using Matrix = test_types::Matrix;

TEMPLATE_LIST_TEST_CASE(
  "co_await propagates promise exceptions like the old driver",
  "[Promise][Chains]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p = WPromise{[]() -> Promise<T> {
         try {
            co_await WPromise{[]() -> Promise<void> {
               throw TestError("TEST_EXCEPTION");
               co_return;
            }};
         } catch (TestError const&) {
            co_return test_types::ValueFromInt<T>(101);
         }

         co_return test_types::ValueFromInt<T>(-1);
      }};

      REQUIRE(p.Resolved());
      REQUIRE(p.Value() == test_types::ValueFromInt<T>(101));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Direct WPromise constructors cover resolve reject and throw paths",
  "[Promise][Chains]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      WPromise resolved{[]() -> T { return test_types::ValueFromInt<T>(42); }};
      WPromise resolved_with_resolver{[](Resolve<T> const& resolve) {
         REQUIRE(resolve(test_types::ValueFromInt<T>(42)));
      }};

      auto rejected = WPromise{[](Resolve<T> const&, Reject const& reject) {
                         reject.Apply<TestError>("reject path");
                      }}.Catch([](TestError const&) { return test_types::ValueFromInt<T>(77); });

      auto throwing_resolver =
        WPromise{[](Resolve<T> const&) {
           throw TestError("resolver throw");
        }}.Catch([](TestError const&) { return test_types::ValueFromInt<T>(88); });

      auto throwing_plain =
        WPromise{[]() -> T { throw TestError("plain throw"); }}.Catch([](TestError const&) {
           return test_types::ValueFromInt<T>(99);
        });

      REQUIRE(resolved.Resolved());
      REQUIRE(resolved.Value() == test_types::ValueFromInt<T>(42));

      REQUIRE(resolved_with_resolver.Resolved());
      REQUIRE(resolved_with_resolver.Value() == test_types::ValueFromInt<T>(42));

      REQUIRE(rejected.Resolved());
      REQUIRE(rejected.Value() == test_types::ValueFromInt<T>(77));

      REQUIRE(throwing_resolver.Resolved());
      REQUIRE(throwing_resolver.Value() == test_types::ValueFromInt<T>(88));

      REQUIRE(throwing_plain.Resolved());
      REQUIRE(throwing_plain.Value() == test_types::ValueFromInt<T>(99));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Resolver-style WPromise can be resolved after dependent chains are created",
  "[Promise][Chains]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      std::shared_ptr<Resolve<T> const> resolver;
      std::shared_ptr<Reject const>     rejecter;

      WPromise prom{
        [&resolver,
         &rejecter](Resolve<T> const& resolve, Reject const& reject) -> Promise<T, true> {
           resolver = resolve.shared_from_this();
           rejecter = reject.shared_from_this();
           co_return;
        }
      };

      auto prom2 = WPromise{[&]() -> Promise<T> {
         auto const result = test_types::BumpValue<T>((co_await prom), 1);
         co_return result;
      }};

      REQUIRE(resolver);
      REQUIRE(rejecter);
      REQUIRE((*resolver)(test_types::ValueFromInt<T>(5)));
      REQUIRE(prom2.Resolved());
      REQUIRE(prom2.Value() == test_types::BumpValue<T>(test_types::ValueFromInt<T>(5), 1));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Asynchronous Catch can await another promise before recovering",
  "[Promise][Chains]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [gate, resolve, reject] = promise::Create<void>();

      auto p = WPromise{[]() -> Promise<void> {
                  throw TestError("test async");
                  co_return;
               }}
                 .Catch([&](TestError const&) -> Promise<T> {
                    co_await gate;
                    co_return test_types::ValueFromInt<T>(123);
                 })
                 .Then([](std::optional<T> const& value) { return value.value(); });

      REQUIRE_FALSE(p.Done());
      REQUIRE((*resolve)());
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

      REQUIRE(p.Done());

      REQUIRE(p.Resolved());
      REQUIRE(p.Value() == test_types::ValueFromInt<T>(123));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "All combines non-void results and skips void",
  "[Promise][Chains]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto all = promise::All(
        Promise<T>::Resolve(test_types::ValueFromInt<T>(4)),
        Promise<void>::Resolve(),
        Promise<T>::Resolve(test_types::ValueFromInt<T>(9))
      );

      REQUIRE(all.Resolved());

      auto const [first, second] = all.Value();
      REQUIRE(first == test_types::ValueFromInt<T>(4));
      REQUIRE(second == test_types::ValueFromInt<T>(9));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "All rejects when any member rejects",
  "[Promise][Chains]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto all = promise::All(
        Promise<T>::Resolve(test_types::ValueFromInt<T>(4)),
        Promise<T>::template Reject<TestError>("boom")
      );

      REQUIRE(all.Rejected());
      RequireException<TestError>(all.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "All accepts a container of non-void promises",
  "[Promise][Chains]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      std::vector<WPromise<T>> promises{};
      promises.emplace_back([]() -> Promise<T> { co_return test_types::ValueFromInt<T>(3); });
      promises.emplace_back([]() -> Promise<T> { co_return test_types::ValueFromInt<T>(7); });
      promises.emplace_back([]() -> Promise<T> { co_return test_types::ValueFromInt<T>(11); });

      auto all = promise::All(std::move(promises));

      REQUIRE(all.Resolved());
      REQUIRE(
        all.Value()
        == std::vector<T>{
          test_types::ValueFromInt<T>(3),
          test_types::ValueFromInt<T>(7),
          test_types::ValueFromInt<T>(11)
        }
      );
   });
}

TEMPLATE_LIST_TEST_CASE(
  "All accepts a container of void promises",
  "[Promise][Chains]",
  Matrix::PromiseValueTypes
) {
   RunWithTimeout(2s, [&] {
      std::vector<WPromise<void>> promises{};
      promises.emplace_back([]() -> Promise<void> { co_return; });
      promises.emplace_back([]() -> Promise<void> { co_return; });

      auto all = promise::All(std::move(promises));

      REQUIRE(all.Resolved());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "All container overload rejects when one promise rejects",
  "[Promise][Chains]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      std::vector<WPromise<T>> promises{};
      promises.emplace_back([]() -> Promise<T> { co_return test_types::ValueFromInt<T>(2); });
      promises.emplace_back([]() -> Promise<T> {
         throw TestError("container boom");
         co_return test_types::ValueFromInt<T>(0);
      });
      promises.emplace_back([]() -> Promise<T> { co_return test_types::ValueFromInt<T>(5); });

      auto all = promise::All(std::move(promises));

      REQUIRE(all.Rejected());
      RequireException<TestError>(all.Exception());
   });
}
