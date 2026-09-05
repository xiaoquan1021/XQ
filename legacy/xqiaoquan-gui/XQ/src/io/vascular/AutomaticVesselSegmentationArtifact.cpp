#include "io/vascular/AutomaticVesselSegmentationArtifact.h"

#include "io/blob/Sha256.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace xq {
namespace {

constexpr std::uint8_t kMagic[8] = {'X', 'Q', 'V', 'M', 'A', 'S', 'K', '1'};
constexpr std::uint32_t kFormatVersion = 6;
constexpr std::size_t kMaximumStringBytes = 1024 * 1024;
constexpr std::size_t kMaximumComponentRecords = 1024 * 1024;
constexpr std::size_t kMaximumUpstreamStages = 4096;
constexpr std::size_t kMaximumStageParameters = 4096;

class Encoder {
public:
    void byte(std::uint8_t value) { bytes_.push_back(value); }

    void raw(const void* data, std::size_t size)
    {
        if (size == 0) {
            return;
        }
        const auto* begin = static_cast<const std::uint8_t*>(data);
        bytes_.insert(bytes_.end(), begin, begin + size);
    }

    void u32(std::uint32_t value)
    {
        for (std::size_t index = 0; index < 4; ++index) {
            byte(static_cast<std::uint8_t>((value >> (index * 8)) & 0xffu));
        }
    }

    void u64(std::uint64_t value)
    {
        for (std::size_t index = 0; index < 8; ++index) {
            byte(static_cast<std::uint8_t>((value >> (index * 8)) & 0xffu));
        }
    }

    void i32(std::int32_t value)
    {
        u32(static_cast<std::uint32_t>(value));
    }

    void floating(double value)
    {
        static_assert(sizeof(double) == sizeof(std::uint64_t),
                      "Artifact encoding requires binary64 storage");
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        u64(bits);
    }

    void text(const std::string& value)
    {
        if (value.size() > kMaximumStringBytes
            || value.size() > (std::numeric_limits<std::uint32_t>::max)()) {
            valid_ = false;
            return;
        }
        u32(static_cast<std::uint32_t>(value.size()));
        raw(value.data(), value.size());
    }

    const std::vector<std::uint8_t>& bytes() const { return bytes_; }
    bool valid() const { return valid_; }

private:
    std::vector<std::uint8_t> bytes_;
    bool valid_ = true;
};

class Decoder {
public:
    explicit Decoder(const std::vector<std::uint8_t>& bytes)
        : bytes_(bytes)
    {
    }

    bool raw(void* output, std::size_t size)
    {
        if (output == nullptr || size > remaining()) {
            valid_ = false;
            return false;
        }
        if (size > 0) {
            std::memcpy(output, bytes_.data() + offset_, size);
        }
        offset_ += size;
        return true;
    }

    bool byte(std::uint8_t* value) { return raw(value, sizeof(*value)); }

    bool u32(std::uint32_t* value)
    {
        if (value == nullptr || remaining() < 4) {
            valid_ = false;
            return false;
        }
        std::uint32_t decoded = 0;
        for (std::size_t index = 0; index < 4; ++index) {
            decoded |= static_cast<std::uint32_t>(bytes_[offset_ + index])
                << (index * 8);
        }
        offset_ += 4;
        *value = decoded;
        return true;
    }

    bool u64(std::uint64_t* value)
    {
        if (value == nullptr || remaining() < 8) {
            valid_ = false;
            return false;
        }
        std::uint64_t decoded = 0;
        for (std::size_t index = 0; index < 8; ++index) {
            decoded |= static_cast<std::uint64_t>(bytes_[offset_ + index])
                << (index * 8);
        }
        offset_ += 8;
        *value = decoded;
        return true;
    }

    bool i32(std::int32_t* value)
    {
        std::uint32_t decoded = 0;
        if (!u32(&decoded) || value == nullptr) {
            return false;
        }
        *value = static_cast<std::int32_t>(decoded);
        return true;
    }

    bool floating(double* value)
    {
        std::uint64_t bits = 0;
        if (!u64(&bits) || value == nullptr) {
            return false;
        }
        std::memcpy(value, &bits, sizeof(bits));
        return true;
    }

    bool text(std::string* value)
    {
        std::uint32_t size = 0;
        if (!u32(&size) || value == nullptr || size > kMaximumStringBytes
            || static_cast<std::size_t>(size) > remaining()) {
            valid_ = false;
            return false;
        }
        value->assign(reinterpret_cast<const char*>(bytes_.data() + offset_), size);
        offset_ += size;
        return true;
    }

    std::size_t remaining() const { return bytes_.size() - offset_; }
    bool valid() const { return valid_; }

private:
    const std::vector<std::uint8_t>& bytes_;
    std::size_t offset_ = 0;
    bool valid_ = true;
};

bool decodeProfile(Decoder* decoder,
                   std::uint32_t version,
                   AutomaticVesselSegmentationProfileV1* profile)
{
    std::uint64_t minimumSamples = 0;
    if (decoder == nullptr || profile == nullptr
        || !decoder->text(&profile->profileId)
        || !decoder->u32(&profile->schemaVersion)
        || !decoder->floating(&profile->candidateVesselnessQuantile)
        || !decoder->floating(&profile->coreVesselnessQuantile)
        || !decoder->floating(&profile->candidateMaximumFractionOfMaximum)
        || !decoder->floating(&profile->coreMaximumFractionOfMaximum)
        || !decoder->u32(&profile->vesselnessHistogramBins)
        || !decoder->floating(&profile->hardIntensityLower)
        || !decoder->floating(&profile->hardIntensityUpper)
        || !decoder->floating(&profile->coreIntensityLowerQuantile)
        || !decoder->floating(&profile->coreIntensityUpperQuantile)
        || !decoder->floating(&profile->intensityMargin)
        || !decoder->u32(&profile->intensityHistogramBins)
        || !decoder->u64(&minimumSamples)) {
        return false;
    }
    if (version >= 3) {
        for (int axis = 0; axis < 3; ++axis) {
            if (!decoder->floating(
                    &profile->automaticSeedAnchorNormalized[axis])
                || !decoder->floating(
                    &profile->automaticSeedSearchRadiusNormalized[axis])) {
                return false;
            }
        }
        if (!decoder->floating(&profile->automaticSeedIntensityLower)
            || !decoder->floating(&profile->automaticSeedIntensityUpper)
            || !decoder->floating(&profile->automaticSeedDistancePenalty)) {
            return false;
        }
    }
    if (version >= 4
        && (!decoder->floating(&profile->confidenceMultiplier)
            || !decoder->u32(&profile->confidenceIterations)
            || !decoder->u32(
                &profile->confidenceInitialNeighborhoodRadiusVoxels)
            || !decoder->floating(&profile->confidenceExpansionRadiusMm)
            || !decoder->floating(
                &profile->maximumConfidenceForegroundFraction))) {
        return false;
    }
    if (!decoder->floating(&profile->closingRadiusMm)
        || !decoder->floating(&profile->openingRadiusMm)
        || !decoder->floating(&profile->minimumComponentVolumeMm3)
        || !decoder->floating(&profile->minimumCoreVolumeMm3)
        || !decoder->floating(&profile->maximumBoundaryFraction)
        || !decoder->floating(&profile->minimumRelativeScore)) {
        return false;
    }
    if (version >= 3
        && !decoder->floating(&profile->additionalComponentScoreFraction)) {
        return false;
    }
    if (!decoder->u32(&profile->maximumRetainedComponents)
        || !decoder->u32(&profile->maximumReportedComponents)
        || minimumSamples > (std::numeric_limits<std::size_t>::max)()) {
        return false;
    }
    profile->minimumIntensitySampleCount =
        static_cast<std::size_t>(minimumSamples);
    return validateAutomaticVesselSegmentationProfile(*profile)
        == AutomaticVesselSegmentationProfileValidationCode::Ok;
}

void encodeGeometry(Encoder* encoder, const ImageGeometry& geometry)
{
    for (int axis = 0; axis < 3; ++axis) {
        encoder->i32(geometry.dimensions[axis]);
        encoder->floating(geometry.spacing[axis]);
        encoder->floating(geometry.origin[axis]);
    }
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            encoder->floating(geometry.direction[row][column]);
        }
    }
    encoder->u32(geometry.coordinateSystem == ImageCoordinateSystem::LPS ? 1u : 2u);
}

