#include "xq_StringUtils.h"

#include <algorithm>
#include <cctype>
#include <sstream>

std::string xq_StringUtils::Trim(const std::string& str)
{
    auto start = str.begin();
    while (start != str.end() && std::isspace(static_cast<unsigned char>(*start)))
    {
        ++start;
    }

    auto end = str.end();
    while (end != start && std::isspace(static_cast<unsigned char>(*(end - 1))))
    {
        --end;
    }

    return std::string(start, end);
}

std::vector<std::string> xq_StringUtils::Split(
    const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::istringstream stream(str);
    std::string token;
    while (std::getline(stream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

std::string xq_StringUtils::ToLower(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string xq_StringUtils::ToUpper(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

bool xq_StringUtils::StartsWith(const std::string& str,
                                 const std::string& prefix)
{
    if (prefix.size() > str.size())
    {
        return false;
    }
    return str.compare(0, prefix.size(), prefix) == 0;
}

bool xq_StringUtils::EndsWith(const std::string& str,
                               const std::string& suffix)
{
    if (suffix.size() > str.size())
    {
        return false;
    }
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string xq_StringUtils::Replace(const std::string& str,
                                     const std::string& from,
                                     const std::string& to)
{
    if (from.empty())
    {
        return str;
    }

    std::string result = str;
    std::string::size_type pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos)
    {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    return result;
}
