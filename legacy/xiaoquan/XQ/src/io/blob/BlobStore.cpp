#include "io/blob/BlobStore.h"

#include "io/blob/Sha256.h"
#include "core/io/PathSafety.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <system_error>

namespace xq {

namespace fs = std::filesystem;

namespace {

// --- little-endian scalar writers (explicit per byte; never memcpy) ---------

void appendLE(std::vector<unsigned char>& out, std::uint64_t bits, std::size_t width)
{
    for (std::size_t i = 0; i < width; ++i) {
        out.push_back(static_cast<unsigned char>((bits >> (8 * i)) & 0xFFu));
    }
}

std::uint64_t readLE(const unsigned char* p, std::size_t width)
{
    std::uint64_t bits = 0;
    for (std::size_t i = 0; i < width; ++i) {
        bits |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    }
    return bits;
}

std::uint64_t bitsOfDouble(double v)
{
    std::uint64_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits)); // scalar reinterpret only; bytes emitted explicitly LE below
    return bits;
}

double doubleOfBits(std::uint64_t bits)
{
    double v = 0.0;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

std::uint32_t bitsOfFloat(float v)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    return bits;
}

bool isFloatWidth(BlobElementType t)
{
    return t == BlobElementType::F64 || t == BlobElementType::F32;
}

bool isIntWidth(BlobElementType t)
{
    return t == BlobElementType::I32 || t == BlobElementType::U32 || t == BlobElementType::U64;
}

bool isKnownElementType(BlobElementType t)
{
    switch (t) {
    case BlobElementType::F64:
    case BlobElementType::F32:
    case BlobElementType::U32:
    case BlobElementType::U64:
    case BlobElementType::U8:
    case BlobElementType::I32:
        return true;
    }
    return false;
}

bool checkedMul(std::size_t a, std::size_t b, std::size_t* out)
{
    if (out == nullptr) {
        return false;
    }
    if (a != 0 && b > (std::numeric_limits<std::size_t>::max)() / a) {
        return false;
    }
    *out = a * b;
    return true;
}

bool checkedScalarCount(std::uint64_t elementCount,
                        std::uint16_t components,
                        std::size_t* out)
{
    if (components == 0 || elementCount > (std::numeric_limits<std::size_t>::max)()) {
        return false;
    }
    return checkedMul(static_cast<std::size_t>(elementCount),
                      static_cast<std::size_t>(components),
                      out);
}

bool checkedLogicalByteCount(const BufferRef& ref, std::size_t* out)
{
    if (ref.formatVersion != 1 || ref.endianness != 0 || ref.components == 0
        || !isKnownElementType(ref.elementType)) {
        return false;
    }
    std::size_t scalarCount = 0;
    std::size_t byteCount = 0;
    return checkedScalarCount(ref.elementCount, ref.components, &scalarCount)
        && checkedMul(scalarCount, BlobStore::byteWidth(ref.elementType), &byteCount)
        && ref.byteCount <= (std::numeric_limits<std::size_t>::max)()
        && byteCount == static_cast<std::size_t>(ref.byteCount)
        && ((*out = byteCount), true);
}

} // namespace

std::size_t BlobStore::byteWidth(BlobElementType elementType)
{
    switch (elementType) {
    case BlobElementType::F64:
    case BlobElementType::U64:
        return 8;
    case BlobElementType::F32:
    case BlobElementType::U32:
    case BlobElementType::I32:
        return 4;
    case BlobElementType::U8:
        return 1;
    }
    return 0;
}

// --- encode ----------------------------------------------------------------

std::vector<unsigned char> BlobStore::encode(BlobElementType elementType,
                                             const std::vector<double>& elements)
{
    std::vector<unsigned char> out;
    const std::size_t w = byteWidth(elementType);
    out.reserve(elements.size() * w);
    for (std::size_t i = 0; i < elements.size(); ++i) {
        if (elementType == BlobElementType::F32) {
            appendLE(out, bitsOfFloat(static_cast<float>(elements[i])), 4);
        } else { // F64
            appendLE(out, bitsOfDouble(elements[i]), 8);
        }
    }
    return out;
}

