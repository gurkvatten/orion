#include <core/profiles_db/profiles_db.h>
#include <common/src/string_util.h>
#include <format>

namespace orion {

//-------------------------------------------------------------------------
// Search Engines
//-------------------------------------------------------------------------

static bool
contains_scheme(const std::string& url_template)
{
    return url_template.find("://") != std::string::npos;
}

std::vector<std::string>
ProfilesDb::get_search_engine_columns() const
{
    return {
        "id",
        "profile_id",
        "name",
        "search_url_template",
        "suggestions_url_template",
        "keyword",
    };
}

void
ProfilesDb::load_search_engine_from_stmt(const DbStmt& stmt, SearchEngineDto* search_engine) const
{
    search_engine->id = SearchEngineId(stmt.integer("id"));
    search_engine->profile_id = ProfileId(stmt.integer("profile_id"));
    search_engine->name = stmt.text("name");
    search_engine->search_url_template = stmt.text("search_url_template");
    search_engine->suggestions_url_template = stmt.text("suggestions_url_template");
    search_engine->keyword = stmt.text("keyword");
}

Error
ProfilesDb::get_search_engine_ids(GetSearchEngineIdsQuery query, std::vector<SearchEngineId>* ids)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    DbStmt stmt;
    VERIFY(m_db->query(R"(
        SELECT id FROM search_engines
        WHERE profile_id = ?1;
    )", {query.profile_id.value}, &stmt));

    while (stmt.next()) {
        ids->push_back(SearchEngineId(stmt.integer("id")));
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_search_engines(
    const std::vector<SearchEngineId>& ids, std::vector<SearchEngineDto>* search_engines)
{
    auto args = to_db_variant_vector(ids);
    std::string sql = std::format(R"(
        SELECT {}
        FROM search_engines
        WHERE id IN ({})
        ORDER BY profile_id, id;
    )", Db::join_strings(get_search_engine_columns(), ", "),
        Db::make_placeholders(ids.size())
    );

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        auto search_engine = SearchEngineDto{};
        load_search_engine_from_stmt(stmt, &search_engine);
        search_engines->push_back(std::move(search_engine));
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::create_search_engine(
    CreateSearchEngineQuery query, SearchEngineId* out_id, SearchEnginesChange* change)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    if (!query.search_url_template.empty() && !contains_scheme(query.search_url_template)) {
        query.search_url_template = "https://" + query.search_url_template;
    }

    if (!query.suggestions_url_template.empty() && !contains_scheme(query.suggestions_url_template)) {
        query.suggestions_url_template = "https://" + query.suggestions_url_template;
    }

    std::string sql = std::format(R"(
        INSERT INTO search_engines ({})
        VALUES ({})
        RETURNING {};
    )", Db::join_strings(get_search_engine_columns(), ", "),
        Db::make_placeholders(get_search_engine_columns().size()),
        Db::join_strings(get_search_engine_columns(), ", ")
    );

    std::vector<DbVariant> args = {
        nullptr, // id will be auto-generated
        query.profile_id.value,
        query.name,
        query.search_url_template,
        query.suggestions_url_template,
        query.keyword,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    SearchEngineDto search_engine{};
    load_search_engine_from_stmt(stmt, &search_engine);

    if (change) {
        change->created.emplace_back(search_engine);
    }
    if (out_id) {
        *out_id = search_engine.id;
    }
    SUCCEED();
}

Error
ProfilesDb::update_search_engine(UpdateSearchEngineQuery query, SearchEnginesChange* change)
{
    if (query.id == SearchEngineId(0)) {
        RAISE(ErrorCode::argument, "search engine id is not set");
    }

    SearchEngineDto search_engine{};
    {
        std::vector<SearchEngineDto> search_engines;

        VERIFY(get_search_engines({query.id}, &search_engines));
        if (search_engines.empty()) {
            RAISE(ErrorCode::not_found, "search engine with id %lld not found", query.id.value);
        }
        search_engine = search_engines[0];
    }

    if (!query.search_url_template.empty() && !contains_scheme(query.search_url_template)) {
        query.search_url_template = "https://" + query.search_url_template;
    }

    if (!query.suggestions_url_template.empty() && !contains_scheme(query.suggestions_url_template)) {
        query.suggestions_url_template = "https://" + query.suggestions_url_template;
    }

    std::vector<std::string> set_columns;
    std::vector<DbVariant> args;

    if (query.update_name) {
        args.push_back(query.name);
        set_columns.push_back("name");
    }

    if (query.update_search_url_template) {
        args.push_back(query.search_url_template);
        set_columns.push_back("search_url_template");
    }

    if (query.update_suggestions_url_template) {
        args.push_back(query.suggestions_url_template);
        set_columns.push_back("suggestions_url_template");
    }

    if (query.update_keyword) {
        args.push_back(query.keyword);
        set_columns.push_back("keyword");
    }

    if (set_columns.empty()) {
        SUCCEED(); // nothing to update
    }

    args.push_back(query.id.value);

    std::string sql = std::format(R"(
        UPDATE search_engines SET {}
        WHERE id = ?
        RETURNING {};
    )", Db::join_into_set_clause(set_columns),
        Db::join_strings(get_search_engine_columns(), ", ")
    );

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        SearchEngineDto search_engine{};
        load_search_engine_from_stmt(stmt, &search_engine);

        if (change) {
            change->updated.emplace_back(search_engine);
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::delete_search_engines(
    const std::vector<SearchEngineId>& ids, TxMode tx_mode, SearchEnginesChange* change)
{
    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    std::string sql = std::format(R"(
        DELETE FROM search_engines WHERE id IN ({})
        RETURNING id;
    )", Db::make_placeholders(ids.size()));

    DbStmt stmt;
    VERIFY(m_db->query(sql, to_db_variant_vector(ids), &stmt));

    while (stmt.next()) {
        auto id = SearchEngineId(stmt.integer("id"));
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
ProfilesDb::match_search_engines(MatchSearchEnginesQuery query, std::vector<SearchEngineMatch>* out_matches)
{
    RAISE_CHECK(out_matches);

    orion::str::ltrim(&query.query_string);
    auto idx = query.query_string.find_first_of(' ');
    if (idx == std::string::npos) {
        // nothing to match
        SUCCEED();
    }

    std::string keyword = query.query_string.substr(0, idx);

    // Collect search engines
    {
        std::vector<SearchEngineId> ids;
        VERIFY(get_search_engine_ids(GetSearchEngineIdsQuery{query.profile_id}, &ids));
        std::vector<SearchEngineDto> search_engines;
        VERIFY(get_search_engines(ids, &search_engines));
        for (const auto& search_engine : search_engines) {
            if (search_engine.keyword == keyword) {
                std::string search_query = query.query_string.substr(idx + 1);
                orion::str::ltrim(&search_query);
                out_matches->push_back(SearchEngineMatch{
                    .search_url_template = search_engine.search_url_template,
                    .search_query = search_query,
                });
            }
        }
    }

    // Collect bookmarks with keyword
    {
        std::vector<BookmarkId> ids;
        VERIFY(get_bookmark_ids(GetBookmarkIdsQuery{query.profile_id}, &ids));
        std::vector<BookmarkDto> bookmarks;
        VERIFY(get_bookmarks(GetBookmarksQuery{ids}, &bookmarks));
        for (const auto& bookmark : bookmarks) {
            if (bookmark.invocation_keyword == keyword) {
                std::string search_query = query.query_string.substr(idx + 1);
                orion::str::ltrim(&search_query);
                out_matches->push_back(SearchEngineMatch{
                    .search_url_template = bookmark.address,
                    .search_query = search_query,
                });
            }
        }
    }
    SUCCEED();
}

} // namespace orion