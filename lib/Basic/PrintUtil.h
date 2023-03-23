#ifndef SCATHA_BASIC_PRINTUTIL_H_
#define SCATHA_BASIC_PRINTUTIL_H_

#include <iosfwd>

namespace scatha {

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

} // namespace scatha

#endif // SCATHA_BASIC_PRINTUTIL_H_
