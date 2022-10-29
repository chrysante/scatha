#ifndef SCATHA_SEMA_VARIABLE_H_
#define SCATHA_SEMA_VARIABLE_H_

#include "Sema/EntityBase.h"

namespace scatha::sema {

class Variable: public EntityBase {
public:
    explicit Variable(
        std::string name, SymbolID symbolID, Scope* parentScope, TypeID typeID = TypeID::Invalid, size_t offset = 0);

    /// Set the type of this variable.
    void setTypeID(TypeID id) { _typeID = id; }

    /// Set the offset of this variable.
    void setOffset(size_t offset) { _offset = offset; }

    /// Type of this variable.
    TypeID typeID() const { return _typeID; }

    /// Offset into the struct this variable is a member of. If this is not a
    /// member variable then offset() == 0.
    size_t offset() const { return _offset; }

    /// Wether this variable is local to a function or potentially globally visible.
    bool isLocal() const;

private:
    TypeID _typeID;
    size_t _offset;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_VARIABLE_H_
