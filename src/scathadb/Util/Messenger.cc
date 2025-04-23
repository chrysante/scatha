#include "Util/Messenger.h"

using namespace sdb;

void Messenger::notify() {
    if (!didNotify.exchange(true) && sendBufferedCallback)
        sendBufferedCallback(*this);
}

void Messenger::flush() {
    didNotify.store(false);
    utl::buffered_messenger::flush();
}
