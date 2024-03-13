#include "Common/FileHandling.h"

#include <cstring>
#include <stdexcept>

#include <microtar.h>
#include <range/v3/view.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Common/Base.h"

using namespace scatha;

std::fstream scatha::createOutputFile(std::filesystem::path const& path,
                                      std::ios::openmode flags) {

    std::filesystem::create_directories(path.parent_path());
    flags |= std::ios::out;
    std::fstream file(path, flags);
    if (!file) {
        throw std::runtime_error(
            utl::strcat("Failed to create ", path, ": ", std::strerror(errno)));
    }
    return file;
}

static Archive::Impl toImpl(mtar_t const& tar) {
    static_assert(sizeof(Archive::Impl) >= sizeof(mtar_t));
    Archive::Impl impl;
    std::memcpy(&impl.data, &tar, sizeof(tar));
    return impl;
}

static mtar_t& asMTar(Archive::Impl& impl) {
    return reinterpret_cast<mtar_t&>(impl.data);
}

Archive::Archive(Mode mode, std::vector<std::string> files, Impl impl):
    _mode(mode), _files(std::move(files)), impl(impl) {}

Archive::Archive(Archive&& rhs) noexcept:
    _mode(rhs._mode), _files(std::move(rhs._files)), impl(rhs.impl) {
    rhs._mode = Mode::Closed;
    std::memset(&rhs.impl, 0, sizeof rhs.impl);
}

Archive& Archive::operator=(Archive&& rhs) noexcept {
    close();
    _mode = rhs._mode;
    _files = std::move(rhs._files);
    impl = rhs.impl;
    rhs._mode = Mode::Closed;
    std::memset(&rhs.impl, 0, sizeof rhs.impl);
    return *this;
}

Archive::~Archive() { close(); }

std::optional<Archive> Archive::Open(std::filesystem::path path) {
    mtar_t tar;
    if (mtar_open(&tar, path.c_str(), "r") != MTAR_ESUCCESS) {
        return std::nullopt;
    }
    mtar_header_t header;
    std::vector<std::string> files;
    while ((mtar_read_header(&tar, &header)) != MTAR_ENULLRECORD) {
        files.emplace_back(header.name);
        mtar_next(&tar);
    }
    return Archive(Mode::Read, std::move(files), toImpl(tar));
}

std::optional<Archive> Archive::Create(std::filesystem::path path) {
    std::filesystem::create_directories(path.parent_path());
    mtar_t tar;
    if (mtar_open(&tar, path.c_str(), "w") != MTAR_ESUCCESS) {
        return std::nullopt;
    }
    return Archive(Mode::Write, {}, toImpl(tar));
}

void Archive::close() {
    if (mode() == Mode::Closed) {
        return;
    }
    auto& tar = asMTar(impl);
    mtar_finalize(&tar);
    mtar_close(&tar);
    _mode = Mode::Closed;
    _files.clear();
    std::memset(&impl, 0, sizeof impl);
}

std::optional<std::string> Archive::openTextFile(std::string_view name) {
    return readImpl<std::string>(name);
}

std::optional<std::vector<unsigned char>> Archive::openBinaryFile(
    std::string_view name) {
    return readImpl<std::vector<unsigned char>>(name);
}

template <typename T>
std::optional<T> Archive::readImpl(std::string_view name) {
    SC_EXPECT(mode() == Mode::Read);
    auto& tar = asMTar(impl);
    mtar_header_t header;
    /// The copy here is hopefully temporary. We can get rid of it by using
    /// another tar library or improving microtar
    std::string zname(name);
    if (mtar_find(&tar, zname.c_str(), &header) != MTAR_ESUCCESS) {
        return std::nullopt;
    }
    T result(header.size, 0);
    mtar_read_data(&tar, result.data(), header.size);
    return result;
}

/// # Writing

void Archive::addTextFile(std::string_view name, std::string_view contents) {
    writeImpl(name, contents.data(), contents.size());
}

void Archive::addBinaryFile(std::string_view name,
                            std::span<unsigned char const> contents) {
    writeImpl(name, contents.data(), contents.size());
}

void Archive::writeImpl(std::string_view name, void const* data, size_t size) {
    SC_EXPECT(mode() == Mode::Write);
    auto& tar = asMTar(impl);
    /// See comment in `readImpl()`
    std::string zname(name);
    mtar_write_file_header(&tar, zname.c_str(),
                           utl::narrow_cast<unsigned>(size));
    mtar_write_data(&tar, data, utl::narrow_cast<unsigned>(size));
}