bool decodeGeometry(Decoder* decoder, ImageGeometry* geometry)
{
    if (decoder == nullptr || geometry == nullptr) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        std::int32_t dimension = 0;
        if (!decoder->i32(&dimension)
            || !decoder->floating(&geometry->spacing[axis])
            || !decoder->floating(&geometry->origin[axis])) {
            return false;
        }
        geometry->dimensions[axis] = dimension;
    }
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            if (!decoder->floating(&geometry->direction[row][column])) {
                return false;
            }
        }
    }
    std::uint32_t coordinateSystem = 0;
    if (!decoder->u32(&coordinateSystem)
        || (coordinateSystem != 1 && coordinateSystem != 2)) {
        return false;
    }
    geometry->coordinateSystem = coordinateSystem == 1
        ? ImageCoordinateSystem::LPS
        : ImageCoordinateSystem::RAS;
    return true;
}

bool decodeComponent(Decoder* decoder,
                     std::uint32_t version,
                     AutomaticVesselComponentRecord* component)
{
    std::uint64_t voxelCount = 0;
    std::uint64_t coreVoxelCount = 0;
    std::uint64_t boundaryVoxelCount = 0;
    if (decoder == nullptr || component == nullptr
        || !decoder->u32(&component->label)
        || !decoder->u64(&voxelCount)
        || !decoder->u64(&coreVoxelCount)
        || !decoder->floating(&component->physicalVolumeMm3)
        || !decoder->floating(&component->coreVolumeMm3)
        || !decoder->u64(&boundaryVoxelCount)
        || !decoder->floating(&component->boundaryFraction)
        || !decoder->floating(&component->meanVesselness)
        || !decoder->floating(&component->maximumVesselness)
        || voxelCount > (std::numeric_limits<std::size_t>::max)()
        || coreVoxelCount > (std::numeric_limits<std::size_t>::max)()
        || boundaryVoxelCount > (std::numeric_limits<std::size_t>::max)()) {
        return false;
    }
    component->voxelCount = static_cast<std::size_t>(voxelCount);
    component->coreVoxelCount = static_cast<std::size_t>(coreVoxelCount);
    component->boundaryVoxelCount = static_cast<std::size_t>(boundaryVoxelCount);
    for (int axis = 0; axis < 3; ++axis) {
        std::int32_t minimum = 0;
        std::int32_t maximum = 0;
        if (!decoder->i32(&minimum) || !decoder->i32(&maximum)
            || !decoder->floating(&component->physicalExtentMm[axis])) {
            return false;
        }
        if (version >= 3
            && !decoder->floating(&component->centroidIndex[axis])) {
            return false;
        }
        component->minimumIndex[axis] = minimum;
        component->maximumIndex[axis] = maximum;
    }
    if (version >= 3
        && !decoder->floating(
            &component->automaticSeedDistanceNormalized)) {
        return false;
    }
    std::uint8_t selected = 0;
    std::uint8_t containsAutomaticSeed = 0;
    std::uint32_t decision = 0;
    if (!decoder->floating(&component->elongation)
        || !decoder->floating(&component->score)
        || !decoder->byte(&selected) || selected > 1
        || (version >= 3
            && (!decoder->byte(&containsAutomaticSeed)
                || containsAutomaticSeed > 1))
        || !decoder->u32(&decision)
        || decision > static_cast<std::uint32_t>(
            AutomaticVesselComponentDecision::RetentionLimit)) {
        return false;
    }
    component->selected = selected != 0;
    component->containsAutomaticSeed = containsAutomaticSeed != 0;
    component->decision = static_cast<AutomaticVesselComponentDecision>(decision);
    return true;
}

bool checkedVoxelCount(const ImageGeometry& geometry, std::size_t* count)
{
    if (count == nullptr) {
        return false;
    }
    std::size_t value = 1;
    for (int axis = 0; axis < 3; ++axis) {
        if (geometry.dimensions[axis] <= 0) {
            return false;
        }
        const std::size_t dimension =
            static_cast<std::size_t>(geometry.dimensions[axis]);
        if (value > (std::numeric_limits<std::size_t>::max)() / dimension) {
            return false;
        }
        value *= dimension;
    }
    *count = value;
    return true;
}

bool decodeSize(Decoder* decoder, std::size_t* value)
{
    std::uint64_t stored = 0;
    if (decoder == nullptr || value == nullptr || !decoder->u64(&stored)
        || stored > (std::numeric_limits<std::size_t>::max)()) {
        return false;
    }
    *value = static_cast<std::size_t>(stored);
    return true;
}

