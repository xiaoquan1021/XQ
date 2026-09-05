#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "io/blob/Sha256.h"
#include "services/image/DicomServiceSupport.h"

#include <gdcmAttribute.h>
#include <gdcmDataSet.h>
#include <gdcmImageReader.h>
#include <gdcmImageWriter.h>
#include <gdcmTag.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string canonicalFrameSeed(const std::string& frameKey,
                               const xq::ImageGeometry& geometry)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17)
        << "XQ-DICOM-DERIVED-FRAME-V1\n"
        << "key=" << frameKey.size() << ":" << frameKey << "\n"
        << "dimensions=" << geometry.dimensions[0] << ","
        << geometry.dimensions[1] << "," << geometry.dimensions[2] << "\n"
        << "spacing=" << geometry.spacing[0] << "," << geometry.spacing[1]
        << "," << geometry.spacing[2] << "\n"
        << "origin=" << geometry.origin[0] << "," << geometry.origin[1]
        << "," << geometry.origin[2] << "\n";
    for (int row = 0; row < 3; ++row) {
        out << "direction" << row << "="
            << geometry.direction[row][0] << ","
            << geometry.direction[row][1] << ","
            << geometry.direction[row][2] << "\n";
    }
    return out.str();
}

int hexadecimalDigit(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    value = static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
    return value >= 'a' && value <= 'f' ? value - 'a' + 10 : -1;
}

std::string decimalFromBytes(const std::array<std::uint8_t, 16>& bytes)
{
    std::string decimal("0");
    for (std::uint8_t byte : bytes) {
        int carry = static_cast<int>(byte);
        for (std::string::reverse_iterator digit = decimal.rbegin();
             digit != decimal.rend(); ++digit) {
            const int value = (*digit - '0') * 256 + carry;
            *digit = static_cast<char>('0' + value % 10);
            carry = value / 10;
        }
        while (carry > 0) {
            decimal.insert(decimal.begin(), static_cast<char>('0' + carry % 10));
            carry /= 10;
        }
    }
    return decimal;
}

