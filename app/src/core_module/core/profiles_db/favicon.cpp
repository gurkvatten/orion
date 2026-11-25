#include <core/profiles_db/profiles_db.h>
#include <ctime>

namespace orion {

static constexpr int64_t ICON_EXPIRATION_SECS = 60 * 60 * 24 * 4;  // 4 days

static int64_t
now_unix()
{
    return static_cast<int64_t>(std::time(nullptr));
}

Error
ProfilesDb::get_favicon_for_page_url(const std::string& page_url, FaviconDto* favicon,
    bool* out_found, bool* out_expired
) {
    if (page_url.empty()) {
        RAISE(ErrorCode::argument, "page_url is empty");
    }

    DbStmt stmt;
    VERIFY(m_db->query(R"(
        SELECT i.icon_url, i.data, i.stamp
        FROM favicon_page_urls p
        INNER JOIN favicon_icons i ON p.icon_id = i.id
        WHERE p.page_url = ?1;
    )", {page_url}, &stmt));

    if (!stmt.next()) {
        VERIFY(stmt.error());
        if (out_found) *out_found = false;
        if (out_expired) *out_expired = false;
        SUCCEED();
    }
    VERIFY(stmt.error());

    if (out_found) *out_found = true;

    favicon->icon_url = stmt.text("icon_url");
    favicon->data = stmt.blob("data");

    int64_t stamp = stmt.integer("stamp");
    if (out_expired) {
        *out_expired = stamp <= (now_unix() - ICON_EXPIRATION_SECS);
    }

    SUCCEED();
}

Error
ProfilesDb::set_favicon_for_page_url(const SetFaviconQuery& query)
{
    if (query.page_url.empty()) {
        RAISE(ErrorCode::argument, "page_url is empty");
    }
    if (query.icon_url.empty()) {
        RAISE(ErrorCode::argument, "icon_url is empty");
    }

    DbTx tx;
    VERIFY(tx.begin(m_db));

    int64_t stamp = now_unix();

    // Upsert the icon: if the icon_url already exists, update data and stamp.
    VERIFY(m_db->exec(R"(
        INSERT INTO favicon_icons (icon_url, data, stamp)
        VALUES (?1, ?2, ?3)
        ON CONFLICT(icon_url) DO UPDATE SET data = excluded.data, stamp = excluded.stamp;
    )", {query.icon_url, query.data, stamp}));

    // Get the icon id.
    DbStmt id_stmt;
    VERIFY(m_db->query(R"(
        SELECT id FROM favicon_icons WHERE icon_url = ?1;
    )", {query.icon_url}, &id_stmt));

    bool has_row = id_stmt.next();
    VERIFY(id_stmt.error());
    RAISE_CHECK(has_row);

    int64_t icon_id = id_stmt.integer("id");

    // Upsert the page URL → icon mapping.
    VERIFY(m_db->exec(R"(
        INSERT INTO favicon_page_urls (page_url, icon_id)
        VALUES (?1, ?2)
        ON CONFLICT(page_url) DO UPDATE SET icon_id = excluded.icon_id;
    )", {query.page_url, icon_id}));

    VERIFY(tx.commit());
    SUCCEED();
}

Error
ProfilesDb::prune_expired_favicons()
{
    static constexpr int64_t NOT_USED_EXPIRATION_SECS = 60 * 60 * 24 * 30; // 30 days

    int64_t threshold = now_unix() - NOT_USED_EXPIRATION_SECS;

    DbTx tx;
    VERIFY(tx.begin(m_db));

    // Delete icons older than 30 days.
    VERIFY(m_db->exec(R"(
        DELETE FROM favicon_icons WHERE stamp <= ?1;
    )", {threshold}));

    // Cascade cleanup: delete orphaned page URL mappings.
    VERIFY(m_db->exec(R"(
        DELETE FROM favicon_page_urls WHERE icon_id NOT IN (SELECT id FROM favicon_icons);
    )", {}));

    VERIFY(tx.commit());
    SUCCEED();
}

} // namespace orion