void encodeProfileV2(Encoder* encoder,
                     const AutomaticVesselSegmentationProfileV2& profile)
{
    encoder->text(profile.profileId);
    encoder->u32(profile.schemaVersion);
    encoder->byte(profile.makeHighResolutionIsotropic ? 1u : 0u);
    encoder->floating(profile.vesselnessMaskMinimum);
    encoder->floating(profile.vesselnessMaskMaximum);
    encoder->floating(profile.inputBlurSigmaMm);
    encoder->floating(profile.inputWindowMinimum);
    encoder->floating(profile.inputWindowMaximum);
    encoder->floating(profile.inputWindowOutputMinimum);
    encoder->floating(profile.inputWindowOutputMaximum);
    encoder->floating(profile.seedBlurSigmaMm);
    encoder->floating(profile.seedMaskRangeMinimumMm);
    encoder->floating(profile.seedMaskRangeMaximumMm);
    encoder->floating(profile.seedExtractionMinimumProbability);
    encoder->floating(profile.minimumCurvature);
    encoder->floating(profile.minimumRoundness);
    encoder->floating(profile.minimumRidgeness);
    encoder->floating(profile.minimumLevelness);
    encoder->floating(profile.radiusInObjectSpaceMm);
    encoder->u32(profile.borderInIndexSpace);
    encoder->byte(profile.optimizeRadius ? 1u : 0u);
    encoder->byte(profile.useSeedMaskAsProbabilities ? 1u : 0u);
    encoder->byte(profile.rasterizeWithRadius ? 1u : 0u);
}

bool decodeProfileV2(Decoder* decoder,
                     AutomaticVesselSegmentationProfileV2* profile)
{
    std::uint8_t makeHighResolutionIsotropic = 0;
    std::uint8_t optimizeRadius = 0;
    std::uint8_t useSeedMaskAsProbabilities = 0;
    std::uint8_t rasterizeWithRadius = 0;
    if (decoder == nullptr || profile == nullptr
        || !decoder->text(&profile->profileId)
        || !decoder->u32(&profile->schemaVersion)
        || !decoder->byte(&makeHighResolutionIsotropic)
        || makeHighResolutionIsotropic > 1
        || !decoder->floating(&profile->vesselnessMaskMinimum)
        || !decoder->floating(&profile->vesselnessMaskMaximum)
        || !decoder->floating(&profile->inputBlurSigmaMm)
        || !decoder->floating(&profile->inputWindowMinimum)
        || !decoder->floating(&profile->inputWindowMaximum)
        || !decoder->floating(&profile->inputWindowOutputMinimum)
        || !decoder->floating(&profile->inputWindowOutputMaximum)
        || !decoder->floating(&profile->seedBlurSigmaMm)
        || !decoder->floating(&profile->seedMaskRangeMinimumMm)
        || !decoder->floating(&profile->seedMaskRangeMaximumMm)
        || !decoder->floating(&profile->seedExtractionMinimumProbability)
        || !decoder->floating(&profile->minimumCurvature)
        || !decoder->floating(&profile->minimumRoundness)
        || !decoder->floating(&profile->minimumRidgeness)
        || !decoder->floating(&profile->minimumLevelness)
        || !decoder->floating(&profile->radiusInObjectSpaceMm)
        || !decoder->u32(&profile->borderInIndexSpace)
        || !decoder->byte(&optimizeRadius) || optimizeRadius > 1
        || !decoder->byte(&useSeedMaskAsProbabilities)
        || useSeedMaskAsProbabilities > 1
        || !decoder->byte(&rasterizeWithRadius)
        || rasterizeWithRadius > 1) {
        return false;
    }
    profile->makeHighResolutionIsotropic =
        makeHighResolutionIsotropic != 0;
    profile->optimizeRadius = optimizeRadius != 0;
    profile->useSeedMaskAsProbabilities =
        useSeedMaskAsProbabilities != 0;
    profile->rasterizeWithRadius = rasterizeWithRadius != 0;
    return validateAutomaticVesselSegmentationProfileV2(*profile)
        == AutomaticVesselSegmentationProfileV2ValidationCode::Ok;
}

void encodeProfileV3(Encoder* encoder,
                     const AutomaticVesselSegmentationProfileV2& profile)
{
    encoder->text(profile.profileId);
    encoder->u32(profile.schemaVersion);
    encoder->floating(profile.coarseDomainDilationMm);
    encoder->floating(profile.edgeSigmaMm);
    encoder->floating(profile.propagationScaling);
    encoder->floating(profile.advectionScaling);
    encoder->floating(profile.curvatureScaling);
    encoder->floating(profile.maximumRmsError);
    encoder->u32(profile.maximumIterations);
    encoder->floating(profile.isosurfaceValue);
    encoder->byte(profile.useImageSpacing ? 1u : 0u);
}

bool decodeProfileV3(Decoder* decoder,
                     AutomaticVesselSegmentationProfileV2* profile)
{
    std::uint8_t useImageSpacing = 0;
    if (decoder == nullptr || profile == nullptr
        || !decoder->text(&profile->profileId)
        || !decoder->u32(&profile->schemaVersion)
        || !decoder->floating(&profile->coarseDomainDilationMm)
        || !decoder->floating(&profile->edgeSigmaMm)
        || !decoder->floating(&profile->propagationScaling)
        || !decoder->floating(&profile->advectionScaling)
        || !decoder->floating(&profile->curvatureScaling)
        || !decoder->floating(&profile->maximumRmsError)
        || !decoder->u32(&profile->maximumIterations)
        || !decoder->floating(&profile->isosurfaceValue)
        || !decoder->byte(&useImageSpacing) || useImageSpacing > 1) {
        return false;
    }
    profile->useImageSpacing = useImageSpacing != 0;
    return profile->schemaVersion == 3
        && validateAutomaticVesselSegmentationProfileV2(*profile)
            == AutomaticVesselSegmentationProfileV2ValidationCode::Ok;
}

void encodeMaskVoxels(Encoder* encoder, const XQSegmentationMask& mask)
{
    const std::vector<XQSegmentationMask::LabelType>& voxels = mask.voxels();
    encoder->u64(static_cast<std::uint64_t>(voxels.size()));
    encoder->raw(voxels.data(), voxels.size());
}

bool decodeMaskVoxels(Decoder* decoder,
                      const ImageGeometry& geometry,
                      const std::string& labelName,
                      std::shared_ptr<XQSegmentationMask>* mask)
{
    if (decoder == nullptr || mask == nullptr) {
        return false;
    }
    std::uint64_t storedVoxelCount = 0;
    std::size_t expectedVoxelCount = 0;
    if (!decoder->u64(&storedVoxelCount)
        || !checkedVoxelCount(geometry, &expectedVoxelCount)
        || storedVoxelCount != expectedVoxelCount
        || storedVoxelCount > decoder->remaining()) {
        return false;
    }
    std::vector<std::uint8_t> voxels(expectedVoxelCount, 0);
    if (!decoder->raw(voxels.data(), voxels.size())) {
        return false;
    }
    std::shared_ptr<XQSegmentationMask> decoded =
        std::make_shared<XQSegmentationMask>(geometry.dimensions);
    if (!decoded->is_valid()) {
        return false;
    }
    decoded->setGeometry(geometry);
    decoded->setLabels({SegmentationLabel{1, labelName}});
    for (std::size_t index = 0; index < voxels.size(); ++index) {
        if (voxels[index] > 1) {
            return false;
        }
        if (voxels[index] != 0) {
            decoded->setLabelAt(index, 1);
        }
    }
    *mask = std::move(decoded);
    return true;
}

