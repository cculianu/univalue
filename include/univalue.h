// Copyright 2014 BitPay Inc.
// Copyright 2015 Bitcoin Core Developers
// Copyright (c) 2020-2021 The Bitcoin developers
// Copyright (c) 2021-2024 Calin A. Culianu <calin.culianu@gmail.com>
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <exception>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace univalue_detail {

/// Exception used by variant::get<>() below to indicate the variant does not hold the type in question.
struct bad_variant_access : std::exception {
    const char *what() const noexcept override { return "bad variant access"; }
};

/// A mostly-compatible replacement for C++17's std::variant.
///
/// This type is a bit more flexible than std::variant in that it readily supports the concept of being "empty" or
/// valueless, which is what it is if default-constructed.
///
/// The need for this class is due to the fact that some older C++17 compilers lack std::variant.
///
/// Note that this class intentionally does not differentiate between T, const T, const T &, etc. It strips all types
/// given to it down to their bare non-cv-qualifiend, non-reference type.
template<typename ...Ts>
class variant {
    template<typename T> struct rmcvr { using type = std::remove_cv_t<std::remove_reference_t<T>>; };
    template<typename T> using rmcvr_t = typename rmcvr<T>::type;

    // Used in recursive_union below (C++20 has this as a constexpr)
    template <typename T, typename ...Args>
    static inline T * construct_at(T *p, Args && ...args) {
        return ::new (static_cast<void *>(p)) T(std::forward<Args>(args)...);
    }

    struct monostate {};

    template<typename...> union recursive_union; // fwd decl req'd since recursive_union references itself

    // Recursive union, used as a way to implement variants (other way would involve a byte blob buffer).
    // This union-like class is unsafe in that it doesn't track which alternative is the active member. The outer
    // `variant` class does, however.
    template<typename T0, typename ...TsN>
    union recursive_union<T0, TsN...> {
        rmcvr_t<T0> t0;
        using maybe_more_ts = std::conditional_t<sizeof...(TsN) != 0U, recursive_union<TsN...>, monostate>;
        maybe_more_ts etc; // default active member, either the other Ts, or a monostate if this is the last T.

        constexpr recursive_union() noexcept : etc{} {}
        ~recursive_union() noexcept { std::destroy_at(&etc); }
        recursive_union &operator=(const recursive_union &) = delete;

        template <size_t I, typename ...Args>
        std::enable_if_t<I < 1u + sizeof...(TsN), void>
        /* void */ construct(Args && ...args) {
            if constexpr (I == 0u) {
                std::destroy_at(&etc);
                construct_at(std::addressof(t0), std::forward<Args>(args)...);
            } else {
                etc.template construct<I - 1U>(std::forward<Args>(args)...);
            }
        }

        template<size_t I, typename ...Args>
        std::enable_if_t<I < 1u + sizeof...(TsN), void>
        /* void */ destroy() {
            if constexpr (I == 0u) {
                std::destroy_at(std::addressof(t0));
                construct_at(&etc);
            } else {
                etc.template destroy<I - 1u>();
            }
        }

        // Note: Potentially unsafe. Enclosing class tracks the state to determine if this is legal or UB.
        template<size_t I, typename ...Args>
        constexpr const auto & get() const noexcept {
            static_assert (I < 1u + sizeof...(TsN));
            if constexpr (I == 0u)
                return t0;
            else
                return etc.template get<I - 1u>();
        }
    };

public:
    static constexpr size_t num_types = sizeof...(Ts);
    using index_t = std::conditional_t<num_types <= std::numeric_limits<uint8_t>::max(), uint8_t,
                                       std::conditional_t<num_types <= std::numeric_limits<uint16_t>::max(), uint16_t,
                                                          std::conditional_t<num_types <= std::numeric_limits<uint32_t>::max(),
                                                                             uint32_t, uint64_t>>>;

    static constexpr index_t invalid_index_value = std::numeric_limits<index_t>::max();

    // Sanity checks to ensure proper usage of this class, and that if this class is modified, invariants always hold.
    static_assert (num_types <= invalid_index_value && invalid_index_value <= std::numeric_limits<size_t>::max(),
                  "Sanity check for the auto-detect logic of index_t and invalid_index_value.");
    static_assert (num_types > 0u, "Must specify at least 1 type.");
    static_assert (((std::is_same_v<Ts, rmcvr_t<Ts>> && !std::is_same_v<Ts, void>) && ...),
                   "All types must be non-reference, non-const, non-volatile, and non-void.");

    /// Returns either the index of type T, or invalid_index_value if this variant cannot contain a T
    template<typename T>
    static constexpr size_t index_of_type() {
        size_t idx_plus_1{};
        const bool found = ((++idx_plus_1, std::is_same_v<rmcvr_t<T>, Ts>) || ...);
        if (found && idx_plus_1 && idx_plus_1 <= num_types) return idx_plus_1 - 1u;
        return invalid_index_value;
    }

private:
    index_t index_value = invalid_index_value;
    recursive_union<Ts...> u;

    static constexpr bool no_dupe_types() {
        size_t idx_plus_1{};
        return ((++idx_plus_1, index_of_type<Ts>() + 1u == idx_plus_1) && ...);
    }

    static_assert (no_dupe_types(), "Duplicate types are not allowed.");

public:
    constexpr variant() noexcept {} // unlike normal variant, this one's default c'tor makes it "valueless"

    template<typename T, std::enable_if_t<index_of_type<T>() < num_types, int> = 0>
    variant(T && t) { emplace<T>(std::forward<T>(t)); }

    variant(const variant & other) { *this = other; }
    variant(variant && other) { *this = std::move(other); }

    variant & operator=(const variant & other) {
        if (other.valueless())
            reset();
        else
            ([&] {
                if (other.holds_alternative<Ts>()) {
                    if (holds_alternative<Ts>())
                        get<Ts>() = other.get<Ts>(); // use direct copy-assign of contained type if they match
                    else
                        emplace<Ts>(other.get<Ts>()); // otherwise copy-construct in-place
                    return true;
                }
                return false;
            }() || ...);
        return *this;
    }

    variant & operator=(variant && other) {
        if (other.valueless())
            reset();
        else
            ([&] {
                if (other.holds_alternative<Ts>()) {
                    if (holds_alternative<Ts>())
                        get<Ts>() = std::move(other.get<Ts>()); // use direct move-assign of contained type if they match
                    else
                        emplace<Ts>(std::move(other.get<Ts>())); // otherwise move-construct in-place
                    other.reset();
                    return true;
                }
                return false;
            }() || ...);
        return *this;
    }

    // copy/move-assign directly using contained type
    template<typename T>
    std::enable_if_t<index_of_type<T>() < num_types, variant &>
    /* variant & */ operator=(T && t) {
        if (holds_alternative<T>())
            // use copy/move assign if we already contain an object of said type
            get<T>() = std::forward<T>(t);
        else
            // otherwise construct in-place
            emplace<T>(std::forward<T>(t));
        return *this;
    }

    ~variant() { reset(); }

    constexpr size_t index() const noexcept { return index_value; }
    constexpr bool valueless() const noexcept { return index_value == invalid_index_value; }
    constexpr explicit operator bool() const noexcept { return !valueless(); }

    void reset() {
        if (!valueless()) {
            ([&] {
                if (holds_alternative<Ts>()) {
                    u.template destroy<index_of_type<Ts>()>();
                    return true;
                }
                return false;
            }() || ...);
            index_value = invalid_index_value;
        }
    }

    template<typename T, typename ...Args>
    std::enable_if_t<index_of_type<T>() < num_types, rmcvr_t<T> &>
    /* T & */ emplace(Args && ...args) {
        reset();
        using BareT = rmcvr_t<T>;
        u.template construct<index_of_type<BareT>()>(std::forward<Args>(args)...);
        index_value = index_of_type<BareT>();
        assert(index_value < num_types);
        return get<BareT>();
    }

    template<typename T>
    constexpr std::enable_if_t<index_of_type<T>() < num_types, const rmcvr_t<T> &>
    /* const T & */ get() const {
        if (!holds_alternative<T>())
            throw bad_variant_access{};
        return u.template get<index_of_type<T>()>();
    }

    template<typename T>
    constexpr std::enable_if_t<index_of_type<T>() < num_types, rmcvr_t<T> &>
    /* T & */ get() { return const_cast<rmcvr_t<T> &>(std::as_const(*this).template get<T>()); }

    template<typename T>
    constexpr std::enable_if_t<index_of_type<T>() < num_types, bool>
    /* bool */ holds_alternative() const noexcept { return !valueless() && index_of_type<T>() == index(); }

    // for simplicity, our version of "visit" doesn't propagate the return result of the visitor
    template<typename Func>
    constexpr void visit(Func && func) {
        ([&] {
           if (holds_alternative<Ts>()) {
               func(get<Ts>());
               return true;
            }
            return false;
        }() || ...);
    }
    template<typename Func>
    constexpr void visit(Func && func) const {
        ([&] {
          if (holds_alternative<Ts>()) {
              func(get<Ts>());
              return true;
           }
           return false;
        }() || ...);
    }

    bool operator==(const variant & o) const noexcept {
        if (index_value != o.index_value) return false; // variants holding different types are always unequal
        else if (valueless()) return true; // nulls compare equal
        bool ret = false;
        ([&] {
          if (holds_alternative<Ts>()) {
              ret = get<Ts>() == o.get<Ts>();
              return true;
          }
          return false;
        }() || ...);
        return ret;
    }
    bool operator!=(const variant & o) const noexcept { return !(*this == o); }
};

// helper type for use with variant::visit above
template<typename ...Ts> struct visitor : Ts... { using Ts::operator()...; };
template<typename ...Ts> visitor(Ts...) -> visitor<Ts...>;

} // namespace univalue_detail


