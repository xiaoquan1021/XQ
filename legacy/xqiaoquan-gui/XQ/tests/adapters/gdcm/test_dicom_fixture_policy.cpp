#include <gdcmByteValue.h>
#include <gdcmDataElement.h>
#include <gdcmDataSet.h>
#include <gdcmFile.h>
#include <gdcmFileMetaInformation.h>
#include <gdcmImageReader.h>
#include <gdcmStringFilter.h>
#include <gdcmTag.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>

namespace {

int fail(const char* message, const std::filesystem::path& path, int line)
{
    std::fprintf(stderr, "FAIL: %s (%s, line %d)\n",
                 message, path.filename().string().c_str(), line);
    return 1;
}

bool isEmpty(const gdcm::DataElement& element)
{
    const gdcm::ByteValue* value = element.GetByteValue();
    return value == nullptr || value->GetLength() == 0;
}

std::string textValue(const gdcm::File& file, const gdcm::Tag& tag)
{
    gdcm::StringFilter filter;
    filter.SetFile(file);
    std::string value = filter.ToString(tag);
    while (!value.empty()
           && (value.back() == ' ' || value.back() == '\0')) {
        value.pop_back();
    }
    return value;
}

bool auditAllowedTags(const gdcm::DataSet& dataSet,
                      const std::set<gdcm::Tag>& allowed,
                      const std::set<gdcm::Tag>& mustBeEmpty,
                      const std::filesystem::path& path)
{
    for (gdcm::DataSet::ConstIterator it = dataSet.Begin(); it != dataSet.End(); ++it) {
        const gdcm::DataElement& element = *it;
        const gdcm::Tag tag = element.GetTag();
        if ((tag.GetGroup() & 1u) != 0u) {
            std::fprintf(stderr, "private tag: (%04x,%04x)\n",
                         tag.GetGroup(), tag.GetElement());
            fail("fixture contains a private tag", path, __LINE__);
            return false;
        }
        if (allowed.find(tag) == allowed.end()) {
            std::fprintf(stderr, "unexpected tag: (%04x,%04x)\n",
                         tag.GetGroup(), tag.GetElement());
            fail("fixture contains a tag outside the technical allowlist", path, __LINE__);
            return false;
        }
        if (mustBeEmpty.find(tag) != mustBeEmpty.end() && !isEmpty(element)) {
            std::fprintf(stderr, "non-empty denied tag: (%04x,%04x)\n",
                         tag.GetGroup(), tag.GetElement());
            fail("fixture contains identifying or free-text content", path, __LINE__);
            return false;
        }
    }
    return true;
}

} // namespace

