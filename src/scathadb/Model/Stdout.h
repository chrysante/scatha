#ifndef SDB_MODEL_STDOUT_H_
#define SDB_MODEL_STDOUT_H_

#include <functional>
#include <sstream>
#include <string>

namespace sdb {

class CallbackStringBuf final: public std::stringbuf {
public:
    explicit CallbackStringBuf(std::function<void()> onNewline):
        onNewline(std::move(onNewline)) {}

protected:
    int_type overflow(int_type ch) override {
        if (ch == '\n') onNewline();
        return std::stringbuf::overflow(ch);
    }

    std::streamsize xsputn(char const* s, std::streamsize count) override {
        if (std::memchr(s, '\n', (size_t)count)) onNewline();
        return std::stringbuf::xsputn(s, count);
    }

private:
    std::function<void()> onNewline;
};

class CallbackStringStream: public std::ostream {
public:
    CallbackStringStream(std::function<void()> onNewline):
        std::ostream(&buf), buf(std::move(onNewline)) {}

    std::string str() const { return buf.str(); }

    void str(std::string value) { buf.str(std::move(value)); }

private:
    CallbackStringBuf buf;
};

} // namespace sdb

#endif // SDB_MODEL_STDOUT_H_