class UniValue {
public:

    /**
     * Value types available in JSON (and thus in UniValue).
     * Every type sets a different bit, so bitmasks can be used.
     * Can be used with is().
     *
     * Numeric values differ from the upstream UniValue API.
     */
    enum VType {
        VNULL  = 1 << 0,
        VFALSE = 1 << 1,
        VTRUE  = 1 << 2,
        VOBJ   = 1 << 3,
        VARR   = 1 << 4,
        VNUM   = 1 << 5,
        VSTR   = 1 << 6,
    };

    /**
     * Type bitmask shorthand for VFALSE|VTRUE.
     * Can be used with is().
     */
    constexpr static auto MBOOL = VFALSE | VTRUE;

    class Object {

    public:
        using mapped_type = UniValue;
        using key_type = std::string;
        using value_type = std::pair<key_type, mapped_type>;

    private:
        using Vector = std::vector<value_type>;
        Vector vector;

    public:
        using size_type = Vector::size_type;
        using iterator = Vector::iterator;
        using const_iterator = Vector::const_iterator;
        using reverse_iterator = Vector::reverse_iterator;
        using const_reverse_iterator = Vector::const_reverse_iterator;

        Object() noexcept = default;
        Object(std::initializer_list<value_type> il) : vector(il) {}
        explicit Object(const Object&) = default;
        Object(Object&&) noexcept = default;
        Object& operator=(const Object&) = default;
        Object& operator=(Object&&) = default;