void encodeRoiLayer(Encoder* encoder, const XQVascularRoiLayer& layer)
{
    encoder->u32(layer.role == VascularRoiRole::Organ ? 1u : 2u);
    encodeGeometry(encoder, layer.sourceGeometry);
    encoder->byte(layer.hasSourceGeometry ? 1u : 0u);
    encoder->byte(layer.resampledToReference ? 1u : 0u);
    encoder->u64(static_cast<std::uint64_t>(
        layer.sourceForegroundVoxelCount));
    encoder->u64(static_cast<std::uint64_t>(
        layer.alignedForegroundVoxelCount));
    encoder->text(layer.sourceFingerprint);
    encoder->text(layer.alignedFingerprint);
    encoder->text(layer.generatorId);
    encoder->text(layer.generatorVersion);
    encodeMaskVoxels(encoder, *layer.mask);
}

bool decodeRoiLayer(Decoder* decoder,
                    const ImageGeometry& alignedGeometry,
                    XQVascularRoiLayer* layer)
{
    if (decoder == nullptr || layer == nullptr) {
        return false;
    }
    std::uint32_t role = 0;
    std::uint8_t hasSourceGeometry = 0;
    std::uint8_t resampled = 0;
    if (!decoder->u32(&role) || (role != 1 && role != 2)
        || !decodeGeometry(decoder, &layer->sourceGeometry)
        || !decoder->byte(&hasSourceGeometry) || hasSourceGeometry > 1
        || !decoder->byte(&resampled) || resampled > 1
        || !decodeSize(decoder, &layer->sourceForegroundVoxelCount)
        || !decodeSize(decoder, &layer->alignedForegroundVoxelCount)
        || !decoder->text(&layer->sourceFingerprint)
        || !decoder->text(&layer->alignedFingerprint)
        || !decoder->text(&layer->generatorId)
        || !decoder->text(&layer->generatorVersion)) {
        return false;
    }
    layer->role = role == 1
        ? VascularRoiRole::Organ
        : VascularRoiRole::CoarseVessel;
    layer->hasSourceGeometry = hasSourceGeometry != 0;
    layer->resampledToReference = resampled != 0;
    if (!decodeMaskVoxels(
            decoder, alignedGeometry,
            std::string("roi_") + vascularRoiRoleToken(layer->role),
            &layer->mask)) {
        return false;
    }
    return layer->isValid();
}

void encodeRoiPrior(Encoder* encoder, const XQVascularRoiPriorV1& prior)
{
    encoder->text(prior.ctInputFingerprint);
    encoder->text(prior.priorFingerprint);
    encoder->u32(static_cast<std::uint32_t>(prior.layers.size()));
    for (const XQVascularRoiLayer& layer : prior.layers) {
        encodeRoiLayer(encoder, layer);
    }
}

bool decodeRoiPrior(Decoder* decoder,
                    const ImageGeometry& alignedGeometry,
                    XQVascularRoiPriorV1* prior)
{
    std::uint32_t layerCount = 0;
    if (decoder == nullptr || prior == nullptr
        || !decoder->text(&prior->ctInputFingerprint)
        || !decoder->text(&prior->priorFingerprint)
        || !decoder->u32(&layerCount) || layerCount != 2) {
        return false;
    }
    prior->layers.resize(layerCount);
    for (XQVascularRoiLayer& layer : prior->layers) {
        if (!decodeRoiLayer(decoder, alignedGeometry, &layer)) {
            return false;
        }
    }
    return prior->isValid();
}

void encodeUpstreamStage(Encoder* encoder,
                         const AutomaticVesselUpstreamStage& stage)
{
    encoder->text(stage.stageId);
    encoder->text(stage.implementation);
    encoder->text(stage.implementationVersion);
    encoder->u32(static_cast<std::uint32_t>(stage.parameters.size()));
    for (const AutomaticVesselFilterParameter& parameter : stage.parameters) {
        encoder->text(parameter.name);
        encoder->floating(parameter.value);
        encoder->text(parameter.unit);
    }
}

bool decodeUpstreamStage(Decoder* decoder,
                         AutomaticVesselUpstreamStage* stage)
{
    std::uint32_t parameterCount = 0;
    if (decoder == nullptr || stage == nullptr
        || !decoder->text(&stage->stageId)
        || !decoder->text(&stage->implementation)
        || !decoder->text(&stage->implementationVersion)
        || !decoder->u32(&parameterCount)
        || parameterCount > kMaximumStageParameters) {
        return false;
    }
    stage->parameters.resize(parameterCount);
    for (AutomaticVesselFilterParameter& parameter : stage->parameters) {
        if (!decoder->text(&parameter.name)
            || !decoder->floating(&parameter.value)
            || !decoder->text(&parameter.unit)
            || !parameter.isValid()) {
            return false;
        }
    }
    return stage->isValid();
}

std::vector<std::uint8_t> encodeV2(
    const XQAutomaticVesselSegmentationV2& segmentation,
    bool* encoded)
{
    Encoder encoder;
    encoder.raw(kMagic, sizeof(kMagic));
    encoder.u32(kFormatVersion);
    encodeGeometry(&encoder, segmentation.mask->geometry());
    encodeProfileV2(&encoder, segmentation.profile);
    encoder.text(segmentation.inputFingerprint);
    encoder.text(segmentation.vesselnessFingerprint);
    encoder.text(segmentation.roiPriorFingerprint);
    encoder.text(segmentation.profileFingerprint);
    encoder.text(segmentation.outputFingerprint);
    encoder.text(segmentation.algorithmId);
    encoder.text(segmentation.algorithmVersion);
    encoder.text(segmentation.itkVersion);
    encoder.text(segmentation.tubeTkVersion);
    encoder.byte(segmentation.hasDicomIdentity ? 1u : 0u);
    encoder.text(segmentation.dicomIdentity.studyInstanceUid);
    encoder.text(segmentation.dicomIdentity.seriesInstanceUid);
    encoder.text(segmentation.dicomIdentity.frameOfReferenceUid);
    encoder.floating(segmentation.workingSpacingMm);
    for (int axis = 0; axis < 3; ++axis) {
        encoder.u32(static_cast<std::uint32_t>(
            segmentation.workingDimensions[axis]));
    }
    encoder.u64(static_cast<std::uint64_t>(segmentation.workingVoxelCount));
    encoder.u64(static_cast<std::uint64_t>(segmentation.domainVoxelCount));
    encoder.u64(static_cast<std::uint64_t>(segmentation.seedVoxelCount));
    encoder.u64(static_cast<std::uint64_t>(segmentation.extractedTubeCount));
    encoder.u64(static_cast<std::uint64_t>(
        segmentation.extractedTubePointCount));
    encoder.u64(static_cast<std::uint64_t>(
        segmentation.workingRasterizedVoxelCount));
    encoder.u64(static_cast<std::uint64_t>(segmentation.foregroundVoxelCount));
    encoder.u64(static_cast<std::uint64_t>(segmentation.componentCount));
    encoder.floating(segmentation.elapsedMilliseconds);
    encodeRoiPrior(&encoder, segmentation.roiPrior);
    if (segmentation.upstreamStages.size() > kMaximumUpstreamStages
        || segmentation.componentVoxelCounts.size()
            > kMaximumComponentRecords) {
        *encoded = false;
        return {};
    }
    encoder.u32(static_cast<std::uint32_t>(
        segmentation.upstreamStages.size()));
    for (const AutomaticVesselUpstreamStage& stage
         : segmentation.upstreamStages) {
        if (stage.parameters.size() > kMaximumStageParameters) {
            *encoded = false;
            return {};
        }
        encodeUpstreamStage(&encoder, stage);
    }
    encoder.u32(static_cast<std::uint32_t>(
        segmentation.componentVoxelCounts.size()));
    for (std::size_t count : segmentation.componentVoxelCounts) {
        encoder.u64(static_cast<std::uint64_t>(count));
    }
    encodeMaskVoxels(&encoder, *segmentation.mask);
    *encoded = encoder.valid();
    return *encoded ? encoder.bytes() : std::vector<std::uint8_t>();
}

