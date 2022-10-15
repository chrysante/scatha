#ifndef SCATHA_BASIC_PRINTUTIL_H_
#define SCATHA_BASIC_PRINTUTIL_H_

#include <iosfwd>

namespace scatha {

struct Indenter {
    explicit Indenter(int spacesPerLevel = 1): level(0), spacesPerLevel(spacesPerLevel) {}
    explicit Indenter(int level, int spacesPerLevel): level(level), spacesPerLevel(spacesPerLevel) {}

    Indenter& increase() & {
        ++level;
        return *this;
    }
    Indenter& decrease() & {
        --level;
        return *this;
    }

    friend std::ostream& operator<<(std::ostream&, Indenter const&);

private:
    int level;
    int spacesPerLevel;
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
