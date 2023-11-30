#pragma once

#include "Component.h"

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

//Точность вычислений задается в Component.h константой ACCURACY

class Combinations
{
    enum class Cardinality
    {
        Fixed,
        Multiple,
        More
    };
    enum class Measure
    {
        Day = 'd',
        Month = 'm',
        Year = 'y',
        Quarter = 'q'
    };
    struct duration_t
    {
        unsigned duration = 0;
        Measure measure = Measure::Day;
    };
    using strike_t = std::variant<std::monostate, char, int>;
    using expiration_t = std::variant<std::monostate, char, int, duration_t>;

    class Combination
    {
        friend class Combinations;
        friend class Checker;
        struct Leg
        {
            InstrumentType type;
            std::variant<bool, double> ratio;
            strike_t strike;
            expiration_t expiration;

            bool match(const Component & component) const;
        };
        Cardinality cardinality;
        std::string name;
        std::size_t mincount;
        std::vector<Leg> legs;
    };
    class Checker;

public:
    Combinations() = default;

    bool load(const std::filesystem::path & resource);

    std::string classify(const std::vector<Component> & components, std::vector<int> & order) const;

private:
    std::vector<Combination> combinations;
};
