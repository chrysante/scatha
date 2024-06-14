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

#include "Common/Utility.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/LifetimeMetadata.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using namespace ranges::views;
using nlohmann::json;

/// MARK: - Enum serialization and deserialization

namespace {

template <typename Enum>
struct EnumSerializer;

template <typename Enum>
    requires requires { EnumSerializer<Enum>::map; }
void serializeEnum(json& j, Enum e) {
    auto const& map = EnumSerializer<Enum>::map;
    auto it = std::find_if(map.begin(), map.end(),
                           [e](auto const& p) { return p.first == e; });

    if (it == map.end()) {
        throw std::runtime_error("Failed to serialize enum");
    }
    j = it->second;
}

template <typename Enum>
    requires requires { EnumSerializer<Enum>::map; }
void deserializeEnum(json const& j, Enum& e) {
    auto const& map = EnumSerializer<Enum>::map;
    auto it = std::find_if(map.begin(), map.end(),
                           [&j](auto const& p) { return p.second == j; });
    if (it == map.end()) {
        throw std::runtime_error("Failed to deserialize enum");
    }
    e = it->first;
}

#define SERIALIZE_ENUM_BEGIN(ENUM_TYPE)                                        \
    template <>                                                                \
    struct EnumSerializer<ENUM_TYPE> {                                         \
        inline static std::array const map = {

#define SERIALIZE_ENUM_END()                                                   \
    }                                                                          \
    ;                                                                          \
    }                                                                          \
    ;

#define SERIALIZE_ENUM_ELEM(ENUM_KEY, VALUE)                                   \
    std::make_pair(ENUM_KEY, json(VALUE)),

/// User code

SERIALIZE_ENUM_BEGIN(EntityType)
#define SC_SEMA_ENTITY_DEF(Type, ...)                                          \
    SERIALIZE_ENUM_ELEM(EntityType::Type, #Type)
#include "Sema/Lists.def.h"
SERIALIZE_ENUM_END()

SERIALIZE_ENUM_BEGIN(AccessControl)
#define SC_SEMA_ACCESS_CONTROL_DEF(Kind, Spelling)                             \
    SERIALIZE_ENUM_ELEM(AccessControl::Kind, Spelling)
#include "Sema/Lists.def.h"
SERIALIZE_ENUM_END()

SERIALIZE_ENUM_BEGIN(SMFKind)
#define SC_SEMA_SMF_DEF(Kind, _) SERIALIZE_ENUM_ELEM(SMFKind::Kind, #Kind)
#include "Sema/Lists.def.h"
SERIALIZE_ENUM_END()

SERIALIZE_ENUM_BEGIN(LifetimeOperation::Kind)
SERIALIZE_ENUM_ELEM(LifetimeOperation::Kind::Trivial, "Trivial")
SERIALIZE_ENUM_ELEM(LifetimeOperation::Kind::Nontrivial, "Nontrivial")
SERIALIZE_ENUM_ELEM(LifetimeOperation::Kind::NontrivialInline,
                    "NontrivialInline")
SERIALIZE_ENUM_ELEM(LifetimeOperation::Kind::Deleted, "Deleted")
SERIALIZE_ENUM_END()

SERIALIZE_ENUM_BEGIN(FunctionKind)
SERIALIZE_ENUM_ELEM(FunctionKind::Native, "Native")
SERIALIZE_ENUM_ELEM(FunctionKind::Foreign, "Foreign")
SERIALIZE_ENUM_ELEM(FunctionKind::Generated, "Generated")
SERIALIZE_ENUM_END()

} // namespace

namespace scatha::sema {

void to_json(json& j, EntityType e) { serializeEnum(j, e); }
void from_json(json const& j, EntityType& e) { deserializeEnum(j, e); }
void to_json(json& j, AccessControl e) { serializeEnum(j, e); }
void from_json(json const& j, AccessControl& e) { deserializeEnum(j, e); }
void to_json(json& j, LifetimeOperation::Kind e) { serializeEnum(j, e); }
void from_json(json const& j, LifetimeOperation::Kind& e) {
    deserializeEnum(j, e);
}
void to_json(json& j, FunctionKind e) { serializeEnum(j, e); }
void from_json(json const& j, FunctionKind& e) { deserializeEnum(j, e); }

} // namespace scatha::sema

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
                if (!scope || isa<FileScope>(scope) ||  isa<GlobalScope>(scope))
                    return;
                rec(rec, scope->parent());
                str << scope->name() << ".";
            };
            rec(rec, type.parent());
            str << type.name();
        },
        [&](FunctionType const& type) {
            str << "(";
            for (bool first = true; auto* arg: type.argumentTypes()) {
                if (!first) str << ", ";
                first = false;
                str << serializeTypename(arg);
            }
            str << ") -> " << serializeTypename(type.returnType());
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
        Arrow,
        ID,
        OpenParan,
        CloseParan,
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

    bool isPunctuation(char c) { return ranges::contains("&*.,->()[]", c); }

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
#define PUNCTUATION_CASE(SPELLING, TYPE)                                       \
    else if (auto tok = getPunctuation(Token::TYPE, SPELLING)) { return *tok; }
        if (false) (void)0;
        PUNCTUATION_CASE("&", Ref)
        PUNCTUATION_CASE("*", Ptr)
        PUNCTUATION_CASE(".", Dot)
        PUNCTUATION_CASE(",", Comma)
        PUNCTUATION_CASE("->", Arrow)
        PUNCTUATION_CASE("(", OpenParan)
        PUNCTUATION_CASE(")", CloseParan)
        PUNCTUATION_CASE("[", OpenBracket)
        PUNCTUATION_CASE("]", CloseBracket)
        else {
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
#undef PUNCTUATION_CASE
    }

private:
    std::optional<Token> getPunctuation(Token::Type type,
                                        std::string_view spelling) {
        if (!text.starts_with(spelling)) {
            return std::nullopt;
        }
        Token tok{ type, text.substr(0, spelling.size()) };
        inc(spelling.size());
        return tok;
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
        case Token::OpenParan:
            return parseFunction();
        case Token::OpenBracket:
            return parseArray();
        case Token::ID:
            return parseID();
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
        if (!baseType) return nullptr;
        return QualType(baseType, mut);
    }

    ReferenceType const* parseRef() {
        lex.get();
        auto base = parseQualType();
        if (!base) return nullptr;
        return sym.reference(base);
    }

    PointerType const* parsePtr() {
        enum Kind { Raw, Unique };
        lex.get();
        Kind kind = Raw;
        if (lex.peek().type == Token::Unique) {
            lex.get();
            kind = Unique;
        }
        auto base = parseQualType();
        if (!base) return nullptr;
        switch (kind) {
        case Raw:
            return sym.pointer(base);
        case Unique:
            return sym.uniquePointer(base);
        }
    }

    static Entity* selectEntity(std::span<Entity* const> entities) {
        if (entities.empty()) {
            return nullptr;
        }
        SC_ASSERT(entities.size() == 1, "Types cannot be overloaded");
        return entities.front();
    }

    ///
    Entity* findEntity(Scope& scope, std::string_view name) {
        return selectEntity(scope.findEntities(name, /* findHidden = */ true));
    }

    ///
    Entity* findEntity(std::string_view name) {
        return selectEntity(
            sym.unqualifiedLookup(name, /* findHidden = */ true));
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

    FunctionType const* parseFunction() {
        lex.get();
        utl::small_vector<Type const*> args;
        bool first = true;
        while (true) {
            if (lex.peek().type == Token::CloseParan) {
                lex.get();
                break;
            }
            if (!first && lex.get().type != Token::Comma) {
                return nullptr;
            }
            first = false;
            auto* arg = parse();
            if (!arg) return nullptr;
            args.push_back(arg);
        }
        if (lex.get().type != Token::Arrow) return nullptr;
        auto* ret = parse();
        if (!ret) return nullptr;
        return sym.functionType(args, ret);
    }

    ObjectType const* parseID() {
        auto tok = lex.get();
        Entity* entity = findEntity(tok.text);
        while (true) {
            if (!entity) return nullptr;
            auto next = lex.peek();
            if (next.type != Token::Dot) {
                return dyncast<ObjectType const*>(entity);
            }
            lex.get();
            auto* scope = dyncast<Scope*>(entity);
            if (!scope) return nullptr;
            auto tok = lex.get();
            entity = findEntity(*scope, tok.text);
        }
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

namespace {

struct Field {
    static constexpr std::string_view Entities = "entities";
    static constexpr std::string_view Children = "children";
    static constexpr std::string_view NativeDependencies =
        "native_dependencies";
    static constexpr std::string_view ForeignDependencies =
        "foreign_dependencies";
    static constexpr std::string_view ReturnType = "return_type";
    static constexpr std::string_view ArgumentTypes = "argument_types";
    static constexpr std::string_view SMFKind = "smf_kind";
    static constexpr std::string_view Lifetime = "lifetime";
    static constexpr std::string_view LifetimeOpKind = "lifetime_op_kind";
    static constexpr std::string_view FunctionKind = "function_kind";
    static constexpr std::string_view BinaryAddress = "binary_address";
    static constexpr std::string_view Size = "size";
    static constexpr std::string_view Align = "align";
    static constexpr std::string_view Type = "type";
    static constexpr std::string_view Mutable = "mutable";
    static constexpr std::string_view Index = "index";
    /// We prepend underscores to entity type and name to make them the first
    /// entries in the json objects so serialized files are easier to read
    static constexpr std::string_view EntityType = "_entity_type";
    static constexpr std::string_view Name = "_name";
    static constexpr std::string_view AccessControl = "access_control";
};

#if 0 // Typesafe fields, not yet implemented

enum class Field {
#define SC_SEMA_FIELD_DEF(Type, Name, Spelling) Name,
#include "Sema/SerializeFields.def.h"
};

std::string_view toString(Field field) {
    static constexpr std::array spellings{
#define SC_SEMA_FIELD_DEF(Type, Name, Spelling) std::string_view(Spelling),
#include "Sema/SerializeFields.def.h"
    };
    return spellings[(size_t)field];
}

template <Field F>
struct FieldToType;

#define SC_SEMA_FIELD_DEF(Type, Name, Spelling)                                \
    template <>                                                                \
    struct FieldToType<Field::Name> {                                          \
        using type = Type;                                                     \
    };
#include "Sema/SerializeFields.def.h"

#endif

struct Serializer {
    utl::hashset<NativeLibrary const*> nativeDependencies;
    utl::hashset<ForeignLibrary const*> foreignDependencies;

    json serialize(Entity const& entity) {
        return visit(entity,
                     [&](auto const& entity) { return serializeImpl(entity); });
    }

    void visitChildren(json& j, Scope const& scope) {
        for (auto* entity: scope.entities()) {
            auto e = serialize(*entity);
            if (!e.is_null()) {
                j.push_back(e);
            }
        }
    }

    json serializeImpl(GlobalScope const& global) {
        json j;
        visitChildren(j[Field::Entities], global);
        for (auto* lib: global.children() | Filter<ForeignLibrary>) {
            foreignDependencies.insert(lib);
        }
        for (auto* lib: nativeDependencies) {
            j[Field::NativeDependencies].push_back(lib->name());
        }
        for (auto* lib: foreignDependencies) {
            j[Field::ForeignDependencies].push_back(lib->name());
        }
        return j;
    }

    json serializeImpl(Function const& function) {
        if (function.isBuiltin()) {
            return json{};
        }
        json j = serializeCommon(function);
        j[Field::ReturnType] = serializeTypename(function.returnType());
        j[Field::ArgumentTypes] = function.argumentTypes() |
                                  transform(serializeTypename);
        if (auto kind = function.smfKind()) {
            j[Field::SMFKind] = *kind;
        }
        j[Field::FunctionKind] = function.kind();
        if (auto address = function.binaryAddress()) {
            j[Field::BinaryAddress] = *address;
        }
        gatherLibraryDependencies(*function.type());
        return j;
    }

    json serializeImpl(StructType const& type) {
        json j = serializeCommon(type);
        j[Field::Size] = type.size();
        j[Field::Align] = type.align();
        j[Field::Lifetime] = serializeLifetime(type.lifetimeMetadata());
        visitChildren(j[Field::Children], type);
        return j;
    }

    json serializeImpl(Variable const& var) {
        json j = serializeCommon(var);
        j[Field::Type] = serializeTypename(var.type());
        j[Field::Mutable] = var.isMut();
        if (isa<StructType>(var.parent())) {
            j[Field::Index] = var.index();
        }
        gatherLibraryDependencies(*var.type());
        return j;
    }

    json serializeImpl(Entity const&) {
        /// Everything else is ignored
        return json{};
    }

    json serializeCommon(Entity const& entity) {
        json j;
        j[Field::EntityType] = entity.entityType();
        j[Field::Name] = std::string(entity.name());
        j[Field::AccessControl] = entity.accessControl();
        return j;
    }

    json serializeLifetime(LifetimeMetadata const& md) {
        json j;
        for (auto op: EnumRange<SMFKind>()) {
            j[toString(op)] = serializeLifetimeOp(md.operation(op));
        }
        return j;
    }

    json serializeLifetimeOp(LifetimeOperation const& op) {
        json j;
        j[Field::LifetimeOpKind] = op.kind();
        if (auto* F = op.function()) {
            j[Field::Name] = F->name();
            j[Field::Type] = serializeTypename(F->type());
        }
        return j;
    }

    void gatherLibraryDependencies(Type const& type) {
        visit(type, [&](auto const& type) { gatherLibDepsImpl(type); });
    }

    void gatherLibDepsImpl(FunctionType const& type) {
        for (auto* argType: type.argumentTypes()) {
            gatherLibraryDependencies(*argType);
        }
        gatherLibraryDependencies(*type.returnType());
    }

    void gatherLibDepsImpl(StructType const& type) {
        if (auto* lib = parentLibrary(type)) {
            nativeDependencies.insert(lib);
        }
    }

    void gatherLibDepsImpl(BuiltinType const&) {}

    void gatherLibDepsImpl(ArrayType const& type) {
        gatherLibraryDependencies(*type.elementType());
    }

    void gatherLibDepsImpl(PointerType const& type) {
        gatherLibraryDependencies(*type.base());
    }

    void gatherLibDepsImpl(ReferenceType const& type) {
        gatherLibraryDependencies(*type.base());
    }

    static NativeLibrary const* parentLibrary(Entity const& entity) {
        SC_EXPECT(!isa<GlobalScope>(entity));
        auto* parent = entity.parent();
        while (true) {
            SC_ASSERT(parent, "Ill-formed symbol table");
            if (auto* lib = dyncast<NativeLibrary const*>(parent)) {
                return lib;
            }
            if (isa<FileScope>(parent) || isa<GlobalScope>(parent)) {
                return nullptr;
            }
            parent = parent->parent();
        }
    }
};

} // namespace

void sema::serialize(SymbolTable const& sym, std::ostream& ostream) {
    auto j = Serializer{}.serialize(sym.globalScope());
    ostream << std::setw(2) << j << std::endl;
}

/// MARK: - deserialize()

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

struct Deserializer: TypeMapBase {
    /// The symbol table do deserialize into. All deserialized entities be
    /// written into the current scope and according child scopes
    SymbolTable& sym;

    Deserializer(SymbolTable& sym): sym(sym) {}

    /// Performs the deserialization
    void run(json const& j) {
        utl::hashset<Library*> dependencies;
        if (auto* deps = tryGet(j, Field::ForeignDependencies)) {
            for (auto& lib: *deps) {
                auto* dependency = sym.importForeignLib(lib.get<std::string>());
                dependencies.insert(dependency);
            }
        }
        if (auto* deps = tryGet(j, Field::NativeDependencies)) {
            for (auto& lib: *deps) {
                auto* dependency = sym.importNativeLib(lib.get<std::string>());
                dependencies.insert(dependency);
            }
        }
        if (auto* lib = dyncast<Library*>(&sym.currentScope())) {
            lib->setDependencies(dependencies.values());
        }
        preparseTypes(get(j, Field::Entities));
        parseEntities(get(j, Field::Entities));
    }

    /// Performs a DFS over the JSON array and declares all encountered struct
    /// types to the symbol table. Types are parsed before all other entities.
    /// Dependencies between types via member variables are resolved because
    /// member variables are parsed later and add themselves to their parent
    /// structs
    void preparseTypes(json const& j) {
        parseArray(j, [this]<typename T>(Tag<T> tag, json const& obj) {
            preparseImpl(tag, obj);
        });
    }

    void preparseImpl(Tag<StructType>, json const& obj) {
        auto* type =
            sym.declareStructureType(get<std::string>(obj, Field::Name),
                                     get(obj, Field::AccessControl));
        insertType(obj, type);
        type->setSize(get<size_t>(obj, Field::Size));
        type->setAlign(get<size_t>(obj, Field::Align));
        type->setLifetimeMetadata(
            parseLifetime(nullptr, get(obj, Field::Lifetime)));
        if (auto children = tryGet(obj, Field::Children)) {
            sym.withScopeCurrent(type, [&] { preparseTypes(*children); });
        }
    }

    template <typename T>
    void preparseImpl(Tag<T>, json const&) {}

    /// Because types are parsed in a prior step we only forward to our children
    void parseImpl(Tag<StructType>, json const& obj) {
        auto* type = getType(obj);
        if (auto children = tryGet(obj, Field::Children)) {
            sym.withScopeCurrent(type, [&] { parseEntities(*children); });
        }
        type->setLifetimeMetadata(
            parseLifetime(type, get(obj, Field::Lifetime)));
        type->setConstructors(type->findFunctions("new"));
    }

    ///
    void parseImpl(Tag<Function>, json const& obj) {
        auto argTypes = get(obj, Field::ArgumentTypes) |
                        transform([&](json const& j) {
            return parseTypename(sym, j.get<std::string>());
        }) | ToSmallVector<>;
        auto* retType =
            parseTypename(sym, get<std::string>(obj, Field::ReturnType));
        auto* function =
            sym.declareFunction(get<std::string>(obj, Field::Name),
                                sym.functionType(argTypes, retType),
                                get(obj, Field::AccessControl));
        function->setKind(get<FunctionKind>(obj, Field::FunctionKind));
        if (auto address = tryGet(obj, Field::BinaryAddress)) {
            function->setBinaryAddress(*address);
        }
    }

    ///
    void parseImpl(Tag<Variable>, json const& obj) {
        auto* type = parseTypename(sym, get<std::string>(obj, Field::Type));
        using enum Mutability;
        auto mut = get<bool>(obj, Field::Mutable) ? Mutable : Const;
        auto* var = sym.defineVariable(get<std::string>(obj, Field::Name), type,
                                       mut, get(obj, Field::AccessControl));
        if (auto index = tryGet<size_t>(obj, Field::Index)) {
            auto* type = dyncast<StructType*>(&sym.currentScope());
            SC_ASSERT(type, "");
            var->setIndex(*index);
            type->setMemberVariable(*index, var);
        }
    }

    /// Base case
    template <typename T>
    void parseImpl(Tag<T>, json const&) {}

    ///
    void parseImpl(Tag<ForeignLibrary>, json const& obj) {
        auto name = get<std::string>(obj, Field::Name);
        sym.importForeignLib(name);
    }

    /// Performs a DFS over the JSON array and declares all encountered entities
    /// but struct types to the symbol table
    void parseEntities(json const& j) {
        parseArray(j, [this]<typename T>(Tag<T> tag, json const& child) {
            parseImpl(tag, child);
        });
    }

    /// This function will be called twice for each type. Once in preparse phase
    /// with \p parent = null, once in actual parse phase with value \p parent
    /// argument.
    /// We need to parse the lifetime twice because in after preparse phase
    /// dependent types like array types require the lifetime to exist. However
    /// in preparse the functions don't exist yet. So after actually parsing a
    /// type we parse the lifetime again, this time including the non-trivial
    /// lifetime functions
    LifetimeMetadata parseLifetime(ObjectType* parent, json const& obj) {
        std::array<LifetimeOperation, 4> ops;
        for (auto op: EnumRange<SMFKind>()) {
            auto const& j = get(obj, toString(op));
            auto kind = get<LifetimeOperation::Kind>(j, Field::LifetimeOpKind);
            auto* function = [&]() -> Function* {
                if (!parent) {
                    return nullptr;
                }
                auto name = tryGet<std::string>(j, Field::Name);
                if (!name) {
                    return nullptr;
                }
                auto* type =
                    parseTypename(sym, get<std::string>(j, Field::Type));
                auto& fnType = dyncast<FunctionType const&>(*type);
                auto const functions = parent->findFunctions(*name);
                auto* function = findBySignature(std::span(functions),
                                                 fnType.argumentTypes());
                if (!function) {
                    throw std::runtime_error(
                        "Failed to parse lifetime function");
                }
                return function;
            }();
            ops[(size_t)op] = LifetimeOperation(kind, function);
        }
        return LifetimeMetadata(ops);
    }

    /// # Helper functions

    /// Parses the type of the JSON object \p obj and dispatches to \p callback
    /// with the corresponding tag
    bool doParse(json const& obj, auto callback) {
        auto type = get<EntityType>(obj, Field::EntityType);
        switch (type) {
#define SC_SEMA_ENTITY_DEF(Type, ...)                                          \
    case EntityType::Type:                                                     \
        callback(Tag<Type>{}, obj);                                            \
        return true;
#include <scatha/Sema/Lists.def.h>
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

static bool deserializeImpl(SymbolTable& sym, auto makeJSON) {
    try {
        json j = makeJSON();
        Deserializer ctx(sym);
        ctx.run(j);
        return true;
    }
    catch (std::exception const&) {
        return false;
    }
}

bool sema::deserialize(SymbolTable& sym, std::istream& istream) {
    return deserializeImpl(sym, [&] { return json::parse(istream); });
}

bool sema::deserialize(SymbolTable& sym, std::string_view text) {
    return deserializeImpl(sym, [&] { return json::parse(text); });
}
