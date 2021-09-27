#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <type_traits>
#include <utility>

namespace cc {

/// Exception used by variant::get<>() below to indicate the variant does not hold the type in question.
struct bad_variant_access : std::exception {
    const char *what() const noexcept override { return "bad variant access"; }
};


template<typename ...Ts>
class variant {
public:
    static constexpr uint8_t invalid_index_value = 0xff;
    static constexpr size_t num_types = sizeof...(Ts);
    static_assert (num_types < invalid_index_value && num_types > 0);

    /// Returns either the index of type T, or invalid_index_value if this variant cannot contain a T
    template<typename T>
    static constexpr size_t index_of_type() {
        int idx = -1;
        int found_ct = 0;
        [[maybe_unused]] auto dummy =
                ((++idx, found_ct += std::is_same_v<const T &, const Ts &>, std::is_same_v<const T &, const Ts &>)
                 || ...);
        if (!found_ct || idx < 0) idx = invalid_index_value;
        return size_t(idx);
    }

private:
    alignas(Ts...) std::byte buffer[std::max({sizeof(Ts)...})];
    uint8_t index_value = invalid_index_value;

public:
    constexpr variant() noexcept {} // unlike normal variant, this one's default c'tor makes it "valueless"

    template<typename T, std::enable_if_t<index_of_type<std::remove_reference_t<std::remove_cv_t<T>>>() < num_types, int> = 0>
    variant(T && t) { emplace<std::remove_reference_t<std::remove_cv_t<T>>>(std::forward<T>(t)); }

    variant(const variant & other) { *this = other; }
    variant(variant && other) { *this = std::move(other); }

    variant & operator=(const variant & other) {
        if (other.valueless())
            reset();
        else
            ([&] {
                if (other.index() == index_of_type<Ts>())
                    emplace<Ts>(other.template get<Ts>());
            }(), ...);
        return *this;
    }

    variant & operator=(variant && other) {
        if (other.valueless())
            reset();
        else
            ([&] {
                if (other.index() == index_of_type<Ts>()) {
                    emplace<Ts>(std::move(other.template get<Ts>()));
                    other.reset();
                }
            }(), ...);
        return *this;
    }

    // copy/move-assign directly from contained type
    template<typename T>
    std::enable_if_t<index_of_type<T>() < num_types, variant &>
    /* variant & */ operator=(T && t) {
        if (index_of_type<T>() == index_value)
            // use copy/move assign if we already contain an object of said type
            get<T>() = std::forward<T>(t);
        else
            // otherwise construct in-place
            emplace<T>(std::forward<T>(t));
        return *this;
    }

    ~variant() { reset(); }

    constexpr size_t index() const noexcept { return index_value; }
    constexpr bool valueless() const noexcept { return index() == invalid_index_value; }
    constexpr operator bool() const noexcept { return !valueless(); }

    void reset() {
        if (!valueless()) {
            ([&] {
                if (index_value == index_of_type<Ts>())
                    get<Ts>().~Ts();
            }(), ...);
            index_value = invalid_index_value;
        }
    }

    template<typename T, typename ...Args>
    std::enable_if_t<index_of_type<T>() < num_types, T &>
    /* T & */ emplace(Args && ...args) {
        reset();
        using BareT = std::remove_cv_t<std::remove_reference_t<T>>;
        T & ret = *new (buffer) BareT(std::forward<Args>(args)...);
        index_value = index_of_type<T>();
        return ret;
    }

    template<typename T>
    std::enable_if_t<index_of_type<T>() < num_types, T &>
    /* T & */ get() {
        if (index_of_type<T>() != index_value)
            throw bad_variant_access{};
        return *reinterpret_cast<T *>(buffer);
    }

    template<typename T>
    std::enable_if_t<index_of_type<T>() < num_types, const T &>
    /* const T & */ get() const {
        return const_cast<variant &>(*this).get<T>();
    }

    template<typename T>
    constexpr std::enable_if_t<index_of_type<T>() < num_types, bool>
    /* bool */ holds_alternative() noexcept { return index_of_type<T>() == index_value; }

    // for simplicity, our version of "visit" doesn't propagate the return result of the visitor
    template<typename Func>
    constexpr void visit(Func && func) {
        ([&] {
           if (index_value == index_of_type<Ts>())
               func(get<Ts>());
        }(), ...);
    }
    template<typename Func>
    constexpr void visit(Func && func) const {
       ([&] {
          if (index_value == index_of_type<Ts>())
              func(get<Ts>());
       }(), ...);
    }

    bool operator==(const variant & o) const noexcept {
        if (index_value != o.index_value) return false;
        bool ret = false;
        visit([&](auto && val){
            ret = val == o.get<std::remove_cv_t<std::remove_reference_t<decltype(val)>>>();
        });
        return ret;
    }
};

// helper type for use with variant::visit above
template<typename ...Ts> struct visitor : Ts... { using Ts::operator()...; };
template<typename ...Ts> visitor(Ts...) -> visitor<Ts...>;

} // namespace cc
