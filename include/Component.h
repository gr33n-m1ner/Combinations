#pragma once

#include <ctime>
#include <istream>
#include <string>

inline constexpr double ACCURACY = 10e-6;

enum class InstrumentType : char
{
    C = 'C',
    F = 'F',
    O = 'O',
    P = 'P',
    U = 'U',
    Unknown = '\0'
};

bool is_option(InstrumentType type);

struct Component
{
    static Component from_stream(std::istream &);
    static Component from_string(const std::string &);

    InstrumentType type{InstrumentType::Unknown};
    double ratio{0};
    double strike{0};
    std::tm expiration;
};

bool operator==(const Component & left, const Component & right);
