#pragma once

#include <stdexcept>

struct InvalidDataError : std::runtime_error {
    explicit InvalidDataError(const char* message): runtime_error(message) {}

    using std::runtime_error::runtime_error;
};

struct ResourceError : std::runtime_error {
    explicit ResourceError(const char* message): runtime_error(message) {}

    using std::runtime_error::runtime_error;
};