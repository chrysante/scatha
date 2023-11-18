#ifndef SDB_MODEL_UIHANDLE_H_
#define SDB_MODEL_UIHANDLE_H_

namespace sdb {

/// Provides callback methods for the model to signal the UI
class UIHandle {
public:
    ///
    virtual void refresh() = 0;

    ///
    virtual void reload() = 0;
};

} // namespace sdb

#endif // SDB_MODEL_UIHANDLE_H_
