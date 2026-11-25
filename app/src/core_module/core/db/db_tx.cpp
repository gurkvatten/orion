#include <core/db/db_tx.h>

namespace orion {

DbTx::~DbTx()
{
    if (m_active) {
        rollback();
    }
}

Error
DbTx::begin(std::shared_ptr<Db> db, int* result_code)
{
    if (m_active) {
        RAISE(ErrorCode::db_tx_active, "transaction already active");
    }
    m_db = db;

    VERIFY(m_db->exec("BEGIN TRANSACTION;", {}, nullptr, result_code));
    m_active = true;
    SUCCEED();
}

Error
DbTx::rollback(int* result_code)
{
    if (!m_active) {
        RAISE(ErrorCode::db_no_tx, "no active transaction to rollback");
    }
    if (!m_db) {
        RAISE(ErrorCode::invalid_state, "db not set");
    }

    VERIFY(m_db->exec("ROLLBACK;", {}, nullptr, result_code));
    m_active = false;
    SUCCEED();
}

Error
DbTx::commit(int* result_code)
{
    if (!m_active) {
        RAISE(ErrorCode::db_no_tx, "no active transaction to commit");
    }
    if (!m_db) {
        RAISE(ErrorCode::invalid_state, "db not set");
    }

    VERIFY(m_db->exec("COMMIT;", {}, nullptr, result_code));
    m_active = false;
    SUCCEED();
}

bool
DbTx::in_transaction() const
{
    return m_db->in_transaction();
}

Error
DbTx::try_commit(int* result_code)
{
    if (in_transaction()) {
        // already in a transaction, do nothing
        SUCCEED();
    }
    VERIFY(commit(result_code));
    SUCCEED();
}

} // namespace orion