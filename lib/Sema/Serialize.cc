#include "Sema/Serialize.h"

#include <charconv>
#include <functional>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/function_view.hpp>

#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using namespace ranges::views;

/// MARK: - Type to string conversion

static void serializeTypenameImpl(Type const& type, std::ostream& str);

/// Serializes the fully qualified name of \p type
static std::string serializeTypename(Type const* type) {
    std::stringstream sstr;
    serializeTypenameImpl(*type, sstr);
    return std::move(sstr).str();
}

/// Recursively traverses all nested types and all parent scopes to serialize
/// the fully qualified typename
static void serializeTypenameImpl(Type const& type, std::ostream& str) {
    auto ptrLikeImpl = [&](std::string_view kind, PtrRefTypeBase const& ptr) {
        str << kind;
        if (ptr.base().isMut()) {
            str << "mut ";
        }
        serializeTypenameImpl(*ptr.base(), str);
    };
    // clang-format off
    SC_MATCH (type) {
        [&](ObjectType const& type) {
            auto rec = [&](auto& rec, auto* scope) {
                if (!scope || scope->kind() == ScopeKind::Global) {
                    return;
                }
                rec(rec, scope->parent());
                str << scope->name() << ".";
            };
            rec(rec, type.parent());
            str << type.name();
        },
        [&](ArrayType const& arr) {
            str << "[";
            serializeTypenameImpl(*arr.elementType(), str);
            if (!arr.isDynamic()) {
                str << ", " << arr.count();
            }
            str << "]";
        },
        [&](ReferenceType const& ref) { ptrLikeImpl("&", ref); },
        [&](RawPtrType const& ptr) { ptrLikeImpl("*", ptr); },
        [&](UniquePtrType const& ptr) { ptrLikeImpl("*unique ", ptr); },
    }; // clang-format on
}

/// MARK: - String to type conversion

/// Parses a type name serialized by `serializeTypename()` and looks it up in
/// the symbol table \p sym \Returns the looked up type or null if an error
/// occurs
static Type const* parseTypename(SymbolTable& sym, std::string_view text);

namespace {

/// Token structure to represent the types that can appear in a qualified
/// typename
struct TypenameToken {
    enum Type {
        Ref,
        Ptr,
        Mut,
        Unique,
        Dot,
        Comma,
        ID,
        OpenBracket,
        CloseBracket,
        END
    };

    Type type;
    std::string_view text;
};

/// On demand lexer with lookahead feature to lex typename tokens
struct TypenameLexer {
    using Token = TypenameToken;

    std::string_view text;
    std::optional<Token> lookahead;

    TypenameLexer(std::string_view text): text(text) {}

    bool isPunctuation(char c) { return ranges::contains("&*.,[]", c); }

    Token peek() {
        if (!lookahead) {
            lookahead = get();
        }
        return *lookahead;
    }

    Token get() {
        if (lookahead) {
            auto tok = *lookahead;
            lookahead = std::nullopt;
            return tok;
        }
        while (!text.empty() && std::isspace(text.front())) {
            inc();
        }
        if (text.empty()) {
            return Token{ Token::END, {} };
        }
#define PUNCTUATION_CASE(CHAR, TYPE)                                           \
    case CHAR: {                                                               \
        Token tok{ Token::TYPE, text.substr(0, 1) };                           \
        inc();                                                                 \
        return tok;                                                            \
    }                                                                          \
        (void)0
        switch (text.front()) {
            PUNCTUATION_CASE('&', Ref);
            PUNCTUATION_CASE('*', Ptr);
            PUNCTUATION_CASE('.', Dot);
            PUNCTUATION_CASE(',', Comma);
            PUNCTUATION_CASE('[', OpenBracket);
            PUNCTUATION_CASE(']', CloseBracket);
        default: {
            size_t index = 0;
            while (index < text.size() && !std::isspace(text[index]) &&
                   !isPunctuation(text[index]))
            {
                ++index;
            }
            auto res = text.substr(0, index);
            inc(index);
            if (res == "mut") {
                return Token{ Token::Mut, res };
            }
            if (res == "unique") {
                return Token{ Token::Unique, res };
            }
            return Token{ Token::ID, res };
        }
        }
#undef PUNCTUATION_CASE
    }

