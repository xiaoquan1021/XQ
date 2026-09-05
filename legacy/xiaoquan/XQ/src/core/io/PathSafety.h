#ifndef XQ_CORE_IO_PATH_SAFETY_H
#define XQ_CORE_IO_PATH_SAFETY_H

#include <string>

namespace xq {

// Lexical confinement: rejects empty, absolute, rooted, and any path containing
// a ".." component. Does NOT touch the filesystem. First, cheap gate.
bool isConfinedRelativePath(const std::string& relPath);

// Physical confinement: canonicalizes both paths (resolving NTFS junctions /
// symlinks via the OS) and returns true only if fullPath resolves to a location
// at or below rootDir. This is the ONLY check that stops a junction whose target
// is outside the asset root. Any canonicalization failure => not confined.
// An empty rootDir returns false (no boundary to enforce; never silently accept).
bool isPathWithinRoot(const std::string& rootDir, const std::string& fullPath);

} // namespace xq

#endif // XQ_CORE_IO_PATH_SAFETY_H
