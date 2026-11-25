#pragma once

#include <memory>
#include <common/src/error.h>
#include <core/db/db.h>

namespace orion {

class Db;

// DbTx class represents a database transaction.
class DbTx {
public:
    DbTx() = default;
    ~DbTx();
    Error begin(std::shared_ptr<Db> db, int* result_code = nullptr);
    Error rollback(int* result_code = nullptr);
    Error commit(int* result_code = nullptr);
    bool in_transaction() const;

    // does nothing if already in a transaction
    Error try_commit(int* result_code = nullptr);

    bool is_active() const { return m_active; }

private:
    bool m_active = false;
    std::shared_ptr<Db> m_db;

    // Prevent copying
    DbTx(const DbTx&) = delete;
    DbTx& operator=(const DbTx&) = delete;
};

} // namespace orion