    void inc(size_t count = 1) {
        SC_EXPECT(text.size() >= count);
        text = text.substr(count);
    }
};

struct TypenameParser {
    using Token = TypenameToken;

    SymbolTable& sym;
    TypenameLexer lex;

    TypenameParser(SymbolTable& sym, std::string_view text):
        sym(sym), lex(text) {}

    Type const* parse() {
        switch (lex.peek().type) {
        case Token::Ref:
            return parseRef();
        case Token::Ptr:
            return parsePtr();
        case Token::ID:
            return parseID();
        case Token::OpenBracket:
            return parseArray();
        default:
            return nullptr;
        }
    }

    QualType parseQualType() {
        auto mut = Mutability::Const;
        if (lex.peek().type == Token::Mut) {
            lex.get();
            mut = Mutability::Mutable;
        }
        auto* baseType = dyncast<ObjectType const*>(parse());
        if (!baseType) {
            return nullptr;
        }
        return QualType(baseType, mut);
    }

    ReferenceType const* parseRef() {
        lex.get();
        return sym.reference(parseQualType());
    }

    PointerType const* parsePtr() {
        lex.get();
        if (lex.peek().type == Token::Unique) {
            lex.get();
            return sym.uniquePointer(parseQualType());
        }
        return sym.pointer(parseQualType());
    }

    ///
    Entity* findEntity(Scope& scope, std::string_view name) {
        auto entities = scope.findEntities(name);
        if (entities.empty()) {
            return nullptr;
        }
        /// TODO: Find a better way to select single entity
        return entities.front();
    }

    ///
    Entity* findEntity(std::string_view name) {
        auto entities = sym.unqualifiedLookup(name);
        if (entities.empty()) {
            return nullptr;
        }
        /// TODO: Find a better way to select single entity
        return entities.front();
    }

    ObjectType const* parseID() {
        auto tok = lex.get();
        Entity* entity = findEntity(tok.text);
        while (true) {
            if (!entity) {
                return nullptr;
            }
            auto next = lex.peek();
            if (next.type != Token::Dot) {
                return dyncast<ObjectType const*>(entity);
            }
            lex.get();
            auto* scope = dyncast<Scope*>(entity);
            if (!scope) {
                return nullptr;
            }
            auto tok = lex.get();
            entity = findEntity(*scope, tok.text);
        }
    }

    ArrayType const* parseArray() {
        lex.get();
        auto* elemType = dyncast<ObjectType const*>(parse());
        auto next = lex.get();
        if (next.type == Token::CloseBracket) {
            return sym.arrayType(elemType);
        }
        if (next.type == Token::Comma) {
            size_t count = 0;
            auto text = lex.get().text;
            auto res =
                std::from_chars(text.data(), text.data() + text.size(), count);
            if (res.ec == std::errc{}) {
                return sym.arrayType(elemType, count);
            }
            return nullptr;
        }
        return nullptr;
    }
};

} // namespace

static Type const* parseTypename(SymbolTable& sym, std::string_view text) {
    TypenameParser parser(sym, text);
    if (auto* type = parser.parse()) {
        return type;
    }
    throw std::runtime_error("Failed to parse type");
}

/// MARK: - serialize()

using namespace nlohmann;

