#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"

#include "core/XQMemoryImageBufferHandle.h"

#include <gdcmDataSet.h>
#include <gdcmFile.h>
#include <gdcmImage.h>
#include <gdcmImageReader.h>
#include <gdcmPixelFormat.h>
#include <gdcmStringFilter.h>
#include <gdcmTag.h>

#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImage.h>
#include <itkImageSeriesReader.h>

#include <picosha2.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace xq {
namespace {

constexpr double kOrientationTolerance = 1e-5;
constexpr double kUnitVectorTolerance = 1e-3;
constexpr double kPositionToleranceMm = 1e-4;
constexpr double kTiltToleranceMm = 1e-3;
constexpr double kSpacingRelativeTolerance = 1e-3;

struct SeriesFiles {
    std::string seriesInstanceUid;
    std::vector<std::string> fileNames;
};

struct SliceRecord {
    std::string fileName;
    DicomSeriesIdentity identity;
    std::string sopInstanceUid;
    ImageModality modality = ImageModality::Unknown;
    std::string photometricInterpretation;
    gdcm::PixelFormat pixelFormat;
    int columns = 0;
    int rows = 0;
    int frameCount = 1;
    bool multiFrame = false;

    bool hasPosition = false;
    double position[3] = {0.0, 0.0, 0.0};
    bool hasOrientation = false;
    double axisX[3] = {0.0, 0.0, 0.0};
    double axisY[3] = {0.0, 0.0, 0.0};
    bool hasPixelSpacing = false;
    double spacingX = 0.0;
    double spacingY = 0.0;
    bool hasSliceThickness = false;
    double sliceThickness = 0.0;
    bool hasSpacingBetweenSlices = false;
    double spacingBetweenSlices = 0.0;

    bool hasRescaleSlopeTag = false;
    bool hasRescaleInterceptTag = false;
    bool rescaleValuesValid = true;
    double rescaleSlope = 1.0;
    double rescaleIntercept = 0.0;

    bool hasWindowCenter = false;
    bool hasWindowWidth = false;
    double windowCenter = 0.0;
    double windowWidth = 0.0;
    double projection = 0.0;
};

void addDiagnostic(std::vector<Diagnostic>* diagnostics,
                   DiagnosticSeverity severity,
                   const std::string& code,
                   const std::string& message)
{
    if (diagnostics != nullptr) {
        diagnostics->emplace_back(severity, code, message);
    }
}

void addStatusError(std::vector<Diagnostic>* diagnostics,
                    DicomSeriesStatus status,
                    const std::string& message)
{
    addDiagnostic(diagnostics,
                  DiagnosticSeverity::Error,
                  std::string("dicom.") + dicomSeriesStatusToken(status),
                  message);
}

std::string trimDicomText(const std::string& value)
{
    std::size_t first = 0;
    while (first < value.size()) {
        const unsigned char ch = static_cast<unsigned char>(value[first]);
        if (ch != 0 && std::isspace(ch) == 0) {
            break;
        }
        ++first;
    }

    std::size_t last = value.size();
    while (last > first) {
        const unsigned char ch = static_cast<unsigned char>(value[last - 1]);
        if (ch != 0 && std::isspace(ch) == 0) {
            break;
        }
        --last;
    }
    return value.substr(first, last - first);
}

bool parseDouble(const std::string& text, double* value)
{
    if (value == nullptr) {
        return false;
    }
    const std::string trimmed = trimDicomText(text);
    if (trimmed.empty()) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(trimmed.c_str(), &end);
    if (errno == ERANGE || end == trimmed.c_str()) {
        return false;
    }
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end)) != 0) {
        ++end;
    }
    if (*end != '\0' || !std::isfinite(parsed)) {
        return false;
    }
    *value = parsed;
    return true;
}

