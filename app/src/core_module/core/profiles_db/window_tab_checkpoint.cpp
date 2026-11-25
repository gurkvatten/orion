#include <core/profiles_db/profiles_db.h>

#include <cmath>

namespace orion {

Error
ProfilesDb::apply_window_tab_checkpoint(const WindowTabCheckpoint& batch,
    WindowsChange* windows, TabsChange* tabs, ProfilesChange* profiles)
{
    RAISE_CHECK(batch.profile_id.value && windows && tabs && profiles);
    DbTx tx;
    VERIFY(m_db->begin(&tx));
    WindowsChange window_changes;
    TabsChange tab_changes;
    ProfilesChange profile_changes;

    // Gather occupied keys for collision-free temporary assignments. Final
    // window keys are unique per world; tab keys are unique per window.
    std::map<WindowId, double> old_windows;
    std::map<TabId, WindowId> old_tabs;
    std::set<double> occupied;
    {
        DbStmt stmt;
        VERIFY(m_db->query("SELECT id, seq_num FROM windows WHERE profile_id = ?;",
            {batch.profile_id.value}, &stmt));
        while(stmt.next()) {
            old_windows.emplace(WindowId(stmt.integer(0)), stmt.real(1));
            occupied.insert(stmt.real(1));
        }
        VERIFY(stmt.error());
    }
    {
        DbStmt stmt;
        VERIFY(m_db->query(R"(SELECT t.id, t.window_id, t.seq_num FROM tabs t
            JOIN windows w ON w.id = t.window_id WHERE w.profile_id = ?;)",
            {batch.profile_id.value}, &stmt));
        while(stmt.next()) {
            old_tabs.emplace(TabId(stmt.integer(0)), WindowId(stmt.integer(1)));
            occupied.insert(stmt.real(2));
        }
        VERIFY(stmt.error());
    }
    // The legacy getter lazily creates this window. Exact checkpoint writes
    // must not allocate additional records or sequence numbers as a side effect.
    int64_t shared_id = 0;
    VERIFY(m_db->query_integer("SELECT COALESCE(shared_pinned_tabs_window_id, 0) FROM profiles WHERE id=?;",
        {batch.profile_id.value}, &shared_id));
    const WindowId shared_window{shared_id};
    std::set<WindowId> changed_windows, created_windows, removed_windows;
    std::set<TabId> changed_tabs, created_tabs, removed_tabs;
    for(const auto& write : batch.windows) {
        const auto& w = write.record;
        RAISE_CHECK(w.id.value && w.profile_id == batch.profile_id && w.id != shared_window);
        RAISE_CHECK(changed_windows.insert(w.id).second);
        RAISE_CHECK(write.create != old_windows.contains(w.id));
        if(write.create || write.order) {
            RAISE_CHECK(std::isfinite(w.seq_num));
            occupied.insert(w.seq_num);
        }
        if(write.create) created_windows.insert(w.id);
    }
    for(WindowId id : batch.deleted_windows) {
        RAISE_CHECK(old_windows.contains(id) && id != shared_window);
        RAISE_CHECK(!changed_windows.contains(id) && removed_windows.insert(id).second);
    }
    for(const auto& write : batch.tabs) {
        const auto& t = write.record;
        RAISE_CHECK(t.id.value && changed_tabs.insert(t.id).second);
        RAISE_CHECK(write.create != old_tabs.contains(t.id));
        if(!write.create) {
            RAISE_CHECK(old_tabs.at(t.id) != shared_window);
            RAISE_CHECK(write.order || old_tabs.at(t.id) == t.window_id);
        }
        RAISE_CHECK((old_windows.contains(t.window_id) || created_windows.contains(t.window_id))
            && !removed_windows.contains(t.window_id) && t.window_id != shared_window);
        if(write.create || write.order) {
            RAISE_CHECK(std::isfinite(t.seq_num));
            occupied.insert(t.seq_num);
        }
        if(write.create) created_tabs.insert(t.id);
    }
    for(TabId id : batch.deleted_tabs) {
        RAISE_CHECK(old_tabs.contains(id) && old_tabs.at(id) != shared_window);
        RAISE_CHECK(!changed_tabs.contains(id) && removed_tabs.insert(id).second);
    }

