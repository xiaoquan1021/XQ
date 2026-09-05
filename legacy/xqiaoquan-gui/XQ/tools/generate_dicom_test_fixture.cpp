#include <gdcmAttribute.h>
#include <gdcmDataElement.h>
#include <gdcmDataSet.h>
#include <gdcmImage.h>
#include <gdcmImageWriter.h>
#include <gdcmPhotometricInterpretation.h>
#include <gdcmPixelFormat.h>
#include <gdcmTag.h>
#include <gdcmTransferSyntax.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct SeriesSpec {
    std::string studyUid;
    std::string seriesUid;
    std::string frameUid;
    std::string modality;
    std::string sopClassUid;
    unsigned int columns = 0;
    unsigned int rows = 0;
    double spacingX = 1.0;
    double spacingY = 1.0;
    double spacingZ = 1.0;
    std::array<double, 3> origin = {{0.0, 0.0, 0.0}};
    std::array<double, 3> axisX = {{1.0, 0.0, 0.0}};
    std::array<double, 3> axisY = {{0.0, 1.0, 0.0}};
    double slope = 1.0;
    double intercept = 0.0;
    double windowCenter = 0.0;
    double windowWidth = 0.0;
    int baseStoredValue = 0;
};

template <std::uint16_t Group, std::uint16_t Element, typename Value>
void insertValue(gdcm::DataSet* dataSet, const Value& value)
{
    gdcm::Attribute<Group, Element> attribute;
    attribute.SetValue(value);
    dataSet->Insert(attribute.GetAsDataElement());
}

template <std::uint16_t Group, std::uint16_t Element, typename Value>
void insertDynamicValue(gdcm::DataSet* dataSet, const Value& value)
{
    gdcm::Attribute<Group, Element> attribute;
    attribute.SetNumberOfValues(1);
    attribute.SetValue(0, value);
    dataSet->Insert(attribute.GetAsDataElement());
}

template <std::uint16_t Group, std::uint16_t Element>
void insertString(gdcm::DataSet* dataSet, const std::string& value)
{
    gdcm::Attribute<Group, Element> attribute;
    attribute.SetValue(value.c_str());
    dataSet->Insert(attribute.GetAsDataElement());
}

template <std::uint16_t Group, std::uint16_t Element, std::size_t Count>
void insertDoubles(gdcm::DataSet* dataSet, const std::array<double, Count>& values)
{
    gdcm::Attribute<Group, Element> attribute;
    attribute.SetValues(values.data());
    dataSet->Insert(attribute.GetAsDataElement());
}

std::array<double, 3> cross(const std::array<double, 3>& a,
                            const std::array<double, 3>& b)
{
    return {{
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]
    }};
}

