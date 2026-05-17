#include "../TestCommon.h"

SCENARIO("All with void promises covers void tuple_cat branches", "[Promise][Scenario]") {
   GIVEN("a mix of void and non-void promises") {
      WHEN("all resolve including void") {
         RunWithTimeout(2s, [&] {
            auto all = promise::All(
              []() -> Promise<void> { co_return; },
              []() -> Promise<int> { co_return 7; },
              []() -> Promise<void> { co_return; }
            );

            all.WaitDone();
            REQUIRE(all.Resolved());
            REQUIRE(std::get<0>(all.Value()) == 7);
         });
      }

      WHEN("void-only All resolves to empty tuple") {
         RunWithTimeout(2s, [&] {
            auto all = promise::All(
              []() -> Promise<void> { co_return; }, []() -> Promise<void> { co_return; }
            );

            all.WaitDone();
            REQUIRE(all.Resolved());
         });
      }
   }
}

SCENARIO(
  "Catch pass-through on resolved sources across callback return kinds",
  "[Promise][Scenario]"
) {
   GIVEN("an already-resolved Promise<int>") {
      WHEN("catch callback returns Promise<int>") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p = Promise<int>::Resolve(5).Catch([&called](TestError const&) -> Promise<int> {
               called = true;
               co_return 1;
            });

            p.WaitDone();
            REQUIRE(p.Resolved());
            REQUIRE_FALSE(called);
         });
      }

      WHEN("catch callback returns Promise<void>") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p = Promise<int>::Resolve(6).Catch([&called](TestError const&) -> Promise<void> {
               called = true;
               co_return;
            });

            p.WaitDone();
            REQUIRE(p.Resolved());
            REQUIRE_FALSE(called);
         });
      }

      WHEN("catch callback takes exception_ptr and returns Promise<int>") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p =
              Promise<int>::Resolve(7).Catch([&called](std::exception_ptr const&) -> Promise<int> {
                 called = true;
                 co_return 2;
              });

            p.WaitDone();
            REQUIRE(p.Resolved());
            REQUIRE_FALSE(called);
         });
      }

      WHEN("catch callback returns void") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p = Promise<int>::Resolve(8).Catch([&called](TestError const&) { called = true; });

            p.WaitDone();
            REQUIRE(p.Resolved());
            REQUIRE_FALSE(called);
         });
      }
   }

   GIVEN("an already-resolved Promise<void>") {
      WHEN("catch callback returns Promise<int>") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p = Promise<void>::Resolve().Catch([&called](TestError const&) -> Promise<int> {
               called = true;
               co_return 3;
            });

            p.WaitDone();
            REQUIRE(p.Resolved());
            REQUIRE_FALSE(called);
         });
      }

      WHEN("catch callback returns Promise<void>") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p = Promise<void>::Resolve().Catch([&called](TestError const&) -> Promise<void> {
               called = true;
               co_return;
            });

            p.WaitDone();
            REQUIRE(p.Resolved());
            REQUIRE_FALSE(called);
         });
      }

      WHEN("catch callback takes exception_ptr and returns value") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p      = Promise<void>::Resolve().Catch([&called](std::exception_ptr const&) {
               called = true;
               return 4;
            });

            p.WaitDone();
            REQUIRE(p.Resolved());
            REQUIRE_FALSE(called);
         });
      }
   }
}

SCENARIO("Catch pass-through on pending sources resolved later", "[Promise][Scenario]") {
   GIVEN("a pending Promise<int>") {
      WHEN("catch callback returns Promise<int>") {
         RunWithTimeout(2s, [&] {
            auto [source, resolve, reject] = promise::Create<int>();
            bool called                    = false;

            auto caught = std::move(source).Catch([&called](TestError const&) -> Promise<int> {
               called = true;
               co_return 10;
            });

            REQUIRE((*resolve)(9));
            REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

            caught.WaitDone();
            REQUIRE(caught.Resolved());
            REQUIRE_FALSE(called);
         });
      }

      WHEN("catch callback returns Promise<void>") {
         RunWithTimeout(2s, [&] {
            auto [source, resolve, reject] = promise::Create<int>();
            bool called                    = false;

            auto caught =
              std::move(source).Catch([&called](std::exception_ptr const&) -> Promise<void> {
                 called = true;
                 co_return;
              });

            REQUIRE((*resolve)(11));
            REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

            caught.WaitDone();
            REQUIRE(caught.Resolved());
            REQUIRE_FALSE(called);
         });
      }
   }

   GIVEN("a pending Promise<void>") {
      WHEN("catch callback returns Promise<int>") {
         RunWithTimeout(2s, [&] {
            auto [source, resolve, reject] = promise::Create<void>();
            bool called                    = false;

            auto caught = std::move(source).Catch([&called](TestError const&) -> Promise<int> {
               called = true;
               co_return 12;
            });

            REQUIRE((*resolve)());
            REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

            caught.WaitDone();
            REQUIRE(caught.Resolved());
            REQUIRE_FALSE(called);
         });
      }

      WHEN("catch callback returns void") {
         RunWithTimeout(2s, [&] {
            auto [source, resolve, reject] = promise::Create<void>();
            bool called                    = false;

            auto caught =
              std::move(source).Catch([&called](std::exception_ptr const&) { called = true; });

            REQUIRE((*resolve)());
            REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

            caught.WaitDone();
            REQUIRE(caught.Resolved());
            REQUIRE_FALSE(called);
         });
      }
   }
}

