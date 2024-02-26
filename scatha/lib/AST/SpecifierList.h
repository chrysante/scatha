#ifndef SCATHA_AST_SPECIFIERLIST_H_
#define SCATHA_AST_SPECIFIERLIST_H_

#include <optional>
#include <string>

#include "Common/SourceLocation.h"
#include "Sema/Fwd.h"

namespace scatha::ast {

///
class SpecifierList {
public:
    std::optional<sema::AccessControl> accessControl() const {
        return accessCtrl != sema::InvalidAccessControl ?
                   std::optional(accessCtrl) :
                   std::nullopt;
    }

    SourceRange accessControlSourceRange() const {
        return accessCtrlSourceRange;
    }

    std::optional<std::string> externalLinkage() const {
        return hasExtLinkage ? std::optional(extLinkage) : std::nullopt;
        return extLinkage;
    }

    SourceRange externalLinkageSourceRange() const {
        return linkageSourceRange;
    }

    /// Sets the respective property
    /// Only sets a property if it has not been set before
    /// \Returns `true` if property has been set
    /// @{
    bool set(sema::AccessControl accessControl, SourceRange sourceRange) {
        if (accessCtrl != sema::InvalidAccessControl) {
            return false;
        }
        accessCtrl = accessControl;
        accessCtrlSourceRange = sourceRange;
        return true;
    }
    bool set(std::string linkage, SourceRange sourceRange) {
        if (hasExtLinkage) {
            return false;
        }
        hasExtLinkage = true;
        extLinkage = std::move(linkage);
        linkageSourceRange = sourceRange;
        return true;
    }
    /// @}

private:
    SourceRange accessCtrlSourceRange{};
    SourceRange linkageSourceRange{};
    sema::AccessControl accessCtrl = sema::InvalidAccessControl;
    bool hasExtLinkage;
    std::string extLinkage;
};

} // namespace scatha::ast

#endif // SCATHA_AST_SPECIFIERLIST_H_