        /**
         * Returns an iterator to the first key-value pair of the object.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_iterator begin() const noexcept { return vector.begin(); }
        [[nodiscard]]
        iterator begin() noexcept { return vector.begin(); }

        /**
         * Returns an iterator to the past-the-last key-value pair of the object.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_iterator end() const noexcept { return vector.end(); }
        [[nodiscard]]
        iterator end() noexcept { return vector.end(); }

        /**
         * Returns an iterator to the first key-value pair of the reversed object.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_reverse_iterator rbegin() const noexcept { return vector.rbegin(); }
        [[nodiscard]]
        reverse_iterator rbegin() noexcept { return vector.rbegin(); }

        /**
         * Returns an iterator to the past-the-last key-value pair of the reversed object.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_reverse_iterator rend() const noexcept { return vector.rend(); }
        [[nodiscard]]
        reverse_iterator rend() noexcept { return vector.rend(); }

        /**
         * Removes all key-value pairs from the object.
         *
         * Complexity: linear in number of elements.
         */
        void clear() noexcept { vector.clear(); }

        /**
         * Returns whether the object is empty.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        bool empty() const noexcept { return vector.empty(); }

        /**
         * Returns the size of the object.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        size_type size() const noexcept { return vector.size(); }

        /**
         * Increases the capacity of the underlying vector to at least new_cap.
         *
         * Complexity: at most linear in number of elements.
         */
        void reserve(size_type new_cap) { vector.reserve(new_cap); }

        /**
         * Returns a reference to the first value associated with the key,
         * or UniValue::Null if the key does not exist.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: linear in number of elements.
         *
         * If you want to distinguish between null values and missing keys, please use locate() instead.
         */
        [[nodiscard]]
        const UniValue& operator[](std::string_view key) const noexcept;

        /**
         * Returns a reference to the value at the numeric index (regardless of key),
         * or UniValue::Null if index >= object size.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         *
         * To access the first or last value, consider using front() or back() instead.
         * If you want an exception thrown on missing indices, please use at() instead.
         */
        [[nodiscard]]
        const UniValue& operator[](size_type index) const noexcept;

        /**
         * Returns a pointer to the first value associated with the key,
         * or nullptr if the key does not exist.
         *
         * The returned pointer follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: linear in the number of elements.
         *
         * If you want to treat missing keys as null values, please use the [] operator instead.
         * If you want an exception thrown on missing keys, please use at() instead.
         */
        [[nodiscard]]
        const UniValue* locate(std::string_view key) const noexcept;
        [[nodiscard]]
        UniValue* locate(std::string_view key) noexcept;

        /**
         * Returns a reference to the first value associated with the key,
         * or throws std::out_of_range if the key does not exist.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: linear in number of elements.
         *
         * If you don't want an exception thrown, please use locate() or the [] operator instead.
         */
        const UniValue& at(std::string_view key) const;
        UniValue& at(std::string_view key);

        /**
         * Returns a reference to the value at the numeric index (regardless of key),
         * or throws std::out_of_range if index >= object size.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         *
         * If you don't want an exception thrown, please use the [] operator instead.
         */
        const UniValue& at(size_type index) const;
        UniValue& at(size_type index);