bool parsePositiveInt(const std::string& text, int* value)
{
    if (value == nullptr) {
        return false;
    }
    const std::string trimmed = trimDicomText(text);
    if (trimmed.empty()) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(trimmed.c_str(), &end, 10);
    if (errno == ERANGE || end == trimmed.c_str()) {
        return false;
    }
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end)) != 0) {
        ++end;
    }
    if (*end != '\0' || parsed <= 0 || parsed > std::numeric_limits<int>::max()) {
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

bool parseDoubles(const std::string& text,
                  std::size_t expectedCount,
                  std::vector<double>* values)
{
    if (values == nullptr) {
        return false;
    }
    values->clear();
    std::size_t cursor = 0;
    while (cursor <= text.size()) {
        const std::size_t separator = text.find('\\', cursor);
        const std::size_t end = separator == std::string::npos
            ? text.size()
            : separator;
        double value = 0.0;
        if (!parseDouble(text.substr(cursor, end - cursor), &value)) {
            values->clear();
            return false;
        }
        values->push_back(value);
        if (separator == std::string::npos) {
            break;
        }
        cursor = separator + 1;
    }
    return values->size() == expectedCount;
}

bool parseFirstDouble(const std::string& text, double* value)
{
    const std::size_t separator = text.find('\\');
    return parseDouble(text.substr(0, separator), value);
}

std::string tagValue(const gdcm::File& file, std::uint16_t group, std::uint16_t element)
{
    const gdcm::Tag tag(group, element);
    if (!file.GetDataSet().FindDataElement(tag)) {
        return std::string();
    }
    gdcm::StringFilter filter;
    filter.SetFile(file);
    return trimDicomText(filter.ToString(tag));
}

bool hasTag(const gdcm::File& file, std::uint16_t group, std::uint16_t element)
{
    return file.GetDataSet().FindDataElement(gdcm::Tag(group, element));
}

ImageModality modalityFromToken(const std::string& token)
{
    if (token == "CT") {
        return ImageModality::CT;
    }
    if (token == "MR") {
        return ImageModality::MR;
    }
    return token.empty() ? ImageModality::Unknown : ImageModality::Other;
}

double dot3(const double a[3], const double b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

double norm3(const double value[3])
{
    return std::sqrt(dot3(value, value));
}

void cross3(const double a[3], const double b[3], double out[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

bool normalize3(double value[3])
{
    const double length = norm3(value);
    if (!std::isfinite(length) || length <= kOrientationTolerance) {
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        value[i] /= length;
    }
    return true;
}

bool nearlyEqual(double a, double b, double absoluteTolerance, double relativeTolerance)
{
    const double scale = std::max(std::abs(a), std::abs(b));
    return std::abs(a - b) <= std::max(absoluteTolerance, relativeTolerance * scale);
}

bool sameVector(const double a[3], const double b[3], double tolerance)
{
    return std::abs(a[0] - b[0]) <= tolerance
        && std::abs(a[1] - b[1]) <= tolerance
        && std::abs(a[2] - b[2]) <= tolerance;
}

DicomSeriesStatus validateSourceDirectory(const std::string& directory,
                                          std::filesystem::path* path,
                                          std::vector<Diagnostic>* diagnostics)
{
    if (directory.empty() || path == nullptr) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::InvalidArgument,
                       "A non-empty DICOM source directory is required.");
        return DicomSeriesStatus::InvalidArgument;
    }

    *path = std::filesystem::path(directory);
    std::error_code error;
    if (!std::filesystem::exists(*path, error)) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::SourceNotFound,
                       "The DICOM source directory does not exist.");
        return DicomSeriesStatus::SourceNotFound;
    }
    if (error || !std::filesystem::is_directory(*path, error)) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::SourceUnreadable,
                       "The DICOM source cannot be opened as a directory.");
        return DicomSeriesStatus::SourceUnreadable;
    }
    if (error) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::SourceUnreadable,
                       "The DICOM source directory cannot be inspected.");
        return DicomSeriesStatus::SourceUnreadable;
    }
    return DicomSeriesStatus::Ok;
}

DicomSeriesStatus enumerateSeriesFiles(const std::string& directory,
                                       std::vector<SeriesFiles>* series,
                                       std::vector<Diagnostic>* diagnostics)
{
    if (series == nullptr) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::InvalidArgument,
                       "A DICOM series output container is required.");
        return DicomSeriesStatus::InvalidArgument;
    }
    series->clear();

    std::filesystem::path sourcePath;
    const DicomSeriesStatus sourceStatus =
        validateSourceDirectory(directory, &sourcePath, diagnostics);
    if (sourceStatus != DicomSeriesStatus::Ok) {
        return sourceStatus;
    }

    std::size_t ignoredFileCount = 0;
    std::error_code iterationError;
    for (std::filesystem::directory_iterator it(sourcePath, iterationError), end;
         !iterationError && it != end;
         it.increment(iterationError)) {
        std::error_code fileError;
        if (!it->is_regular_file(fileError) || fileError) {
            continue;
        }
        gdcm::ImageReader probe;
        const std::string fileName = it->path().string();
        probe.SetFileName(fileName.c_str());
        if (!probe.Read()) {
            ++ignoredFileCount;
        }
    }
    if (iterationError) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::SourceUnreadable,
                       "The DICOM source directory cannot be enumerated.");
        return DicomSeriesStatus::SourceUnreadable;
    }
    if (ignoredFileCount > 0) {
        std::ostringstream message;
        message << ignoredFileCount
                << " non-DICOM or unsupported image file(s) were ignored.";
        addDiagnostic(diagnostics,
                      DiagnosticSeverity::Warning,
                      "dicom.ignored_files",
                      message.str());
    }

    try {
        itk::GDCMSeriesFileNames::Pointer names = itk::GDCMSeriesFileNames::New();
        names->SetUseSeriesDetails(false);
        names->SetRecursive(false);
        names->SetLoadSequences(false);
        names->SetLoadPrivateTags(false);
        names->SetInputDirectory(directory);

        const itk::GDCMSeriesFileNames::SeriesUIDContainerType uids =
            names->GetSeriesUIDs();
        std::set<std::string> seenUids;
        for (const std::string& rawUid : uids) {
            const std::string uid = trimDicomText(rawUid);
            if (uid.empty()) {
                continue;
            }
            if (!seenUids.insert(uid).second) {
                addStatusError(diagnostics,
                               DicomSeriesStatus::InconsistentSeries,
                               "The DICOM source contains duplicate series identities.");
                return DicomSeriesStatus::InconsistentSeries;
            }
            const itk::GDCMSeriesFileNames::FileNamesContainerType& files =
                names->GetFileNames(rawUid);
            if (files.empty()) {
                continue;
            }
            SeriesFiles group;
            group.seriesInstanceUid = uid;
            group.fileNames.assign(files.begin(), files.end());
            series->push_back(std::move(group));
        }
    } catch (const itk::ExceptionObject&) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::SourceUnreadable,
                       "The DICOM source directory could not be indexed.");
        return DicomSeriesStatus::SourceUnreadable;
    } catch (...) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::ReadFailed,
                       "DICOM series discovery failed.");
        return DicomSeriesStatus::ReadFailed;
    }

    std::sort(series->begin(), series->end(), [](const SeriesFiles& a, const SeriesFiles& b) {
        return a.seriesInstanceUid < b.seriesInstanceUid;
    });
    if (series->empty()) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::NoSeries,
                       "No readable DICOM image series were found.");
        return DicomSeriesStatus::NoSeries;
    }
    return DicomSeriesStatus::Ok;
}

