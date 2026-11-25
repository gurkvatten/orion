#include <core/profiles_db/profiles_db.h>
#include <format>
#include <chrono>

namespace orion {

//-------------------------------------------------------------------------
// Downloads
//-------------------------------------------------------------------------

std::vector<std::string>
ProfilesDb::get_download_columns() const
{
    return {
        "id",
        "profile_id",
        "name",
        "icon_string",
        "path",
        "operation_state",
        "timestamp",
        "total_size",
        "current_size",
        "origin_uri"
    };
}

void
ProfilesDb::load_download_from_stmt(const DbStmt& stmt, DownloadDto* download) const
{
    download->id = DownloadId(stmt.integer("id"));
    download->profile_id = ProfileId(stmt.integer("profile_id"));
    download->name = stmt.text("name");
    download->icon_string = stmt.text("icon_string");
    download->path = stmt.text("path");
    download->operation_state = DownloadOperationState(stmt.integer("operation_state"));
    download->timestamp = UnixTimeUtc64(stmt.integer("timestamp"));
    download->total_size = stmt.integer("total_size");
    download->current_size = stmt.integer("current_size");
    download->origin_uri = stmt.text("origin_uri");
}

Error
ProfilesDb::get_download_ids(GetDownloadIdsQuery query, std::vector<DownloadId>* ids)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    DbStmt stmt;
    VERIFY(m_db->query(R"(
        SELECT id FROM downloads
        WHERE profile_id = ?1
        ORDER BY profile_id, timestamp;
    )", {query.profile_id.value}, &stmt));

    while (stmt.next()) {
        ids->push_back(DownloadId(stmt.integer("id")));
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_downloads(const std::vector<DownloadId>& ids, std::vector<DownloadDto>* downloads)
{
    auto args = to_db_variant_vector(ids);
    std::string sql = std::format(R"(
        SELECT {}
        FROM downloads
        WHERE id IN ({})
        ORDER BY profile_id, timestamp;
    )", Db::join_strings(get_download_columns(), ", "),
        Db::make_placeholders(ids.size())
    );

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        auto download = DownloadDto{};
        load_download_from_stmt(stmt, &download);
        downloads->push_back(std::move(download));
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::create_download(CreateDownloadQuery query, DownloadId* result_id, DownloadsChange* change)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::string sql = std::format(R"(
        INSERT INTO downloads ({})
        VALUES ({})
        RETURNING {};
    )", Db::join_strings(get_download_columns(), ", "),
        Db::make_placeholders(get_download_columns().size()),
        Db::join_strings(get_download_columns(), ", ")
    );

    std::vector<DbVariant> args = {
        nullptr, // id will be auto-generated
        query.profile_id.value,
        query.name,
        query.icon_string,
        query.path,
        int64_t(query.operation_state),
        query.timestamp.value,
        query.total_size,
        query.current_size,
        query.origin_uri,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    DownloadDto download{};
    load_download_from_stmt(stmt, &download);

    if (change) {
        change->created.emplace_back(download);
    }
    if (result_id)
    {
        *result_id = download.id;
    }
    SUCCEED();
}

Error
ProfilesDb::update_download(UpdateDownloadQuery query, DownloadsChange* download_change)
{
    if (query.id == DownloadId(0)) {
        RAISE(ErrorCode::argument, "download id is not set");
    }

    DownloadDto download{};
    {
        std::vector<DownloadDto> downloads;
        VERIFY(get_downloads({query.id}, &downloads));
        if (downloads.empty()) {
            RAISE(ErrorCode::not_found, "download with id %lld not found", query.id.value);
        }
        download = downloads[0];
    }

    std::vector<std::string> set_columns;
    std::vector<DbVariant> args;

    if (query.update_name) {
        args.push_back(query.name);
        set_columns.push_back("name");
    }

    if (query.update_icon_string) {
        args.push_back(query.icon_string);
        set_columns.push_back("icon_string");
    }

    if (query.update_path) {
        args.push_back(query.path);
        set_columns.push_back("path");
    }

    if (query.update_operation_state) {
        args.push_back(int64_t(query.operation_state)); // use current tab id for prev_selected_tab_id
        set_columns.push_back("operation_state");
    }

    if (query.update_timestamp) {
        args.push_back(query.timestamp.value);
        set_columns.push_back("timestamp");
    }

    if (query.update_total_size) {
        args.push_back(query.total_size);
        set_columns.push_back("total_size");
    }

    if (query.update_current_size) {
        args.push_back(query.current_size);
        set_columns.push_back("current_size");
    }

    if (query.update_origin_uri) {
        args.push_back(query.origin_uri);
        set_columns.push_back("origin_uri");
    }

    if (set_columns.empty()) {
        SUCCEED(); // nothing to update
    }

    args.push_back(query.id.value);

    std::string sql = std::format(R"(
        UPDATE downloads SET {}
        WHERE id = ?
        RETURNING {};
    )", Db::join_into_set_clause(set_columns),
        Db::join_strings(get_download_columns(), ", ")
    );

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        DownloadDto download{};
        load_download_from_stmt(stmt, &download);

        if (download_change) {
            download_change->updated.emplace_back(download);
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::delete_downloads(const DeleteDownloadsQuery& query, TxMode tx_mode, DownloadsChange* change)
{
    if (query.profile_id == ProfileId(0) && query.ids.empty()) {
        RAISE(ErrorCode::argument, "profile_id is not set and ids vector is empty");
    }

    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    std::vector<std::string> where_clauses;
    std::vector<DbVariant> args;

    if (query.profile_id != ProfileId(0)) {
        where_clauses.push_back("profile_id = ?");
        args.push_back(query.profile_id.value);
    }

    if (!query.ids.empty()) {
        where_clauses.push_back(std::format("id IN ({})", Db::make_placeholders(query.ids.size())));
        for (const auto& id : query.ids) {
            args.push_back(id.value);
        }
    }

    if (query.older_than_timestamp.has_value()) {
        where_clauses.push_back("timestamp < ?");
        args.push_back(query.older_than_timestamp->value);
    }

    std::string sql = std::format(R"(
        DELETE FROM downloads
        WHERE {}
        RETURNING id;
    )", Db::join_strings(where_clauses, " AND "));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        auto id = DownloadId(stmt.integer("id"));
        if (change) {
            change->deleted.push_back(id);
        }
    }
    VERIFY(stmt.error());

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::reset_downloads_state()
{

    std::string sql = std::format(R"(
        UPDATE downloads
        SET operation_state = {}
        WHERE operation_state = {};
    )", int64_t(orion::DownloadOperationState::failed), int64_t(orion::DownloadOperationState::downloading));

    VERIFY(m_db->exec(sql));

    SUCCEED();
}

} // namespace orion