bool writeSlice(const std::filesystem::path& path,
                const SeriesSpec& spec,
                int sliceIndex,
                double positionOffset,
                const std::array<double, 3>* axisYOverride = nullptr)
{
    const std::array<double, 3> axisY = axisYOverride == nullptr
        ? spec.axisY
        : *axisYOverride;
    const std::array<double, 3> normal = cross(spec.axisX, axisY);
    const std::array<double, 3> position = {{
        spec.origin[0] + normal[0] * positionOffset,
        spec.origin[1] + normal[1] * positionOffset,
        spec.origin[2] + normal[2] * positionOffset
    }};

    std::vector<std::int16_t> pixels(
        static_cast<std::size_t>(spec.columns) * static_cast<std::size_t>(spec.rows));
    for (unsigned int y = 0; y < spec.rows; ++y) {
        for (unsigned int x = 0; x < spec.columns; ++x) {
            const std::size_t linear = static_cast<std::size_t>(x)
                + static_cast<std::size_t>(spec.columns) * static_cast<std::size_t>(y);
            pixels[linear] = static_cast<std::int16_t>(
                spec.baseStoredValue + sliceIndex * 20 + static_cast<int>(linear));
        }
    }

    gdcm::ImageWriter writer;
    gdcm::Image& image = writer.GetImage();
    image.SetNumberOfDimensions(2);
    image.SetDimension(0, spec.columns);
    image.SetDimension(1, spec.rows);
    image.SetSpacing(0, spec.spacingX);
    image.SetSpacing(1, spec.spacingY);
    image.SetSpacing(2, spec.spacingZ);
    image.SetOrigin(position.data());
    // GDCM stores only NumberOfDimensions entries in the array overload. A
    // classic single-frame image is 2D, but DICOM patient position is always
    // three-dimensional, so preserve the through-plane coordinate explicitly.
    image.SetOrigin(2, position[2]);
    const std::array<double, 6> direction = {{
        spec.axisX[0], spec.axisX[1], spec.axisX[2],
        axisY[0], axisY[1], axisY[2]
    }};
    image.SetDirectionCosines(direction.data());
    image.SetPixelFormat(gdcm::PixelFormat(gdcm::PixelFormat::INT16));
    image.SetPhotometricInterpretation(gdcm::PhotometricInterpretation::MONOCHROME2);
    image.SetTransferSyntax(gdcm::TransferSyntax::ExplicitVRLittleEndian);
    image.SetIntercept(spec.intercept);
    image.SetSlope(spec.slope);

    gdcm::DataElement pixelData(gdcm::Tag(0x7fe0, 0x0010));
    pixelData.SetByteValue(
        reinterpret_cast<const char*>(pixels.data()),
        static_cast<std::uint32_t>(pixels.size() * sizeof(std::int16_t)));
    image.SetDataElement(pixelData);

    gdcm::DataSet& dataSet = writer.GetFile().GetDataSet();
    insertString<0x0008, 0x0016>(&dataSet, spec.sopClassUid);
    insertString<0x0008, 0x0018>(
        &dataSet, spec.seriesUid + "." + std::to_string(sliceIndex + 1));
    insertString<0x0008, 0x0020>(&dataSet, "20000101");
    insertString<0x0008, 0x0030>(&dataSet, "000000");
    insertString<0x0008, 0x0060>(&dataSet, spec.modality);
    insertString<0x0020, 0x000d>(&dataSet, spec.studyUid);
    insertString<0x0020, 0x000e>(&dataSet, spec.seriesUid);
    insertString<0x0020, 0x0052>(&dataSet, spec.frameUid);
    insertValue<0x0020, 0x0013>(&dataSet, sliceIndex + 1);
    insertDoubles<0x0020, 0x0032>(&dataSet, position);
    insertDoubles<0x0020, 0x0037>(&dataSet, direction);
    const std::array<double, 2> pixelSpacing = {{spec.spacingY, spec.spacingX}};
    insertDoubles<0x0028, 0x0030>(&dataSet, pixelSpacing);
    insertValue<0x0018, 0x0050>(&dataSet, spec.spacingZ);
    insertValue<0x0018, 0x0088>(&dataSet, spec.spacingZ);
    insertDynamicValue<0x0028, 0x1050>(&dataSet, spec.windowCenter);
    insertDynamicValue<0x0028, 0x1051>(&dataSet, spec.windowWidth);
    insertValue<0x0028, 0x1052>(&dataSet, spec.intercept);
    insertValue<0x0028, 0x1053>(&dataSet, spec.slope);
    insertString<0x0028, 0x1054>(
        &dataSet, spec.modality == "CT" ? "HU" : "US");

    writer.SetFileName(path.string().c_str());
    return writer.Write();
}

bool writeRegularOblique(const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "regular-oblique";
    std::filesystem::create_directories(directory);
    SeriesSpec spec;
    spec.studyUid = "1.2.826.0.1.3680043.10.543.100";
    spec.seriesUid = "1.2.826.0.1.3680043.10.543.101";
    spec.frameUid = "1.2.826.0.1.3680043.10.543.109";
    spec.modality = "CT";
    spec.sopClassUid = "1.2.840.10008.5.1.4.1.1.2";
    spec.columns = 4;
    spec.rows = 3;
    spec.spacingX = 0.7;
    spec.spacingY = 0.8;
    spec.spacingZ = 1.5;
    spec.origin = {{10.0, 20.0, 30.0}};
    spec.axisX = {{0.8660254037844386, 0.5, 0.0}};
    spec.axisY = {{-0.4330127018922193, 0.75, 0.5}};
    spec.slope = 2.0;
    spec.intercept = -1024.0;
    spec.windowCenter = 40.0;
    spec.windowWidth = 400.0;
    spec.baseStoredValue = 100;
    for (int slice = 0; slice < 4; ++slice) {
        if (!writeSlice(directory / ("slice-" + std::to_string(slice + 1) + ".dcm"),
                        spec, slice, spec.spacingZ * slice)) {
            return false;
        }
    }
    return true;
}

