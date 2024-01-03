#include "Sema/Serialize.h"

#include <charconv>
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

static void scopedNameImpl(Type const& type, std::ostream& str) {
    auto ptrLikeImpl = [&](std::string_view kind, PtrRefTypeBase const& ptr) {
        str << kind;
        if (ptr.base().isMut()) {
            str << "mut ";
        }
        scopedNameImpl(*ptr.base(), str);
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
            scopedNameImpl(*arr.elementType(), str);
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

static std::string scopedName(Type const* type) {
    std::stringstream sstr;
    scopedNameImpl(*type, sstr);
    return std::move(sstr).str();
}

/// MARK: - String to type conversion

namespace {

struct TypeNameParser {
    struct Token {
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

    struct Lexer {
        std::string_view text;
        std::optional<Token> lookahead;

        Lexer(std::string_view text): text(text) {}

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
            switch (text.front()) {
#define PUNCTUATION(CHAR, TYPE)                                                \
CHAR: {                                                                        \
    Token tok{ Token::TYPE, text.substr(0, 1) };                               \
    inc();                                                                     \
    return tok;                                                                \
}
            case PUNCTUATION('&', Ref) case PUNCTUATION('*', Ptr) case PUNCTUATION('.', Dot) case PUNCTUATION(
                ',',
                Comma) case PUNCTUATION('[',
                                        OpenBracket) case PUNCTUATION(']',
                                                                      CloseBracket) default: {
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
        }

        void inc(size_t count = 1) {
            SC_EXPECT(text.size() >= count);
            text = text.substr(count);
        }
    };

    SymbolTable& sym;
    Lexer lex;

    TypeNameParser(SymbolTable& sym, std::string_view text):
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

    ObjectType const* parseID() {
        Scope* scope = &sym.globalScope();
        while (true) {
            auto tok = lex.get();
            auto entities = scope->findEntities(tok.text);
            if (entities.empty()) {
                return nullptr;
            }
            auto* entity = entities.front();
            auto next = lex.peek();
            if (next.type != Token::Dot) {
                return dyncast<ObjectType const*>(entity);
            }
            lex.get();
            scope = dyncast<Scope*>(entity);
            if (!scope) {
                return nullptr;
            }
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

static Type const* parseTypeName(SymbolTable& sym, std::string_view text) {
    TypeNameParser parser(sym, text);
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
    j["return_type"] = scopedName(function.returnType());
    j["argument_types"] = function.argumentTypes() | transform(scopedName);
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
    j["type"] = scopedName(var.type());
    j["mutable"] = var.isMut();
    if (isa<StructType>(var.parent())) {
        j["index"] = var.index();
    }
}

static void to_json_impl(json&, ForeignLibrary const&) {
    /// No-op because we only need the name and the entity type
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

template <typename>
struct Tag {};

struct DeserializeContext {
    SymbolTable& sym;
    utl::hashmap<json const*, Entity*> map;

    DeserializeContext(SymbolTable& sym): sym(sym) {}

    void run(json const& j) {
        parseTypes(j);
        parseOther(j);
    }

    bool parse(json const& j, auto parseCallback) {
        auto type = toEntityType(j["entity_type"]);
        auto callback = utl::overload{ parseCallback, [](auto...) {} };
        switch (type.value()) {
#define SC_SEMA_ENTITY_DEF(Type, _)                                            \
    case EntityType::Type:                                                     \
        callback(Tag<Type>{});                                                 \
        return true;
#include <scatha/Sema/Lists.def>
        case EntityType::_count:
            return false;
        }
        SC_UNREACHABLE();
    }

    std::string getName(json const& j) { return j["name"].get<std::string>(); }

    void parseTypes(json const& j) {
        for (auto& child: j) {
            // clang-format off
            parse(child, utl::overload{
                [&](Tag<StructType>) {
                    auto* type = sym.declareStructureType(getName(child));
                    map[&child] = type;
                    type->setSize(child["size"].get<size_t>());
                    type->setAlign(child["align"].get<size_t>());
                    sym.withScopeCurrent(type, [&] {
                        parseTypes(child["children"]);
                    });
                }
            }); // clang-format on
        }
    }

    void parseOther(json const& j) {
        for (auto& child: j) {
            // clang-format off
            parse(child, utl::overload{
                [&](Tag<StructType>) {
                    auto* type = cast<StructType*>(map[&child]);
                    sym.withScopeCurrent(type, [&] {
                        parseOther(child["children"]);
                    });
                },
                [&](Tag<Function>) {
                    auto argType = child["argument_types"] |
                                   transform([&](json const& j) {
                                       return parseTypeName(
                                           sym, j.get<std::string>());
                                   }) | ToSmallVector<>;
                    auto* retType = parseTypeName(
                        sym, child["return_type"].get<std::string>());
                    sym.declareFunction(getName(child),
                                        FunctionSignature(std::move(argType),
                                                          retType));
                },
                [&](Tag<Variable>) {
                    auto* type =
                        parseTypeName(sym, child["type"].get<std::string>());
                    using enum Mutability;
                    auto mut = child["mutable"].get<bool>() ? Mutable : Const;
                    auto* var = sym.defineVariable(getName(child), type, mut);
                    auto indexItr = child.find("index");
                    if (indexItr != child.end()) {
                        auto index = indexItr->get<size_t>();
                        auto* type = dyncast<StructType*>(&sym.currentScope());
                        type->setMemberVariable(index, var);
                    }
                },
                [&](Tag<ForeignLibrary>) {
                    auto name = child["name"].get<std::string>();
                    sym.importForeignLib(name);
                },
            }); // clang-format on
        }
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
