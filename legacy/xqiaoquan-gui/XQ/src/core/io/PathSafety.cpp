#include "core/io/PathSafety.h"

#include <filesystem>
#include <system_error>

namespace xq {

bool isConfinedRelativePath(const std::string& relPath)
{
    if (relPath.empty()) {
        return false;
    }
    const std::filesystem::path rel(relPath);
    if (rel.is_absolute() || rel.has_root_name() || rel.has_root_directory()) {
        return false;
    }
    for (std::filesystem::path::const_iterator it = rel.begin(); it != rel.end(); ++it) {
        if (*it == "..") {
            return false;
        }
    }
    return true;
}

bool isPathWithinRoot(const std::string& rootDir, const std::string& fullPath)
{
    namespace fs = std::filesystem;
    // Empty root has no confinement boundary to enforce. Current production never
    // constructs GRM with an empty root (XQMainWindow gates on !assetRootDir.empty()),
    // and BlobStore always has a real rootDir. Treat empty root as "cannot confine" =>
    // reject, so an empty root can never silently accept an escaping path.
    if (rootDir.empty()) {
        return false;
    }
    std::error_code ec1, ec2;
    // weakly_canonical resolves junctions/symlinks (MSVC: GetFinalPathNameByHandle)
    // and does not require the leaf to exist -- fine for pre-open checks.
    const fs::path canonRoot = fs::weakly_canonical(fs::path(rootDir), ec1);
    const fs::path canonPath = fs::weakly_canonical(fs::path(fullPath), ec2);
    if (ec1 || ec2 || canonRoot.empty()) {
        return false;
    }
    // Element-wise prefix: every component of canonRoot must match canonPath.
    fs::path::const_iterator rb = canonRoot.begin();
    fs::path::const_iterator pb = canonPath.begin();
    for (; rb != canonRoot.end(); ++rb, ++pb) {
        if (pb == canonPath.end() || *pb != *rb) {
            return false;
        }
    }
    return true;
}

} // namespace xq