DicomSeriesStatus readSliceRecord(const std::string& fileName,
                                  SliceRecord* record,
                                  std::vector<Diagnostic>* diagnostics)
{
    if (record == nullptr) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::InvalidArgument,
                       "A DICOM slice metadata output is required.");
        return DicomSeriesStatus::InvalidArgument;
    }

    gdcm::ImageReader reader;
    reader.SetFileName(fileName.c_str());
    if (!reader.Read()) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::ReadFailed,
                       "A DICOM image instance could not be read.");
        return DicomSeriesStatus::ReadFailed;
    }

    const gdcm::File& file = reader.GetFile();
    const gdcm::Image& image = reader.GetImage();
    const unsigned int* dimensions = image.GetDimensions();
    record->fileName = fileName;
    record->columns = static_cast<int>(dimensions[0]);
    record->rows = static_cast<int>(dimensions[1]);
    record->pixelFormat = image.GetPixelFormat();
    record->identity.studyInstanceUid = tagValue(file, 0x0020, 0x000d);
    record->identity.seriesInstanceUid = tagValue(file, 0x0020, 0x000e);
    record->identity.frameOfReferenceUid = tagValue(file, 0x0020, 0x0052);
    record->sopInstanceUid = tagValue(file, 0x0008, 0x0018);
    record->modality = modalityFromToken(tagValue(file, 0x0008, 0x0060));
    record->photometricInterpretation = tagValue(file, 0x0028, 0x0004);

    record->frameCount = 1;
    const std::string numberOfFrames = tagValue(file, 0x0028, 0x0008);
    if (!numberOfFrames.empty() && !parsePositiveInt(numberOfFrames, &record->frameCount)) {
        record->frameCount = 2;
    }
    if (image.GetNumberOfDimensions() > 2 && dimensions[2] > 1) {
        record->frameCount = static_cast<int>(dimensions[2]);
    }
    record->multiFrame = record->frameCount > 1;

    std::vector<double> values;
    if (parseDoubles(tagValue(file, 0x0020, 0x0032), 3, &values)) {
        record->hasPosition = true;
        for (int i = 0; i < 3; ++i) {
            record->position[i] = values[static_cast<std::size_t>(i)];
        }
    }
    if (parseDoubles(tagValue(file, 0x0020, 0x0037), 6, &values)) {
        record->hasOrientation = true;
        for (int i = 0; i < 3; ++i) {
            record->axisX[i] = values[static_cast<std::size_t>(i)];
            record->axisY[i] = values[static_cast<std::size_t>(i + 3)];
        }
    }
    if (parseDoubles(tagValue(file, 0x0028, 0x0030), 2, &values)) {
        record->hasPixelSpacing = true;
        record->spacingY = values[0];
        record->spacingX = values[1];
    }

    record->hasSliceThickness =
        parseDouble(tagValue(file, 0x0018, 0x0050), &record->sliceThickness);
    record->hasSpacingBetweenSlices =
        parseDouble(tagValue(file, 0x0018, 0x0088), &record->spacingBetweenSlices);

    record->hasRescaleInterceptTag = hasTag(file, 0x0028, 0x1052);
    record->hasRescaleSlopeTag = hasTag(file, 0x0028, 0x1053);
    if (record->hasRescaleInterceptTag) {
        record->rescaleValuesValid = parseDouble(
            tagValue(file, 0x0028, 0x1052), &record->rescaleIntercept);
    }
    if (record->hasRescaleSlopeTag) {
        record->rescaleValuesValid = record->rescaleValuesValid
            && parseDouble(tagValue(file, 0x0028, 0x1053), &record->rescaleSlope);
    }
    record->hasWindowCenter =
        parseFirstDouble(tagValue(file, 0x0028, 0x1050), &record->windowCenter);
    record->hasWindowWidth =
        parseFirstDouble(tagValue(file, 0x0028, 0x1051), &record->windowWidth);
    return DicomSeriesStatus::Ok;
}