std::vector<unsigned char> BlobStore::encode(BlobElementType elementType,
                                             const std::vector<int>& elements)
{
    std::vector<unsigned char> out;
    const std::size_t w = byteWidth(elementType);
    out.reserve(elements.size() * w);
    for (std::size_t i = 0; i < elements.size(); ++i) {
        // int -> fixed-width two's-complement LE (I32/U32: 4 bytes, U64: 8).
        appendLE(out, static_cast<std::uint64_t>(static_cast<std::int64_t>(elements[i])), w);
    }
    return out;
}

std::vector<unsigned char> BlobStore::encode(BlobElementType /*elementType*/,
                                             const std::vector<std::uint8_t>& elements)
{
    return std::vector<unsigned char>(elements.begin(), elements.end());
}

// --- put -------------------------------------------------------------------

bool BlobStore::put(BlobElementType elementType, std::uint16_t components,
                    std::uint64_t elementCount, const std::vector<double>& elements,
                    BufferRef* out)
{
    if (!isFloatWidth(elementType)) {
        return false;
    }
    std::size_t expected = 0;
    if (!checkedScalarCount(elementCount, components, &expected) || elements.size() != expected) {
        return false;
    }
    return publish(elementType, components, elementCount, encode(elementType, elements), out);
}

bool BlobStore::put(BlobElementType elementType, std::uint16_t components,
                    std::uint64_t elementCount, const std::vector<int>& elements,
                    BufferRef* out)
{
    if (!isIntWidth(elementType)) {
        return false;
    }
    std::size_t expected = 0;
    if (!checkedScalarCount(elementCount, components, &expected) || elements.size() != expected) {
        return false;
    }
    return publish(elementType, components, elementCount, encode(elementType, elements), out);
}

bool BlobStore::put(BlobElementType elementType, std::uint16_t components,
                    std::uint64_t elementCount, const std::vector<std::uint8_t>& elements,
                    BufferRef* out)
{
    if (elementType != BlobElementType::U8) {
        return false;
    }
    std::size_t expected = 0;
    if (!checkedScalarCount(elementCount, components, &expected) || elements.size() != expected) {
        return false;
    }
    return publish(elementType, components, elementCount, encode(elementType, elements), out);
}

bool BlobStore::publish(BlobElementType elementType, std::uint16_t components,
                        std::uint64_t elementCount, const std::vector<unsigned char>& bytes,
                        BufferRef* out) const
{
    const std::string sha = Sha256::hashHex(bytes);

    const std::string prefix = sha.substr(0, 2);
    const std::string relPath = "blobs/" + prefix + "/" + sha + ".bin";

    const fs::path root(rootDir_);
    const fs::path dir = root / "blobs" / prefix;
    const fs::path finalPath = dir / (sha + ".bin");

    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        return false;
    }

    if (!fs::exists(finalPath, ec)) {
        // Write to a sha-scoped temp file in the same directory, flush, then
        // atomically rename into place (D7 atomic publish; dedup-skip above).
        const fs::path tmpPath = dir / (sha + ".bin.tmp");
        {
            std::ofstream os(tmpPath.string(), std::ios::binary | std::ios::trunc);
            if (!os) {
                return false;
            }
            if (!bytes.empty()) {
                os.write(reinterpret_cast<const char*>(&bytes[0]),
                         static_cast<std::streamsize>(bytes.size()));
            }
            os.flush();
            if (!os) {
                return false;
            }
        }
        fs::rename(tmpPath, finalPath, ec);
        if (ec) {
            // Lost a race or rename failed; if the target now exists (another
            // writer published identical content) treat as success, else fail.
            fs::remove(tmpPath, ec);
            if (!fs::exists(finalPath)) {
                return false;
            }
        }
    }

    out->relPath = relPath;
    out->byteCount = bytes.size();
    out->sha256 = sha;
    out->formatVersion = 1;
    out->endianness = 0;
    out->elementType = elementType;
    out->components = components;
    out->elementCount = elementCount;
    return true;
}

// --- get -------------------------------------------------------------------

