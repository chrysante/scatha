#ifndef SCATHA_COMMON_PRINTUTIL_H_
#define SCATHA_COMMON_PRINTUTIL_H_

#include <iosfwd>
#include <string>

#include <utl/streammanip.hpp>

namespace scatha {

///
std::ostream& operator<<(std::ostream&, struct Indenter const&);

///
std::ostream& operator<<(std::ostream&, struct EndlIndenter const&);

///
struct Indenter {
    explicit Indenter(int spacesPerLevel = 1):
        _level(0), _spacesPerLevel(spacesPerLevel) {}
    explicit Indenter(int level, int spacesPerLevel):
        _level(level), _spacesPerLevel(spacesPerLevel) {}

    Indenter& increase() & {
        ++_level;
        return *this;
    }
    Indenter& decrease() & {
        --_level;
        return *this;
    }

    int level() const { return _level; }

    int spacesPerLevel() const { return _spacesPerLevel; }

    int totalIndent() const { return level() * spacesPerLevel(); }

    friend std::ostream& operator<<(std::ostream&, Indenter const&);

private:
    int _level;
    int _spacesPerLevel;
};

///
struct EndlIndenter: Indenter {
    using Indenter::Indenter;

    EndlIndenter& increase() & {
        Indenter::increase();
        return *this;
    }

    EndlIndenter& decrease() & {
        Indenter::decrease();
        return *this;
    }

    friend std::ostream& operator<<(std::ostream&, EndlIndenter const&);
};

/// # HTML formatting helpers

utl::vstreammanip<> tableBegin(int border = 0, int cellborder = 0,
                               int cellspacing = 0);

utl::vstreammanip<> tableEnd();

utl::vstreammanip<> fontBegin(std::string fontname);

utl::vstreammanip<> fontEnd();

utl::vstreammanip<> rowBegin();

utl::vstreammanip<> rowEnd();

} // namespace scatha

#endif // SCATHA_COMMON_PRINTUTIL_H_
