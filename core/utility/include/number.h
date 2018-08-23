#ifndef LIGHTDB_NUMBER_H
#define LIGHTDB_NUMBER_H

#include "errors.h"
#include <boost/math/constants/constants.hpp>
#include <boost/math/special_functions/relative_difference.hpp>
#include <boost/rational.hpp>
#include <ratio>

namespace lightdb {
    static const long double PI = 3.141592653589793238512808959406186204433;

    #define _LOGICAL_RATIONAL_OPERATOR(op)                                                       \
    inline bool operator op(const rational& other) const noexcept {                              \
        return static_cast<rational_base>(*this) op static_cast<rational_base>(other); }

    #define _ARITHMATIC_RATIONAL_OPERATOR(op)                                                    \
    inline rational operator op(const rational& other) const {                                   \
        return rational{static_cast<rational_base>(*this) op static_cast<rational_base>(other)}; }


    #define _LOGICAL_REAL_OPERATOR(op)                                                           \
    inline bool operator op(const number& other) const noexcept {                                \
        return type_ == type::rational && other.type_ == type::rational                          \
            ? rational_ op other.rational_                                                       \
            : static_cast<const real_type>(*this) op static_cast<const real_type>(other); }      \
    inline bool operator op(const rational& other) const noexcept {                              \
        return type_ == type::rational                                                           \
            ? rational_ op other                                                                 \
            : static_cast<const real_type>(*this) op static_cast<const real_type>(other); }      \
    template<typename T>                                                                         \
    inline bool operator op(const T& other) const noexcept {                                     \
        return static_cast<const real_type>(*this) op static_cast<const real_type>(other); }

    #define _ARITHMATIC_REAL_OPERATOR(op)                                                        \
    constexpr inline number operator op(const number& other) const noexcept {                    \
        if(type_ == type::rational && other.type_ == type::rational)                             \
            return number{rational_ op other.rational_};                                         \
        else                                                                                     \
            return number{static_cast<const real_type>(*this) op                                 \
                              static_cast<const real_type>(other)};  }

    #define _ARITHMATIC_FREE_REAL_OPERATOR(op)                                                   \
    template<typename T>                                                                         \
    constexpr inline number operator op(const T &left, const number &right) noexcept {           \
            return number{left} op right; }

    using real_type = long double;
    using rational_type = long long;
    using rational_base = boost::rational<rational_type>;

    class rational: public rational_base {
    public:
        constexpr rational() = default;
        constexpr rational(const rational_type numerator) noexcept : rational_base(numerator) { }
        rational(const rational_type numerator, const rational_type denominator) noexcept
                : rational_base(numerator, denominator) { }
        constexpr rational(const rational &other) noexcept
                : rational_base(other)
        { }

        inline explicit operator double() const noexcept {
            return numerator() / static_cast<double>(denominator());
        }
        inline explicit operator real_type() const noexcept {
            return numerator() / static_cast<real_type>(denominator());
        }

        _LOGICAL_RATIONAL_OPERATOR(==)
        _LOGICAL_RATIONAL_OPERATOR(>=)
        _LOGICAL_RATIONAL_OPERATOR(<=)
        _LOGICAL_RATIONAL_OPERATOR(>)
        _LOGICAL_RATIONAL_OPERATOR(<)

        _ARITHMATIC_RATIONAL_OPERATOR(+)
        _ARITHMATIC_RATIONAL_OPERATOR(-)
        _ARITHMATIC_RATIONAL_OPERATOR(*)
        _ARITHMATIC_RATIONAL_OPERATOR(/)

        inline std::string to_string() const noexcept {
            return std::to_string(numerator()) + '/' + std::to_string(denominator());
        }

    private:
        constexpr explicit rational(rational_base value) : rational_base(value) { }
    };

    class rational_times_real {
    public:
        constexpr rational_times_real(const rational &rational, const real_type real) noexcept
                : rational_(rational), real_(real) { }
        constexpr rational_times_real(const rational_times_real&) noexcept = default;
        constexpr rational_times_real(rational_times_real&&) noexcept = default;

        inline explicit operator double() const noexcept {
            return static_cast<double>(static_cast<real_type>(*this));
        }
        inline explicit operator real_type() const noexcept {
            return static_cast<real_type>(rational_) * real_;
        }

        inline rational_times_real& operator=(const rational_times_real&) noexcept = default;

        inline std::string to_string() const noexcept {
            return std::to_string(real_) + '*' + std::to_string(rational_.numerator()) + '/' + std::to_string(rational_.denominator());
        }

    private:
        rational rational_;
        real_type real_;
    };