SCENARIO(
  "Catch callbacks throwing or rejecting propagate as rejected result",
  "[Promise][Scenario]"
) {
   GIVEN("a rejected Promise<int>") {
      WHEN("Promise-returning catch callback throws before producing value") {
         RunWithTimeout(2s, [&] {
            auto p = Promise<int>::Reject<TestError>("throw from catch")
                       .Catch([](TestError const&) -> Promise<int> {
                          throw CatchError{"catch promise throw int"};
                          co_return 0;
                       });

            p.WaitDone();
            REQUIRE(p.Rejected());
            RequireException<CatchError>(p.Exception());
         });
      }

      WHEN("Promise<void>-returning catch callback throws") {
         RunWithTimeout(2s, [&] {
            auto p = Promise<int>::Reject<TestError>("throw from catch void")
                       .Catch([](TestError const&) -> Promise<void> {
                          throw CatchError{"catch promise throw void"};
                          co_return;
                       });

            p.WaitDone();
            REQUIRE(p.Rejected());
            RequireException<CatchError>(p.Exception());
         });
      }

      WHEN("void catch callback throws") {
         RunWithTimeout(2s, [&] {
            auto p =
              Promise<int>::Reject<TestError>("throw from catch sync").Catch([](TestError const&) {
                 throw CatchError{"catch sync throw int"};
              });

            p.WaitDone();
            REQUIRE(p.Rejected());
            RequireException<CatchError>(p.Exception());
         });
      }
   }

   GIVEN("a rejected Promise<void>") {
      WHEN("value-returning catch callback throws") {
         RunWithTimeout(2s, [&] {
            auto p = Promise<void>::Reject<TestError>("void throw from catch")
                       .Catch([](std::exception_ptr const&) -> int {
                          throw CatchError{"catch sync throw void source"};
                       });

            p.WaitDone();
            REQUIRE(p.Rejected());
            RequireException<CatchError>(p.Exception());
         });
      }

      WHEN("Promise-returning catch callback throws") {
         RunWithTimeout(2s, [&] {
            auto p = Promise<void>::Reject<TestError>("void promise throw")
                       .Catch([](TestError const&) -> Promise<int> {
                          throw CatchError{"catch promise throw void source"};
                          co_return 0;
                       });

            p.WaitDone();
            REQUIRE(p.Rejected());
            RequireException<CatchError>(p.Exception());
         });
      }
   }
}

SCENARIO("Catch recovery on rejected sources for matching callback shapes", "[Promise][Scenario]") {
   GIVEN("an already-rejected Promise<int>") {
      WHEN("catch callback returns Promise<int>") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p      = Promise<int>::Reject<TestError>("recover int")
                            .Catch([&called](TestError const&) -> Promise<int> {
                          called = true;
                          co_return 101;
                            });

            p.WaitDone();
            REQUIRE(called);
            REQUIRE(p.Resolved());
         });
      }

      WHEN("catch callback returns Promise<void>") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p      = Promise<int>::Reject<TestError>("recover void")
                            .Catch([&called](TestError const&) -> Promise<void> {
                          called = true;
                          co_return;
                            });

            p.WaitDone();
            REQUIRE(called);
            REQUIRE(p.Resolved());
         });
      }

      WHEN("catch callback takes exception_ptr and returns Promise<int>") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p      = Promise<int>::Reject<TestError>("recover ptr")
                            .Catch([&called](std::exception_ptr const& ex) -> Promise<int> {
                          called = true;
                          RequireException<TestError>(ex);
                          co_return 102;
                            });

            p.WaitDone();
            REQUIRE(called);
            REQUIRE(p.Resolved());
         });
      }

      WHEN("catch callback returns void") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p =
              Promise<int>::Reject<TestError>("recover sync").Catch([&called](TestError const&) {
                 called = true;
              });

            p.WaitDone();
            REQUIRE(called);
            REQUIRE(p.Resolved());
         });
      }
   }

   GIVEN("an already-rejected Promise<void>") {
      WHEN("catch callback returns Promise<int>") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p      = Promise<void>::Reject<TestError>("void recover int")
                            .Catch([&called](TestError const&) -> Promise<int> {
                          called = true;
                          co_return 103;
                            });

            p.WaitDone();
            REQUIRE(called);
            REQUIRE(p.Resolved());
         });
      }

      WHEN("catch callback returns Promise<void>") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p      = Promise<void>::Reject<TestError>("void recover void")
                            .Catch([&called](TestError const&) -> Promise<void> {
                          called = true;
                          co_return;
                            });

            p.WaitDone();
            REQUIRE(called);
            REQUIRE(p.Resolved());
         });
      }

      WHEN("catch callback takes exception_ptr and returns value") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p      = Promise<void>::Reject<TestError>("void recover value")
                            .Catch([&called](std::exception_ptr const& ex) {
                          called = true;
                          RequireException<TestError>(ex);
                          return 104;
                            });

            p.WaitDone();
            REQUIRE(called);
            REQUIRE(p.Resolved());
         });
      }
   }

   GIVEN("a pending promise that rejects later") {
      WHEN("pending Promise<int> reject path with Promise<int> catch") {
         RunWithTimeout(2s, [&] {
            auto [source, resolve, reject] = promise::Create<int>();
            bool called                    = false;

            auto caught = std::move(source).Catch([&called](TestError const&) -> Promise<int> {
               called = true;
               co_return 105;
            });

            REQUIRE((*reject)(std::make_exception_ptr(TestError{"pending reject int"})));
            REQUIRE_FALSE((*resolve)(1));

            caught.WaitDone();
            REQUIRE(called);
            REQUIRE(caught.Resolved());
         });
      }

      WHEN("pending Promise<void> reject path with Promise<void> catch") {
         RunWithTimeout(2s, [&] {
            auto [source, resolve, reject] = promise::Create<void>();
            bool called                    = false;

            auto caught = std::move(source).Catch([&called](TestError const&) -> Promise<void> {
               called = true;
               co_return;
            });

            REQUIRE((*reject)(std::make_exception_ptr(TestError{"pending reject void"})));
            REQUIRE_FALSE((*resolve)());

            caught.WaitDone();
            REQUIRE(called);
            REQUIRE(caught.Resolved());
         });
      }
   }
}