DicomSeriesDescriptor descriptorFromRecord(const SeriesFiles& group,
                                           const SliceRecord& record)
{
    DicomSeriesDescriptor descriptor;
    descriptor.identity = record.identity;
    descriptor.identity.seriesInstanceUid = group.seriesInstanceUid;
    descriptor.modality = record.modality;
    descriptor.dimensions[0] = record.columns;
    descriptor.dimensions[1] = record.rows;
    descriptor.dimensions[2] = record.multiFrame
        ? record.frameCount
        : static_cast<int>(group.fileNames.size());
    descriptor.sliceCount = static_cast<std::size_t>(descriptor.dimensions[2]);
    descriptor.safeDisplayName = makeSafeDicomSeriesDisplayName(descriptor);
    return descriptor;
}

DicomSeriesStatus validateAndSortSeries(std::vector<SliceRecord>* records,
                                       ImageGeometry* geometry,
                                       std::vector<Diagnostic>* diagnostics)
{
    if (records == nullptr || geometry == nullptr || records->empty()) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::NoSeries,
                       "The selected DICOM series contains no image instances.");
        return DicomSeriesStatus::NoSeries;
    }

    SliceRecord& first = records->front();
    if (first.modality != ImageModality::CT && first.modality != ImageModality::MR) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::UnsupportedPixelFormat,
                       "Shell A supports single-component CT and MR series only.");
        return DicomSeriesStatus::UnsupportedPixelFormat;
    }
    if (first.multiFrame) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::UnsupportedMultiFrame,
                       "Enhanced or multi-frame DICOM is not supported in shell A.");
        return DicomSeriesStatus::UnsupportedMultiFrame;
    }
    if (first.columns <= 0 || first.rows <= 0
        || first.identity.studyInstanceUid.empty()
        || first.identity.seriesInstanceUid.empty()
        || first.sopInstanceUid.empty()) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::InconsistentSeries,
                       "The selected series is missing required technical identity metadata.");
        return DicomSeriesStatus::InconsistentSeries;
    }
    if (first.identity.frameOfReferenceUid.empty()) {
        addDiagnostic(diagnostics,
                      DiagnosticSeverity::Warning,
                      "dicom.frame_uid_absent",
                      "The selected series has no FrameOfReferenceUID; patient-space geometry remains explicit.");
    }
    if (!first.hasPosition || !first.hasOrientation || !first.hasPixelSpacing) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::UnsupportedGeometry,
                       "The selected series is missing required patient-space geometry metadata.");
        return DicomSeriesStatus::UnsupportedGeometry;
    }
    if (first.pixelFormat.GetSamplesPerPixel() != 1
        || first.photometricInterpretation != "MONOCHROME2"
        || (first.pixelFormat.GetScalarType() != gdcm::PixelFormat::INT8
            && first.pixelFormat.GetScalarType() != gdcm::PixelFormat::UINT8
            && first.pixelFormat.GetScalarType() != gdcm::PixelFormat::INT16
            && first.pixelFormat.GetScalarType() != gdcm::PixelFormat::UINT16)) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::UnsupportedPixelFormat,
                       "The selected DICOM series is not a supported scalar pixel format.");
        return DicomSeriesStatus::UnsupportedPixelFormat;
    }
    if (first.hasRescaleSlopeTag != first.hasRescaleInterceptTag
        || !first.rescaleValuesValid
        || !std::isfinite(first.rescaleSlope)
        || !std::isfinite(first.rescaleIntercept)
        || first.rescaleSlope == 0.0) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::UnsupportedRescale,
                       "The selected series has incomplete or invalid modality rescale metadata.");
        return DicomSeriesStatus::UnsupportedRescale;
    }
    if (!(first.spacingX > 0.0) || !(first.spacingY > 0.0)
        || !std::isfinite(first.spacingX) || !std::isfinite(first.spacingY)) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::UnsupportedGeometry,
                       "The selected series has invalid in-plane spacing.");
        return DicomSeriesStatus::UnsupportedGeometry;
    }

    double axisX[3] = {first.axisX[0], first.axisX[1], first.axisX[2]};
    double axisY[3] = {first.axisY[0], first.axisY[1], first.axisY[2]};
    const double axisXLength = norm3(axisX);
    const double axisYLength = norm3(axisY);
    if (!normalize3(axisX) || !normalize3(axisY)
        || std::abs(axisXLength - 1.0) > kUnitVectorTolerance
        || std::abs(axisYLength - 1.0) > kUnitVectorTolerance
        || std::abs(dot3(axisX, axisY)) > kUnitVectorTolerance) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::UnsupportedGeometry,
                       "The selected series has invalid image orientation vectors.");
        return DicomSeriesStatus::UnsupportedGeometry;
    }
    double normal[3] = {0.0, 0.0, 0.0};
    cross3(axisX, axisY, normal);
    if (!normalize3(normal)) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::UnsupportedGeometry,
                       "The selected series orientation is singular.");
        return DicomSeriesStatus::UnsupportedGeometry;
    }

    std::set<std::string> sopUids;
    for (SliceRecord& record : *records) {
        if (record.multiFrame) {
            addStatusError(diagnostics,
                           DicomSeriesStatus::UnsupportedMultiFrame,
                           "Enhanced or multi-frame DICOM is not supported in shell A.");
            return DicomSeriesStatus::UnsupportedMultiFrame;
        }
        if (record.columns != first.columns || record.rows != first.rows
            || record.identity.studyInstanceUid != first.identity.studyInstanceUid
            || record.identity.seriesInstanceUid != first.identity.seriesInstanceUid
            || record.identity.frameOfReferenceUid != first.identity.frameOfReferenceUid
            || record.modality != first.modality
            || record.photometricInterpretation != first.photometricInterpretation
            || record.pixelFormat != first.pixelFormat
            || !record.hasPosition || !record.hasOrientation || !record.hasPixelSpacing
            || !sameVector(record.axisX, first.axisX, kOrientationTolerance)
            || !sameVector(record.axisY, first.axisY, kOrientationTolerance)
            || !nearlyEqual(record.spacingX, first.spacingX,
                            kPositionToleranceMm, kSpacingRelativeTolerance)
            || !nearlyEqual(record.spacingY, first.spacingY,
                            kPositionToleranceMm, kSpacingRelativeTolerance)) {
            addStatusError(diagnostics,
                           DicomSeriesStatus::InconsistentSeries,
                           "The selected DICOM series has inconsistent identity, geometry, or pixel metadata.");
            return DicomSeriesStatus::InconsistentSeries;
        }
        if (record.sopInstanceUid.empty() || !sopUids.insert(record.sopInstanceUid).second) {
            addStatusError(diagnostics,
                           DicomSeriesStatus::InconsistentSeries,
                           "The selected DICOM series contains a missing or duplicate instance identity.");
            return DicomSeriesStatus::InconsistentSeries;
        }
        if (record.hasRescaleSlopeTag != record.hasRescaleInterceptTag
            || record.hasRescaleSlopeTag != first.hasRescaleSlopeTag
            || !record.rescaleValuesValid
            || !nearlyEqual(record.rescaleSlope, first.rescaleSlope, 1e-12, 1e-12)
            || !nearlyEqual(record.rescaleIntercept, first.rescaleIntercept, 1e-12, 1e-12)) {
            addStatusError(diagnostics,
                           DicomSeriesStatus::UnsupportedRescale,
                           "The selected series does not have one consistent modality rescale mapping.");
            return DicomSeriesStatus::UnsupportedRescale;
        }
        record.projection = dot3(record.position, normal);
        if (!std::isfinite(record.projection)) {
            addStatusError(diagnostics,
                           DicomSeriesStatus::UnsupportedGeometry,
                           "The selected series contains non-finite slice positions.");
            return DicomSeriesStatus::UnsupportedGeometry;
        }
    }

    std::sort(records->begin(), records->end(), [](const SliceRecord& a, const SliceRecord& b) {
        if (a.projection == b.projection) {
            return a.sopInstanceUid < b.sopInstanceUid;
        }
        return a.projection < b.projection;
    });

    double spacingZ = 0.0;
    if (records->size() == 1) {
        const SliceRecord& only = records->front();
        if (only.hasSpacingBetweenSlices && only.spacingBetweenSlices > 0.0) {
            spacingZ = only.spacingBetweenSlices;
        } else if (only.hasSliceThickness && only.sliceThickness > 0.0) {
            spacingZ = only.sliceThickness;
        } else {
            addStatusError(diagnostics,
                           DicomSeriesStatus::UnsupportedGeometry,
                           "A single-slice series requires explicit through-plane spacing.");
            return DicomSeriesStatus::UnsupportedGeometry;
        }
    } else {
        std::vector<double> gaps;
        gaps.reserve(records->size() - 1);
        for (std::size_t i = 1; i < records->size(); ++i) {
            const SliceRecord& previous = (*records)[i - 1];
            const SliceRecord& current = (*records)[i];
            const double gap = current.projection - previous.projection;
            if (!(gap > kPositionToleranceMm)) {
                addStatusError(diagnostics,
                               DicomSeriesStatus::InconsistentSeries,
                               "The selected series contains duplicate or non-monotonic slice positions.");
                return DicomSeriesStatus::InconsistentSeries;
            }
            double delta[3] = {
                current.position[0] - previous.position[0],
                current.position[1] - previous.position[1],
                current.position[2] - previous.position[2]
            };
            double residual[3] = {
                delta[0] - gap * normal[0],
                delta[1] - gap * normal[1],
                delta[2] - gap * normal[2]
            };
            if (norm3(residual) > std::max(kTiltToleranceMm,
                                           kSpacingRelativeTolerance * gap)) {
                addStatusError(diagnostics,
                               DicomSeriesStatus::UnsupportedGeometry,
                               "Gantry tilt or sheared slice stacking is not supported in shell A.");
                return DicomSeriesStatus::UnsupportedGeometry;
            }
            gaps.push_back(gap);
        }
        double sum = 0.0;
        for (double gap : gaps) {
            sum += gap;
        }
        spacingZ = sum / static_cast<double>(gaps.size());
        for (double gap : gaps) {
            if (!nearlyEqual(gap, spacingZ,
                             kPositionToleranceMm, kSpacingRelativeTolerance)) {
                addStatusError(diagnostics,
                               DicomSeriesStatus::UnsupportedGeometry,
                               "The selected series has non-uniform slice spacing.");
                return DicomSeriesStatus::UnsupportedGeometry;
            }
        }
    }

    *geometry = ImageGeometry{};
    geometry->dimensions[0] = first.columns;
    geometry->dimensions[1] = first.rows;
    geometry->dimensions[2] = static_cast<int>(records->size());
    geometry->spacing[0] = first.spacingX;
    geometry->spacing[1] = first.spacingY;
    geometry->spacing[2] = spacingZ;
    for (int i = 0; i < 3; ++i) {
        geometry->origin[i] = records->front().position[i];
        geometry->direction[i][0] = axisX[i];
        geometry->direction[i][1] = axisY[i];
        geometry->direction[i][2] = normal[i];
    }
    geometry->coordinateSystem = ImageCoordinateSystem::LPS;
    return DicomSeriesStatus::Ok;
}

