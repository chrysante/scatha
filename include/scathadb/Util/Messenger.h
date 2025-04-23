#ifndef SDB_UTIL_MESSENGER_H_
#define SDB_UTIL_MESSENGER_H_

#include <atomic>
#include <functional>
#include <mutex>

#include <utl/messenger.hpp>

namespace sdb {

class Messenger:
    private utl::buffered_messenger,
    public std::enable_shared_from_this<Messenger> {
public:
    using utl::buffered_messenger::listen;
    using utl::buffered_messenger::listener_id_type;
    using utl::buffered_messenger::send_now;
    using utl::buffered_messenger::unlisten;

    /// \p sendBufferedCallback will be invoked once by a call to
    /// `send_buffered()` until the next call to `flush()`. Callback may be
    /// called from any thread.
    static std::shared_ptr<Messenger> Make(
        std::function<void(Messenger&)> sendBufferedCallback) {
        return std::shared_ptr<Messenger>(
            new Messenger(std::move(sendBufferedCallback)));
    }

    void send_buffered(std::any const& message) {
        utl::buffered_messenger::send_buffered(message);
        notify();
    }

    void send_buffered(std::any&& message) {
        utl::buffered_messenger::send_buffered(std::move(message));
        notify();
    }

    void flush();

private:
    explicit Messenger(std::function<void(Messenger&)> sendBufferedCallback):
        sendBufferedCallback(std::move(sendBufferedCallback)) {}

    void notify();
    std::atomic_bool didNotify = false;
    std::function<void(Messenger&)> sendBufferedCallback;
};

static_assert(utl::buffered_messenger_type<Messenger>);

using Transceiver = utl::transceiver<Messenger>;

} // namespace sdb

#endif // SDB_UTIL_MESSENGER_H_