bool writeMultiSeries(const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "multi-series";
    std::filesystem::create_directories(directory);

    SeriesSpec ct;
    ct.studyUid = "1.2.826.0.1.3680043.10.543.200";
    ct.seriesUid = "1.2.826.0.1.3680043.10.543.201";
    ct.frameUid = "1.2.826.0.1.3680043.10.543.209";
    ct.modality = "CT";
    ct.sopClassUid = "1.2.840.10008.5.1.4.1.1.2";
    ct.columns = 3;
    ct.rows = 2;
    ct.spacingX = 0.9;
    ct.spacingY = 1.1;
    ct.spacingZ = 2.0;
    ct.origin = {{-5.0, 4.0, 3.0}};
    ct.baseStoredValue = 10;
    for (int slice = 0; slice < 2; ++slice) {
        if (!writeSlice(directory / ("ct-" + std::to_string(slice + 1) + ".dcm"),
                        ct, slice, ct.spacingZ * slice)) {
            return false;
        }
    }

    SeriesSpec mr;
    mr.studyUid = "1.2.826.0.1.3680043.10.543.300";
    mr.seriesUid = "1.2.826.0.1.3680043.10.543.301";
    mr.frameUid = "1.2.826.0.1.3680043.10.543.309";
    mr.modality = "MR";
    mr.sopClassUid = "1.2.840.10008.5.1.4.1.1.4";
    mr.columns = 2;
    mr.rows = 2;
    mr.spacingX = 1.2;
    mr.spacingY = 1.3;
    mr.spacingZ = 2.5;
    mr.origin = {{7.0, 8.0, 9.0}};
    mr.baseStoredValue = 500;
    for (int slice = 0; slice < 3; ++slice) {
        if (!writeSlice(directory / ("mr-" + std::to_string(slice + 1) + ".dcm"),
                        mr, slice, mr.spacingZ * slice)) {
            return false;
        }
    }
    return true;
}

bool writeNonUniform(const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "non-uniform";
    std::filesystem::create_directories(directory);
    SeriesSpec spec;
    spec.studyUid = "1.2.826.0.1.3680043.10.543.400";
    spec.seriesUid = "1.2.826.0.1.3680043.10.543.401";
    spec.frameUid = "1.2.826.0.1.3680043.10.543.409";
    spec.modality = "CT";
    spec.sopClassUid = "1.2.840.10008.5.1.4.1.1.2";
    spec.columns = 2;
    spec.rows = 2;
    spec.spacingZ = 1.0;
    const double offsets[3] = {0.0, 1.0, 3.0};
    for (int slice = 0; slice < 3; ++slice) {
        if (!writeSlice(directory / ("slice-" + std::to_string(slice + 1) + ".dcm"),
                        spec, slice, offsets[slice])) {
            return false;
        }
    }
    return true;
}

bool writeMixedOrientation(const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "mixed-orientation";
    std::filesystem::create_directories(directory);
    SeriesSpec spec;
    spec.studyUid = "1.2.826.0.1.3680043.10.543.500";
    spec.seriesUid = "1.2.826.0.1.3680043.10.543.501";
    spec.frameUid = "1.2.826.0.1.3680043.10.543.509";
    spec.modality = "CT";
    spec.sopClassUid = "1.2.840.10008.5.1.4.1.1.2";
    spec.columns = 2;
    spec.rows = 2;
    spec.spacingZ = 1.0;
    const std::array<double, 3> changedAxisY = {{0.0, 0.999, 0.0447101778}};
    if (!writeSlice(directory / "slice-1.dcm", spec, 0, 0.0)) {
        return false;
    }
    return writeSlice(directory / "slice-2.dcm", spec, 1, 1.0, &changedAxisY);
}

bool writeInvalid(const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "invalid";
    std::filesystem::create_directories(directory);
    std::ofstream output(directory / "not-dicom.dcm", std::ios::binary);
    output << "XQ synthetic invalid DICOM sentinel\n";
    return static_cast<bool>(output);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "usage: xq_generate_dicom_fixture <output-root>\n";
        return 2;
    }
    const std::filesystem::path root(argv[1]);
    std::error_code error;
    std::filesystem::create_directories(root, error);
    if (error) {
        std::cerr << "cannot create fixture output root\n";
        return 1;
    }
    if (!writeRegularOblique(root)
        || !writeMultiSeries(root)
        || !writeNonUniform(root)
        || !writeMixedOrientation(root)
        || !writeInvalid(root)) {
        std::cerr << "failed to write synthetic DICOM fixture\n";
        return 1;
    }
    std::cout << "synthetic DICOM fixture generated\n";
    return 0;
}