bool decodeV2(const std::vector<std::uint8_t>& bytes,
              XQAutomaticVesselSegmentationV2* segmentation)
{
    if (segmentation == nullptr) {
        return false;
    }
    Decoder decoder(bytes);
    std::uint8_t magic[sizeof(kMagic)] = {};
    std::uint32_t version = 0;
    ImageGeometry geometry{};
    std::uint8_t hasDicomIdentity = 0;
    std::uint32_t workingDimensions[3] = {0, 0, 0};
    if (!decoder.raw(magic, sizeof(magic))
        || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0
        || !decoder.u32(&version) || version != 5
        || !decodeGeometry(&decoder, &geometry)
        || !decodeProfileV2(&decoder, &segmentation->profile)
        || !decoder.text(&segmentation->inputFingerprint)
        || !decoder.text(&segmentation->vesselnessFingerprint)
        || !decoder.text(&segmentation->roiPriorFingerprint)
        || !decoder.text(&segmentation->profileFingerprint)
        || !decoder.text(&segmentation->outputFingerprint)
        || !decoder.text(&segmentation->algorithmId)
        || !decoder.text(&segmentation->algorithmVersion)
        || !decoder.text(&segmentation->itkVersion)
        || !decoder.text(&segmentation->tubeTkVersion)
        || !decoder.byte(&hasDicomIdentity) || hasDicomIdentity > 1
        || !decoder.text(&segmentation->dicomIdentity.studyInstanceUid)
        || !decoder.text(&segmentation->dicomIdentity.seriesInstanceUid)
        || !decoder.text(&segmentation->dicomIdentity.frameOfReferenceUid)
        || !decoder.floating(&segmentation->workingSpacingMm)
        || !decoder.u32(&workingDimensions[0])
        || !decoder.u32(&workingDimensions[1])
        || !decoder.u32(&workingDimensions[2])
        || !decodeSize(&decoder, &segmentation->workingVoxelCount)
        || !decodeSize(&decoder, &segmentation->domainVoxelCount)
        || !decodeSize(&decoder, &segmentation->seedVoxelCount)
        || !decodeSize(&decoder, &segmentation->extractedTubeCount)
        || !decodeSize(&decoder, &segmentation->extractedTubePointCount)
        || !decodeSize(
            &decoder, &segmentation->workingRasterizedVoxelCount)
        || !decodeSize(&decoder, &segmentation->foregroundVoxelCount)
        || !decodeSize(&decoder, &segmentation->componentCount)) {
        return false;
    }
    segmentation->hasDicomIdentity = hasDicomIdentity != 0;
    for (int axis = 0; axis < 3; ++axis) {
        if (workingDimensions[axis] == 0
            || workingDimensions[axis]
                > static_cast<std::uint32_t>(
                    (std::numeric_limits<int>::max)())) {
            return false;
        }
        segmentation->workingDimensions[axis] =
            static_cast<int>(workingDimensions[axis]);
    }
    if (!decoder.floating(&segmentation->elapsedMilliseconds)
        || !decodeRoiPrior(&decoder, geometry, &segmentation->roiPrior)) {
        return false;
    }
    std::uint32_t stageCount = 0;
    if (!decoder.u32(&stageCount) || stageCount > kMaximumUpstreamStages) {
        return false;
    }
    segmentation->upstreamStages.resize(stageCount);
    for (AutomaticVesselUpstreamStage& stage : segmentation->upstreamStages) {
        if (!decodeUpstreamStage(&decoder, &stage)) {
            return false;
        }
    }
    std::uint32_t componentCount = 0;
    if (!decoder.u32(&componentCount)
        || componentCount > kMaximumComponentRecords) {
        return false;
    }
    segmentation->componentVoxelCounts.resize(componentCount);
    for (std::size_t& count : segmentation->componentVoxelCounts) {
        if (!decodeSize(&decoder, &count)) {
            return false;
        }
    }
    if (!decodeMaskVoxels(&decoder, geometry, "vessel", &segmentation->mask)
        || decoder.remaining() != 0 || !decoder.valid()) {
        return false;
    }
    return segmentation->isValid();
}

