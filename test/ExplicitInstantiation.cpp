#include "TestCommon.h"

namespace promise::details {
template class IPromise<int, false>;
template class IPromise<int, true>;
template class WPromise<int>;

template class IPromise<double, false>;
template class IPromise<double, true>;
template class WPromise<double>;

template class IPromise<void, false>;
template class IPromise<void, true>;
template class WPromise<void>;

template class IPromise<std::string, false>;
template class IPromise<std::string, true>;
template class WPromise<std::string>;

template class IPromise<std::tuple<int>, false>;
template class IPromise<std::tuple<int>, true>;
template class WPromise<std::tuple<int>>;

template class IPromise<std::variant<std::string, int>, false>;
template class IPromise<std::variant<std::string, int>, true>;
template class WPromise<std::variant<std::string, int>>;

template class IPromise<std::variant<int, double>, false>;
template class IPromise<std::variant<int, double>, true>;
template class WPromise<std::variant<int, double>>;

template class IPromise<std::optional<std::variant<double, int>>, false>;
template class IPromise<std::optional<std::variant<double, int>>, true>;
template class WPromise<std::optional<std::variant<double, int>>>;

}  // namespace promise::details
