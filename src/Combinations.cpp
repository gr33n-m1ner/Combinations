#include "Combinations.h"

#include "pugixml.hpp"

#include <algorithm>
#include <ctime>
#include <map>
#include <numeric>
#include <string_view>
#include <tuple>

namespace {
class Date
{
public:
    std::tm m_date = {};
    Date()
    {
        m_date.tm_mday = 1;
    }
    Date(std::tm date)
    {
        m_date.tm_mday = date.tm_mday;
        m_date.tm_mon = date.tm_mon;
        m_date.tm_year = date.tm_year;
    }
    void add_day(unsigned day)
    {
        m_date.tm_mday += day;
        std::mktime(&m_date);
    }
    void add_month(unsigned month)
    {
        m_date.tm_mon += month;
        std::mktime(&m_date);
    }
    void add_year(unsigned year)
    {
        m_date.tm_year += year;
        std::mktime(&m_date);
    }
    int get_month() const
    {
        return m_date.tm_year * 12 + m_date.tm_mon;
    }
    friend bool operator==(const Date & date1, const Date & date2)
    {
        const std::tm & left = date1.m_date;
        const std::tm & right = date2.m_date;
        return std::tie(left.tm_year, left.tm_mon, left.tm_mday) == std::tie(right.tm_year, right.tm_mon, right.tm_mday);
    }
    friend bool operator<(const Date & date1, const Date & date2)
    {
        const std::tm & left = date1.m_date;
        const std::tm & right = date2.m_date;
        return std::tie(left.tm_year, left.tm_mon, left.tm_mday) < std::tie(right.tm_year, right.tm_mon, right.tm_mday);
    }
    friend bool operator>(const Date & date1, const Date & date2)
    {
        return date2 < date1;
    }
};
bool equal(double left, double right)
{
    return std::abs(left - right) <= ACCURACY;
}
bool equal(const Date & left, const Date & right)
{
    return left == right;
}
constexpr int month_dur[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
} //anonymous namespace

bool Combinations::Combination::Leg::match(const Component & comp) const
{
    return (type == comp.type || (type == InstrumentType::O && is_option(comp.type))) &&
            ((std::holds_alternative<double>(ratio) && equal(std::get<double>(ratio), comp.ratio)) ||
             (std::holds_alternative<bool>(ratio) && !equal(comp.ratio, 0) && std::get<bool>(ratio) == (comp.ratio > 0))); //проверяю, что одного знака и не 0
}

class Combinations::Checker
{
    friend class Combinations;
    const Combination & combination;
    Checker(const Combination & comb)
        : combination(comb){};

    template <class T>
    struct Last
    {
        T value = {};
        int offset = 0;
    };

    bool check(const std::vector<Component> & components, std::vector<int> & order);
    bool check_fixed(const std::vector<Component> & components, std::vector<int> & order);
    bool check_multiple(const std::vector<Component> & components, std::vector<int> & order);
    bool check_more(const std::vector<Component> & components, std::vector<int> & order);
    bool check_dates(Date last, const Date & next, unsigned duration, Measure measure);