        /**
         * Returns a reference to the first value (regardless of key),
         * or UniValue::Null if the object is empty.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const UniValue& front() const noexcept;

        /**
         * Returns a reference to the last value (regardless of key),
         * or UniValue::Null if the object is empty.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const UniValue& back() const noexcept;

        /**
         * Pushes the key-value pair onto the end of the object.
         *
         * Be aware that this method appends the new key-value pair regardless of whether the key already exists.
         * If you want to avoid duplicate keys, use locate() and check its result before calling push_back().
         * You can use the return value of locate() to update the existing value associated with the key, if any.
         *
         * Complexity: amortized constant (or constant if properly reserve()d).
         */
        void push_back(const value_type& entry) { vector.push_back(entry); }
        void push_back(value_type&& entry) { vector.push_back(std::move(entry)); }

        /**
         * Constructs a key-value pair in-place at the end of the object.
         *
         * Be aware that this method appends the new key-value pair regardless of whether the key already exists.
         * If you want to avoid duplicate keys, use locate() and check its result before calling emplace_back().
         * You can use the return value of locate() to update the existing value associated with the key, if any.
         *
         * Complexity: amortized constant (or constant if properly reserve()d).
         */
        template<class... Args>
        void emplace_back(Args&&... args) { vector.emplace_back(std::forward<Args>(args)...); }

        /**
         * Removes the key-value pairs in the range [first, last).
         * Returns the iterator following the last removed key-value pair.
         *
         * Complexity: linear in the number of elements removed and linear in the number of elements after those.
         */
        iterator erase(const_iterator first, const_iterator last) { return vector.erase(first, last); }

        /**
         * Returns whether the objects contain equal data.
         * Two objects are not considered equal if elements are ordered differently.
         *
         * Complexity: linear in the amount of data to compare.
         */
        [[nodiscard]]
        bool operator==(const Object& other) const noexcept { return vector == other.vector; }

        /**
         * Returns whether the objects contain unequal data.
         * Two objects are not considered equal if elements are ordered differently.
         *
         * Complexity: linear in the amount of data to compare.
         */
        [[nodiscard]]
        bool operator!=(const Object& other) const noexcept { return !(*this == other); }
    };

    class Array {

    public:
        using value_type = UniValue;

    private:
        using Vector = std::vector<value_type>;
        Vector vector;

    public:
        using size_type = Vector::size_type;
        using iterator = Vector::iterator;
        using const_iterator = Vector::const_iterator;
        using reverse_iterator = Vector::reverse_iterator;
        using const_reverse_iterator = Vector::const_reverse_iterator;

        Array() noexcept = default;
        Array(std::initializer_list<value_type> il) : vector(il) {}
        explicit Array(const Array&) = default;
        Array(Array&&) noexcept = default;
        Array& operator=(const Array&) = default;
        Array& operator=(Array&&) = default;

        /**
         * Returns an iterator to the first value of the array.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_iterator begin() const noexcept { return vector.begin(); }
        [[nodiscard]]
        iterator begin() noexcept { return vector.begin(); }

        /**
         * Returns an iterator to the past-the-last value of the array.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_iterator end() const noexcept { return vector.end(); }
        [[nodiscard]]
        iterator end() noexcept { return vector.end(); }

        /**
         * Returns an iterator to the first value of the reversed array.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_reverse_iterator rbegin() const noexcept { return vector.rbegin(); }
        [[nodiscard]]
        reverse_iterator rbegin() noexcept { return vector.rbegin(); }

        /**
         * Returns an iterator to the past-the-last value of the reversed array.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_reverse_iterator rend() const noexcept { return vector.rend(); }
        [[nodiscard]]
        reverse_iterator rend() noexcept { return vector.rend(); }

        /**
         * Removes all values from the array.
         *
         * Complexity: linear in number of elements.
         */
        void clear() noexcept { vector.clear(); }

        /**
         * Returns whether the array is empty.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        bool empty() const noexcept { return vector.empty(); }

        /**
         * Returns the size of the array.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        size_type size() const noexcept { return vector.size(); }

        /**
         * Increases the capacity of the underlying vector to at least new_cap.
         *
         * Complexity: at most linear in number of elements.
         */
        void reserve(size_type new_cap) { vector.reserve(new_cap); }

        /**
         * Returns a reference to the value at the index,
         * or UniValue::Null if index >= array size.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         *
         * To access the first or last value, consider using front() or back() instead.
         * If you want an exception thrown on missing indices, please use at() instead.
         */
        [[nodiscard]]
        const UniValue& operator[](size_type index) const noexcept;

        /**
         * Returns a reference to the value at the index,
         * or throws std::out_of_range if index >= array size.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         *
         * If you don't want an exception thrown, please use the [] operator instead.
         */
        const UniValue& at(size_type index) const;
        UniValue& at(size_type index);