int main()
{
    const std::filesystem::path root(XQ_DICOM_FIXTURE_ROOT);
    const std::set<std::string> validDirectories = {
        "regular-oblique", "multi-series", "non-uniform", "mixed-orientation"
    };

    const std::set<gdcm::Tag> fileMetaAllowlist = {
        gdcm::Tag(0x0002, 0x0000), gdcm::Tag(0x0002, 0x0001),
        gdcm::Tag(0x0002, 0x0002), gdcm::Tag(0x0002, 0x0003),
        gdcm::Tag(0x0002, 0x0010), gdcm::Tag(0x0002, 0x0012),
        gdcm::Tag(0x0002, 0x0013), gdcm::Tag(0x0002, 0x0016)
    };
    const std::set<gdcm::Tag> dataSetAllowlist = {
        gdcm::Tag(0x0008, 0x0016), gdcm::Tag(0x0008, 0x0018),
        gdcm::Tag(0x0008, 0x0020), gdcm::Tag(0x0008, 0x0030),
        gdcm::Tag(0x0008, 0x0050), gdcm::Tag(0x0008, 0x0060),
        gdcm::Tag(0x0008, 0x0090),
        gdcm::Tag(0x0010, 0x0010), gdcm::Tag(0x0010, 0x0020),
        gdcm::Tag(0x0010, 0x0030), gdcm::Tag(0x0010, 0x0040),
        gdcm::Tag(0x0018, 0x0050), gdcm::Tag(0x0018, 0x0088),
        gdcm::Tag(0x0020, 0x000d), gdcm::Tag(0x0020, 0x000e),
        gdcm::Tag(0x0020, 0x0010), gdcm::Tag(0x0020, 0x0011),
        gdcm::Tag(0x0020, 0x0013), gdcm::Tag(0x0020, 0x0032),
        gdcm::Tag(0x0020, 0x0037), gdcm::Tag(0x0020, 0x0052),
        gdcm::Tag(0x0028, 0x0002), gdcm::Tag(0x0028, 0x0004),
        gdcm::Tag(0x0028, 0x0010), gdcm::Tag(0x0028, 0x0011),
        gdcm::Tag(0x0028, 0x0030), gdcm::Tag(0x0028, 0x0100),
        gdcm::Tag(0x0028, 0x0101), gdcm::Tag(0x0028, 0x0102),
        gdcm::Tag(0x0028, 0x0103), gdcm::Tag(0x0028, 0x1050),
        gdcm::Tag(0x0028, 0x1051), gdcm::Tag(0x0028, 0x1052),
        gdcm::Tag(0x0028, 0x1053), gdcm::Tag(0x0028, 0x1054),
        gdcm::Tag(0x7fe0, 0x0010)
    };
    const std::set<gdcm::Tag> mustBeEmpty = {
        gdcm::Tag(0x0008, 0x0050), gdcm::Tag(0x0008, 0x0090),
        gdcm::Tag(0x0010, 0x0010), gdcm::Tag(0x0010, 0x0020),
        gdcm::Tag(0x0010, 0x0030), gdcm::Tag(0x0010, 0x0040),
        gdcm::Tag(0x0020, 0x0010), gdcm::Tag(0x0020, 0x0011)
    };

    std::size_t validFileCount = 0;
    std::uintmax_t totalBytes = 0;
    for (const std::string& directoryName : validDirectories) {
        const std::filesystem::path directory = root / directoryName;
        for (const std::filesystem::directory_entry& entry
             : std::filesystem::directory_iterator(directory)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".dcm") {
                return fail("valid fixture directory contains an unexpected file",
                            entry.path(), __LINE__);
            }
            ++validFileCount;
            totalBytes += entry.file_size();

            gdcm::ImageReader reader;
            const std::string fileName = entry.path().string();
            reader.SetFileName(fileName.c_str());
            if (!reader.Read()) {
                return fail("fixture is not a readable DICOM image", entry.path(), __LINE__);
            }
            const gdcm::File& file = reader.GetFile();
            if (!auditAllowedTags(file.GetHeader(), fileMetaAllowlist, {}, entry.path())
                || !auditAllowedTags(file.GetDataSet(), dataSetAllowlist,
                                     mustBeEmpty, entry.path())) {
                return 1;
            }
            if (textValue(file, gdcm::Tag(0x0008, 0x0020)) != "20000101"
                || textValue(file, gdcm::Tag(0x0008, 0x0030)) != "000000") {
                return fail("fixture date and time are not fixed synthetic values",
                            entry.path(), __LINE__);
            }
            const std::string studyUid = textValue(file, gdcm::Tag(0x0020, 0x000d));
            const std::string seriesUid = textValue(file, gdcm::Tag(0x0020, 0x000e));
            const std::string frameUid = textValue(file, gdcm::Tag(0x0020, 0x0052));
            const std::string sopUid = textValue(file, gdcm::Tag(0x0008, 0x0018));
            const std::string testUidRoot = "1.2.826.0.1.3680043.10.543.";
            if (studyUid.rfind(testUidRoot, 0) != 0
                || seriesUid.rfind(testUidRoot, 0) != 0
                || frameUid.rfind(testUidRoot, 0) != 0
                || sopUid.rfind(testUidRoot, 0) != 0) {
                return fail("fixture uses an UID outside the fixed synthetic root",
                            entry.path(), __LINE__);
            }
        }
    }

    if (validFileCount != 14 || totalBytes > 64u * 1024u) {
        return fail("fixture file count or total size changed unexpectedly", root, __LINE__);
    }

    const std::filesystem::path invalidPath = root / "invalid" / "not-dicom.dcm";
    std::ifstream invalidInput(invalidPath, std::ios::binary);
    const std::string invalidBytes((std::istreambuf_iterator<char>(invalidInput)),
                                   std::istreambuf_iterator<char>());
    if (invalidBytes != "XQ synthetic invalid DICOM sentinel\n") {
        return fail("invalid fixture sentinel changed", invalidPath, __LINE__);
    }
    gdcm::ImageReader invalidReader;
    const std::string invalidFileName = invalidPath.string();
    invalidReader.SetFileName(invalidFileName.c_str());
    if (invalidReader.Read()) {
        return fail("invalid fixture unexpectedly parses as DICOM", invalidPath, __LINE__);
    }

    std::printf("OK: synthetic DICOM fixture allowlist, privacy and reproducibility policy\n");
    return 0;
}
