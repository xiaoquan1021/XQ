#ifndef XQ_STRING_UTILS_H
#define XQ_STRING_UTILS_H

#include <xqModuleCommonExports.h>

#include <string>
#include <vector>

// Static-only utility class for common string operations
class XQMODULECOMMON_EXPORT xq_StringUtils
{
public:
    static std::string Trim(const std::string& str);
    static std::vector<std::string> Split(const std::string& str, char delimiter);
    static std::string ToLower(const std::string& str);
    static std::string ToUpper(const std::string& str);
    static bool StartsWith(const std::string& str, const std::string& prefix);
    static bool EndsWith(const std::string& str, const std::string& suffix);
    static std::string Replace(const std::string& str,
                               const std::string& from,
                               const std::string& to);
};

#endif // XQ_STRING_UTILS_H