SCENARIO("Internal Promise<int,true> continuation branches", "[Promise][Scenario]") {
   GIVEN("a pending Promise<int, true>") {
      WHEN("Then with value callback resolves") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::CreatePending<int, true>();
            auto chained = TestHelper::Then(*source, [](int value) { return value + 10; });

            REQUIRE(TestHelper::Resolve(*source, 5));
            chained.WaitDone();
            REQUIRE(chained.Resolved());
            REQUIRE(chained.Value() == 15);
         });
      }

      WHEN("Then with Promise callback resolves") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<int, true>();
            auto chained =
              TestHelper::Then(*source, [](int value) -> Promise<int> { co_return value * 3; });

            REQUIRE(TestHelper::Resolve(*source, 7));
            chained.WaitDone();
            REQUIRE(chained.Resolved());
            REQUIRE(chained.Value() == 21);
         });
      }

      WHEN("Then with value callback propagates rejection") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::CreatePending<int, true>();
            auto chained = TestHelper::Then(*source, [](int value) { return value + 1; });

            REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"then reject"})));
            chained.WaitDone();
            REQUIRE(chained.Rejected());
            RequireException<TestError>(chained.Exception());
         });
      }

      WHEN("Catch typed callback handles rejection") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<int, true>();
            auto caught = TestHelper::Catch(*source, [](TestError const&) { return 44; });

            REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"catch typed"})));
            caught.WaitDone();
            REQUIRE(caught.Resolved());
            REQUIRE(caught.Value() == 44);
         });
      }

      WHEN("Catch exception_ptr callback returns Promise") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<int, true>();
            auto caught =
              TestHelper::Catch(*source, [](std::exception_ptr const& ex) -> Promise<int> {
                 RequireException<TestError>(ex);
                 co_return 55;
              });

            REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"catch ptr"})));
            caught.WaitDone();
            REQUIRE(caught.Resolved());
            REQUIRE(caught.Value() == 55);
         });
      }

      WHEN("Catch typed Promise callback handles matching rejection") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<int, true>();
            auto caught =
              TestHelper::Catch(*source, [](TestError const&) -> Promise<int> { co_return 56; });

            REQUIRE(
              TestHelper::Reject(*source, std::make_exception_ptr(TestError{"catch typed promise"}))
            );
            caught.WaitDone();
            REQUIRE(caught.Resolved());
            REQUIRE(caught.Value() == 56);
         });
      }

      WHEN("Catch typed callback with non-matching exception rethrows") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<int, true>();
            auto caught = TestHelper::Catch(*source, [](CatchError const&) { return 100; });

            REQUIRE(
              TestHelper::Reject(*source, std::make_exception_ptr(TestError{"type mismatch"}))
            );
            caught.WaitDone();
            REQUIRE(caught.Rejected());
            RequireException<TestError>(caught.Exception());
         });
      }

      WHEN("Catch callback throwing rejects returned promise") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<int, true>();
            auto caught = TestHelper::Catch(*source, [](TestError const&) -> int {
               throw CatchError{"catch throws"};
            });

            REQUIRE(
              TestHelper::Reject(*source, std::make_exception_ptr(TestError{"catch throw source"}))
            );
            caught.WaitDone();
            REQUIRE(caught.Rejected());
            RequireException<CatchError>(caught.Exception());
         });
      }

      WHEN("Finally callback preserves resolved value") {
         RunWithTimeout(2s, [&] {
            bool invoked = false;

            auto source  = TestHelper::CreatePending<int, true>();
            auto finally = TestHelper::Finally(*source, [&invoked] { invoked = true; });

            REQUIRE(TestHelper::Resolve(*source, 13));
            finally.WaitDone();
            REQUIRE(finally.Resolved());
            REQUIRE(finally.Value() == 13);
            REQUIRE(invoked);
         });
      }

      WHEN("Finally callback throwing overrides rejection") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<int, true>();
            auto final =
              TestHelper::Finally(*source, [] { throw FinallyError{"finally overrides"}; });

            REQUIRE(
              TestHelper::Reject(*source, std::make_exception_ptr(TestError{"original reject"}))
            );
            final.WaitDone();
            REQUIRE(final.Rejected());
            RequireException<FinallyError>(final.Exception());
         });
      }
   }
}

