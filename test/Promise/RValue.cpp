#include "../TestCommon.h"

TEST_CASE("Rvalue Then Catch Finally paths are exercised", "[Promise][RValue]") {
   auto then_rvalue = Promise<int>::Resolve(5).Then([](int value) { return value + 1; });
   REQUIRE(then_rvalue.Resolved());
   REQUIRE(then_rvalue.Value() == 6);

   auto catch_rvalue =
     Promise<int>::Reject<TestError>("rvalue catch").Catch([](TestError const&) { return 77; });
   REQUIRE(catch_rvalue.Resolved());
   REQUIRE(catch_rvalue.Value() == 77);

   bool finally_called = false;
   auto finally_rvalue =
     Promise<int>::Resolve(8).Finally([&finally_called]() { finally_called = true; });
   REQUIRE(finally_rvalue.Resolved());
   REQUIRE(finally_rvalue.Value() == 8);
   REQUIRE(finally_called);
}

TEST_CASE("Member Race handles done race promise and done source promise", "[Promise][RValue]") {
   auto [already_done_race, resolve_done_race, reject_done_race] = promise::Create<int>();
   REQUIRE((*resolve_done_race)(41));
   REQUIRE_FALSE((*reject_done_race)(std::make_exception_ptr(TestError{"ignored"})));

   auto source_done = Promise<int>::Resolve(99);
   auto winner1 =
     source_done.Race(std::move(already_done_race), resolve_done_race, reject_done_race);
   REQUIRE(winner1.Resolved());
   REQUIRE(winner1.Value() == 41);

   auto [pending_race, resolve_pending_race, reject_pending_race] = promise::Create<int>();
   auto source_done_2                                             = Promise<int>::Resolve(123);
   auto winner2 =
     source_done_2.Race(std::move(pending_race), resolve_pending_race, reject_pending_race);

   REQUIRE(winner2.Resolved());
   REQUIRE(winner2.Value() == 123);
   REQUIRE_FALSE((*resolve_pending_race)(77));
   REQUIRE_FALSE((*reject_pending_race)(std::make_exception_ptr(TestError{"ignored"})));
}

TEST_CASE(
  "Member Race unregisters awaiter when race promise completes first",
  "[Promise][RValue]"
) {
   auto [pending_source, resolve_source, reject_source] = promise::Create<int>();
   auto [pending_race, resolve_race, reject_race]       = promise::Create<int>();

   auto winner = pending_source.Race(std::move(pending_race), resolve_race, reject_race);

   REQUIRE((*resolve_race)(555));
   REQUIRE_FALSE((*reject_race)(std::make_exception_ptr(TestError{"ignored"})));
   REQUIRE(winner.Resolved());
   REQUIRE(winner.Value() == 555);

   REQUIRE((*resolve_source)(10));
   REQUIRE_FALSE((*reject_source)(std::make_exception_ptr(TestError{"ignored"})));
   REQUIRE(winner.Resolved());
   REQUIRE(winner.Value() == 555);
}
