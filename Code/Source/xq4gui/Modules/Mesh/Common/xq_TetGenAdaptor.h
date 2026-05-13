#pragma once

#include <xqMeshCommonExports.h>

#include "xq_GridAdaptor.h"

class XQMESHCOMMON_EXPORT xq_TetGenAdaptor : public xq_GridAdaptor
{
public:
    xq_TetGenAdaptor() = default;
    ~xq_TetGenAdaptor() override = default;

    bool Adapt() override;
    [[nodiscard]] xq_AdaptResult AdaptWithResult();

    void SetMaxRefinementRatio(double ratio);
    [[nodiscard]] double GetMaxRefinementRatio() const;

    void SetMaxIterations(int iterations);
    [[nodiscard]] int GetMaxIterations() const;

private:
    double m_MaxRefinementRatio = 2.0;
    int m_MaxIterations = 5;
};