    template<class tolerance, typename T1, typename T2>
    constexpr inline bool epsilon_equal(const T1 &left, const T2 &right) noexcept {
        return rational{tolerance::num, tolerance::den} > boost::math::relative_difference(left, right);
    }

    struct number {
    public:
        constexpr number(int integer) noexcept : number(rational{integer}) { }
        constexpr number(long integer) noexcept : number(rational{integer}) { }
        constexpr number(long long integer) noexcept : number(rational{integer}) { }
        constexpr number(unsigned int integer) noexcept : number(rational{integer}) { }
        constexpr number(size_t integer) : number(rational{static_cast<rational_type>(integer)}) {
            if(integer > std::numeric_limits<rational_type>::max()) {
                type_ = type::real;
                real_ = integer;
            }
        }
        constexpr number(const rational &rational) noexcept : type_(type::rational), rational_{rational} { }
        constexpr explicit number(const real_type real) noexcept : type_(type::real), real_{real} { }
        constexpr number(rational_times_real value) noexcept : type_(type::rational_times_real), rational_times_real_{std::move(value)} { }
        constexpr number(const double real) noexcept : type_(type::real), real_{real} { }
        constexpr number(const number& other) noexcept
                : type_(other.type_), real_(0) { *this = other; }

        constexpr inline explicit operator real_type() const noexcept {
            switch(type_) {
                case type::rational:
                    return static_cast<real_type>(rational_);
                case type::real:
                    return real_;
                case type::rational_times_real:
                    return static_cast<real_type>(rational_times_real_);
                default:
                    assert(false);
            }
        }

        constexpr inline explicit operator double() const noexcept {
            return static_cast<double>(static_cast<real_type>(*this));
        }

        constexpr inline explicit operator unsigned int() const noexcept {
            return static_cast<unsigned int>(static_cast<real_type>(*this));
        }

        constexpr inline explicit operator size_t() const noexcept {
            return static_cast<size_t>(static_cast<real_type>(*this));
        }

        inline number& operator=(const number& other) noexcept {
            type_ = other.type_;
            switch(type_) {
                case type::rational:
                    rational_ = other.rational_;
                    break;
                case type::real:
                    real_ = other.real_;
                    break;
                case type::rational_times_real:
                    rational_times_real_ = other.rational_times_real_;
                    break;
                default:
                    assert(false);
            }
            return *this;
        }

        _LOGICAL_REAL_OPERATOR(==)
        _LOGICAL_REAL_OPERATOR(>=)
        _LOGICAL_REAL_OPERATOR(<=)
        _LOGICAL_REAL_OPERATOR(>)
        _LOGICAL_REAL_OPERATOR(<)
        _ARITHMATIC_REAL_OPERATOR(+)
        _ARITHMATIC_REAL_OPERATOR(-)
        _ARITHMATIC_REAL_OPERATOR(*)
        _ARITHMATIC_REAL_OPERATOR(/)

        std::string to_string() const noexcept {
            switch(type_) {
                case type::rational:
                    return rational_.to_string();
                case type::real:
                    return std::to_string(real_);
                case type::rational_times_real:
                    return rational_times_real_.to_string();
                default:
                    assert(false);
            }
        }

        template<typename tolerance=std::pico, typename T>
        inline bool epsilon_equal(const T &other) const noexcept {
            if(type_ == type::rational && other.type_ == type::rational)
                return (rational_ - other) < rational{tolerance::num, tolerance::den};
            else
                return lightdb::epsilon_equal<tolerance>(static_cast<real_type>(*this), other);
        }

    private:
        enum class type {
            rational,
            real,
            rational_times_real
        } type_;
        union {
            rational rational_;
            real_type real_;
            rational_times_real rational_times_real_;
        };
    };

    _ARITHMATIC_FREE_REAL_OPERATOR(+)
    _ARITHMATIC_FREE_REAL_OPERATOR(-)
    _ARITHMATIC_FREE_REAL_OPERATOR(*)
    _ARITHMATIC_FREE_REAL_OPERATOR(/)

    inline std::string to_string(const number &value) noexcept {
        return value.to_string();
    }

    inline std::string to_string(const rational &value) noexcept {
        return value.to_string();
    }

    inline std::ostream &operator <<(std::ostream &stream, const number &value) noexcept {
        return stream << to_string(value);
    }

    inline std::ostream &operator <<(std::ostream &stream, const rational &value) noexcept {
        return stream << to_string(value);
    }

    constexpr inline number round(const number &value) noexcept {
        return {std::lround((real_type)value)};
    }

} // namespace lightdb

#endif //LIGHTDB_NUMBER_H