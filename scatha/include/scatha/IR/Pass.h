#ifndef SCATHA_IR_PASS_H_
#define SCATHA_IR_PASS_H_

#include <any>
#include <functional>
#include <span>
#include <string>
#include <string_view>

#include <utl/hashtable.hpp>

#include <scatha/Common/Base.h>
#include <scatha/IR/Fwd.h>

namespace scatha::ir {

/// Different pass categories
enum class PassCategory {
    ///
    Analysis,

    /// Canonicalization passes bring the IR in canonical form
    Canonicalization,

    ///
    Simplification,

    ///
    Optimization,

    /// We put experimental passes here so we can access them through the pass
    /// manager but we can ignore them in the automatic pass tests
    Experimental,

    /// For now here we have 'print' and 'foreach'
    Other
};

/// Abstract base class for pass arguments
class PassArgument {
public:
    virtual ~PassArgument() = default;

    std::unique_ptr<PassArgument> clone() const { return doClone(); }

    /// Matches the string \p value against this argument.
    /// \Returns `true` on success and `false` on failure
    bool match(std::string_view value) { return doMatch(value); }

    /// \Returns the argument as concrete type \p T
    template <typename T>
    T get() const {
        return std::any_cast<T>(doGet());
    }

protected:
    PassArgument() = default;
    PassArgument(PassArgument const&) = default;

private:
    virtual std::unique_ptr<PassArgument> doClone() const = 0;
    virtual bool doMatch(std::string_view arg) = 0;
    virtual std::any doGet() const = 0;
};

/// CRTP mixin that derives the `doClone()` method
template <typename Derived>
class PipelineArgImpl: public PassArgument {
private:
    std::unique_ptr<PassArgument> doClone() const override {
        return std::make_unique<Derived>(static_cast<Derived const&>(*this));
    }
};

/// Boolean pipeline argument
class PassFlagArgument: public PipelineArgImpl<PassFlagArgument> {
public:
    PassFlagArgument(bool value): _value(value) {}
    PassFlagArgument(PassFlagArgument const&) = default;

    bool value() const { return _value; }

private:
    bool doMatch(std::string_view arg) override;
    std::any doGet() const override { return value(); }

    bool _value = false;
};

///
class PassNumericArgument: public PipelineArgImpl<PassNumericArgument> {
public:
    PassNumericArgument(double value): _value(value) {}
    PassNumericArgument(PassNumericArgument const&) = default;

    double value() const { return _value; }

private:
    bool doMatch(std::string_view arg) override;
    std::any doGet() const override { return value(); }

    double _value;
};

///
class PassStringArgument: public PipelineArgImpl<PassStringArgument> {
public:
    PassStringArgument(std::string value): _value(std::move(value)) {}
    PassStringArgument(PassStringArgument const&) = default;

    std::string const& value() const { return _value; }

private:
    bool doMatch(std::string_view arg) override;
    std::any doGet() const override { return value(); }

    std::string _value;
};

///
template <typename E>
class PassEnumArgument: public PipelineArgImpl<PassEnumArgument<E>> {
public:
    PassEnumArgument(E value): _value(value) {}
    PassEnumArgument(PassEnumArgument const&) = default;

    E value() const { return _value; }

private:
    bool doMatch(std::string_view) override { SC_UNIMPLEMENTED(); }

    std::any doGet() const override { return value(); }

    static utl::hashmap<std::string, E> const map;

    E _value;
};

namespace passParameterTypes {

using Numeric = std::pair<std::string, PassNumericArgument>;
using Flag = std::pair<std::string, PassFlagArgument>;
using String = std::pair<std::string, PassStringArgument>;
template <typename E>
using Enum = std::pair<std::string, PassEnumArgument<E>>;

} // namespace passParameterTypes

/// Result type for `PassArgumentMap::match()`
enum class ArgumentMatchResult { Success, UnknownArgument, BadValue };

/// Maps argument names to arguments
class PassArgumentMap {
public:
    using Map = utl::hashmap<std::string, std::unique_ptr<PassArgument>>;

    PassArgumentMap() = default;

    explicit PassArgumentMap(Map params): map(std::move(params)) {}

    template <std::derived_from<PassArgument>... Params>
    PassArgumentMap(std::pair<std::string, Params>... params):
        PassArgumentMap(makeMap(std::move(params)...)) {}

    PassArgumentMap(PassArgumentMap&&) noexcept = default;

    PassArgumentMap& operator=(PassArgumentMap&&) noexcept = default;

    PassArgumentMap(PassArgumentMap const& other): map(copyMap(other.map)) {}