std::vector<std::uint8_t> encodeV3(
    const XQAutomaticVesselSegmentationV2& segmentation,
    bool* encoded)
{
    Encoder encoder;
    encoder.raw(kMagic, sizeof(kMagic));
    encoder.u32(kFormatVersion);
    encodeGeometry(&encoder, segmentation.mask->geometry());
    encodeProfileV3(&encoder, segmentation.profile);
    encoder.text(segmentation.inputFingerprint);
    encoder.text(segmentation.vesselnessFingerprint);
    encoder.text(segmentation.roiPriorFingerprint);
    encoder.text(segmentation.profileFingerprint);
    encoder.text(segmentation.outputFingerprint);
    encoder.text(segmentation.algorithmId);
    encoder.text(segmentation.algorithmVersion);
    encoder.text(segmentation.itkVersion);
    encoder.byte(segmentation.hasDicomIdentity ? 1u : 0u);
    encoder.text(segmentation.dicomIdentity.studyInstanceUid);
    encoder.text(segmentation.dicomIdentity.seriesInstanceUid);
    encoder.text(segmentation.dicomIdentity.frameOfReferenceUid);
    encoder.u64(static_cast<std::uint64_t>(segmentation.domainVoxelCount));
    encoder.u64(
        static_cast<std::uint64_t>(segmentation.initialSurfaceVoxelCount));
    encoder.u64(static_cast<std::uint64_t>(
        segmentation.edgePotentialPositiveVoxelCount));
    encoder.floating(segmentation.edgePotentialMinimum);
    encoder.floating(segmentation.edgePotentialMaximum);
    encoder.u32(segmentation.levelSetElapsedIterations);
    encoder.floating(segmentation.levelSetRmsChange);
    encoder.byte(segmentation.levelSetConverged ? 1u : 0u);
    for (int axis = 0; axis < 3; ++axis) {
        encoder.u32(segmentation.coarseDomainDilationRadiusVoxels[axis]);
    }
    encoder.u64(static_cast<std::uint64_t>(segmentation.foregroundVoxelCount));
    encoder.u64(static_cast<std::uint64_t>(segmentation.componentCount));
    encoder.floating(segmentation.elapsedMilliseconds);
    encodeRoiPrior(&encoder, segmentation.roiPrior);
    if (segmentation.upstreamStages.size() > kMaximumUpstreamStages
        || segmentation.componentVoxelCounts.size()
            > kMaximumComponentRecords) {
        *encoded = false;
        return {};
    }
    encoder.u32(static_cast<std::uint32_t>(
        segmentation.upstreamStages.size()));
    for (const AutomaticVesselUpstreamStage& stage
         : segmentation.upstreamStages) {
        if (stage.parameters.size() > kMaximumStageParameters) {
            *encoded = false;
            return {};
        }
        encodeUpstreamStage(&encoder, stage);
    }
    encoder.u32(static_cast<std::uint32_t>(
        segmentation.componentVoxelCounts.size()));
    for (std::size_t count : segmentation.componentVoxelCounts) {
        encoder.u64(static_cast<std::uint64_t>(count));
    }
    encodeMaskVoxels(&encoder, *segmentation.mask);
    *encoded = encoder.valid();
    return *encoded ? encoder.bytes() : std::vector<std::uint8_t>();
}

bool decodeV3(const std::vector<std::uint8_t>& bytes,
              XQAutomaticVesselSegmentationV2* segmentation)
{
    if (segmentation == nullptr) {
        return false;
    }
    Decoder decoder(bytes);
    std::uint8_t magic[sizeof(kMagic)] = {};
    std::uint32_t version = 0;
    ImageGeometry geometry{};
    std::uint8_t hasDicomIdentity = 0;
    std::uint8_t levelSetConverged = 0;
    std::uint32_t dilationRadius[3] = {0, 0, 0};
    if (!decoder.raw(magic, sizeof(magic))
        || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0
        || !decoder.u32(&version) || version != 6
        || !decodeGeometry(&decoder, &geometry)
        || !decodeProfileV3(&decoder, &segmentation->profile)
        || !decoder.text(&segmentation->inputFingerprint)
        || !decoder.text(&segmentation->vesselnessFingerprint)
        || !decoder.text(&segmentation->roiPriorFingerprint)
        || !decoder.text(&segmentation->profileFingerprint)
        || !decoder.text(&segmentation->outputFingerprint)
        || !decoder.text(&segmentation->algorithmId)
        || !decoder.text(&segmentation->algorithmVersion)
        || !decoder.text(&segmentation->itkVersion)
        || !decoder.byte(&hasDicomIdentity) || hasDicomIdentity > 1
        || !decoder.text(&segmentation->dicomIdentity.studyInstanceUid)
        || !decoder.text(&segmentation->dicomIdentity.seriesInstanceUid)
        || !decoder.text(&segmentation->dicomIdentity.frameOfReferenceUid)
        || !decodeSize(&decoder, &segmentation->domainVoxelCount)
        || !decodeSize(&decoder, &segmentation->initialSurfaceVoxelCount)
        || !decodeSize(
            &decoder, &segmentation->edgePotentialPositiveVoxelCount)
        || !decoder.floating(&segmentation->edgePotentialMinimum)
        || !decoder.floating(&segmentation->edgePotentialMaximum)
        || !decoder.u32(&segmentation->levelSetElapsedIterations)
        || !decoder.floating(&segmentation->levelSetRmsChange)
        || !decoder.byte(&levelSetConverged) || levelSetConverged > 1
        || !decoder.u32(&dilationRadius[0])
        || !decoder.u32(&dilationRadius[1])
        || !decoder.u32(&dilationRadius[2])
        || !decodeSize(&decoder, &segmentation->foregroundVoxelCount)
        || !decodeSize(&decoder, &segmentation->componentCount)
        || !decoder.floating(&segmentation->elapsedMilliseconds)
        || !decodeRoiPrior(&decoder, geometry, &segmentation->roiPrior)) {
        return false;
    }
    segmentation->hasDicomIdentity = hasDicomIdentity != 0;
    segmentation->levelSetConverged = levelSetConverged != 0;
    for (int axis = 0; axis < 3; ++axis) {
        segmentation->coarseDomainDilationRadiusVoxels[axis] =
            dilationRadius[axis];
    }
    std::uint32_t stageCount = 0;
    if (!decoder.u32(&stageCount) || stageCount > kMaximumUpstreamStages) {
        return false;
    }
    segmentation->upstreamStages.resize(stageCount);
    for (AutomaticVesselUpstreamStage& stage : segmentation->upstreamStages) {
        if (!decodeUpstreamStage(&decoder, &stage)) {
            return false;
        }
    }
    std::uint32_t componentCount = 0;
    if (!decoder.u32(&componentCount)
        || componentCount > kMaximumComponentRecords) {
        return false;
    }
    segmentation->componentVoxelCounts.resize(componentCount);
    for (std::size_t& count : segmentation->componentVoxelCounts) {
        if (!decodeSize(&decoder, &count)) {
            return false;
        }
    }
    if (!decodeMaskVoxels(&decoder, geometry, "vessel", &segmentation->mask)
        || decoder.remaining() != 0 || !decoder.valid()) {
        return false;
    }
    return segmentation->isValid();
}

