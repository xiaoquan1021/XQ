#pragma once

#include <xqModulePathExports.h>
#include "xq_CenterlineSegment.h"

#include <mitkBaseData.h>
#include <itkEventObject.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// XQ Design: VesselCenterline composes CenterlineSegments via unique_ptr ownership.
// Each time step owns exactly one segment; callers observe via raw pointer (non-owning).
class XQMODULEPATH_EXPORT xq_VesselCenterline : public mitk::BaseData
{
public:
    mitkClassMacro(xq_VesselCenterline, mitk::BaseData)
    itkFactorylessNewMacro(Self)
    itkCloneMacro(Self)

    enum InsertionStrategy { AUTO_LOCATE = 0, PREPEND, APPEND, INSERT_PRIOR, INSERT_NEXT };

    // BaseData overrides
    void UpdateOutputInformation() override;
    void SetRequestedRegionToLargestPossibleRegion() override;
    bool RequestedRegionIsOutsideOfTheBufferedRegion() override;
    bool VerifyRequestedRegion() override;
    void SetRequestedRegion(const itk::DataObject* data) override;

    void Expand(unsigned int timeSteps) override;
    void ExecuteOperation(mitk::Operation* operation) override;
    [[nodiscard]] bool IsEmptyTimeStep(unsigned int t) const override;

    // XQ Design: observer access — caller does NOT own the returned pointer
    [[nodiscard]] xq_CenterlineSegment* GetSegment(unsigned int t = 0) const;
    // XQ Design: takes ownership of pathElement via unique_ptr
    void SetSegment(xq_CenterlineSegment* pathElement, unsigned int t = 0);

    [[nodiscard]] InsertionStrategy GetInsertionStrategy() const;
    void SetInsertionStrategy(InsertionStrategy mode);

    void SetAttribute(const std::string& key, const std::string& value);
    [[nodiscard]] std::string GetAttribute(const std::string& key) const;
    [[nodiscard]] std::map<std::string, std::string> GetAttributes() const;

    [[nodiscard]] int GetNodeCount() const;

protected:
    xq_VesselCenterline();
    xq_VesselCenterline(const xq_VesselCenterline& other);
    ~xq_VesselCenterline() override = default;

    void ClearData() override;
    void InitializeEmpty() override;

    // XQ Design: unique_ptr enforces single-owner semantics per time step
    std::vector<std::unique_ptr<xq_CenterlineSegment>> m_Segments;
    InsertionStrategy m_InsertionStrategy = AUTO_LOCATE;
    std::map<std::string, std::string> m_Attributes;
};

// Custom event hierarchy for path notifications
itkEventMacroDeclaration(xq_PathEvent, itk::AnyEvent);
itkEventMacroDeclaration(xq_PathPointEvent, xq_PathEvent);
itkEventMacroDeclaration(xq_PathPointSelectEvent, xq_PathPointEvent);
itkEventMacroDeclaration(xq_PathPointMoveEvent, xq_PathPointEvent);
itkEventMacroDeclaration(xq_PathSizeChangeEvent, xq_PathEvent);
