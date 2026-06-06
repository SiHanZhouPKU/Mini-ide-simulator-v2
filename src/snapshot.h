#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <vector>
#include <map>
#include <string>
#include "variant_value.h"

struct Snapshot {
    int lineNumber;
    std::map<std::string, VariantValue> variables;
    std::string consoleOutput;
    /** Simulated memory map: variable name → (address, value) */
    std::map<std::string, std::pair<int, VariantValue>> memoryMap;
};

class SnapshotManager {
    std::vector<Snapshot> snapshots_;
    int currentIndex_ = -1;

public:
    void takeSnapshot(int line,
                      const std::map<std::string, VariantValue>& vars,
                      const std::string& output,
                      const std::map<std::string, std::pair<int, VariantValue>>& memMap = {}) {
        snapshots_.push_back({line, vars, output, memMap});
        currentIndex_ = static_cast<int>(snapshots_.size()) - 1;
    }

    const Snapshot& getSnapshot(int idx) const { return snapshots_[idx]; }
    int count() const { return static_cast<int>(snapshots_.size()); }

    void clear() {
        snapshots_.clear();
        currentIndex_ = -1;
    }

    void setCurrentIndex(int idx) { currentIndex_ = idx; }
    int currentIndex() const { return currentIndex_; }
};

#endif // SNAPSHOT_H