bool decodeLegacy(const std::vector<std::uint8_t>& bytes,
                  XQAutomaticVesselSegmentation* segmentation,
                  std::uint32_t* decodedVersion)
{
    if (segmentation == nullptr || decodedVersion == nullptr) {
        return false;
    }
    Decoder decoder(bytes);
    std::uint8_t magic[sizeof(kMagic)] = {};
    std::uint32_t version = 0;
    ImageGeometry geometry{};
    if (!decoder.raw(magic, sizeof(magic))
        || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0
        || !decoder.u32(&version) || version < 1 || version > 4
        || !decodeGeometry(&decoder, &geometry)
        || !decodeProfile(&decoder, version, &segmentation->profile)
        || !decoder.text(&segmentation->inputFingerprint)
        || !decoder.text(&segmentation->vesselnessFingerprint)
        || !decoder.text(&segmentation->profileFingerprint)
        || !decoder.text(&segmentation->outputFingerprint)
        || !decoder.text(&segmentation->algorithmId)
        || !decoder.text(&segmentation->algorithmVersion)
        || !decoder.text(&segmentation->itkVersion)) {
        return false;
    }
    std::uint8_t hasDicomIdentity = 0;
    if (version >= 2
        && (!decoder.byte(&hasDicomIdentity) || hasDicomIdentity > 1
            || !decoder.text(&segmentation->dicomIdentity.studyInstanceUid)
            || !decoder.text(&segmentation->dicomIdentity.seriesInstanceUid)
            || !decoder.text(&segmentation->dicomIdentity.frameOfReferenceUid))) {
        return false;
    }
    if (!decoder.floating(&segmentation->candidateVesselnessThreshold)
        || !decoder.floating(&segmentation->coreVesselnessThreshold)
        || !decoder.floating(&segmentation->automaticIntensityLower)
        || !decoder.floating(&segmentation->automaticIntensityUpper)) {
        return false;
    }
    segmentation->hasDicomIdentity = hasDicomIdentity != 0;
    if (version >= 3) {
        for (int axis = 0; axis < 3; ++axis) {
            std::int32_t seedIndex = 0;
            if (!decoder.i32(&seedIndex)) {
                return false;
            }
            segmentation->automaticSeedIndex[axis] = seedIndex;
        }
        if (!decoder.floating(&segmentation->automaticSeedIntensity)
            || !decoder.floating(&segmentation->automaticSeedVesselness)
            || !decoder.u32(
                &segmentation->automaticSeedComponentLabel)) {
            return false;
        }
    } else {
        // Legacy drafts predate automatic seed lineage. They remain readable
        // for diagnostics but are not valid frozen production artifacts.
        segmentation->automaticSeedComponentLabel = 1;
        segmentation->automaticSeedVesselness = 1.0;
    }
    std::uint64_t confidenceCount = 0;
    if (version >= 4
        && (!decoder.floating(&segmentation->confidenceMean)
            || !decoder.floating(&segmentation->confidenceVariance)
            || !decoder.u64(&confidenceCount)
            || confidenceCount
                > (std::numeric_limits<std::size_t>::max)())) {
        return false;
    }
    segmentation->confidenceVoxelCount =
        static_cast<std::size_t>(confidenceCount);
    std::uint64_t coreCount = 0;
    std::uint64_t rawCandidateCount = 0;
    std::uint64_t candidateCount = 0;
    std::uint64_t componentCount = 0;
    std::uint64_t retainedCount = 0;
    std::uint64_t foregroundCount = 0;
    if (!decoder.u64(&coreCount) || !decoder.u64(&rawCandidateCount)
        || !decoder.u64(&candidateCount) || !decoder.u64(&componentCount)
        || !decoder.u64(&retainedCount) || !decoder.u64(&foregroundCount)
        || coreCount > (std::numeric_limits<std::size_t>::max)()
        || rawCandidateCount > (std::numeric_limits<std::size_t>::max)()
        || candidateCount > (std::numeric_limits<std::size_t>::max)()
        || componentCount > (std::numeric_limits<std::size_t>::max)()
        || retainedCount > (std::numeric_limits<std::size_t>::max)()
        || foregroundCount > (std::numeric_limits<std::size_t>::max)()) {
        return false;
    }
    segmentation->coreVoxelCount = static_cast<std::size_t>(coreCount);
    segmentation->rawCandidateVoxelCount =
        static_cast<std::size_t>(rawCandidateCount);
    segmentation->candidateVoxelCount = static_cast<std::size_t>(candidateCount);
    segmentation->componentCount = static_cast<std::size_t>(componentCount);
    segmentation->retainedComponentCount = static_cast<std::size_t>(retainedCount);
    segmentation->foregroundVoxelCount = static_cast<std::size_t>(foregroundCount);
    for (int axis = 0; axis < 3; ++axis) {
        if (!decoder.u32(&segmentation->closingRadiusVoxels[axis])
            || !decoder.u32(&segmentation->openingRadiusVoxels[axis])) {
            return false;
        }
        if (version >= 4
            && !decoder.u32(
                &segmentation->confidenceExpansionRadiusVoxels[axis])) {
            return false;
        }
    }
    std::uint8_t thresholdExecuted = 0;
    std::uint8_t confidenceConnectedExecuted = 0;
    std::uint8_t confidenceExpansionExecuted = 0;
    std::uint8_t morphologyExecuted = 0;
    std::uint8_t connectedExecuted = 0;
    std::uint8_t fullyConnected = 0;
    if (!decoder.byte(&thresholdExecuted) || thresholdExecuted > 1
        || (version >= 4
            && (!decoder.byte(&confidenceConnectedExecuted)
                || confidenceConnectedExecuted > 1
                || !decoder.byte(&confidenceExpansionExecuted)
                || confidenceExpansionExecuted > 1))
        || !decoder.byte(&morphologyExecuted) || morphologyExecuted > 1
        || !decoder.byte(&connectedExecuted) || connectedExecuted > 1
        || !decoder.byte(&fullyConnected) || fullyConnected > 1
        || !decoder.floating(&segmentation->elapsedMilliseconds)) {
        return false;
    }
    segmentation->itkThresholdExecuted = thresholdExecuted != 0;
    segmentation->itkConfidenceConnectedExecuted =
        confidenceConnectedExecuted != 0;
    segmentation->itkConfidenceExpansionExecuted =
        confidenceExpansionExecuted != 0;
    segmentation->itkMorphologyExecuted = morphologyExecuted != 0;
    segmentation->itkConnectedComponentsExecuted = connectedExecuted != 0;
    segmentation->fullyConnected = fullyConnected != 0;

    std::uint32_t recordCount = 0;
    if (!decoder.u32(&recordCount) || recordCount > kMaximumComponentRecords) {
        return false;
    }
    segmentation->components.resize(recordCount);
    for (AutomaticVesselComponentRecord& component : segmentation->components) {
        if (!decodeComponent(&decoder, version, &component)) {
            return false;
        }
    }

    std::uint64_t storedVoxelCount = 0;
    std::size_t expectedVoxelCount = 0;
    if (!decoder.u64(&storedVoxelCount)
        || !checkedVoxelCount(geometry, &expectedVoxelCount)
        || storedVoxelCount != expectedVoxelCount
        || storedVoxelCount > decoder.remaining()) {
        return false;
    }
    std::vector<std::uint8_t> voxels(expectedVoxelCount, 0);
    if (!decoder.raw(voxels.data(), voxels.size()) || decoder.remaining() != 0
        || !decoder.valid()) {
        return false;
    }
    segmentation->mask =
        std::make_shared<XQSegmentationMask>(geometry.dimensions);
    if (!segmentation->mask->is_valid()) {
        return false;
    }
    segmentation->mask->setGeometry(geometry);
    segmentation->mask->setLabels({SegmentationLabel{1, "vessel"}});
    for (std::size_t index = 0; index < voxels.size(); ++index) {
        if (voxels[index] > 1) {
            return false;
        }
        if (voxels[index] != 0) {
            segmentation->mask->setLabelAt(index, 1);
        }
    }
    *decodedVersion = version;
    return true;
}

} // namespace