SCENARIO("Internal Promise<void,true> continuation branches", "[Promise][Scenario]") {
   GIVEN("a pending Promise<void, true>") {
      WHEN("Then callback returns value") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::CreatePending<void, true>();
            auto chained = TestHelper::Then(*source, [] { return 9; });

            REQUIRE(TestHelper::Resolve(*source));
            chained.WaitDone();
            REQUIRE(chained.Resolved());
            REQUIRE(chained.Value() == 9);
         });
      }

      WHEN("Then callback returns Promise") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::CreatePending<void, true>();
            auto chained = TestHelper::Then(*source, []() -> Promise<int> { co_return 12; });

            REQUIRE(TestHelper::Resolve(*source));
            chained.WaitDone();
            REQUIRE(chained.Resolved());
            REQUIRE(chained.Value() == 12);
         });
      }

      WHEN("Catch on resolved void source keeps success") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<void, true>();
            auto caught = TestHelper::Catch(*source, [](std::exception_ptr const&) { return 99; });

            REQUIRE(TestHelper::Resolve(*source));
            caught.WaitDone();
            REQUIRE(caught.Resolved());
            REQUIRE_FALSE(caught.Value().has_value());
         });
      }

      WHEN("Catch typed callback handles void rejection") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<void, true>();
            auto caught = TestHelper::Catch(*source, [](TestError const&) { return 66; });

            REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"void catch"})));
            caught.WaitDone();
            REQUIRE(caught.Resolved());
            REQUIRE(caught.Value() == 66);
         });
      }

      WHEN("Finally Promise callback on resolved void source") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<void, true>();
            auto final  = TestHelper::Finally(*source, []() -> Promise<void> { co_return; });

            REQUIRE(TestHelper::Resolve(*source));
            final.WaitDone();
            REQUIRE(final.Resolved());
         });
      }

      WHEN("Finally Promise callback keeps original rejection") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<void, true>();
            auto final  = TestHelper::Finally(*source, []() -> Promise<void> { co_return; });

            REQUIRE(
              TestHelper::Reject(*source, std::make_exception_ptr(TestError{"void final reject"}))
            );
            final.WaitDone();
            REQUIRE(final.Rejected());
            RequireException<TestError>(final.Exception());
         });
      }

      WHEN("Finally callback keeps original rejection") {
         RunWithTimeout(2s, [&] {
            bool invoked = false;

            auto source = TestHelper::CreatePending<void, true>();
            auto final  = TestHelper::Finally(*source, [&invoked] { invoked = true; });

            REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"void reject"})));
            final.WaitDone();
            REQUIRE(final.Rejected());
            REQUIRE(invoked);
            RequireException<TestError>(final.Exception());
         });
      }
   }
}

SCENARIO("Done Promise<T,false> immediate branches", "[Promise][Scenario]") {
   GIVEN("an already-done Promise<int, false>") {
      WHEN("continuations are attached") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::Create<int, false>();

            auto then_value = TestHelper::Then(*source, [](int value) { return value + 2; });
            auto catch_passthrough =
              TestHelper::Catch(*source, [](std::exception_ptr const&) { return 88; });
            auto final_value = TestHelper::Finally(*source, [] {});

            REQUIRE(then_value.Resolved());
            REQUIRE(then_value.Value() == 2);
            REQUIRE(catch_passthrough.Resolved());
            REQUIRE(catch_passthrough.Value() == 0);
            REQUIRE(final_value.Resolved());
            REQUIRE(final_value.Value() == 0);
         });
      }
   }

   GIVEN("an already-done Promise<void, false>") {
      WHEN("continuations are attached") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::Create<void, false>();

            auto then_value = TestHelper::Then(*source, [] { return 3; });
            auto catch_passthrough =
              TestHelper::Catch(*source, [](std::exception_ptr const&) { return 77; });
            auto final_value = TestHelper::Finally(*source, [] {});

            REQUIRE(then_value.Resolved());
            REQUIRE(then_value.Value() == 3);
            REQUIRE(catch_passthrough.Resolved());
            REQUIRE_FALSE(catch_passthrough.Value().has_value());
            REQUIRE(final_value.Resolved());
         });
      }
   }
}

SCENARIO("MakePromise promise-returning signature dispatch", "[Promise][Scenario]") {
   GIVEN("promise-returning callables with different argument signatures") {
      WHEN("callable takes resolve and reject placeholders") {
         RunWithTimeout(2s, [&] {
            auto promise = MakePromise(
              [](Resolve<int> const&, Reject const&, int value) -> Promise<int> {
                 co_return value + 1;
              },
              10
            );

            promise.WaitDone();
            REQUIRE(promise.Resolved());
            REQUIRE(promise.Value() == 11);
         });
      }

      WHEN("callable takes resolve placeholder only") {
         RunWithTimeout(2s, [&] {
            auto promise = MakePromise(
              [](Resolve<int> const&, int value) -> Promise<int> { co_return value + 2; }, 10
            );

            promise.WaitDone();
            REQUIRE(promise.Resolved());
            REQUIRE(promise.Value() == 12);
         });
      }

      WHEN("callable takes regular arguments only") {
         RunWithTimeout(2s, [&] {
            auto promise = MakePromise([](int value) -> Promise<int> { co_return value + 3; }, 10);

            promise.WaitDone();
            REQUIRE(promise.Resolved());
            REQUIRE(promise.Value() == 13);
         });
      }

      WHEN("callable has no arguments") {
         RunWithTimeout(2s, [&] {
            auto promise = MakePromise([]() -> Promise<int> { co_return 14; });

            promise.WaitDone();
            REQUIRE(promise.Resolved());
            REQUIRE(promise.Value() == 14);
         });
      }

      WHEN("callable throws before returning promise") {
         RunWithTimeout(2s, [&] {
            auto promise = MakePromise([]() -> Promise<int> {
               throw TestError{"make promise throw"};
               co_return 0;
            });

            promise.WaitDone();
            REQUIRE(promise.Rejected());
            RequireException<TestError>(promise.Exception());
         });
      }
   }
}

SCENARIO("MakePromise value-returning resolver-style overloads", "[Promise][Scenario]") {
   GIVEN("value-returning callables using resolve and reject arguments") {
      WHEN("resolve and reject are both provided") {
         RunWithTimeout(2s, [&] {
            auto promise = MakePromise(
              [](Resolve<int> const& resolve, Reject const&, int value) {
                 REQUIRE(resolve(value + 4));
              },
              10
            );

            promise.WaitDone();
            REQUIRE(promise.Resolved());
            REQUIRE(promise.Value() == 14);
         });
      }

      WHEN("only resolve is provided") {
         RunWithTimeout(2s, [&] {
            auto promise = MakePromise(
              [](Resolve<int> const& resolve, int value) { REQUIRE(resolve(value + 5)); }, 10
            );

            promise.WaitDone();
            REQUIRE(promise.Resolved());
            REQUIRE(promise.Value() == 15);
         });
      }

      WHEN("void callback path auto-resolves") {
         RunWithTimeout(2s, [&] {
            bool called  = false;
            auto promise = MakePromise([&called] { called = true; });

            promise.WaitDone();
            REQUIRE(called);
            REQUIRE(promise.Resolved());
         });
      }
   }
}