namespace scatha::sema {

static void to_json(json& j, Entity const& entity);

static void to_json_impl(json&, Entity const&) { SC_UNREACHABLE(); }

static json serializeChildren(
    Entity const& entity,
    utl::function_view<bool(Entity const*)> filterFn = [](auto*) {
        return true;
    }) {
    auto children = [&] {
        if (auto* scope = dyncast<Scope const*>(&entity)) {
            auto children = scope->entities() | transform(stripAlias) |
                            filter(filterFn) |
                            ranges::to<utl::hashset<Entity const*>>;
            for (auto* entity: scope->children() | filter(filterFn)) {
                children.insert(entity);
            }
            return children;
        }
        if (auto* os = dyncast<OverloadSet const*>(&entity)) {
            return *os | filter(filterFn) |
                   ranges::to<utl::hashset<Entity const*>>;
        }
        return utl::hashset<Entity const*>{};
    }();
    json j;
    for (auto [index, child]: children | enumerate) {
        j[index] = *child;
    }
    return j;
}

static void to_json_impl(json& j, GlobalScope const& scope) {
    j = serializeChildren(scope, /* filter = */ [](Entity const* child) {
        // clang-format off
        return SC_MATCH (*child) {
            [](Function const& function) { return function.isNative(); },
            [](StructType const&) { return true; },
            /// We serialize foreign libraries because targets that import us
            /// must know about out foreign libraries
            [](ForeignLibrary const&) { return true; },
            [](Entity const&) { return false; },
        }; // clang-format on
    });
}

static void to_json_impl(json& j, Function const& function) {
    j["return_type"] = serializeTypename(function.returnType());
    j["argument_types"] = function.argumentTypes() |
                          transform(serializeTypename);
}

static void to_json_impl(json& j, StructType const& type) {
    j["children"] = serializeChildren(type, [](Entity const* child) {
        return isa<StructType>(child) || isa<Function>(child) ||
               isa<Variable>(child);
    });
    j["size"] = type.size();
    j["align"] = type.align();
}

static void to_json_impl(json& j, Variable const& var) {
    j["type"] = serializeTypename(var.type());
    j["mutable"] = var.isMut();
    if (isa<StructType>(var.parent())) {
        j["index"] = var.index();
    }
}

static void to_json_impl(json& j, ForeignLibrary const& lib) {
    j["libname"] = lib.libName();
}

static void to_json(json& j, Entity const& entity) {
    j["entity_type"] = toString(entity.entityType());
    j["name"] = std::string(entity.name());
    visit(entity, [&j](auto& entity) { to_json_impl(j, entity); });
}

} // namespace scatha::sema

void sema::serialize(SymbolTable const& sym, std::ostream& ostream) {
    json j = sym.globalScope();
    ostream << std::setw(2) << j << std::endl;
}

/// MARK: - deserialize()