bool hashFile(const std::string& fileName, std::string* digest)
{
    if (digest == nullptr) {
        return false;
    }
    std::ifstream input(fileName, std::ios::binary);
    if (!input) {
        return false;
    }
    picosha2::hash256_one_by_one hasher;
    std::array<unsigned char, 64 * 1024> buffer = {};
    while (input) {
        input.read(reinterpret_cast<char*>(buffer.data()),
                   static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            hasher.process(buffer.data(), buffer.data() + count);
        }
    }
    if (!input.eof()) {
        return false;
    }
    hasher.finish();
    *digest = picosha2::get_hash_hex_string(hasher);
    return true;
}

void hashText(picosha2::hash256_one_by_one* hasher, const std::string& text)
{
    hasher->process(text.begin(), text.end());
}

bool computeSeriesFingerprint(const std::vector<SliceRecord>& records,
                              std::string* fingerprint)
{
    if (fingerprint == nullptr || records.empty()) {
        return false;
    }
    picosha2::hash256_one_by_one aggregate;
    hashText(&aggregate, "XQ-DICOM-SERIES-FINGERPRINT-V1\n");
    for (const SliceRecord& record : records) {
        std::string fileDigest;
        if (!hashFile(record.fileName, &fileDigest)) {
            return false;
        }
        std::ostringstream entry;
        entry << "SOP " << record.sopInstanceUid.size() << ":"
              << record.sopInstanceUid << "\nSHA256 " << fileDigest << "\n";
        hashText(&aggregate, entry.str());
    }
    aggregate.finish();
    *fingerprint = std::string("dicom-series-v1:sha256:")
        + picosha2::get_hash_hex_string(aggregate);
    return true;
}