SCENARIO("details::Promise Create dispatch across callable signatures", "[Promise][Scenario]") {
   using DetailsPromise = promise::details::Promise<int, false>;

   GIVEN("Create<false> with resolver and rejector arguments") {
      RunWithTimeout(2s, [&] {
         auto p = DetailsPromise::template Create<false>(
           [](Resolve<int> const& resolve, Reject const& reject, int base) -> Promise<int> {
              (void)resolve;
              (void)reject;
              co_return base + 1;
           },
           10
         );

         p.WaitDone();
         REQUIRE(p.Resolved());
         REQUIRE(p.Value() == 11);
      });
   }

   GIVEN("Create<false> with resolver-only first argument") {
      RunWithTimeout(2s, [&] {
         auto p = DetailsPromise::template Create<false>(
           [](Resolve<int> const& resolve, int base) -> Promise<int> {
              (void)resolve;
              co_return base + 2;
           },
           10
         );

         p.WaitDone();
         REQUIRE(p.Resolved());
         REQUIRE(p.Value() == 12);
      });
   }

   GIVEN("Create<false> with regular arguments only") {
      RunWithTimeout(2s, [&] {
         auto p = DetailsPromise::template Create<false>(
           [](int base) -> Promise<int> { co_return base + 3; }, 10
         );

         p.WaitDone();
         REQUIRE(p.Resolved());
         REQUIRE(p.Value() == 13);
      });
   }

   GIVEN("Create<false> with zero-argument callable") {
      RunWithTimeout(2s, [&] {
         auto p = DetailsPromise::template Create<false>([]() -> Promise<int> { co_return 14; });

         p.WaitDone();
         REQUIRE(p.Resolved());
         REQUIRE(p.Value() == 14);
      });
   }

   GIVEN("Create<true> returns resolver handles for each callable shape") {
      RunWithTimeout(2s, [&] {
         auto [p_with_rr, resolve_rr, reject_rr] = DetailsPromise::template Create<true>(
           [](Resolve<int> const& resolve, Reject const& reject, int base) -> Promise<int> {
              (void)resolve;
              (void)reject;
              co_return base + 100;
           },
           1
         );

         p_with_rr.WaitDone();
         REQUIRE(p_with_rr.Resolved());
         REQUIRE(p_with_rr.Value() == 101);
         REQUIRE_FALSE((*resolve_rr)(999));
         REQUIRE_FALSE((*reject_rr)(std::make_exception_ptr(TestError{"ignored"})));

         auto [p_with_r, resolve_r, reject_r] = DetailsPromise::template Create<true>(
           [](Resolve<int> const& resolve, int base) -> Promise<int> {
              (void)resolve;
              co_return base + 200;
           },
           2
         );

         p_with_r.WaitDone();
         REQUIRE(p_with_r.Resolved());
         REQUIRE(p_with_r.Value() == 202);
         REQUIRE_FALSE((*reject_r)(std::make_exception_ptr(TestError{"ignored"})));
         REQUIRE_FALSE((*resolve_r)(333));

         auto [p_plain, resolve_plain, reject_plain] = DetailsPromise::template Create<true>(
           [](int base) -> Promise<int> { co_return base + 30; }, 3
         );

         p_plain.WaitDone();
         REQUIRE(p_plain.Resolved());
         REQUIRE(p_plain.Value() == 33);
         REQUIRE_FALSE((*resolve_plain)(999));
         REQUIRE_FALSE((*reject_plain)(std::make_exception_ptr(TestError{"ignored"})));

         auto [p_zero, resolve_zero, reject_zero] =
           DetailsPromise::template Create<true>([]() -> Promise<int> { co_return 44; });

         p_zero.WaitDone();
         REQUIRE(p_zero.Resolved());
         REQUIRE(p_zero.Value() == 44);
         REQUIRE_FALSE((*resolve_zero)(444));
         REQUIRE_FALSE((*reject_zero)(std::make_exception_ptr(TestError{"ignored"})));
      });
   }
}

SCENARIO("Then on done promise with Promise-returning callback", "[Promise][Scenario]") {
   GIVEN("an already-done Promise<int, false>") {
      WHEN("Then callback returns Promise<int>") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::Create<int, false>();
            auto chained =
              TestHelper::Then(*source, [](int value) -> Promise<int> { co_return value + 10; });

            chained.WaitDone();
            REQUIRE(chained.Resolved());
            REQUIRE(chained.Value() == 10);
         });
      }

      WHEN("Then callback returns Promise<void>") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::Create<int, false>();
            bool called  = false;
            auto chained = TestHelper::Then(*source, [&called](int) -> Promise<void> {
               called = true;
               co_return;
            });

            chained.WaitDone();
            REQUIRE(called);
            REQUIRE(chained.Resolved());
         });
      }
   }

   GIVEN("an already-done Promise<void, false>") {
      WHEN("Then callback returns Promise<int>") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::Create<void, false>();
            auto chained = TestHelper::Then(*source, []() -> Promise<int> { co_return 7; });

            chained.WaitDone();
            REQUIRE(chained.Resolved());
            REQUIRE(chained.Value() == 7);
         });
      }

      WHEN("Then callback returns Promise<void>") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::Create<void, false>();
            bool called  = false;
            auto chained = TestHelper::Then(*source, [&called]() -> Promise<void> {
               called = true;
               co_return;
            });

            chained.WaitDone();
            REQUIRE(called);
            REQUIRE(chained.Resolved());
         });
      }
   }
}

