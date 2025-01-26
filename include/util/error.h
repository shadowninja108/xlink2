#pragma once

#include <stdexcept>

struct InvalidDataError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct ResourceError : std::runtime_error {
    using std::runtime_error::runtime_error;
};