    // Two-phase exact assignments allow swaps and cross-window moves with the
    // UNIQUE indexes still enabled. Temporary keys avoid every old/final key.
    double temporary = -1.0;
    auto temporary_key = [&] {
        while(occupied.contains(temporary)) temporary -= 1.0;
        double result = temporary;
        occupied.insert(temporary);
        temporary -= 1.0;
        return result;
    };
    for(const auto& write : batch.windows) {
        if(!write.create && write.order)
            VERIFY(m_db->exec("UPDATE windows SET seq_num = ? WHERE id = ?;",
                {temporary_key(), write.record.id.value}));
    }
    // Deleted windows must remain alive until surviving tabs have moved out.
    for(WindowId id : removed_windows)
        VERIFY(m_db->exec("UPDATE windows SET seq_num = ? WHERE id = ?;",
            {temporary_key(), id.value}));
    for(const auto& write : batch.tabs) {
        if(!write.create && write.order)
            VERIFY(m_db->exec("UPDATE tabs SET seq_num = ? WHERE id = ?;",
                {temporary_key(), write.record.id.value}));
    }
    if(!batch.deleted_tabs.empty())
        VERIFY(delete_tabs(batch.deleted_tabs, TxMode::no_tx, &tab_changes, nullptr));

    for(const auto& write : batch.windows) {
        const auto& w = write.record;
        if(write.create) {
            track_window_id(w.id);
            VERIFY(m_db->exec(R"(INSERT INTO windows
                (id, profile_id, name, icon_name, keyboard_shortcut, seq_num, is_tab_group,
                 has_ui_window, is_focus_mode, is_private, is_suspended, selected_tab_id)
                VALUES (?,?,?,?,?,?,?,?,?,?,?,NULLIF(?, 0));)",
                {w.id.value, w.profile_id.value, w.name, w.icon_name, w.keyboard_shortcut, w.seq_num,
                 w.is_tab_group, w.has_ui_window, w.is_focus_mode, w.is_private, w.is_suspended,
                 w.selected_tab_id.value}));
        } else {
            if(write.properties) {
                VERIFY(m_db->exec(R"(UPDATE windows SET name=?, icon_name=?, keyboard_shortcut=?, is_tab_group=?,
                    has_ui_window=?, is_focus_mode=?, is_private=?, is_suspended=?,
                    prev_selected_tab_id=CASE WHEN COALESCE(selected_tab_id, 0) != ?
                        THEN COALESCE(selected_tab_id, 0) ELSE prev_selected_tab_id END,
                    selected_tab_id=NULLIF(?, 0) WHERE id=?;)",
                    {w.name, w.icon_name, w.keyboard_shortcut, w.is_tab_group, w.has_ui_window,
                     w.is_focus_mode, w.is_private, w.is_suspended, w.selected_tab_id.value,
                     w.selected_tab_id.value, w.id.value}));
                window_changes.updated_name = window_changes.updated_icon_name = true;
                window_changes.updated_keyboard_shortcut = true;
                window_changes.updated_is_tab_group = window_changes.updated_has_ui_window = true;
                window_changes.updated_is_focus_mode = window_changes.updated_is_private = true;
                window_changes.updated_is_suspended = true;
                window_changes.updated_selected_tab_id = true;
            }
            if(write.order) {
                VERIFY(m_db->exec("UPDATE windows SET seq_num=? WHERE id=?;", {w.seq_num, w.id.value}));
                window_changes.updated_seq_num = true;
            }
        }
    }
    for(const auto& write : batch.tabs) {
        const auto& t = write.record;
        changed_windows.insert(t.window_id);
        if(auto old = old_tabs.find(t.id); old != old_tabs.end()) changed_windows.insert(old->second);
        if(write.create) {
            track_tab_id(t.id);
            VERIFY(m_db->exec(R"(INSERT INTO tabs
                (id, window_id, name, icon_name, is_pinned, is_suspended, seq_num, uri,
                 session_state, parent_tab_id, is_collapsed, back_forward_list_index,
                 webkit_back_forward_list_index, merged_back_forward_list_length, snapshot_image)
                VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);)",
                {t.id.value, t.window_id.value, t.name, t.icon_name, t.is_pinned, t.is_suspended,
                 t.seq_num, t.uri, t.session_state, t.parent_tab_id.value, t.is_collapsed,
                 t.back_forward_list_index, t.webkit_back_forward_list_index,
                 t.merged_back_forward_list_length, t.snapshot_image}));
        } else {
            if(write.properties) {
                VERIFY(m_db->exec(R"(UPDATE tabs SET name=?, icon_name=?, is_pinned=?, is_suspended=?,
                    uri=?, session_state=?, parent_tab_id=?, is_collapsed=?, back_forward_list_index=?,
                    webkit_back_forward_list_index=?, merged_back_forward_list_length=?, snapshot_image=? WHERE id=?;)",
                    {t.name, t.icon_name, t.is_pinned, t.is_suspended, t.uri, t.session_state,
                     t.parent_tab_id.value, t.is_collapsed, t.back_forward_list_index,
                     t.webkit_back_forward_list_index, t.merged_back_forward_list_length,
                     t.snapshot_image, t.id.value}));
                tab_changes.updated_name = tab_changes.updated_icon_name = true;
                tab_changes.updated_is_pinned = tab_changes.updated_is_suspended = true;
                tab_changes.updated_uri = tab_changes.updated_session_state = true;
                tab_changes.updated_parent_tab_id = tab_changes.updated_is_collapsed = true;
                tab_changes.updated_back_forward_list_index = tab_changes.updated_webkit_back_forward_list_index = true;
                tab_changes.updated_merged_back_forward_list_length = tab_changes.updated_snapshot_image = true;
            }
            if(write.order) {
                VERIFY(m_db->exec("UPDATE tabs SET window_id=?, seq_num=? WHERE id=?;",
                    {t.window_id.value, t.seq_num, t.id.value}));
                tab_changes.updated_window_id = tab_changes.updated_seq_num = true;
            }
        }
    }
    for(TabId id : removed_tabs) changed_windows.insert(old_tabs.at(id));
    std::vector<WindowDto> deleted_windows;
    VERIFY(get_windows(batch.deleted_windows, &deleted_windows));
    for(const auto& window : deleted_windows) {
        // Remaining tabs are deleted by the FK cascade; tabs moved out above
        // are already safe in their final destination.
        VERIFY(get_tab_ids({.window_id = window.id}, &tab_changes.deleted));
        VERIFY(m_db->exec("DELETE FROM windows WHERE id=?;", {window.id.value}));
        window_changes.deleted.push_back({
            .id = window.id, .profile_id = window.profile_id, .is_private = window.is_private,
        });
    }
    VERIFY(reset_selected_window_ids(batch.deleted_windows, &profile_changes));

    // Query notifications only after all ordering and parent changes are final.
    // In particular, never expose the temporary keys or stale derived indices.
    std::vector<WindowId> surviving_windows;
    for(WindowId id : changed_windows) {
        if(removed_windows.contains(id)) continue;
        VERIFY(refresh_window_num_tabs(id, nullptr));
        surviving_windows.push_back(id);
        std::vector<TabId> ids;
        VERIFY(get_tab_ids({.window_id = id}, &ids));
        changed_tabs.insert(ids.begin(), ids.end());
    }
    if(!tab_changes.updated.empty()) tab_changes.updated_parent_tab_id = true;
    for(const auto& tab : tab_changes.updated) changed_tabs.insert(tab.id);
    tab_changes.updated.clear();
    std::vector<TabDto> final_tabs;
    VERIFY(get_tabs({changed_tabs.begin(), changed_tabs.end()}, &final_tabs));
    for(auto& tab : final_tabs) {
        (created_tabs.contains(tab.id) ? tab_changes.created : tab_changes.updated).push_back(std::move(tab));
    }
    std::vector<WindowDto> final_windows;
    VERIFY(get_windows(surviving_windows, &final_windows));
    for(auto& window : final_windows) {
        (created_windows.contains(window.id) ? window_changes.created : window_changes.updated).push_back(std::move(window));
    }
    VERIFY(tx.commit());
    *windows = std::move(window_changes);
    *tabs = std::move(tab_changes);
    *profiles = std::move(profile_changes);
    SUCCEED();
}

} // namespace orion
