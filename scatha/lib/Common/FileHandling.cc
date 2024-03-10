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

    if (auto parent = path.parent_path();
        !parent.empty() && !std::filesystem::exists(parent))
    {
        std::filesystem::create_directories(parent);
    }
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

Archive Archive::Open(std::filesystem::path path) {
    mtar_t tar;
    mtar_open(&tar, path.c_str(), "r");
    mtar_header_t header;
    std::vector<std::string> files;
    while ((mtar_read_header(&tar, &header)) != MTAR_ENULLRECORD) {
        files.emplace_back(header.name);
        mtar_next(&tar);
    }
    return Archive(Mode::Read, std::move(files), toImpl(tar));
}

Archive Archive::Create(std::filesystem::path path) {
    mtar_t tar;
    mtar_open(&tar, path.c_str(), "w");
    return Archive(Mode::Write, {}, toImpl(tar));
}

void Archive::close() {
    auto& tar = asMTar(impl);
    mtar_finalize(&tar);
    mtar_close(&tar);
}

std::optional<std::string> Archive::openTextFile(std::string const& name) {
    return readImpl<std::string>(name);
}

std::optional<std::vector<unsigned char>> Archive::openBinaryFile(
    std::string const& name) {
    return readImpl<std::vector<unsigned char>>(name);
}

template <typename T>
std::optional<T> Archive::readImpl(std::string const& name) {
    SC_EXPECT(mode() == Mode::Read);
    auto& tar = asMTar(impl);
    mtar_header_t header;
    if (mtar_find(&tar, name.c_str(), &header) != MTAR_ESUCCESS) {
        return std::nullopt;
    }
    T result(header.size, 0);
    mtar_read_data(&tar, result.data(), header.size);
    return result;
}

/// # Writing

void Archive::addTextFile(std::string const& name, std::string_view contents) {
    writeImpl(name, contents.data(), contents.size());
}

void Archive::addBinaryFile(std::string const& name,
                            std::span<unsigned char const> contents) {
    writeImpl(name, contents.data(), contents.size());
}

void Archive::writeImpl(std::string const& name, void const* data,
                        size_t size) {
    SC_EXPECT(mode() == Mode::Write);
    auto& tar = asMTar(impl);
    mtar_write_file_header(&tar, name.c_str(),
                           utl::narrow_cast<unsigned>(size));
    mtar_write_data(&tar, data, utl::narrow_cast<unsigned>(size));
}

Archive::Archive(Mode mode, std::vector<std::string> files, Impl impl):
    _mode(mode), _files(std::move(files)), impl(impl) {}