DicomSeriesStatus decodeSeries(const std::vector<SliceRecord>& records,
                               const ImageGeometry& geometry,
                               std::shared_ptr<XQMemoryImageBufferHandle>* buffer,
                               IntensityRange* intensityRange,
                               std::vector<Diagnostic>* diagnostics)
{
    if (buffer == nullptr || intensityRange == nullptr || records.empty()) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::InvalidArgument,
                       "A DICOM decode output is required.");
        return DicomSeriesStatus::InvalidArgument;
    }

    using ImageType = itk::Image<float, 3>;
    using ReaderType = itk::ImageSeriesReader<ImageType>;
    ReaderType::Pointer reader = ReaderType::New();
    itk::GDCMImageIO::Pointer imageIo = itk::GDCMImageIO::New();
    imageIo->SetLoadPrivateTags(false);
    imageIo->SetLoadSequences(false);

    std::vector<std::string> fileNames;
    fileNames.reserve(records.size());
    for (const SliceRecord& record : records) {
        fileNames.push_back(record.fileName);
    }
    reader->SetImageIO(imageIo);
    reader->SetFileNames(fileNames);
    reader->MetaDataDictionaryArrayUpdateOff();
    reader->ForceOrthogonalDirectionOn();
    reader->SetSpacingWarningRelThreshold(kSpacingRelativeTolerance);

    try {
        reader->UpdateLargestPossibleRegion();
    } catch (const itk::ExceptionObject&) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::ReadFailed,
                       "The selected DICOM series could not be decoded.");
        return DicomSeriesStatus::ReadFailed;
    } catch (...) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::ReadFailed,
                       "The selected DICOM series decode failed.");
        return DicomSeriesStatus::ReadFailed;
    }

    ImageType* image = reader->GetOutput();
    if (image == nullptr || image->GetBufferPointer() == nullptr) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::ReadFailed,
                       "The DICOM decoder returned no scalar buffer.");
        return DicomSeriesStatus::ReadFailed;
    }
    const ImageType::SizeType size = image->GetLargestPossibleRegion().GetSize();
    if (size[0] != static_cast<ImageType::SizeType::SizeValueType>(geometry.dimensions[0])
        || size[1] != static_cast<ImageType::SizeType::SizeValueType>(geometry.dimensions[1])
        || size[2] != static_cast<ImageType::SizeType::SizeValueType>(geometry.dimensions[2])) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::InconsistentSeries,
                       "Decoded DICOM dimensions do not match the validated series metadata.");
        return DicomSeriesStatus::InconsistentSeries;
    }

    const ImageType::PointType origin = image->GetOrigin();
    const ImageType::SpacingType spacing = image->GetSpacing();
    const ImageType::DirectionType direction = image->GetDirection();
    for (int i = 0; i < 3; ++i) {
        if (!nearlyEqual(origin[static_cast<unsigned int>(i)], geometry.origin[i],
                         kPositionToleranceMm, kSpacingRelativeTolerance)
            || !nearlyEqual(spacing[static_cast<unsigned int>(i)], geometry.spacing[i],
                            kPositionToleranceMm, kSpacingRelativeTolerance)) {
            addStatusError(diagnostics,
                           DicomSeriesStatus::UnsupportedGeometry,
                           "Decoded DICOM geometry disagrees with validated patient-space metadata.");
            return DicomSeriesStatus::UnsupportedGeometry;
        }
        for (int column = 0; column < 3; ++column) {
            if (std::abs(direction[static_cast<unsigned int>(i)]
                                  [static_cast<unsigned int>(column)]
                         - geometry.direction[i][column]) > kOrientationTolerance) {
                addStatusError(diagnostics,
                               DicomSeriesStatus::UnsupportedGeometry,
                               "Decoded DICOM orientation disagrees with validated patient-space metadata.");
                return DicomSeriesStatus::UnsupportedGeometry;
            }
        }
    }

    const std::size_t voxelCount = image->GetLargestPossibleRegion().GetNumberOfPixels();
    if (voxelCount == 0
        || voxelCount > std::numeric_limits<std::size_t>::max() / sizeof(float)) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::ReadFailed,
                       "The decoded DICOM scalar buffer has an invalid size.");
        return DicomSeriesStatus::ReadFailed;
    }
    const float* values = image->GetBufferPointer();
    float minimum = std::numeric_limits<float>::infinity();
    float maximum = -std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < voxelCount; ++i) {
        if (!std::isfinite(values[i])) {
            addStatusError(diagnostics,
                           DicomSeriesStatus::UnsupportedRescale,
                           "Modality rescale produced a non-finite scalar value.");
            return DicomSeriesStatus::UnsupportedRescale;
        }
        minimum = std::min(minimum, values[i]);
        maximum = std::max(maximum, values[i]);
    }

    std::vector<std::uint8_t> bytes(voxelCount * sizeof(float));
    std::memcpy(bytes.data(), values, bytes.size());
    std::shared_ptr<XQMemoryImageBufferHandle> decoded =
        std::make_shared<XQMemoryImageBufferHandle>(
            ScalarType::Float32, geometry.dimensions, 1, std::move(bytes));
    if (!decoded->is_valid()) {
        addStatusError(diagnostics,
                       DicomSeriesStatus::ReadFailed,
                       "The decoded DICOM scalar buffer is internally inconsistent.");
        return DicomSeriesStatus::ReadFailed;
    }
    intensityRange->minimum = static_cast<double>(minimum);
    intensityRange->maximum = static_cast<double>(maximum);
    *buffer = std::move(decoded);
    return DicomSeriesStatus::Ok;
}

} // namespace

