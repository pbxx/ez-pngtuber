#include "GroupStore.h"

#include "AppPaths.h"

#include <sqlite3.h>

#include <cstdlib>
#include <utility>

namespace {
constexpr const char* ActiveGroupKey = "active_group_id";

bool StepDone(sqlite3_stmt* statement)
{
    return sqlite3_step(statement) == SQLITE_DONE;
}
}

GroupStore::GroupStore()
{
    Open();
}

GroupStore::~GroupStore()
{
    if (db_) {
        sqlite3_close(static_cast<sqlite3*>(db_));
        db_ = nullptr;
    }
}

bool GroupStore::IsReady() const
{
    return db_ != nullptr;
}

const std::string& GroupStore::GetLastError() const
{
    return lastError_;
}

std::vector<StreamKitGroup> GroupStore::ListGroups() const
{
    std::vector<StreamKitGroup> groups;
    if (!db_) {
        return groups;
    }

    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "SELECT id, name, overlay_url, browser_path, show_browser_window, "
        "bypass_local_network_prompt, poll_interval_ms "
        "FROM groups ORDER BY name COLLATE NOCASE, id";
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &statement, nullptr) != SQLITE_OK) {
        SetLastError("Could not list groups.");
        return groups;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        StreamKitGroup group;
        group.id = sqlite3_column_int(statement, 0);
        group.name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        group.overlayUrl = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
        group.browserPath = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
        group.showBrowserWindow = sqlite3_column_int(statement, 4) != 0;
        group.bypassLocalNetworkPrompt = sqlite3_column_int(statement, 5) != 0;
        group.pollIntervalMs = sqlite3_column_int(statement, 6);
        groups.push_back(std::move(group));
    }

    sqlite3_finalize(statement);
    return groups;
}

std::optional<int> GroupStore::GetActiveGroupId() const
{
    if (!db_) {
        return std::nullopt;
    }

    sqlite3_stmt* statement = nullptr;
    const char* sql = "SELECT value FROM app_settings WHERE key = ?";
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &statement, nullptr) != SQLITE_OK) {
        SetLastError("Could not read the active group.");
        return std::nullopt;
    }

    sqlite3_bind_text(statement, 1, ActiveGroupKey, -1, SQLITE_STATIC);

    std::optional<int> activeGroupId;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        activeGroupId = std::atoi(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)));
    }

    sqlite3_finalize(statement);
    return activeGroupId;
}

bool GroupStore::SetActiveGroup(int groupId)
{
    if (!db_) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "INSERT INTO app_settings(key, value) VALUES(?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value";
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &statement, nullptr) != SQLITE_OK) {
        return SetLastError("Could not save the active group.");
    }

    const std::string value = std::to_string(groupId);
    sqlite3_bind_text(statement, 1, ActiveGroupKey, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = StepDone(statement);
    sqlite3_finalize(statement);

    return ok ? true : SetLastError("Could not save the active group.");
}

std::optional<StreamKitGroup> GroupStore::CreateGroup(const std::string& name)
{
    if (!db_) {
        return std::nullopt;
    }

    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "INSERT INTO groups(name, overlay_url, browser_path, show_browser_window, "
        "bypass_local_network_prompt, poll_interval_ms) VALUES(?, '', '', 0, 1, 250)";
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &statement, nullptr) != SQLITE_OK) {
        SetLastError("Could not create the group.");
        return std::nullopt;
    }

    sqlite3_bind_text(statement, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = StepDone(statement);
    sqlite3_finalize(statement);
    if (!ok) {
        SetLastError("Could not create the group. Group names must be unique.");
        return std::nullopt;
    }

    StreamKitGroup group;
    group.id = static_cast<int>(sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_)));
    group.name = name;
    return group;
}

bool GroupStore::RenameGroup(int groupId, const std::string& name)
{
    if (!db_) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    const char* sql = "UPDATE groups SET name = ? WHERE id = ?";
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &statement, nullptr) != SQLITE_OK) {
        return SetLastError("Could not rename the group.");
    }

    sqlite3_bind_text(statement, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 2, groupId);
    const bool ok = StepDone(statement) && sqlite3_changes(static_cast<sqlite3*>(db_)) > 0;
    sqlite3_finalize(statement);

    return ok ? true : SetLastError("Could not rename the group. Group names must be unique.");
}