    PassArgumentMap& operator=(PassArgumentMap const& other) {
        if (this != &other) {
            map = copyMap(other.map);
        }
        return *this;
    }

    template <typename T>
    T get(std::string_view argName) const {
        auto itr = map.find(argName);
        return itr != map.end() ? itr->second->get<T>() : T{};
    }

    void insert(std::string name, std::unique_ptr<PassArgument> arg) {
        map.insert({ std::move(name), std::move(arg) });
    }

    ///
    ArgumentMatchResult match(std::string_view key, std::string_view value);

private:
    template <typename... Params>
    static Map makeMap(std::pair<std::string, Params>&&... params) {
        utl::hashmap<std::string, std::unique_ptr<PassArgument>> map;
        bool result =
            (map.insert({ std::move(params.first),
                          std::make_unique<Params>(std::move(params.second)) })
                 .second &&
             ...);
        SC_ASSERT(result, "Duplicate parameter name");
        return map;
    }

    static Map copyMap(Map const& map);

    Map map;
};

/// Common base class of `LocalPass` and `GlobalPass`
class PassBase {
public:
    /// The name of the pass
    std::string const& name() const { return _name; }

    /// The category of this pass
    PassCategory category() const { return _cat; }

    /// \Returns the pass arguments
    PassArgumentMap const& arguments() const { return _args; }

    /// Matches the argument at \p key against \p value
    ArgumentMatchResult matchArgument(std::string_view key,
                                      std::string_view value) {
        return _args.match(key, value);
    }

protected:
    PassBase(): PassBase({}, {}, PassCategory::Other) {}

    PassBase(PassArgumentMap args, std::string name, PassCategory category):
        _args(std::move(args)), _name(std::move(name)), _cat(category) {
        if (_name.empty()) {
            _name = "anonymous";
        }
    }

private:
    PassArgumentMap _args;
    std::string _name;
    PassCategory _cat;
};

/// Represents a local pass that operates on a single function
class LocalPass: public PassBase {
public:
    /// The signature of the pass type
    using Sig = bool(ir::Context&, ir::Function&, PassArgumentMap const&);

    /// Construct an empty local pass. Empty passes are invalid an can not be
    /// executed.
    LocalPass() = default;

    /// Construct a named local pass from a function without pass parameters
    template <std::invocable<ir::Context&, ir::Function&> P>
        requires std::same_as<
                     std::invoke_result_t<P, ir::Context&, ir::Function&>,
                     bool> &&
                 (!std::derived_from<std::remove_cvref_t<P>, PassBase>)
    LocalPass(P&& p, PassArgumentMap params = {}, std::string name = {},
              PassCategory category = PassCategory::Other):
        LocalPass(
            [p = std::forward<P>(p)](ir::Context& ctx, ir::Function& F,
                                     PassArgumentMap const&) {
                return std::invoke(p, ctx, F);
            },
            std::move(params), std::move(name), category) {}

    /// Construct a named local pass from a function
    LocalPass(std::function<Sig> p, PassArgumentMap params = {},
              std::string name = {},
              PassCategory category = PassCategory::Other):
        PassBase(std::move(params), std::move(name), category),
        p(std::move(p)) {}

    /// Invoke the pass
    bool operator()(ir::Context& ctx, ir::Function& function) const {
        return p(ctx, function, arguments());
    }

    /// \Returns `true` is the pass is non-empty
    operator bool() const { return !!p; }

private:
    std::function<Sig> p;
};

/// Represents a global pass that operates on an entire module
class GlobalPass: public PassBase {
public:
    using Sig = bool(ir::Context&, ir::Module&, PassArgumentMap const&,
                     LocalPass);

    /// Construct an empty global pass. Empty passes are invalid an can not be
    /// executed.
    GlobalPass() = default;

    GlobalPass(Sig* p): GlobalPass(p, {}) {}

    /// Construct a named global pass from a function
    GlobalPass(std::function<Sig> p, PassArgumentMap params = {},
               std::string name = {},
               PassCategory category = PassCategory::Other):
        PassBase(std::move(params), std::move(name), category),
        p(std::move(p)) {}

    /// Invoke the pass
    bool operator()(ir::Context& ctx, ir::Module& mod,
                    LocalPass localPass) const {
        return p(ctx, mod, arguments(), std::move(localPass));
    }

    /// \Returns `true` is the pass is non-empty
    operator bool() const { return !!p; }

private:
    std::function<Sig> p;
};

} // namespace scatha::ir

#endif // SCATHA_IR_PASS_H_