const char* automaticVesselSegmentationArtifactStatusToken(
    AutomaticVesselSegmentationArtifactStatus status)
{
    switch (status) {
    case AutomaticVesselSegmentationArtifactStatus::Ok: return "ok";
    case AutomaticVesselSegmentationArtifactStatus::InvalidArgument:
        return "invalid_argument";
    case AutomaticVesselSegmentationArtifactStatus::InvalidSegmentation:
        return "invalid_segmentation";
    case AutomaticVesselSegmentationArtifactStatus::TargetExists:
        return "target_exists";
    case AutomaticVesselSegmentationArtifactStatus::IoError: return "io_error";
    case AutomaticVesselSegmentationArtifactStatus::InvalidFormat:
        return "invalid_format";
    case AutomaticVesselSegmentationArtifactStatus::UnsupportedVersion:
        return "unsupported_version";
    case AutomaticVesselSegmentationArtifactStatus::IntegrityError:
        return "integrity_error";
    }
    return "unknown";
}

bool AutomaticVesselSegmentationArtifactWriteResult::ok() const
{
    return status == AutomaticVesselSegmentationArtifactStatus::Ok
        && artifactSha256.size() == 64;
}

bool AutomaticVesselSegmentationArtifactReadResult::ok() const
{
    if (status != AutomaticVesselSegmentationArtifactStatus::Ok
        || artifactSha256.size() != 64) {
        return false;
    }
    if (formatVersion == 5 || formatVersion == 6) {
        return segmentation.has_value() && segmentation->isValid()
            && !legacySegmentation.has_value();
    }
    return formatVersion >= 1 && formatVersion <= 4
        && legacySegmentation.has_value() && !segmentation.has_value();
}

bool AutomaticVesselSegmentationArtifactReadResult::isLegacy() const
{
    return formatVersion >= 1 && formatVersion <= 4
        && legacySegmentation.has_value();
}

AutomaticVesselSegmentationArtifactWriteResult
writeAutomaticVesselSegmentationArtifact(
    const std::string& path,
    const XQAutomaticVesselSegmentationV2& segmentation)
{
    AutomaticVesselSegmentationArtifactWriteResult result;
    if (path.empty()) {
        result.status = AutomaticVesselSegmentationArtifactStatus::InvalidArgument;
        return result;
    }
    if (!segmentation.isValid() || segmentation.profile.schemaVersion != 3) {
        result.status = AutomaticVesselSegmentationArtifactStatus::InvalidSegmentation;
        return result;
    }
    const std::filesystem::path target(path);
    const std::filesystem::path part(path + ".part");
    std::error_code error;
    if (std::filesystem::exists(target, error)
        || std::filesystem::exists(part, error) || error) {
        result.status = error
            ? AutomaticVesselSegmentationArtifactStatus::IoError
            : AutomaticVesselSegmentationArtifactStatus::TargetExists;
        return result;
    }
    bool encoded = false;
    const std::vector<std::uint8_t> bytes = encodeV3(segmentation, &encoded);
    if (!encoded || bytes.empty()
        || bytes.size()
            > static_cast<std::size_t>(
                (std::numeric_limits<std::streamsize>::max)())) {
        result.status =
            AutomaticVesselSegmentationArtifactStatus::InvalidSegmentation;
        return result;
    }
    std::ofstream output(part, std::ios::binary | std::ios::out);
    if (!output) {
        result.status = AutomaticVesselSegmentationArtifactStatus::IoError;
        return result;
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    output.flush();
    const bool written = output.good();
    output.close();
    if (!written || !output.good()) {
        std::filesystem::remove(part, error);
        result.status = AutomaticVesselSegmentationArtifactStatus::IoError;
        return result;
    }
    std::filesystem::rename(part, target, error);
    if (error) {
        std::error_code removeError;
        std::filesystem::remove(part, removeError);
        result.status = AutomaticVesselSegmentationArtifactStatus::IoError;
        return result;
    }
    result.status = AutomaticVesselSegmentationArtifactStatus::Ok;
    result.artifactSha256 = Sha256::hashHex(bytes.data(), bytes.size());
    return result;
}

AutomaticVesselSegmentationArtifactReadResult
readAutomaticVesselSegmentationArtifact(const std::string& path)
{
    AutomaticVesselSegmentationArtifactReadResult result;
    if (path.empty()) {
        result.status = AutomaticVesselSegmentationArtifactStatus::InvalidArgument;
        return result;
    }
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        result.status = AutomaticVesselSegmentationArtifactStatus::IoError;
        return result;
    }
    const std::streamoff length = input.tellg();
    if (length <= 0
        || static_cast<std::uint64_t>(length)
            > (std::numeric_limits<std::size_t>::max)()) {
        result.status = AutomaticVesselSegmentationArtifactStatus::InvalidFormat;
        return result;
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.read(reinterpret_cast<char*>(bytes.data()), length);
    if (!input.good()) {
        result.status = AutomaticVesselSegmentationArtifactStatus::IoError;
        return result;
    }
    Decoder header(bytes);
    std::uint8_t magic[sizeof(kMagic)] = {};
    std::uint32_t version = 0;
    if (!header.raw(magic, sizeof(magic))
        || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0
        || !header.u32(&version) || version == 0) {
        result.status = AutomaticVesselSegmentationArtifactStatus::InvalidFormat;
        return result;
    }
    if (version > kFormatVersion) {
        result.status =
            AutomaticVesselSegmentationArtifactStatus::UnsupportedVersion;
        return result;
    }
    result.formatVersion = version;
    if (version == 5 || version == 6) {
        XQAutomaticVesselSegmentationV2 segmentation;
        const bool decoded = version == 5
            ? decodeV2(bytes, &segmentation)
            : decodeV3(bytes, &segmentation);
        if (!decoded) {
            result.status =
                AutomaticVesselSegmentationArtifactStatus::InvalidFormat;
            return result;
        }
        result.segmentation.emplace(std::move(segmentation));
    } else {
        XQAutomaticVesselSegmentation legacy;
        std::uint32_t decodedVersion = 0;
        if (!decodeLegacy(bytes, &legacy, &decodedVersion)
            || decodedVersion != version) {
            result.status =
                AutomaticVesselSegmentationArtifactStatus::InvalidFormat;
            return result;
        }
        result.legacyProductionValid = legacy.isValid();
        result.legacySegmentation.emplace(std::move(legacy));
    }
    result.status = AutomaticVesselSegmentationArtifactStatus::Ok;
    result.artifactSha256 = Sha256::hashHex(bytes.data(), bytes.size());
    return result;
}

} // namespace xq