        /**
         * Returns a reference to the first value,
         * or UniValue::Null if the array is empty.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const UniValue& front() const noexcept;

        /**
         * Returns a reference to the last value,
         * or UniValue::Null if the array is empty.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const UniValue& back() const noexcept;

        /**
         * Pushes the value onto the end of the array.
         *
         * Complexity: amortized constant (or constant if properly reserve()d).
         */
        void push_back(const value_type& entry) { vector.push_back(entry); }
        void push_back(value_type&& entry) { vector.push_back(std::move(entry)); }

        /**
         * Constructs a value in-place at the end of the array.
         *
         * Complexity: amortized constant (or constant if properly reserve()d).
         */
        template<class... Args>
        void emplace_back(Args&&... args) { vector.emplace_back(std::forward<Args>(args)...); }

        /**
         * Removes the values in the range [first, last).
         * Returns the iterator following the last removed value.
         *
         * Complexity: linear in the number of elements removed and linear in the number of elements after those.
         */
        iterator erase(const_iterator first, const_iterator last) { return vector.erase(first, last); }

        /**
         * Returns whether the arrays contain equal data.
         *
         * Complexity: linear in the amount of data to compare.
         */
        [[nodiscard]]
        bool operator==(const Array& other) const noexcept { return vector == other.vector; }

        /**
         * Returns whether the arrays contain unequal data.
         *
         * Complexity: linear in the amount of data to compare.
         */
        [[nodiscard]]
        bool operator!=(const Array& other) const noexcept { return !(*this == other); }
    };

    using size_type = Object::size_type;
    static_assert(std::is_same_v<size_type, Array::size_type>,
                  "UniValue::size_type should be equal to both UniValue::Object::size_type and UniValue::Array::size_type.");

    explicit UniValue(VType initialType = VNULL) noexcept {
        switch (initialType) {
        case VNULL:
            break;
        case VTRUE:
        case VFALSE:
            var.emplace<bool>(initialType == VTRUE);
            break;
        case VNUM:
            var.emplace<NumStr>();
            break;
        case VSTR:
            var.emplace<std::string>();
            break;
        case VOBJ:
            var.emplace<Object>();
            break;
        case VARR:
            var.emplace<Array>();
            break;
        }
    }

    // Note: The below "string-like" constructors are unsafe since they do not check or validate the contents of
    // the supplied string. They are offered as a performance optimization for advanced usage.
    // - Calling these 3 constructors with anything other than: VNUM or VSTR as the initialType leads to this instance
    //   being force-set to VNULL, and initialStr is discarded.
    // - If specifying VNUM for initialType, initialStr must be parseable as numeric otherwise this class will
    //   produce incorrect JSON (and may exhibit other weird behavior).
    // - If specifying VSTR for initialType, initialStr must be a valid utf-8 string. Invalid codepoints may lead to
    //   invalid JSON being produced.
    UniValue(VType initialType, std::string&& initialStr) noexcept {
        switch (initialType) {
        case VNUM:
            var.emplace<NumStr>(std::move(initialStr));
            break;
        case VSTR:
            var.emplace<std::string>(std::move(initialStr));
            break;
        case VNULL:
            break;
        case VTRUE:
        case VFALSE:
            var.emplace<bool>(initialType == VTRUE);
            break;
        case VOBJ:
            var.emplace<Object>();
            break;
        case VARR:
            var.emplace<Array>();
            break;
        }
    }
    UniValue(VType initialType, std::string_view initialStr) : UniValue(initialType, std::string{initialStr}) {}
    UniValue(VType initialType, const char* initialStr) : UniValue(initialType, std::string{initialStr}) {}

    // Copy construction, move construction, copy assignment, and move-assignment
    explicit UniValue(const UniValue& o) = default;
    UniValue(UniValue&& o) noexcept = default;
    UniValue& operator=(const UniValue &o) = default;
    UniValue& operator=(UniValue &&o) = default;

    // Misc. convenience constructors
    UniValue(bool val_) noexcept : var(val_) {}
    explicit UniValue(const Object& object) : var(object) {}
    UniValue(Object&& object) noexcept : var(std::move(object)) {}
    explicit UniValue(const Array& array) : var(array) {}
    UniValue(Array&& array) noexcept : var(std::move(array)) {}
    UniValue(short val_) { *this = val_; }
    UniValue(int val_) { *this = val_; }
    UniValue(long val_) { *this = val_; }
    UniValue(long long val_) { *this = val_; }
    UniValue(unsigned short val_) { *this = val_; }
    UniValue(unsigned val_) { *this = val_; }
    UniValue(unsigned long val_) { *this = val_; }
    UniValue(unsigned long long val_) { *this = val_; }
    UniValue(double val_) { *this = val_; }
    UniValue(std::string_view val_) : UniValue(VSTR, val_) {}
    UniValue(std::string&& val_) noexcept : UniValue(VSTR, std::move(val_)) {}
    UniValue(const char* val_) : UniValue(VSTR, val_) {}