    template <class T, class R>
    bool check_attribute(const R & leg, T next, std::map<char, T> & vars, Last<T> & last);
};

bool Combinations::Checker::check(const std::vector<Component> & components, std::vector<int> & order)
{
    switch (combination.cardinality) {
    case Cardinality::Fixed:
        std::iota(order.begin(), order.end(), 1);
        return check_fixed(components, order);
    case Cardinality::Multiple:
        return check_multiple(components, order);
    case Cardinality::More:
        return check_more(components, order);
    default:
        return false;
    }
}

bool Combinations::Checker::check_fixed(const std::vector<Component> & components, std::vector<int> & order)
{
    if (order.size() != combination.mincount) {
        return false;
    }
    order[0] = order[0];
    do {
        bool is_good = true;
        std::map<char, double> strike_vars;
        Last<double> last_strike = {components[order[0] - 1].strike, 0};
        std::map<char, Date> expiration_vars;
        Last<Date> last_expiration = {components[order[0] - 1].expiration, 0}; //нужен для обычного offset
        Date last_date;                                                        //нужен для offset с конкретными промежутками времени

        for (size_t i = 0; i < order.size(); ++i) {
            const auto & comp = components[order[i] - 1];
            const auto & leg = combination.legs[i];
            if (!std::holds_alternative<duration_t>(leg.expiration)) {
                last_date = comp.expiration;
            }

            if (!leg.match(comp) ||                                                                                       //проверка type и ratio
                !check_attribute(leg.expiration, static_cast<Date>(comp.expiration), expiration_vars, last_expiration) || //часть проверки expiration
                (is_option(leg.type) && !check_attribute(leg.strike, comp.strike, strike_vars, last_strike))) {           //проверка strike
                is_good = false;
                break;
            }
            if (const duration_t * lexp = std::get_if<duration_t>(&leg.expiration)) { //проверка остальной части expiration
                if (!check_dates(last_date, comp.expiration, lexp->duration, lexp->measure)) {
                    is_good = false;
                    break;
                }
            }
        }
        if (is_good) {
            return true;
        }
    } while (std::next_permutation(order.begin(), order.end()));
    return false;
}

bool Combinations::Checker::check_multiple(const std::vector<Component> & components, std::vector<int> & order)
{
    if (order.size() % combination.mincount != 0) {
        return false;
    }
    std::vector<int> fixed_order;                  //индексы элементов компоненты кратности 1
    std::vector<std::vector<std::size_t>> indexes; //indexes[i] хранит индексы компонент, которые равны друг другу
    std::map<std::size_t, std::size_t> permut;     //нужно, чтобы получить обратную перестановку к той, которая получится после check_fixed
    std::size_t p_ind = 1;

    for (std::size_t i = 0; i < order.size(); ++i) {
        bool is_new = true;
        for (std::size_t j = 0; j < fixed_order.size(); ++j) {
            if (components[i] == components[fixed_order[j] - 1]) {
                is_new = false;
                indexes[j].push_back(i);
                break;
            }
        }
        if (is_new) {
            permut[i + 1] = p_ind++;
            fixed_order.emplace_back(i + 1);
            indexes.emplace_back(1, i);
        }
    }
    if (!check_fixed(components, fixed_order)) {
        return false;
    }

    for (std::size_t i = 0; i < fixed_order.size(); ++i) {
        for (std::size_t j = 0; j < indexes[0].size(); ++j) {
            order[indexes[i][j]] = permut[fixed_order[i]] + j * static_cast<int>(fixed_order.size());
        }
    }
    return true;
}

bool Combinations::Checker::check_more(const std::vector<Component> & components, std::vector<int> & order)
{
    if (order.size() < combination.mincount) {
        return false;
    }
    const auto & leg = combination.legs[0];
    for (const auto & component : components) {
        if (!leg.match(component)) {
            return false;
        }
    }
    order[0] = order[0];
    std::iota(order.begin(), order.end(), 1);
    return true;
}

bool Combinations::Checker::check_dates(Date last, const Date & next, unsigned duration, Measure measure)
{
    switch (measure) {
    case Measure::Day:
        last.add_day(duration);
        return last == next;
    case Measure::Month:
        last.add_month(duration);
        return last == next;
    case Measure::Year:
        last.add_year(duration);
        return last == next;
    case Measure::Quarter:
        unsigned month_diff = next.get_month() - last.get_month();
        return (month_diff == duration * 3) ||
                (month_diff == duration * 3 + 1 && last.m_date.tm_mday > month_dur[next.m_date.tm_mon - 1]);
    }
    return false;
}

template <class T, class R>
bool Combinations::Checker::check_attribute(const R & leg, T next, std::map<char, T> & vars, Last<T> & last)
{
    if (std::holds_alternative<char>(leg)) {
        auto [value, success] = vars.emplace(std::get<char>(leg), next);
        if (!success && !equal(value->second, next)) {
            return false;
        }
    }
    if (std::holds_alternative<int>(leg)) {
        int offset = std::get<int>(leg) - last.offset;
        if (!(offset == 0 && equal(last.value, next)) && !(offset > 0 && next > last.value) && !(offset < 0 && next < last.value)) {
            return false;
        }
        last.offset += offset;
    }
    else {
        last.offset = 0;
    }
    last.value = next;
    return true;
}

bool Combinations::load(const std::filesystem::path & resource)
{
    pugi::xml_document document;
    if (!document.load_file(resource.c_str())) {
        return false;
    }
    for (const auto & xml_comb : document.child("combinations")) {
        Combination comb;

        for (const auto & xml_leg : xml_comb.child("legs")) {
            Combination::Leg leg;
            leg.type = static_cast<InstrumentType>(xml_leg.attribute("type").value()[0]);

            std::string_view xml_ratio = xml_leg.attribute("ratio").as_string();
            if (xml_ratio == "+" || xml_ratio == "-") {
                leg.ratio = xml_ratio == "+";
            }
            else {
                leg.ratio = std::stod(xml_ratio.data());
            }

            auto xml_strike = xml_leg.attribute("strike");
            auto xml_strike_off = xml_leg.attribute("strike_offset");
            if (xml_strike != nullptr) {
                leg.strike = xml_strike.value()[0];
            }
            else if (xml_strike_off != nullptr) {
                std::string_view offset = xml_strike_off.value();
                leg.strike = static_cast<int>(offset.length()) * (offset[0] == '-' ? -1 : 1);
            }

            auto xml_exp = xml_leg.attribute("expiration");
            auto xml_exp_off = xml_leg.attribute("expiration_offset");
            if (xml_exp != nullptr) {
                leg.expiration = xml_exp.value()[0];
            }
            else if (xml_exp_off != nullptr) {
                std::string_view offset = xml_exp_off.value();
                if (offset[0] == '+' || offset[0] == '-') {
                    leg.expiration = static_cast<int>(offset.length()) * (offset[0] == '-' ? -1 : 1);
                }
                else {
                    Measure measure = static_cast<Measure>(offset.back());
                    unsigned duration = std::strtol(offset.begin(), nullptr, 10);
                    leg.expiration = duration_t{duration, measure};
                }
            }
            comb.legs.push_back(std::move(leg));
        }

        comb.name = xml_comb.attribute("name").value();
        std::string_view cardinality = xml_comb.child("legs").attribute("cardinality").value();
        if (cardinality == "fixed") {
            comb.cardinality = Cardinality::Fixed;
        }
        else if (cardinality == "multiple") {
            comb.cardinality = Cardinality::Multiple;
        }
        else if (cardinality == "more") {
            comb.cardinality = Cardinality::More;
        }
        comb.mincount = (comb.cardinality == Cardinality::More) ? xml_comb.child("legs").attribute("mincount").as_int() : comb.legs.size();
        combinations.push_back(std::move(comb));
    }
    return true;
}

std::string Combinations::classify(const std::vector<Component> & components, std::vector<int> & order) const
{
    order.resize(components.size());
    for (const auto & combination : combinations) {
        if ((Checker(combination)).check(components, order)) {
            return combination.name;
        }
    }
    order.clear();
    return "Unclassified";
}