SCENARIO("Catch on done-rejected promise with IS_PROMISE_FUNCTION", "[Promise][Scenario]") {
   GIVEN("a done-rejected Promise<int>") {
      WHEN("Catch typed callback returns Promise<int>") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<int, true>();
            REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"catch ipr"})));

            auto caught =
              TestHelper::Catch(*source, [](TestError const&) -> Promise<int> { co_return 42; });
            caught.WaitDone();
            REQUIRE(caught.Resolved());
            REQUIRE(caught.Value() == 42);
         });
      }

      WHEN("Catch exception_ptr callback returns Promise<int>") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<int, true>();
            REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"catch ipr2"})));

            auto caught = TestHelper::Catch(*source, [](std::exception_ptr const&) -> Promise<int> {
               co_return 43;
            });
            caught.WaitDone();
            REQUIRE(caught.Resolved());
            REQUIRE(caught.Value() == 43);
         });
      }

      WHEN("Catch typed callback returns Promise<int>, non-matching exception passes through") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<int, true>();
            REQUIRE(
              TestHelper::Reject(*source, std::make_exception_ptr(TestError{"catch ipr3 mismatch"}))
            );

            auto caught =
              TestHelper::Catch(*source, [](CatchError const&) -> Promise<int> { co_return 99; });
            caught.WaitDone();
            REQUIRE(caught.Rejected());
            RequireException<TestError>(caught.Exception());
         });
      }
   }

   GIVEN("a done-rejected Promise<void>") {
      WHEN("Catch typed callback returns Promise<void>") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<void, true>();
            REQUIRE(
              TestHelper::Reject(*source, std::make_exception_ptr(TestError{"void catch ipr"}))
            );

            bool called = false;
            auto caught = TestHelper::Catch(*source, [&called](TestError const&) -> Promise<void> {
               called = true;
               co_return;
            });
            caught.WaitDone();
            REQUIRE(called);
            REQUIRE(caught.Resolved());
         });
      }
   }
}

SCENARIO(
  "Finally with Promise-returning callback covers IS_PROMISE_FUNCTION branches",
  "[Promise][Scenario]"
) {
   GIVEN("a done-resolved Promise<int>") {
      WHEN("Finally callback returns Promise<void>, value passes through") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<int, true>();
            REQUIRE(TestHelper::Resolve(*source, 10));

            bool called  = false;
            auto finally = TestHelper::Finally(*source, [&called]() -> Promise<void> {
               called = true;
               co_return;
            });
            finally.WaitDone();
            REQUIRE(called);
            REQUIRE(finally.Resolved());
            REQUIRE(finally.Value() == 10);
         });
      }

      WHEN("Finally Promise callback throws, overrides value") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<int, true>();
            REQUIRE(TestHelper::Resolve(*source, 10));

            auto finally = TestHelper::Finally(*source, []() -> Promise<void> {
               throw FinallyError{"finally promise throw"};
               co_return;
            });
            finally.WaitDone();
            REQUIRE(finally.Rejected());
            RequireException<FinallyError>(finally.Exception());
         });
      }
   }

   GIVEN("a done-rejected Promise<int>") {
      WHEN("Finally Promise callback runs, rejection passes through") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<int, true>();
            REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"final rej"})));

            bool called  = false;
            auto finally = TestHelper::Finally(*source, [&called]() -> Promise<void> {
               called = true;
               co_return;
            });
            finally.WaitDone();
            REQUIRE(called);
            REQUIRE(finally.Rejected());
            RequireException<TestError>(finally.Exception());
         });
      }
   }

   GIVEN("a done-resolved Promise<void>") {
      WHEN("Finally Promise callback runs, void passes through") {
         RunWithTimeout(2s, [&] {
            auto source = TestHelper::CreatePending<void, true>();
            REQUIRE(TestHelper::Resolve(*source));

            bool called  = false;
            auto finally = TestHelper::Finally(*source, [&called]() -> Promise<void> {
               called = true;
               co_return;
            });
            finally.WaitDone();
            REQUIRE(called);
            REQUIRE(finally.Resolved());
         });
      }
   }

   GIVEN("a pending Promise<int> that later resolves") {
      WHEN("Finally Promise callback runs after resolution") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::CreatePending<int, true>();
            bool called  = false;
            auto finally = TestHelper::Finally(*source, [&called]() -> Promise<void> {
               called = true;
               co_return;
            });

            REQUIRE(TestHelper::Resolve(*source, 15));
            finally.WaitDone();
            REQUIRE(called);
            REQUIRE(finally.Resolved());
            REQUIRE(finally.Value() == 15);
         });
      }

      WHEN("Finally Promise callback runs after rejection") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::CreatePending<int, true>();
            bool called  = false;
            auto finally = TestHelper::Finally(*source, [&called]() -> Promise<void> {
               called = true;
               co_return;
            });

            REQUIRE(
              TestHelper::Reject(*source, std::make_exception_ptr(TestError{"final pend rej"}))
            );
            finally.WaitDone();
            REQUIRE(called);
            REQUIRE(finally.Rejected());
            RequireException<TestError>(finally.Exception());
         });
      }
   }

   GIVEN("a pending Promise<void> that later resolves") {
      WHEN("Finally Promise callback runs after void resolution") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::CreatePending<void, true>();
            bool called  = false;
            auto finally = TestHelper::Finally(*source, [&called]() -> Promise<void> {
               called = true;
               co_return;
            });

            REQUIRE(TestHelper::Resolve(*source));
            finally.WaitDone();
            REQUIRE(called);
            REQUIRE(finally.Resolved());
         });
      }
   }
}