    void setNull() { var.reset(); }
    void operator=(bool val) { var = val; }
    Object& setObject() { return var.emplace<Object>(); }
    Object& operator=(const Object& object) { return var.emplace<Object>(object); }
    Object& operator=(Object&& object) { return var.emplace<Object>(std::move(object)); }
    Array& setArray() { return var.emplace<Array>(); }
    Array& operator=(const Array& array) { return var.emplace<Array>(array); }
    Array& operator=(Array&& array) { return var.emplace<Array>(std::move(array)); }
    void setNumStr(const char* val); // TODO: refactor to assign null on failure
    void operator=(short val);
    void operator=(int val);
    void operator=(long val);
    void operator=(long long val);
    void operator=(unsigned short val);
    void operator=(unsigned val);
    void operator=(unsigned long val);
    void operator=(unsigned long long val);
    void operator=(double val);
    std::string& operator=(std::string_view val) { return var.emplace<std::string>(val); }
    std::string& operator=(std::string&& val) { return var.emplace<std::string>(std::move(val)); }
    std::string& operator=(const char* val_) { return operator=(std::string_view(val_)); }

    [[nodiscard]]
    constexpr VType getType() const noexcept {
        VType ret = VNULL;
        var.visit(univalue_detail::visitor{
           [&](bool b) { ret = b ? VTRUE : VFALSE; },
           [&](const NumStr &) { ret = VNUM; },
           [&](const std::string &) { ret = VSTR; },
           [&](const Object &) { ret = VOBJ; },
           [&](const Array &) { ret = VARR; },
        });
        return ret;
    }

    [[nodiscard]]
    constexpr const std::string& getValStr() const noexcept {
        switch (type()) {
        case VSTR: return var.get<std::string>();
        case VNUM: return var.get<NumStr>();
        default: return emptyVal;
        }
    }

    /**
     * VOBJ/VARR: Returns whether the object/array is empty.
     * Other types: Returns true.
     *
     * Complexity: constant.
     *
     * Compatible with the upstream UniValue API.
     */
    [[nodiscard]]
    bool empty() const noexcept {
        switch (type()) {
        case VOBJ: return var.get<Object>().empty();
        case VARR: return var.get<Array>().empty();
        default: return true;
        }
    }

    /**
     * VOBJ/VARR: Returns the size of the object/array.
     * Other types: Returns zero.
     *
     * Complexity: constant.
     *
     * Compatible with the upstream UniValue API.
     */
    [[nodiscard]]
    size_type size() const noexcept {
        switch (type()) {
        case VOBJ: return var.get<Object>().size();
        case VARR: return var.get<Array>().size();
        default: return 0;
        }
    }

    [[nodiscard]]
    constexpr bool getBool() const noexcept { return isTrue(); }

    /**
     * VOBJ: Returns a reference to the first value associated with the key,
     *       or UniValue::Null if the key does not exist.
     * Other types: Returns UniValue::Null.
     *
     * The returned reference follows the iterator invalidation rules of the underlying vector.
     *
     * Complexity: linear in number of elements.
     *
     * Compatible with the upstream UniValue API.
     *
     * If you want to distinguish between null values and missing keys, please use locate() instead.
     * If you want an exception thrown on missing keys, please use at() instead.
     */
    [[nodiscard]]
    const UniValue& operator[](std::string_view key) const noexcept;

    /**
     * VOBJ: Returns a reference to the value at the numeric index (regardless of key),
     *       or UniValue::Null if index >= object size.
     * VARR: Returns a reference to the element at the index,
     *       or UniValue::Null if index >= array size.
     * Other types: Returns UniValue::Null.
     *
     * The returned reference follows the iterator invalidation rules of the underlying vector.
     *
     * Complexity: constant.
     *
     * Compatible with the upstream UniValue API.
     *
     * To access the first or last value, consider using front() or back() instead.
     * If you want an exception thrown on missing indices, please use at() instead.
     */
    [[nodiscard]]
    const UniValue& operator[](size_type index) const noexcept;

    /**
     * Returns whether the UniValues are of the same type and contain equal data.
     * Two objects/arrays are not considered equal if elements are ordered differently.
     *
     * Complexity: linear in the amount of data to compare.
     */
    [[nodiscard]]
    bool operator==(const UniValue& other) const noexcept { return var == other.var; }

    /**
     * Returns whether the UniValues are not of the same type or contain unequal data.
     * Two objects/arrays are not considered equal if elements are ordered differently.
     *
     * Complexity: linear in the amount of data to compare.
     */
    [[nodiscard]]
    bool operator!=(const UniValue& other) const noexcept { return !(*this == other); }

    /**
     * VOBJ: Returns a reference to the first value (regardless of key),
     *       or UniValue::Null if the object is empty.
     * VARR: Returns a reference to the first element,
     *       or UniValue::Null if the array is empty.
     * Other types: Returns UniValue::Null.
     *
     * The returned reference follows the iterator invalidation rules of the underlying vector.
     *
     * Complexity: constant.
     */
    [[nodiscard]]
    const UniValue& front() const noexcept;