bool derivedFrameUid(const std::string& frameKey,
                     const xq::ImageGeometry& geometry,
                     std::string* uid,
                     std::string* seedHash)
{
    if (frameKey.empty() || uid == nullptr || seedHash == nullptr) {
        return false;
    }
    const std::string seed = canonicalFrameSeed(frameKey, geometry);
    *seedHash = xq::Sha256::hashHex(seed.data(), seed.size());
    if (seedHash->size() != 64) {
        return false;
    }

    std::array<std::uint8_t, 16> uuid = {};
    for (std::size_t index = 0; index < uuid.size(); ++index) {
        const int high = hexadecimalDigit((*seedHash)[index * 2]);
        const int low = hexadecimalDigit((*seedHash)[index * 2 + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        uuid[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    uuid[6] = static_cast<std::uint8_t>((uuid[6] & 0x0fU) | 0x50U);
    uuid[8] = static_cast<std::uint8_t>((uuid[8] & 0x3fU) | 0x80U);
    *uid = std::string("2.25.") + decimalFromBytes(uuid);
    return uid->size() <= 64;
}

bool listRegularFiles(const std::filesystem::path& directory,
                      std::vector<std::filesystem::path>* files)
{
    if (files == nullptr) {
        return false;
    }
    files->clear();
    std::error_code error;
    for (std::filesystem::directory_iterator it(directory, error), end;
         !error && it != end; it.increment(error)) {
        std::error_code fileError;
        if (it->is_regular_file(fileError) && !fileError) {
            files->push_back(it->path());
        }
    }
    if (error) {
        return false;
    }
    std::sort(files->begin(), files->end());
    return !files->empty();
}

bool writeNormalizedFile(const std::filesystem::path& source,
                         const std::filesystem::path& destination,
                         const std::string& frameUid)
{
    gdcm::ImageReader reader;
    reader.SetFileName(source.string().c_str());
    if (!reader.Read()) {
        return false;
    }
    gdcm::DataSet& dataSet = reader.GetFile().GetDataSet();
    const gdcm::Tag frameTag(0x0020, 0x0052);
    if (dataSet.FindDataElement(frameTag)) {
        return false;
    }
    gdcm::Attribute<0x0020, 0x0052> frame;
    frame.SetValue(frameUid.c_str());
    dataSet.Insert(frame.GetAsDataElement());

    gdcm::ImageWriter writer;
    writer.SetFile(reader.GetFile());
    writer.SetImage(reader.GetImage());
    writer.SetFileName(destination.string().c_str());
    return writer.Write();
}

void printUsage(const char* executable)
{
    std::fprintf(stderr,
                 "Usage: %s <source-dicom-directory> <new-output-directory> <public-frame-key>\n",
                 executable == nullptr
                     ? "xq_normalize_dicom_frame"
                     : executable);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 4 || argv == nullptr) {
        printUsage(argc > 0 && argv != nullptr ? argv[0] : nullptr);
        return 1;
    }

    const std::filesystem::path sourceDirectory(argv[1]);
    const std::filesystem::path outputDirectory(argv[2]);
    const std::string frameKey(argv[3]);
    std::error_code error;
    if (!std::filesystem::is_directory(sourceDirectory, error) || error
        || frameKey.empty()) {
        std::fprintf(stderr, "normalization input is invalid\n");
        return 2;
    }
    if (std::filesystem::exists(outputDirectory, error) || error) {
        std::fprintf(stderr, "normalization output directory must not already exist\n");
        return 2;
    }

    xq::GdcmItkDicomSeriesReader productionReader;
    const xq::DicomSeriesDiscoveryResult discovery =
        productionReader.discover(sourceDirectory.string());
    if (!discovery.ok() || discovery.series.size() != 1
        || !discovery.series.front().identity.frameOfReferenceUid.empty()) {
        std::fprintf(stderr,
                     "source must contain exactly one series with an absent frame UID\n");
        return 3;
    }
    xq::DicomSeriesReadResult source = productionReader.read(
        sourceDirectory.string(),
        discovery.series.front().identity.seriesInstanceUid);
    if (!source.ok()
        || !source.volume.dicomIdentity().frameOfReferenceUid.empty()) {
        std::fprintf(stderr, "source series could not be decoded safely\n");
        return 3;
    }

    std::string frameUid;
    std::string seedHash;
    if (!derivedFrameUid(frameKey, source.volume.geometry(),
                         &frameUid, &seedHash)) {
        std::fprintf(stderr, "derived frame UID could not be generated\n");
        return 4;
    }
    const std::string sourceBufferHash = xq::Sha256::hashHex(
        source.buffer->bytes().data(), source.buffer->bytes().size());
    const xq::ImageGeometry sourceGeometry = source.volume.geometry();
    const std::string sourceSeriesUid =
        source.volume.dicomIdentity().seriesInstanceUid;
    source.buffer.reset();

    std::vector<std::filesystem::path> files;
    if (!listRegularFiles(sourceDirectory, &files)
        || !std::filesystem::create_directories(outputDirectory, error)
        || error) {
        std::fprintf(stderr, "normalization output could not be prepared\n");
        return 5;
    }
    for (const std::filesystem::path& file : files) {
        if (!writeNormalizedFile(
                file, outputDirectory / file.filename(), frameUid)) {
            std::fprintf(stderr, "a DICOM instance could not be normalized\n");
            return 5;
        }
    }

    xq::GdcmItkDicomSeriesReader verificationReader;
    const xq::DicomSeriesDiscoveryResult normalizedDiscovery =
        verificationReader.discover(outputDirectory.string());
    if (!normalizedDiscovery.ok() || normalizedDiscovery.series.size() != 1
        || normalizedDiscovery.series.front().identity.seriesInstanceUid
            != sourceSeriesUid
        || normalizedDiscovery.series.front().identity.frameOfReferenceUid
            != frameUid) {
        std::fprintf(stderr, "normalized series identity verification failed\n");
        return 6;
    }
    xq::DicomSeriesReadResult normalized = verificationReader.read(
        outputDirectory.string(), sourceSeriesUid);
    if (!normalized.ok()
        || normalized.volume.dicomIdentity().frameOfReferenceUid != frameUid
        || !xq::sameImageGeometry(sourceGeometry, normalized.volume.geometry())) {
        std::fprintf(stderr, "normalized series geometry verification failed\n");
        return 6;
    }
    const std::string normalizedBufferHash = xq::Sha256::hashHex(
        normalized.buffer->bytes().data(), normalized.buffer->bytes().size());
    if (normalizedBufferHash != sourceBufferHash) {
        std::fprintf(stderr, "normalized series decoded pixels changed\n");
        return 6;
    }

    std::printf("normalization.status=ok\n");
    std::printf("normalization.version=xq-dicom-derived-frame-v1\n");
    std::printf("normalization.file_count=%zu\n", files.size());
    std::printf("normalization.frame_seed_sha256=%s\n", seedHash.c_str());
    std::printf("normalization.derived_frame_uid=%s\n", frameUid.c_str());
    std::printf("normalization.series_uid=%s\n", sourceSeriesUid.c_str());
    std::printf("normalization.source_buffer_sha256=%s\n",
                sourceBufferHash.c_str());
    std::printf("normalization.output_buffer_sha256=%s\n",
                normalizedBufferHash.c_str());
    std::printf("normalization.geometry_equal=true\n");
    return 0;
}
