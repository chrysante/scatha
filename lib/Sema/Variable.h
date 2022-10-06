#ifndef SCATHA_SEMA_VARIABLE_H_
#define SCATHA_SEMA_VARIABLE_H_

#include "Sema/EntityBase.h"

namespace scatha::sema {

class Variable: public EntityBase {
  public:
    explicit Variable(std::string name, SymbolID symbolID, Scope *parentScope, TypeID typeID, size_t offset = 0):
        EntityBase(std::move(name), symbolID, parentScope), _typeID(typeID), _offset(offset) {}

    /// Set the type of this variable.
    void setTypeID(TypeID id) { _typeID = id; }

    /// Type of this variable.
    TypeID typeID() const { return _typeID; }

    /// Offset into the struct this variable is a member of. If this is not a
    /// member variable then offset() == 0.
    size_t offset() const { return _offset; }

  private:
    TypeID _typeID;
    bool   _offset;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_VARIABLE_H_