    /**
     * VOBJ: Returns a reference to the last value (regardless of key),
     *       or UniValue::Null if the object is empty.
     * VARR: Returns a reference to the last element,
     *       or UniValue::Null if the array is empty.
     * Other types: Returns UniValue::Null.
     *
     * The returned reference follows the iterator invalidation rules of the underlying vector.
     *
     * Complexity: constant.
     */
    [[nodiscard]]
    const UniValue& back() const noexcept;

    /**
     * VOBJ: Returns a pointer to the first value associated with the key,
     *       or nullptr if the key does not exist.
     * Other types: Returns nullptr.
     *
     * The returned pointer follows the iterator invalidation rules of the underlying vector.
     *
     * Complexity: linear in the number of elements.
     *
     * If you want to treat missing keys as null values, please use the [] operator instead.
     * If you want an exception thrown on missing keys, please use at() instead.
     */
    [[nodiscard]]
    const UniValue* locate(std::string_view key) const noexcept;
    [[nodiscard]]
    UniValue* locate(std::string_view key) noexcept;

    /**
     * VOBJ: Returns a reference to the first value associated with the key,
     *       or throws std::out_of_range if the key does not exist.
     * Other types: Throws std::domain_error.
     *
     * The returned reference follows the iterator invalidation rules of the underlying vector.
     *
     * Complexity: linear in number of elements.
     *
     * If you don't want an exception thrown, please use locate() or the [] operator instead.
     */
    const UniValue& at(std::string_view key) const;
    UniValue& at(std::string_view key);

    /**
     * VOBJ: Returns a reference to the value at the numeric index (regardless of key),
     *       or throws std::out_of_range if index >= object size.
     * VARR: Returns a reference to the element at the index,
     *       or throws std::out_of_range if index >= array size.
     * Other types: Throws std::domain_error.
     *
     * The returned reference follows the iterator invalidation rules of the underlying vector.
     *
     * Complexity: constant.
     *
     * If you don't want an exception thrown, please use the [] operator instead.
     */
    const UniValue& at(size_type index) const;
    UniValue& at(size_type index);

    /**
     * Returns whether the value's type is the same as the given type.
     * If a type bitmask is given, returns whether the value's type is any of the given types.
     *
     * Complexity: constant.
     */
    [[nodiscard]]
    constexpr bool is(int types) const noexcept { return type() & types; }

    [[nodiscard]]
    constexpr bool isNull() const noexcept { return is(VNULL); }
    [[nodiscard]]
    constexpr bool isFalse() const noexcept { return is(VFALSE); }
    [[nodiscard]]
    constexpr bool isTrue() const noexcept { return is(VTRUE); }
    [[nodiscard]]
    constexpr bool isBool() const noexcept { return is(MBOOL); }
    [[nodiscard]]
    constexpr bool isObject() const noexcept { return is(VOBJ); }
    [[nodiscard]]
    constexpr bool isArray() const noexcept { return is(VARR); }
    [[nodiscard]]
    constexpr bool isNum() const noexcept { return is(VNUM); }
    [[nodiscard]]
    constexpr bool isStr() const noexcept { return is(VSTR); }

    /**
     * Returns the JSON string representation of the provided value.
     *
     * @param value - The type of `value` can be the generic UniValue, or a more
     * specific type: UniValue::Object, UniValue::Array, or std::string.
     *
     * @param prettyIndent - This optional argument indicates the number of spaces for
     * indentation in pretty formatting. Use 0 (default) to disable pretty formatting
     * and use compact formatting instead. Note that pretty formatting only affects
     * arrays and objects.
     *
     * @param reserve - This optional argument is for performance. If you know ahead of
     * time approximately how large the serialized JSON will be, preallocating `reserve`
     * bytes to the returned string may save CPU cycles.
     */
    template<typename Value>
    static std::string stringify(const Value& value, unsigned int prettyIndent = 0, std::size_t reserve = 1024) {
        std::string s; // we do it this way for RVO to work on all compilers
        Stream ss{s};
        if (reserve) s.reserve(reserve);
        stringify(ss, value, prettyIndent, 0);
        return s;
    }

    /**
     * Parses a NUL-terminated JSON string.
     *
     * If valid JSON, a pointer to the terminating NUL of the JSON is returned, and the object state represents the read value.
     * If invalid JSON, nullptr is returned, and the object is in a valid but unspecified state.
     *
     * (If the return value is cast to bool, the resulting boolean indicates success.)
     *
     * Compatible with the upstream UniValue API, although upstream simply returns a bool.
     *
     * Optional arg errpos: If specified, the pointer will be set to point to where parsing failed in the input string.
     * The pointer is only set to a valid value on failure, otherwise it is set to nullptr.
     */
    [[nodiscard]]
    const char* read(const char* raw, const char **errpos = nullptr);