DicomSeriesDiscoveryResult GdcmItkDicomSeriesReader::discover(
    const std::string& directory)
{
    DicomSeriesDiscoveryResult result;
    std::vector<SeriesFiles> groups;
    result.status = enumerateSeriesFiles(directory, &groups, &result.diagnostics);
    if (result.status != DicomSeriesStatus::Ok) {
        return result;
    }

    for (const SeriesFiles& group : groups) {
        SliceRecord first;
        std::vector<Diagnostic> localDiagnostics;
        const DicomSeriesStatus status =
            readSliceRecord(group.fileNames.front(), &first, &localDiagnostics);
        if (status != DicomSeriesStatus::Ok) {
            addDiagnostic(&result.diagnostics,
                          DiagnosticSeverity::Warning,
                          "dicom.series_descriptor_read_failed",
                          "One DICOM series could not read its first image instance.");
            continue;
        }
        if (first.columns <= 0 || first.rows <= 0) {
            addDiagnostic(&result.diagnostics,
                          DiagnosticSeverity::Warning,
                          "dicom.series_descriptor_invalid_dimensions",
                          "One DICOM series reported invalid image dimensions.");
            continue;
        }
        if (first.identity.studyInstanceUid.empty()) {
            addDiagnostic(&result.diagnostics,
                          DiagnosticSeverity::Warning,
                          "dicom.series_descriptor_missing_study_uid",
                          "One DICOM series is missing StudyInstanceUID.");
            continue;
        }
        if (first.identity.frameOfReferenceUid.empty()) {
            addDiagnostic(&result.diagnostics,
                          DiagnosticSeverity::Warning,
                          "dicom.series_descriptor_missing_frame_uid",
                          "One DICOM series is missing FrameOfReferenceUID; discovery retained its technical series metadata.");
        }
        if (!first.identity.seriesInstanceUid.empty()
            && first.identity.seriesInstanceUid != group.seriesInstanceUid) {
            addDiagnostic(&result.diagnostics,
                          DiagnosticSeverity::Warning,
                          "dicom.series_identity_mismatch",
                          "One DICOM series reported inconsistent series identity metadata.");
            continue;
        }
        result.series.push_back(descriptorFromRecord(group, first));
    }

    std::sort(result.series.begin(), result.series.end(),
              [](const DicomSeriesDescriptor& a, const DicomSeriesDescriptor& b) {
                  return a.identity.seriesInstanceUid < b.identity.seriesInstanceUid;
              });
    if (result.series.empty()) {
        result.status = DicomSeriesStatus::ReadFailed;
        addStatusError(&result.diagnostics,
                       result.status,
                       "No DICOM series supplied the required technical discovery metadata.");
    }
    return result;
}