BlobStore::Status BlobStore::readVerified(const BufferRef& ref,
                                          std::vector<unsigned char>* bytes) const
{
    if (bytes == nullptr || !isConfinedRelativePath(ref.relPath)) {
        return Status::InvalidMetadata;
    }

    std::size_t expectedBytes = 0;
    if (!checkedLogicalByteCount(ref, &expectedBytes)) {
        return Status::InvalidMetadata;
    }

    const fs::path path = fs::path(rootDir_) / ref.relPath;

    if (!isPathWithinRoot(rootDir_, path.string())) {
        return Status::MissingBlob;   // junction/symlink escapes the asset root
    }

    std::error_code ec;
    if (!fs::exists(path, ec) || ec) {
        return Status::MissingBlob;
    }

    const std::uintmax_t fileSize = fs::file_size(path, ec);
    if (ec) {
        return Status::MissingBlob;
    }
    if (fileSize < ref.byteCount) {
        return Status::TruncatedBlob;
    }
    if (fileSize > ref.byteCount) {
        return Status::ByteCountMismatch;
    }
    if (fileSize > (std::numeric_limits<std::size_t>::max)()) {
        return Status::InvalidMetadata;
    }

    std::ifstream is(path.string(), std::ios::binary);
    if (!is) {
        return Status::MissingBlob;
    }
    std::vector<unsigned char> raw(static_cast<std::size_t>(fileSize));
    if (fileSize > 0) {
        is.read(reinterpret_cast<char*>(&raw[0]), static_cast<std::streamsize>(fileSize));
        if (static_cast<std::uintmax_t>(is.gcount()) != fileSize) {
            return Status::TruncatedBlob;
        }
    }

    if (raw.size() < expectedBytes) {
        return Status::TruncatedBlob;
    }

    if (Sha256::hashHex(raw) != ref.sha256) {
        return Status::ChecksumMismatch;
    }

    *bytes = raw;
    return Status::Ok;
}

BlobStore::Status BlobStore::get(const BufferRef& ref, std::vector<double>* out) const
{
    if (out == nullptr || !isFloatWidth(ref.elementType)) {
        return Status::InvalidMetadata;
    }

    std::vector<unsigned char> raw;
    const Status st = readVerified(ref, &raw);
    if (st != Status::Ok) {
        return st;
    }
    const std::size_t w = byteWidth(ref.elementType);
    std::size_t count = 0;
    if (!checkedScalarCount(ref.elementCount, ref.components, &count)) {
        return Status::InvalidMetadata;
    }
    out->clear();
    out->reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint64_t bits = readLE(&raw[i * w], w);
        if (ref.elementType == BlobElementType::F32) {
            float f = 0.0f;
            const std::uint32_t b32 = static_cast<std::uint32_t>(bits);
            std::memcpy(&f, &b32, sizeof(f));
            out->push_back(static_cast<double>(f));
        } else {
            out->push_back(doubleOfBits(bits));
        }
    }
    return Status::Ok;
}

BlobStore::Status BlobStore::get(const BufferRef& ref, std::vector<int>* out) const
{
    if (out == nullptr || !isIntWidth(ref.elementType)) {
        return Status::InvalidMetadata;
    }

    std::vector<unsigned char> raw;
    const Status st = readVerified(ref, &raw);
    if (st != Status::Ok) {
        return st;
    }
    const std::size_t w = byteWidth(ref.elementType);
    std::size_t count = 0;
    if (!checkedScalarCount(ref.elementCount, ref.components, &count)) {
        return Status::InvalidMetadata;
    }
    out->clear();
    out->reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint64_t bits = readLE(&raw[i * w], w);
        // I32: sign-extend the 32-bit two's-complement payload back to int.
        if (w == 4) {
            out->push_back(static_cast<int>(static_cast<std::int32_t>(static_cast<std::uint32_t>(bits))));
        } else {
            out->push_back(static_cast<int>(static_cast<std::int64_t>(bits)));
        }
    }
    return Status::Ok;
}

BlobStore::Status BlobStore::get(const BufferRef& ref, std::vector<std::uint8_t>* out) const
{
    if (out == nullptr || ref.elementType != BlobElementType::U8) {
        return Status::InvalidMetadata;
    }

    std::vector<unsigned char> raw;
    const Status st = readVerified(ref, &raw);
    if (st != Status::Ok) {
        return st;
    }
    std::size_t count = 0;
    if (!checkedScalarCount(ref.elementCount, ref.components, &count)) {
        return Status::InvalidMetadata;
    }
    out->assign(raw.begin(), raw.begin() + static_cast<std::ptrdiff_t>(count));
    return Status::Ok;
}

BlobStore::BlobStore(const std::string& assetRootDir)
    : rootDir_(assetRootDir)
{
}

} // namespace xq
