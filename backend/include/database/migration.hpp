#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace wt {

class Database;

struct AppliedMigration {
  std::int64_t version = 0;
  std::string name;
  std::string applied_at;
};

// Applies versioned SQL migrations from a directory. Migration files must be
// named `<version>_<description>.sql` (for example `001_init.sql`).
class MigrationRunner {
 public:
  explicit MigrationRunner(Database& database) : database_(database) {}

  // Ensures the bookkeeping table exists and returns the highest version.
  std::int64_t current_version();

  std::vector<AppliedMigration> applied();

  // Applies every migration with a version greater than the current one, in
  // ascending order. Each migration runs inside its own transaction. Returns
  // the versions that were applied. Throws if a migration fails.
  std::vector<std::int64_t> run(const std::string& migrations_dir);

 private:
  Database& database_;
};

}  // namespace wt
