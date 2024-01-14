#pragma once
#include <cerrno>
#include <stdexcept>

#include "utils.h"

namespace error {

class Error : public std::exception {
private:
    std::string desc_;

public:
    explicit Error(std::string desc) : desc_{utils::GetCurrentTime() + " | " + desc} {
        perror(desc_.c_str());
        std::abort();
    }
};

class IoError : public Error {
public:
    explicit IoError(std::string desc) : Error{"IoError | " + desc} {};
};

class BadArgument : public Error {
public:
    explicit BadArgument(std::string desc) : Error{"BadArgument | " + desc} {};
};

class NotImplemented : public Error {
public:
    explicit NotImplemented(std::string desc) : Error{"NotImplemented | " + desc} {};
};

}  // namespace error