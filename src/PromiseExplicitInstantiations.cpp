// Explicit template instantiations for Promise library to reduce coverage overhead.
// Include umbrella header so all template definitions are visible.

#include <promise.h>

#include <optional>
#include <string>
#include <tuple>
#include <variant>

namespace promise::details {
// Explicitly instantiate template types exercised in tests.
template class Promise<int, false>;
template class Promise<int, true>;
template class Promise<void, false>;
template class Promise<void, true>;
template class Promise<double, false>;
template class Promise<double, true>;
template class Promise<std::string, false>;
template class Promise<std::string, true>;
template class Promise<std::optional<int>, false>;
template class Promise<std::optional<int>, true>;
template class Promise<std::tuple<>, false>;
template class Promise<std::tuple<>, true>;
template class Promise<std::tuple<int>, false>;
template class Promise<std::tuple<int>, true>;
template class Promise<std::tuple<int, int>, false>;
template class Promise<std::tuple<int, int>, true>;
template class Promise<std::tuple<int, std::string>, false>;
template class Promise<std::tuple<int, std::string>, true>;
template class Promise<std::variant<std::string, int>, false>;
template class Promise<std::variant<std::string, int>, true>;
template class Promise<std::variant<int, double>, false>;
template class Promise<std::variant<int, double>, true>;

template class WPromise<int>;
template class WPromise<double>;
template class WPromise<std::string>;
template class WPromise<std::tuple<>>;
template class WPromise<std::tuple<int>>;
template class WPromise<std::variant<std::string, int>>;
template class WPromise<std::variant<int, double>>;
}  // namespace promise::details

namespace promise {
template class Handle<int, false>;
template class Handle<int, true>;
template class Handle<void, false>;
template class Handle<void, true>;
template class Handle<double, false>;
template class Handle<double, true>;
template class Handle<std::string, false>;
template class Handle<std::string, true>;
template class Handle<std::tuple<>, false>;
template class Handle<std::tuple<>, true>;
template class Handle<std::tuple<int>, false>;
template class Handle<std::tuple<int>, true>;
template class Handle<std::tuple<int, int>, false>;
template class Handle<std::tuple<int, int>, true>;
template class Handle<std::tuple<int, std::string>, false>;
template class Handle<std::tuple<int, std::string>, true>;
template class Handle<std::variant<std::string, int>, false>;
template class Handle<std::variant<std::string, int>, true>;
template class Handle<std::variant<int, double>, false>;
template class Handle<std::variant<int, double>, true>;
}  // namespace promise
