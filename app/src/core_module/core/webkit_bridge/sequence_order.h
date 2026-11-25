#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>
#include <vector>

namespace orion::bridge {

// Input is final sibling order; a missing key denotes a new/reparented item.
// Keep a longest increasing subsequence of existing keys as stable anchors.
// Only an exhausted floating-point gap requires re-keying the whole list.
inline std::vector<double>
reconcile_sequence_order(const std::vector<std::optional<double>>& previous,
    const std::set<double>& reserved = {})
{
    const size_t size = previous.size();
    std::vector<double> result(size);
    std::vector<size_t> tails, predecessor(size, size);
    for(size_t i = 0; i < size; ++i) {
        if(!previous[i] || !std::isfinite(*previous[i]) || reserved.contains(*previous[i])) continue;
        auto pos = std::lower_bound(tails.begin(), tails.end(), *previous[i],
            [&](size_t j, double value) { return *previous[j] < value; });
        if(pos != tails.begin()) predecessor[i] = *(pos - 1);
        if(pos == tails.end()) tails.push_back(i);
        else *pos = i;
    }
    std::vector<size_t> anchors;
    for(size_t i = tails.empty() ? size : tails.back(); i != size; i = predecessor[i]) {
        anchors.push_back(i);
        result[i] = *previous[i];
    }
    std::reverse(anchors.begin(), anchors.end());
    anchors.push_back(size);

    size_t start = 0;
    bool exhausted = false;
    for(size_t end : anchors) {
        const size_t count = end - start;
        const double low = start ? result[start - 1]
            : end < size ? result[end] - 1024.0 * (count + 1) : -1024.0;
        const double high = end < size ? result[end] : low + 1024.0 * (count + 1);
        double last = low;
        for(size_t i = start; i < end; ++i) {
            // lerp avoids overflow for anchors of opposite signs.
            double value = std::lerp(low, high, double(i - start + 1) / double(count + 1));
            if(value <= last) value = std::nextafter(last, high);
            while(std::isfinite(value) && value < high && reserved.contains(value)) {
                value = std::nextafter(value, high);
            }
            if(!std::isfinite(value) || !(value > last && value < high)) {
                exhausted = true;
                break;
            }
            result[i] = last = value;
        }
        if(exhausted) break;
        start = end + 1;
    }
    if(exhausted) {
        double value = 0.0;
        for(double& key : result) {
            while(reserved.contains(value)) value += 1024.0;
            key = value;
            value += 1024.0;
        }
    }
    return result;
}

} // namespace orion::bridge
