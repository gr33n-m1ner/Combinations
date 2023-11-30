#include "Component.h"

#include <iomanip>
#include <sstream>
#include <tuple>

Component Component::from_stream(std::istream & strm)
{
    Component component;

    char type = '\0';
    strm >> type;
    if (strm.fail()) {
        return {};
    }
    bool read_strike = false;
    component.type = static_cast<InstrumentType>(type);
    switch (component.type) {
    case InstrumentType::C: [[fallthrough]];
    case InstrumentType::O: [[fallthrough]];
    case InstrumentType::P:
        read_strike = true;
        break;
    case InstrumentType::F: [[fallthrough]];
    case InstrumentType::U:
        break;
    case InstrumentType::Unknown: [[fallthrough]];
    default:
        return {};
    }

    strm >> component.ratio;
    if (strm.fail()) {
        return {};
    }

    if (read_strike) {
        strm >> component.strike;
        if (strm.fail()) {
            return {};
        }
    }

    strm >> std::get_time(&component.expiration, "%Y-%m-%d");
    if (strm.fail()) {
        return {};
    }

    return component;
}

Component Component::from_string(const std::string & str)
{
    std::istringstream strm{str};
    return from_stream(strm);
}

bool is_option(InstrumentType type)
{
    return type == InstrumentType::O || type == InstrumentType::P || type == InstrumentType::C;
}

bool operator==(const Component & left, const Component & right)
{
    const std::tm & ldate = left.expiration;
    const std::tm & rdate = right.expiration;
    return left.type == right.type && std::abs(left.ratio - right.ratio) <= ACCURACY && std::abs(left.strike - right.strike) <= ACCURACY &&
            std::tie(ldate.tm_year, ldate.tm_mon, ldate.tm_mday) == std::tie(rdate.tm_year, rdate.tm_mon, rdate.tm_mday);
}