    /**
     * Parses a JSON std::string.
     *
     * If valid JSON, true is returned, and the object state represents the read value.
     * If invalid JSON, false is returned, and the object is in a valid but unspecified state.
     *
     * Compatible with the upstream UniValue API.
     *
     * Optional arg errpos: If specified, the pointer will be set to the position where parsing failed in the input string.
     * The pointer is only set to the position on failure, otherwise it is set to std::string::npos.
     */
    [[nodiscard]]
    bool read(const std::string& raw, std::string::size_type *errpos = nullptr);

private:
    // "type tag" to differentiate a string containing a JSON numeric from a JSON string
    struct NumStr : std::string {
        using std::string::string;
        NumStr(const std::string &s) : std::string(s) {}
        NumStr(std::string &&s) noexcept : std::string(std::move(s)) {}
        NumStr & operator=(const std::string &s) {
            std::string::operator=(s);
            return *this;
        }
        NumStr & operator=(std::string &&s) noexcept {
            std::string::operator=(std::move(s));
            return *this;
        }
    };
    univalue_detail::variant<bool, NumStr, std::string, Object, Array> var;

    static const std::string emptyVal; ///< returned by getValStr() if this is not a VNUM or VSTR

    // Opaque type used for writing. This can be further optimized later.
    struct Stream {
        std::string & str; // this is a reference for RVO to always work in UniValue::stringify()
        void put(char c) { str.push_back(c); }
        void put(char c, size_t nFill) { str.append(nFill, c); }
        Stream & operator<<(std::string_view s) { str.append(s); return *this; }
    };
    static inline void startNewLine(Stream & stream, unsigned int prettyIndent, unsigned int indentLevel);
    static void jsonEscape(Stream & stream, std::string_view inString);

    static void stringify(Stream & stream, const UniValue& value, unsigned int prettyIndent, unsigned int indentLevel);
    static void stringify(Stream & stream, const UniValue::Object& value, unsigned int prettyIndent, unsigned int indentLevel);
    static void stringify(Stream & stream, const UniValue::Array& value, unsigned int prettyIndent, unsigned int indentLevel);
    static void stringify(Stream & stream, std::string_view value, unsigned int prettyIndent, unsigned int indentLevel);

    // Used internally by the integral operator=() overloads
    template<typename Int64>
    void setInt64(Int64 val);

public:
    // Strict type-specific getters, these throw std::runtime_error if the
    // value is of unexpected type

    bool get_bool() const;

    int get_int() const; // may also throw if out of range
    int64_t get_int64() const; // may also throw if out of range
    unsigned get_uint() const; // may also throw if out of range or value is negative
    uint64_t get_uint64() const; // may also throw if out of range or value is negative

    double get_real() const;

    /**
     * VSTR: Returns a std::string reference to this value.
     * Other types: Throws std::runtime_error.
     *
     * Destroying the string (e.g. destroying the UniValue wrapper or
     * assigning a different type to it) invalidates the returned reference.
     *
     * Complexity: constant.
     *
     * Compatible with the upstream UniValue API.
     */
    const std::string& get_str() const;
    std::string& get_str();

    /**
     * VOBJ: Returns a UniValue::Object reference to this value.
     * Other types: Throws std::runtime_error.
     *
     * Destroying the object (e.g. destroying the UniValue wrapper or
     * assigning a different type to it) invalidates the returned reference.
     *
     * Complexity: constant.
     *
     * Compatible with the upstream UniValue API,
     * but with a different return type.
     */
    const Object& get_obj() const;
    Object& get_obj();

    /**
     * VARR: Returns a UniValue::Array reference to this value.
     * Other types: Throws std::runtime_error.
     *
     * Destroying the array (e.g. destroying the UniValue wrapper or
     * assigning a different type to it) invalidates the returned reference.
     *
     * Complexity: constant.
     *
     * Compatible with the upstream UniValue API,
     * but with a different return type.
     */
    const Array& get_array() const;
    Array& get_array();

    // Misc utility functions
    [[nodiscard]]
    constexpr VType type() const noexcept { return getType(); }

    /**
     * Returns the human-readable name of the JSON value type.
     *
     * Compatible with the upstream UniValue API (but note VBOOL has been replaced with VFALSE and VTRUE).
     */
    [[nodiscard]]
    static const char *typeName(UniValue::VType t) noexcept;

    /**
     * Returns the human-readable name of the composite JSON value type bitmask.
     * Argument must be a bitmask consisting of one or more UniValue::VType elements.
     */
    [[nodiscard]]
    static std::string typeName(int t);

    /**
     * @brief Version - query the library version
     * @return A tuple of library (major,minor,revision)
     */
    [[nodiscard]]
    static std::tuple<int, int, int> version();

    /// Rerpresents the "null" UniValue. A reference to this singleton is returned from some methods to indicate
    /// not found, etc. Its state is identical to a default-constructed UniValue instance.
    static const UniValue Null;
};
