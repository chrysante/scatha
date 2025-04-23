#ifndef SDB_UTIL_MESSENGER_H_
#define SDB_UTIL_MESSENGER_H_

#include <atomic>
#include <functional>
#include <mutex>

#include <utl/messenger.hpp>

namespace sdb {

class Messenger: private utl::buffered_messenger {
public:
    using utl::buffered_messenger::listen;
    using utl::buffered_messenger::listener_id_type;
    using utl::buffered_messenger::send_now;
    using utl::buffered_messenger::unlisten;

    using Task = std::function<void()>;

    explicit Messenger(std::function<void(Task)> submitTaskCb):
        submitTaskCb(std::move(submitTaskCb)) {}

    void send_buffered(std::any const& message) {
        utl::buffered_messenger::send_buffered(message);
        notifyMainTread();
    }

    void send_buffered(std::any&& message) {
        utl::buffered_messenger::send_buffered(std::move(message));
        notifyMainTread();
    }

    void flush();

private:
    void notifyMainTread();
    std::atomic_bool didNotifyMain = false;
    std::function<void(std::function<void()>)> submitTaskCb;
};

static_assert(utl::buffered_messenger_type<Messenger>);

using Transceiver = utl::transceiver<Messenger>;

} // namespace sdb

#endif // SDB_UTIL_MESSENGER_H_
