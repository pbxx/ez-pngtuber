#pragma once

#include <optional>
#include <string>
#include <vector>

struct StreamKitGroup {
    int id = 0;
    std::string name;
    std::string overlayUrl;
    std::string browserPath;
    bool showBrowserWindow = false;
    bool bypassLocalNetworkPrompt = true;
    int pollIntervalMs = 250;
};

class GroupStore final {
public:
    GroupStore();
    ~GroupStore();

    GroupStore(const GroupStore&) = delete;
    GroupStore& operator=(const GroupStore&) = delete;

    bool IsReady() const;
    const std::string& GetLastError() const;

    std::vector<StreamKitGroup> ListGroups() const;
    std::optional<int> GetActiveGroupId() const;
    bool SetActiveGroup(int groupId);
    std::optional<StreamKitGroup> CreateGroup(const std::string& name);
    bool RenameGroup(int groupId, const std::string& name);
    bool UpdateGroup(const StreamKitGroup& group);
    bool DeleteGroup(int groupId);

private:
    bool Open();
    bool InitializeSchema();
    bool EnsureDefaultGroup();
    bool SetLastError(const std::string& error) const;

    void* db_ = nullptr;
    mutable std::string lastError_;
};
