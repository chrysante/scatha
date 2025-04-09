#ifndef SCATHA_ISSUE_SOURCESTRUCTURE_H_
#define SCATHA_ISSUE_SOURCESTRUCTURE_H_

#include <string>
#include <string_view>
#include <vector>

#include <scatha/Common/Base.h>

namespace scatha {

/// View over the lines of a source text
class SCATHA_API SourceStructure {
public:
    explicit SourceStructure(std::string filename, std::string_view text);

    /// The name of this soure file
    std::string_view name() const { return _name; }

    /// The underlying source text
    std::string_view text() const { return source; }

    /// Range accessors
    /// @{
    template <std::integral T>
    std::string_view operator[](T index) const {
        assert(index >= T(0));
        assert(index < T(lines.size()));
        return lines[size_t(index)];
    }
    auto begin() const { return lines.begin(); }
    auto end() const { return lines.end(); }
    auto front() const { return lines.front(); }
    auto back() const { return lines.back(); }
    auto size() const { return lines.size(); }
    auto empty() const { return lines.empty(); }
    /// @}

private:
    std::string _name;
    std::string_view source;
    std::vector<std::string_view> lines;
};

} // namespace scatha

#endif // SCATHA_ISSUE_SOURCESTRUCTURE_H_