SCENARIO("Helper wrappers cover strict reject and callable adaptation", "[Promise][Scenario]") {
   GIVEN("MakeReject helper") {
      WHEN("RELAXED=true ignores duplicate rejection") {
         RunWithTimeout(2s, [&] {
            auto [promise, resolve, reject] = promise::Create<int>();

            REQUIRE(MakeReject<TestError, true>(*reject, "first"));
            REQUIRE_FALSE(MakeReject<TestError, true>(*reject, "second"));
            REQUIRE_FALSE((*resolve)(1));

            REQUIRE(promise.Rejected());
            RequireException<TestError>(promise.Exception());
         });
      }
   }

   GIVEN("promise::All receives non-WPromise inputs") {
      WHEN("values and coroutine callables are adapted") {
         RunWithTimeout(2s, [&] {
            auto all = promise::All(
              [] { return 2; },
              []() -> Promise<int> { co_return 3; },
              []() -> Promise<void> { co_return; }
            );

            REQUIRE(all.Resolved());
            auto const [a, b] = all.Value();
            REQUIRE(a == 2);
            REQUIRE(b == 3);
         });
      }
   }

   GIVEN("promise::Race receives non-WPromise inputs") {
      WHEN("all candidates resolve to the same value type") {
         RunWithTimeout(2s, [&] {
            auto raced = promise::Race([]() -> Promise<int> { co_return 5; }, [] { return 7; });

            REQUIRE(raced.Resolved());
            REQUIRE(raced.Value() == 5);
         });
      }

      WHEN("mix of void and value returns optional result") {
         RunWithTimeout(2s, [&] {
            auto raced = promise::Race([]() -> Promise<void> { co_return; }, [] { return 8; });

            REQUIRE(raced.Resolved());
            REQUIRE_FALSE(raced.Value().has_value());
         });
      }

      WHEN("mixing pre-built WPromise with callable still adapts non-WPromise") {
         RunWithTimeout(2s, [&] {
            auto ready = Promise<int>::Resolve(9);
            auto raced = promise::Race(ready, []() -> Promise<int> { co_return 10; });

            REQUIRE(raced.Resolved());
            REQUIRE(raced.Value() == 9);
         });
      }
   }
}

SCENARIO(
  "Then pending with Promise<void>-returning callback covers T2=void IS_PROMISE_FUNCTION",
  "[Promise][Scenario]"
) {
   GIVEN("a pending Promise<int, true>") {
      WHEN("Then callback returns Promise<void>, source resolves") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::CreatePending<int, true>();
            bool called  = false;
            auto chained = TestHelper::Then(*source, [&called](int) -> Promise<void> {
               called = true;
               co_return;
            });

            REQUIRE(TestHelper::Resolve(*source, 5));
            chained.WaitDone();
            REQUIRE(called);
            REQUIRE(chained.Resolved());
         });
      }

      WHEN("Then callback returns Promise<void>, source rejects") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::CreatePending<int, true>();
            bool called  = false;
            auto chained = TestHelper::Then(*source, [&called](int) -> Promise<void> {
               called = true;
               co_return;
            });

            REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"rej"})));
            chained.WaitDone();
            REQUIRE_FALSE(called);
            REQUIRE(chained.Rejected());
            RequireException<TestError>(chained.Exception());
         });
      }
   }

   GIVEN("a pending Promise<void, true>") {
      WHEN("Then callback returns Promise<void>, source resolves") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::CreatePending<void, true>();
            bool called  = false;
            auto chained = TestHelper::Then(*source, [&called]() -> Promise<void> {
               called = true;
               co_return;
            });

            REQUIRE(TestHelper::Resolve(*source));
            chained.WaitDone();
            REQUIRE(called);
            REQUIRE(chained.Resolved());
         });
      }

      WHEN("Then callback returns Promise<void>, source rejects") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::CreatePending<void, true>();
            bool called  = false;
            auto chained = TestHelper::Then(*source, [&called]() -> Promise<void> {
               called = true;
               co_return;
            });

            REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"rej"})));
            chained.WaitDone();
            REQUIRE_FALSE(called);
            REQUIRE(chained.Rejected());
         });
      }
   }
}

SCENARIO(
  "Race with void and mixed-type sources covers IS_VOID and T2=void branches",
  "[Promise][Scenario]"
) {
   GIVEN("two void promises") {
      WHEN("first void promise wins") {
         RunWithTimeout(2s, [&] {
            auto r = promise::Race(
              []() -> Promise<void> { co_return; }, []() -> Promise<void> { co_return; }
            );

            r.WaitDone();
            REQUIRE(r.Resolved());
         });
      }
   }

   GIVEN("a mixed int and void promise") {
      WHEN("int promise wins, result is optional<int>") {
         RunWithTimeout(2s, [&] {
            auto r = promise::Race(
              []() -> Promise<int> { co_return 5; }, []() -> Promise<void> { co_return; }
            );

            r.WaitDone();
            REQUIRE(r.Resolved());
            REQUIRE(r.Value().has_value());
            REQUIRE(r.Value().value() == 5);
         });
      }

      WHEN("void promise wins first, result is nullopt") {
         RunWithTimeout(2s, [&] {
            auto [gate, open_gate, reject_gate] = promise::Create<void>();

            auto r = promise::Race(
              [&]() -> Promise<int> {
                 co_await gate;
                 co_return 99;
              },
              []() -> Promise<void> { co_return; }
            );

            r.WaitDone();
            REQUIRE(r.Resolved());
            REQUIRE_FALSE(r.Value().has_value());

            REQUIRE((*open_gate)());
         });
      }
   }

   GIVEN("a pending source and a resolved race promise") {
      WHEN("source is void and resolves after race starts") {
         RunWithTimeout(2s, [&] {
            auto [pending_source, resolve_source, reject_source] = promise::Create<void>();
            auto r = promise::Race(std::move(pending_source), []() -> Promise<void> { co_return; });

            r.WaitDone();
            REQUIRE(r.Resolved());
            REQUIRE((*resolve_source)());
         });
      }
   }
}

