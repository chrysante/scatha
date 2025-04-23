#include "Util/Messenger.h"

using namespace sdb;

void Messenger::notifyMainTread() {
    if (!didNotifyMain.exchange(true)) submitTaskCb([this] { flush(); });
}

void Messenger::flush() {
    didNotifyMain.store(false);
    utl::buffered_messenger::flush();
}
