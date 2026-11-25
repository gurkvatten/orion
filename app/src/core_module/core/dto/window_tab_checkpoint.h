#pragma once

#include <core/dto/tab_dto.h>

namespace orion {

// Exact writes for the live model. No sequence refinement, normalization,
// implicit tabs, pinned-tab redirection, or other legacy UI policy is applied.
struct CheckpointWindow {
    WindowDto record{};
    bool create = false;
    bool properties = false;
    bool order = false;
};

struct CheckpointTab {
    TabDto record{};
    bool create = false;
    bool properties = false;
    bool order = false;
};

struct WindowTabCheckpoint {
    ProfileId profile_id{};
    std::vector<CheckpointWindow> windows{};
    std::vector<CheckpointTab> tabs{};
    std::vector<WindowId> deleted_windows{};
    std::vector<TabId> deleted_tabs{};

    bool empty() const {
        return windows.empty() && tabs.empty()
            && deleted_windows.empty() && deleted_tabs.empty();
    }
};

} // namespace orion