DicomSeriesReadResult GdcmItkDicomSeriesReader::read(
    const std::string& directory,
    const std::string& seriesInstanceUid)
{
    DicomSeriesReadResult result;
    std::vector<SeriesFiles> groups;
    result.status = enumerateSeriesFiles(directory, &groups, &result.diagnostics);
    if (result.status != DicomSeriesStatus::Ok) {
        return result;
    }

    if (seriesInstanceUid.empty()) {
        result.status = groups.size() > 1
            ? DicomSeriesStatus::AmbiguousSeries
            : DicomSeriesStatus::SeriesSelectionRequired;
        addStatusError(&result.diagnostics,
                       result.status,
                       groups.size() > 1
                           ? "Multiple DICOM series are available; an explicit series UID is required."
                           : "An explicit DICOM series UID is required.");
        return result;
    }

    const auto selected = std::find_if(
        groups.begin(), groups.end(), [&](const SeriesFiles& group) {
            return group.seriesInstanceUid == seriesInstanceUid;
        });
    if (selected == groups.end()) {
        result.status = DicomSeriesStatus::SeriesNotFound;
        addStatusError(&result.diagnostics,
                       result.status,
                       "The requested DICOM series UID was not found in the source directory.");
        return result;
    }

    std::vector<SliceRecord> records;
    records.reserve(selected->fileNames.size());
    for (const std::string& fileName : selected->fileNames) {
        SliceRecord record;
        result.status = readSliceRecord(fileName, &record, &result.diagnostics);
        if (result.status != DicomSeriesStatus::Ok) {
            return result;
        }
        records.push_back(std::move(record));
    }

    ImageGeometry geometry = {};
    result.status = validateAndSortSeries(&records, &geometry, &result.diagnostics);
    if (result.status != DicomSeriesStatus::Ok) {
        return result;
    }
    if (records.front().identity.seriesInstanceUid != seriesInstanceUid) {
        result.status = DicomSeriesStatus::InconsistentSeries;
        addStatusError(&result.diagnostics,
                       result.status,
                       "The selected files do not match the requested DICOM series identity.");
        return result;
    }

    result.descriptor = descriptorFromRecord(*selected, records.front());
    result.descriptor.dimensions[0] = geometry.dimensions[0];
    result.descriptor.dimensions[1] = geometry.dimensions[1];
    result.descriptor.dimensions[2] = geometry.dimensions[2];
    result.descriptor.sliceCount = records.size();
    result.descriptor.safeDisplayName = makeSafeDicomSeriesDisplayName(result.descriptor);
    if (!computeSeriesFingerprint(records, &result.descriptor.contentFingerprint)) {
        result.status = DicomSeriesStatus::SourceUnreadable;
        addStatusError(&result.diagnostics,
                       result.status,
                       "The selected DICOM series content could not be fingerprinted.");
        return result;
    }

    IntensityRange intensityRange = {};
    result.status = decodeSeries(
        records, geometry, &result.buffer, &intensityRange, &result.diagnostics);
    if (result.status != DicomSeriesStatus::Ok) {
        result.buffer.reset();
        return result;
    }

    result.volume.setGeometry(geometry);
    result.volume.setScalarType(ScalarType::Float32);
    result.volume.setComponentCount(1);
    result.volume.setIntensityRange(intensityRange);
    result.volume.setBuffer(std::make_shared<ImageBufferHandle>());
    result.volume.setModality(records.front().modality);
    result.volume.setDicomIdentity(records.front().identity);
    if (records.front().hasWindowCenter) {
        result.volume.setWindowCenter(records.front().windowCenter);
    }
    if (records.front().hasWindowWidth) {
        result.volume.setWindowWidth(records.front().windowWidth);
    }
    // The resident buffer already contains modality-rescaled values. Keeping
    // the pending XQ transform at identity prevents downstream double-rescale.
    result.volume.setRescaleSlope(1.0);
    result.volume.setRescaleIntercept(0.0);
    result.status = DicomSeriesStatus::Ok;
    return result;
}

} // namespace xq
