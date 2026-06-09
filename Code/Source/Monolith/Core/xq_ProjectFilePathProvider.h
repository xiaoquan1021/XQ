#ifndef XQ_PROJECTFILEPATHPROVIDER_H
#define XQ_PROJECTFILEPATHPROVIDER_H

#include <QString>

namespace xq::core
{

struct ProjectFilePath
{
    QString ProjectName;
    QString ProjectFilePath;
};

class ProjectFilePathProvider
{
public:
    virtual ~ProjectFilePathProvider() = default;

    virtual ProjectFilePath NewProjectFilePath() = 0;
    virtual QString OpenProjectFilePath() = 0;
};

} // namespace xq::core

#endif // XQ_PROJECTFILEPATHPROVIDER_H
