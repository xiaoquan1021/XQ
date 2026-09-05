#include "io/blob/Sha256.h"

#include "picosha2.h"

namespace xq {

namespace {

picosha2::hash256_one_by_one& hasher(void* impl)
{
    return *static_cast<picosha2::hash256_one_by_one*>(impl);
}

} // namespace

Sha256::Sha256()
    : impl_(new picosha2::hash256_one_by_one())
    , finalized_(false)
{
}

Sha256::~Sha256()
{
    delete static_cast<picosha2::hash256_one_by_one*>(impl_);
}

void Sha256::update(const void* data, std::size_t byteCount)
{
    const unsigned char* first = static_cast<const unsigned char*>(data);
    hasher(impl_).process(first, first + byteCount);
}

std::string Sha256::finalHex()
{
    if (!finalized_) {
        hasher(impl_).finish();
        finalized_ = true;
    }
    return picosha2::get_hash_hex_string(hasher(impl_));
}

std::string Sha256::hashHex(const void* data, std::size_t byteCount)
{
    Sha256 h;
    h.update(data, byteCount);
    return h.finalHex();
}

std::string Sha256::hashHex(const std::vector<unsigned char>& bytes)
{
    return hashHex(bytes.empty() ? static_cast<const void*>("") : static_cast<const void*>(&bytes[0]),
                   bytes.size());
}

} // namespace xq