static std::optional<EntityType> toEntityType(json const& j) {
    static utl::hashmap<std::string_view, EntityType> const map = {
#define SC_SEMA_ENTITY_DEF(Type, _)                                            \
    { std::string_view(#Type), EntityType::Type },
#include "Sema/Lists.def"
    };
    auto str = j.get<std::string_view>();
    auto itr = map.find(str);
    if (itr != map.end()) {
        return itr->second;
    }
    return std::nullopt;
}

namespace {

/// Used for tag dispatch during parsing
template <typename>
struct Tag {};

/// Base class of the deserialization context that provides a type map
class TypeMapBase {
public:
    /// Maps \p obj to the type \p type. Throws if \p obj is already mapped
    void insertType(json const& obj, StructType* type) {
        if (!map.insert({ &obj, type }).second) {
            throw std::runtime_error("Failed to map type");
        }
    }

    /// Retrieves the struct type corresponding to \p obj or throws if nonesuch
    /// is found
    StructType* getType(json const& obj) const {
        auto itr = map.find(&obj);
        if (itr != map.end()) {
            return itr->second;
        }
        throw std::runtime_error("Failed to retrieve type");
    }

private:
    utl::hashmap<json const*, StructType*> map;
};

struct DeserializeContext: TypeMapBase {
    /// The symbol table do deserialize into. All deserialized entities be
    /// written into the current scope and according child scopes
    SymbolTable& sym;

    DeserializeContext(SymbolTable& sym): sym(sym) {}

    /// Performs the deserialization
    void run(json const& j) {
        preparseTypes(j);
        parseEntities(j);
    }

    /// Performs a DFS over the JSON array and declares all encountered struct
    /// types to the symbol table. Types are parsed before all other entities.
    /// Dependencies between types via member variables are resolved because
    /// member variables are parsed later and add themselves to their parent
    /// structs
    void preparseTypes(json const& j) {
        parseArray(j, [&](Tag<StructType>, json const& obj) {
            auto* type =
                sym.declareStructureType(get<std::string>(obj, "name"));
            insertType(obj, type);
            type->setSize(get<size_t>(obj, "size"));
            type->setAlign(get<size_t>(obj, "align"));
            sym.withScopeCurrent(type,
                                 [&] { preparseTypes(get(obj, "children")); });
        });
    }

    /// Because types are parsed in a prior step we only forward to our children
    void parseImpl(Tag<StructType>, json const& obj) {
        auto* type = getType(obj);
        sym.withScopeCurrent(type,
                             [&] { parseEntities(get(obj, "children")); });
    }

    ///
    void parseImpl(Tag<Function>, json const& obj) {
        auto argTypes = get(obj, "argument_types") |
                        transform([&](json const& j) {
                            return parseTypename(sym, j.get<std::string>());
                        }) |
                        ToSmallVector<>;
        auto* retType =
            parseTypename(sym, get<std::string>(obj, "return_type"));
        sym.declareFunction(get<std::string>(obj, "name"),
                            FunctionSignature(std::move(argTypes), retType));
    }

    ///
    void parseImpl(Tag<Variable>, json const& obj) {
        auto* type = parseTypename(sym, get<std::string>(obj, "type"));
        using enum Mutability;
        auto mut = get<bool>(obj, "mutable") ? Mutable : Const;
        auto* var =
            sym.defineVariable(get<std::string>(obj, "name"), type, mut);
        if (auto index = tryGet<size_t>(obj, "index")) {
            auto* type = dyncast<StructType*>(&sym.currentScope());
            // TODO: CHECK type is not null
            type->setMemberVariable(*index, var);
        }
    }

    /// Base case
    template <typename T>
    void parseImpl(Tag<T>, json const&) {}

    ///
    void parseImpl(Tag<ForeignLibrary>, json const& obj) {
        auto libname = get<std::string>(obj, "libname");
        sym.importForeignLib(libname);
    }

    /// Performs a DFS over the JSON array and declares all encountered entities
    /// but struct types to the symbol table
    void parseEntities(json const& j) {
        parseArray(j, [this]<typename T>(Tag<T> tag, json const& child) {
            parseImpl(tag, child);
        });
    }

    /// # Helper functions

    /// Parses the type of the JSON object \p obj and dispatches to \p callback
    /// with the corresponding tag
    bool doParse(json const& obj, auto callback) {
        auto type = toEntityType(obj["entity_type"]);
        auto callbackWithDefault = utl::overload{ callback, [](auto...) {} };
        switch (type.value()) {
#define SC_SEMA_ENTITY_DEF(Type, _)                                            \
    case EntityType::Type:                                                     \
        callbackWithDefault(Tag<Type>{}, obj);                                 \
        return true;
#include <scatha/Sema/Lists.def>
        case EntityType::_count:
            return false;
        }
        SC_UNREACHABLE();
    }

    /// Invokes `doParse(elem, callback)` for every element `elem` in the array
    /// \p array
    bool parseArray(json const& array, auto callback) {
        bool result = true;
        for (auto& elem: array) {
            result &= doParse(elem, callback);
        }
        return result;
    }

    /// \Returns the JSON subobject named \p field or throws if the name is not
    /// found
    json const& get(json const& obj, std::string_view field) {
        return obj.at(field);
    }

    /// \Returns a pointer to the JSON subobject named \p field or null if the
    /// name is not found
    json const* tryGet(json const& obj, std::string_view field) {
        auto itr = obj.find(field);
        if (itr != obj.end()) {
            return &*itr;
        }
        return nullptr;
    }

    /// \Returns the JSON subobject named \p field as static type \p T or throws
    /// if the name is not found
    template <typename T>
    T get(json const& obj, std::string_view field) {
        return get(obj, field).get<T>();
    }

    /// \Returns the JSON subobject named \p field as static type \p T or
    /// `std::nullopt` if the name is not found
    template <typename T>
    std::optional<T> tryGet(json const& obj, std::string_view field) {
        if (auto* j = tryGet(obj, field)) {
            return j->get<T>();
        }
        return std::nullopt;
    }
};

} // namespace

bool sema::deserialize(SymbolTable& sym, std::istream& istream) {
    try {
        json j = json::parse(istream);
        DeserializeContext ctx(sym);
        ctx.run(j);
        return true;
    }
    catch (std::exception const&) {
        return false;
    }
}