SCENARIO("details::Promise reject overloads for value and void", "[Promise][Scenario]") {
   GIVEN("non-template reject overload with exception_ptr") {
      RunWithTimeout(2s, [&] {
         auto p = promise::details::Promise<int, false>::Reject(
           std::make_exception_ptr(TestError{"reject ptr"})
         );

         REQUIRE(p.Rejected());
         RequireException<TestError>(p.Exception());
      });
   }

   GIVEN("typed reject overload for value promise") {
      RunWithTimeout(2s, [&] {
         auto p = promise::details::Promise<int, false>::Reject<TestError>("reject value typed");

         REQUIRE(p.Rejected());
         RequireException<TestError>(p.Exception());
      });
   }

   GIVEN("typed reject overload for void promise") {
      RunWithTimeout(2s, [&] {
         auto p = promise::details::Promise<void, false>::Reject<TestError>("reject void typed");

         REQUIRE(p.Rejected());
         RequireException<TestError>(p.Exception());
      });
   }
}

SCENARIO("Then with void-returning callback covers T2=void branches", "[Promise][Scenario]") {
   GIVEN("a pending Promise<int, true>") {
      WHEN("Then callback returns void, source resolves") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::CreatePending<int, true>();
            bool called  = false;
            auto chained = TestHelper::Then(*source, [&called](int v) {
               called = true;
               (void)v;
            });

            REQUIRE(TestHelper::Resolve(*source, 3));
            chained.WaitDone();
            REQUIRE(called);
            REQUIRE(chained.Resolved());
         });
      }

      WHEN("Then callback returns void, source rejects") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::CreatePending<int, true>();
            bool called  = false;
            auto chained = TestHelper::Then(*source, [&called](int v) {
               called = true;
               (void)v;
            });

            REQUIRE(
              TestHelper::Reject(*source, std::make_exception_ptr(TestError{"rej void then"}))
            );
            chained.WaitDone();
            REQUIRE_FALSE(called);
            REQUIRE(chained.Rejected());
            RequireException<TestError>(chained.Exception());
         });
      }
   }

   GIVEN("a pending Promise<void, true>") {
      WHEN("Then callback returns void, source resolves") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::CreatePending<void, true>();
            bool called  = false;
            auto chained = TestHelper::Then(*source, [&called] { called = true; });

            REQUIRE(TestHelper::Resolve(*source));
            chained.WaitDone();
            REQUIRE(called);
            REQUIRE(chained.Resolved());
         });
      }
   }

   GIVEN("an already-done Promise<int, false>") {
      WHEN("Then callback returns void") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::Create<int, false>();
            bool called  = false;
            auto chained = TestHelper::Then(*source, [&called](int v) {
               called = true;
               (void)v;
            });

            REQUIRE(called);
            REQUIRE(chained.Resolved());
         });
      }
   }

   GIVEN("an already-done Promise<void, false>") {
      WHEN("Then callback returns void") {
         RunWithTimeout(2s, [&] {
            auto source  = TestHelper::Create<void, false>();
            bool called  = false;
            auto chained = TestHelper::Then(*source, [&called] { called = true; });

            REQUIRE(called);
            REQUIRE(chained.Resolved());
         });
      }
   }
}

SCENARIO(
  "WPromise<void> public API covers Then Catch Finally on void promise type",
  "[Promise][Scenario]"
) {
   GIVEN("a resolved WPromise<void>") {
      WHEN("Then returns int") {
         RunWithTimeout(2s, [&] {
            auto p = Promise<void>::Resolve();
            auto r = std::move(p).Then([] { return 42; });

            REQUIRE(r.Resolved());
            REQUIRE(r.Value() == 42);
         });
      }

      WHEN("Then returns void") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p      = Promise<void>::Resolve();
            auto r      = std::move(p).Then([&called] { called = true; });

            REQUIRE(called);
            REQUIRE(r.Resolved());
         });
      }

      WHEN("Catch on resolved void passes through") {
         RunWithTimeout(2s, [&] {
            auto p = Promise<void>::Resolve();
            auto r = std::move(p).Catch([](TestError const&) {});

            REQUIRE(r.Resolved());
         });
      }

      WHEN("Finally preserves resolved void") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p      = Promise<void>::Resolve();
            auto r      = std::move(p).Finally([&called] { called = true; });

            REQUIRE(called);
            REQUIRE(r.Resolved());
         });
      }
   }

   GIVEN("a rejected WPromise<void>") {
      WHEN("Then skips callback and propagates rejection") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p      = Promise<void>::Reject<TestError>("void rej");
            auto r      = std::move(p).Then([&called] { called = true; });

            REQUIRE_FALSE(called);
            REQUIRE(r.Rejected());
            RequireException<TestError>(r.Exception());
         });
      }

      WHEN("Catch handles rejection on void promise") {
         RunWithTimeout(2s, [&] {
            auto p = Promise<void>::Reject<TestError>("void catch");
            auto r = std::move(p).Catch([](TestError const&) {});

            REQUIRE(r.Resolved());
         });
      }

      WHEN("Finally runs on rejected void and rejection passes through") {
         RunWithTimeout(2s, [&] {
            bool called = false;
            auto p      = Promise<void>::Reject<TestError>("void finally");
            auto r      = std::move(p).Finally([&called] { called = true; });

            REQUIRE(called);
            REQUIRE(r.Rejected());
            RequireException<TestError>(r.Exception());
         });
      }
   }
}