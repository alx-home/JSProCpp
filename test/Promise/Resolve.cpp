#include "../TestCommon.h"

namespace {

struct ForwardProbe {
   int value_{0};

   static inline int copy_constructed{0};
   static inline int move_constructed{0};

   explicit ForwardProbe(int value)
      : value_(value) {}

   ForwardProbe(ForwardProbe const& other)
      : value_(other.value_) {
      ++copy_constructed;
   }

   ForwardProbe(ForwardProbe&& other) noexcept
      : value_(other.value_) {
      ++move_constructed;
      other.value_ = -1;
   }

   static void Reset() {
      copy_constructed = 0;
      move_constructed = 0;
   }
};

// struct MoveOnlyToUniquePtr {
//    std::unique_ptr<int> payload_;

//    explicit MoveOnlyToUniquePtr(int value)
//       : payload_(std::make_unique<int>(value)) {}

//    MoveOnlyToUniquePtr(MoveOnlyToUniquePtr&&) noexcept            = default;
//    MoveOnlyToUniquePtr& operator=(MoveOnlyToUniquePtr&&) noexcept = default;
//    MoveOnlyToUniquePtr(MoveOnlyToUniquePtr const&)                = delete;
//    MoveOnlyToUniquePtr& operator=(MoveOnlyToUniquePtr const&)     = delete;

//    operator std::unique_ptr<int>() && { return std::move(payload_); }
// };

}  // namespace

TEST_CASE("Resolve<void> bool reflects resolver completion", "[Promise][Resolve]") {
   RunWithTimeout(2s, [&] {
      auto [promise, resolve, reject] = promise::Create<void>();

      REQUIRE_FALSE(static_cast<bool>(*resolve));
      REQUIRE((*resolve)());
      REQUIRE(static_cast<bool>(*resolve));
      REQUIRE_FALSE((*resolve)());
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

      REQUIRE(promise.Resolved());
   });
}

TEST_CASE("Resolve<T> forwards rvalues without copying", "[Promise][Resolve]") {
   RunWithTimeout(2s, [&] {
      auto [promise, resolve, reject] = promise::Create<ForwardProbe>();
      ForwardProbe::Reset();

      ForwardProbe value{27};

      REQUIRE((*resolve)(std::move(value)));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

      REQUIRE(promise.Resolved());
      REQUIRE(promise.Value().value_ == 27);
      REQUIRE(ForwardProbe::copy_constructed == 0);
      REQUIRE(ForwardProbe::move_constructed >= 1);
      REQUIRE(value.value_ == -1);
   });
}

TEST_CASE("Resolve<T> accepts implicit const char* conversion", "[Promise][Resolve]") {
   RunWithTimeout(2s, [&] {
      auto [promise, resolve, reject] = promise::Create<std::string>();

      REQUIRE((*resolve)("converted value"));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

      REQUIRE(promise.Resolved());
      REQUIRE(promise.Value() == "converted value");
   });
}

// TEST_CASE("Resolve<T> supports move-only convertible arguments", "[Promise][Resolve]") {
//    auto [promise, resolve, reject] = promise::Create<std::unique_ptr<int>>();

//    MoveOnlyToUniquePtr value{42};

//    REQUIRE((*resolve)(std::move(value)));
//    REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

//    REQUIRE(promise.Resolved());
//    REQUIRE(value.payload_ == nullptr);
//    REQUIRE(promise.Value() != nullptr);
//    REQUIRE(*promise.Value() == 42);
// }