bool GroupStore::UpdateGroup(const StreamKitGroup& group)
{
    if (!db_) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "UPDATE groups SET overlay_url = ?, browser_path = ?, show_browser_window = ?, "
        "bypass_local_network_prompt = ?, poll_interval_ms = ? WHERE id = ?";
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &statement, nullptr) != SQLITE_OK) {
        return SetLastError("Could not save the group settings.");
    }

    sqlite3_bind_text(statement, 1, group.overlayUrl.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, group.browserPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 3, group.showBrowserWindow ? 1 : 0);
    sqlite3_bind_int(statement, 4, group.bypassLocalNetworkPrompt ? 1 : 0);
    sqlite3_bind_int(statement, 5, group.pollIntervalMs);
    sqlite3_bind_int(statement, 6, group.id);
    const bool ok = StepDone(statement) && sqlite3_changes(static_cast<sqlite3*>(db_)) > 0;
    sqlite3_finalize(statement);

    return ok ? true : SetLastError("Could not save the group settings.");
}

bool GroupStore::DeleteGroup(int groupId)
{
    if (!db_) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    const char* sql = "DELETE FROM groups WHERE id = ?";
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &statement, nullptr) != SQLITE_OK) {
        return SetLastError("Could not delete the group.");
    }

    sqlite3_bind_int(statement, 1, groupId);
    const bool ok = StepDone(statement) && sqlite3_changes(static_cast<sqlite3*>(db_)) > 0;
    sqlite3_finalize(statement);
    if (!ok) {
        return SetLastError("Could not delete the group.");
    }

    auto activeGroupId = GetActiveGroupId();
    if (activeGroupId && *activeGroupId == groupId) {
        const auto groups = ListGroups();
        if (!groups.empty()) {
            SetActiveGroup(groups.front().id);
        }
    }

    return EnsureDefaultGroup();
}

bool GroupStore::Open()
{
    sqlite3* database = nullptr;
    if (sqlite3_open(GetGroupDatabasePath().c_str(), &database) != SQLITE_OK) {
        if (database) {
            lastError_ = sqlite3_errmsg(database);
            sqlite3_close(database);
        } else {
            lastError_ = "Could not open the groups database.";
        }
        return false;
    }

    db_ = database;
    return InitializeSchema() && EnsureDefaultGroup();
}

bool GroupStore::InitializeSchema()
{
    const char* sql =
        "CREATE TABLE IF NOT EXISTS groups ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL UNIQUE,"
        "overlay_url TEXT NOT NULL DEFAULT '',"
        "browser_path TEXT NOT NULL DEFAULT '',"
        "show_browser_window INTEGER NOT NULL DEFAULT 0,"
        "bypass_local_network_prompt INTEGER NOT NULL DEFAULT 1,"
        "poll_interval_ms INTEGER NOT NULL DEFAULT 250"
        ");"
        "CREATE TABLE IF NOT EXISTS app_settings ("
        "key TEXT PRIMARY KEY,"
        "value TEXT NOT NULL"
        ");";

    char* errorMessage = nullptr;
    const int result = sqlite3_exec(static_cast<sqlite3*>(db_), sql, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        lastError_ = errorMessage ? errorMessage : "Could not initialize the groups database.";
        sqlite3_free(errorMessage);
        return false;
    }

    return true;
}

bool GroupStore::EnsureDefaultGroup()
{
    const auto groups = ListGroups();
    if (!groups.empty()) {
        auto activeGroupId = GetActiveGroupId();
        if (!activeGroupId) {
            return SetActiveGroup(groups.front().id);
        }
        return true;
    }

    auto group = CreateGroup("Default");
    if (!group) {
        return false;
    }

    return SetActiveGroup(group->id);
}

bool GroupStore::SetLastError(const std::string& error) const
{
    lastError_ = error;
    return false;
}
