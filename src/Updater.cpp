#include "Engine/Updater.hpp"

#include "Engine/FileExplorer.hpp"
#include "Engine/Log.hpp"
#include "Game/UpdateMenu.hpp"

#include <algorithm>
#include <chrono>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>

#include <cpr/cpr.h>

namespace
{
constexpr const char* kReleaseApiEndpoint = "https://api.github.com/repos/Renardjojo/PetDesktop/releases/latest";
constexpr std::size_t kMaxDownloadBytes = 1024u * 1024u * 300u; // 300MB hard cap per update payload
constexpr std::size_t kMaxMetadataBytes = 1024u * 1024u; // 1MB metadata payload cap
constexpr std::size_t kMaxReleaseNotesBytes = 1024u * 1024u; // 1MB
constexpr std::size_t kMaxAssetUrlLength = 4096;
constexpr std::size_t kMaxMetadataTextBytes = 128u * 1024u;
constexpr std::size_t kManifestMaxPackageNameLength = 255u;
constexpr std::size_t kManifestMinChecksumLength = 32u;
constexpr std::size_t kManifestMaxChecksumLength = 64u;
constexpr std::array<const char*, 4> kTrustedUpdateHosts = {"github.com", "api.github.com", "objects.githubusercontent.com",
                                                           "github-releases.githubusercontent.com"};

bool containsPathTraversal(std::string_view value)
{
    for (const char c : value)
    {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' ||
            c == '|' )
        {
            return true;
        }
    }

    return false;
}

struct Version
{
    int major = 0;
    int minor = 0;
    int patch = 0;
};

bool parseVersion(const std::string& tag, Version& version)
{
    const auto firstDigit = tag.find_first_of("0123456789");
    if (firstDigit == std::string::npos)
        return false;

    std::istringstream stream{tag.substr(firstDigit)};
    char               dot;
    stream >> version.major >> dot >> version.minor >> dot >> version.patch;
    return static_cast<bool>(stream);
}

bool isGreaterVersion(const std::string& current, const std::string& incoming)
{
    Version currentVersion;
    Version incomingVersion;

    if (!parseVersion(current, currentVersion) || !parseVersion(incoming, incomingVersion))
        return incoming != current;

    if (incomingVersion.major != currentVersion.major)
        return incomingVersion.major > currentVersion.major;

    if (incomingVersion.minor != currentVersion.minor)
        return incomingVersion.minor > currentVersion.minor;

    return incomingVersion.patch > currentVersion.patch;
}

bool parseJsonStringField(const std::string& text, const std::string& key, std::string& value, std::size_t begin = 0)
{
    const std::string pattern = "\"" + key + "\"";
    const auto keyPos         = text.find(pattern, begin);
    if (keyPos == std::string::npos)
        return false;

    const auto colonPos = text.find(':', keyPos);
    if (colonPos == std::string::npos)
        return false;

    auto quotePos = text.find('"', colonPos + 1);
    if (quotePos == std::string::npos)
        return false;

    std::string raw;
    for (std::size_t i = quotePos + 1; i < text.size(); ++i)
    {
        const char c = text[i];
        if (c == '\\')
        {
            if (i + 1 >= text.size())
                return false;

            const char next = text[i + 1];
            switch (next)
            {
            case '"':
                raw.push_back('"');
                ++i;
                break;
            case '\\':
                raw.push_back('\\');
                ++i;
                break;
            case '/':
                raw.push_back('/');
                ++i;
                break;
            case 'b':
                raw.push_back('\b');
                ++i;
                break;
            case 'f':
                raw.push_back('\f');
                ++i;
                break;
            case 'n':
                raw.push_back('\n');
                ++i;
                break;
            case 'r':
                raw.push_back('\r');
                ++i;
                break;
            case 't':
                raw.push_back('\t');
                ++i;
                break;
            case 'u':
                if (i + 5 < text.size())
                {
                    raw.push_back(text[i + 1]);
                    i += 5;
                }
                else
                {
                    return false;
                }
                break;
            default:
                return false;
            }
        }
        else if (c == '"')
        {
            value = raw;
            return true;
        }
        else
        {
            raw.push_back(c);
        }
    }

    return false;
}

bool parseJsonUnsignedField(const std::string& text, const std::string& key, std::uint64_t& value,
                           std::size_t begin = 0)
{
    const std::string pattern = "\"" + key + "\"";
    const auto keyPos         = text.find(pattern, begin);
    if (keyPos == std::string::npos)
        return false;

    const auto colonPos = text.find(':', keyPos);
    if (colonPos == std::string::npos)
        return false;

    std::size_t cursor = colonPos + 1;
    while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor])))
        ++cursor;

    if (cursor >= text.size() || !std::isdigit(static_cast<unsigned char>(text[cursor])))
        return false;

    std::size_t end = cursor;
    while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])))
        ++end;

    try
    {
        const auto valueText = text.substr(cursor, end - cursor);
        if (valueText.size() > 20)
            return false;
        value = std::stoull(valueText);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool isValidHexDigest(const std::string& value)
{
    if (value.empty())
        return false;

    for (unsigned char c : value)
    {
        if (!std::isxdigit(c))
            return false;
    }

    return true;
}

bool isValidSignatureFormat(const std::string& signature)
{
    if (signature.empty())
        return false;

    for (unsigned char c : signature)
    {
        if (std::isspace(c) || c < 33 || c > 126)
            return false;
    }

    return true;
}

std::string toLowerAscii(std::string value)
{
    for (auto& c : value)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return value;
}

bool isTrustedHost(const std::string& host)
{
    if (host.empty())
        return false;

    const std::string hostLower = toLowerAscii(host);
    for (const auto trustedHost : kTrustedUpdateHosts)
    {
        if (hostLower == trustedHost)
            return true;
    }

    return false;
}

bool isTrustedHostForUpdate(const std::string& urlHost)
{
    if (urlHost.empty())
        return false;

    for (const auto& host : kTrustedUpdateHosts)
    {
        if (urlHost == host)
            return true;

        if (urlHost.size() > strlen(host) + 1 && urlHost.compare(urlHost.size() - strlen(host), strlen(host), host) == 0)
        {
            const char preceding = urlHost[urlHost.size() - strlen(host) - 1];
            if (preceding == '.')
                return true;
        }
    }

    return false;
}

bool extractUrlHost(const std::string& url, std::string& host)
{
    const auto schemePos = url.find("://");
    if (schemePos == std::string::npos)
        return false;

    std::string authority = url.substr(schemePos + 3);

    const auto pathPos = authority.find_first_of("/?#");
    if (pathPos != std::string::npos)
        authority = authority.substr(0, pathPos);

    const auto atPos = authority.rfind('@');
    if (atPos != std::string::npos)
        authority = authority.substr(atPos + 1);

    const auto colonPos = authority.find(':');
    host = (colonPos == std::string::npos) ? authority : authority.substr(0, colonPos);

    return !host.empty();
}

std::string extractFilenameFromUrl(const std::string& url)
{
    std::string input = url;
    const auto queryPos = input.find('?');
    if (queryPos != std::string::npos)
        input = input.substr(0, queryPos);

    const auto hashPos = input.find('#');
    if (hashPos != std::string::npos)
        input = input.substr(0, hashPos);

    const auto slashPos = input.find_last_of('/');
    if (slashPos == std::string::npos)
        return input;

    return input.substr(slashPos + 1);
}

bool isHttpsUrl(const std::string& value)
{
    if (value.rfind("https://", 0) != 0)
        return false;

    if (value.size() > kMaxAssetUrlLength)
        return false;

    if (value.find('\n') != std::string::npos || value.find('\r') != std::string::npos ||
        value.find('\t') != std::string::npos)
        return false;

    std::string host;
    if (!extractUrlHost(value, host))
        return false;

    if (!isTrustedHostForUpdate(toLowerAscii(host)))
        return false;

    return true;
}

std::string sanitizeFileName(std::string value)
{
    if (value.empty())
        return std::string("PetForDesktop-Update.bin");

    for (auto& c : value)
    {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' ||
            c == '|' || c == ';')
            c = '_';
    }

    while (!value.empty() && (value[0] == '.' || value[0] == ' '))
        value.erase(value.begin());

    if (value.empty())
        return std::string("PetForDesktop-Update.bin");

    return value;
}

std::string sanitizeMetadataText(std::string value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.erase(value.begin());

    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.pop_back();

    if (value.size() > kManifestMaxPackageNameLength)
        value.resize(kManifestMaxPackageNameLength);

    return value;
}

std::string normalizeHexToken(std::string value)
{
    value.erase(std::remove_if(value.begin(), value.end(), [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }),
                value.end());

    for (auto& c : value)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    return value;
}

bool isValidHexTokenLength(std::string_view value)
{
    if (value.size() < kManifestMinChecksumLength || value.size() > kManifestMaxChecksumLength)
        return false;

    return std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isxdigit(c); });
}

bool parseAssetList(const std::string& releaseJson, std::vector<UpdateAssetMetadata>& assets)
{
    const auto assetsPos = releaseJson.find("\"assets\"");
    if (assetsPos == std::string::npos)
        return false;

    const auto arrayStart = releaseJson.find('[', assetsPos);
    const auto arrayEnd   = releaseJson.find(']', arrayStart);
    if (arrayStart == std::string::npos || arrayEnd == std::string::npos || arrayEnd <= arrayStart)
        return false;

    const std::string block = releaseJson.substr(arrayStart, arrayEnd - arrayStart + 1);

    std::size_t cursor = 0;
    while (cursor < block.size())
    {
        const auto objectStart = block.find('{', cursor);
        if (objectStart == std::string::npos)
            break;

        int depth = 0;
        std::size_t objectEnd = objectStart;
        for (; objectEnd < block.size(); ++objectEnd)
        {
            if (block[objectEnd] == '{')
                ++depth;
            else if (block[objectEnd] == '}')
            {
                --depth;
                if (depth == 0)
                    break;
            }
        }

        if (objectEnd >= block.size() || depth != 0)
            break;

        const std::string item = block.substr(objectStart, objectEnd - objectStart + 1);

        UpdateAssetMetadata asset;
        std::uint64_t     sizeBytes = 0;
        if (!parseJsonStringField(item, "name", asset.name))
        {
            cursor = objectEnd + 1;
            continue;
        }

        if (!parseJsonStringField(item, "browser_download_url", asset.downloadUrl))
        {
            cursor = objectEnd + 1;
            continue;
        }

        if (!isHttpsUrl(asset.downloadUrl))
        {
            cursor = objectEnd + 1;
            continue;
        }

        if (parseJsonUnsignedField(item, "size", sizeBytes))
            asset.sizeBytes = sizeBytes;

        if (asset.sizeBytes > 0)
            asset.size = std::to_string(asset.sizeBytes);
        else
            parseJsonStringField(item, "size", asset.size);

        assets.emplace_back(std::move(asset));
        cursor = objectEnd + 1;
    }

    return !assets.empty();
}

bool parseManifestFromText(const std::string& manifestText, UpdateMetadata& metadata)
{
    metadata = UpdateMetadata{};

    bool hasAny = false;
    bool packageFieldPresent = false;

    std::string value;
    auto parsePackageField = [&](const std::string& fieldValue) -> bool {
        const std::string parsedValue = sanitizeMetadataText(fieldValue);
        if (parsedValue.empty())
            return false;

        packageFieldPresent = true;
        if (!isHttpsUrl(parsedValue))
            return false;

        metadata.packageUrl = parsedValue;
        metadata.packageName = extractFilenameFromUrl(parsedValue);
        if (metadata.packageName.size() > kManifestMaxPackageNameLength)
            metadata.packageName = metadata.packageName.substr(0, kManifestMaxPackageNameLength);

        return true;
    };

    bool hasPackageUrl = false;
    if (parseJsonStringField(manifestText, "package", value))
        hasPackageUrl = parsePackageField(value);
    else if (parseJsonStringField(manifestText, "url", value))
        hasPackageUrl = parsePackageField(value);
    else if (parseJsonStringField(manifestText, "packageUrl", value))
        hasPackageUrl = parsePackageField(value);

    if (packageFieldPresent && !hasPackageUrl)
        return false;

    if (hasPackageUrl)
    {
        hasAny = true;
    }

    if (parseJsonStringField(manifestText, "tag", value))
    {
        metadata.tag = sanitizeMetadataText(value);
        if (!metadata.tag.empty())
            hasAny = true;
    }
    else if (parseJsonStringField(manifestText, "version", value))
    {
        metadata.tag = sanitizeMetadataText(value);
        if (!metadata.tag.empty())
            hasAny = true;
    }

    if (parseJsonStringField(manifestText, "checksum", value) || parseJsonStringField(manifestText, "sha256", value))
    {
        const std::string normalizedChecksum = normalizeHexToken(value);
        if (!normalizedChecksum.empty())
        {
            metadata.checksum = normalizedChecksum;
            if (metadata.checksumAlgorithm.empty())
                metadata.checksumAlgorithm = "sha256";
            hasAny         = true;
        }
    }

    if (parseJsonStringField(manifestText, "signature", value))
    {
        metadata.signature = normalizeHexToken(sanitizeMetadataText(value));
    }

    if (parseJsonStringField(manifestText, "signature_algorithm", value))
    {
        metadata.signatureAlgorithm = toLowerAscii(sanitizeMetadataText(value));
    }

    if (parseJsonStringField(manifestText, "signature_public_key", value))
    {
        metadata.signaturePublicKey = sanitizeMetadataText(value);
    }

    if (parseJsonStringField(manifestText, "checksum_algorithm", value))
    {
        metadata.checksumAlgorithm = toLowerAscii(sanitizeMetadataText(value));
        if (metadata.checksum.empty())
            metadata.checksumAlgorithm.clear();
    }

    std::uint64_t packageSize = 0;
    if (parseJsonUnsignedField(manifestText, "package_size", packageSize))
        metadata.packageSize = packageSize;

    return hasAny;
}

bool downloadToString(const std::string& url, std::string& payload, std::string& error)
{
    if (!isHttpsUrl(url))
    {
        error = "Update URL is not HTTPS";
        return false;
    }

    const auto response = cpr::Get(cpr::Url{url}, cpr::VerifySsl{true}, cpr::Header{{"User-Agent", PROJECT_NAME "/" PROJECT_VERSION}},
                                   cpr::Timeout{20000});
    if (response.error)
    {
        error = response.error.message;
        return false;
    }

    if (response.status_code < 200 || response.status_code >= 300)
    {
        error = "HTTP status " + std::to_string(response.status_code);
        return false;
    }

    if (response.text.size() > kMaxMetadataBytes)
    {
        error = "Metadata response exceeds security cap";
        return false;
    }

    payload = response.text;
    return true;
}

bool downloadToFile(const std::string& url, const std::filesystem::path& stagingPath, std::string& error)
{
    std::string responsePayload;
    if (!downloadToString(url, responsePayload, error))
        return false;

    if (responsePayload.empty())
    {
        error = "Downloaded payload is empty";
        return false;
    }

    if (responsePayload.size() > kMaxDownloadBytes)
    {
        error = "Downloaded payload exceeds update size limit";
        return false;
    }

    std::ofstream output(stagingPath, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        error = "Could not open temporary file for update payload";
        return false;
    }

    output.write(responsePayload.data(), static_cast<std::streamsize>(responsePayload.size()));
    output.close();

    if (!output)
    {
        error = "Could not write update payload";
        return false;
    }

    return true;
}

bool validateMetadataEnvelopeImpl(const UpdateMetadata& metadata, std::string& error)
{
    if (!metadata.tag.empty() && metadata.tag.size() > 64u)
    {
        error = "Metadata tag is too long";
        return false;
    }

    if (metadata.tag.empty())
    {
        error = "Metadata missing release tag";
        return false;
    }

    if (!metadata.packageUrl.empty() && !isHttpsUrl(metadata.packageUrl))
    {
        error = "Package URL is not HTTPS";
        return false;
    }

    if (metadata.packageUrl.empty() && metadata.packageName.empty() && metadata.assets.empty())
    {
        error = "Metadata missing package download target";
        return false;
    }

    if (!metadata.packageUrl.empty() && !isHttpsUrl(metadata.packageUrl))
    {
        error = "Package URL failed HTTPS/host validation";
        return false;
    }

    if (metadata.packageName.size() > kManifestMaxPackageNameLength)
    {
        error = "Metadata package name is too long";
        return false;
    }

    if (containsPathTraversal(metadata.packageName))
    {
        error = "Metadata package name contains invalid path characters";
        return false;
    }

    if (metadata.packageSize > 0 && metadata.packageSize > kMaxDownloadBytes)
    {
        error = "Manifest package size exceeds policy";
        return false;
    }

    if (!metadata.checksum.empty() && !isValidHexTokenLength(metadata.checksum))
    {
        error = "Checksum length or format is invalid";
        return false;
    }

    if (!metadata.checksum.empty())
    {
        const std::string checksumAlgorithm = toLowerAscii(metadata.checksumAlgorithm);
        if (checksumAlgorithm.empty())
        {
            error = "Checksum algorithm missing";
            return false;
        }

        if (checksumAlgorithm != "sha256")
        {
            error = "Unsupported checksum algorithm";
            return false;
        }

        if (!isValidHexDigest(metadata.checksum))
        {
            error = "Checksum contains non-hex characters";
            return false;
        }
    }

    if (!metadata.signature.empty())
    {
        const std::string signatureAlgorithm =
            metadata.signatureAlgorithm.empty() ? std::string("sha256") : metadata.signatureAlgorithm;

        if (signatureAlgorithm != "rsa-sha256" && signatureAlgorithm != "ed25519" && signatureAlgorithm != "sha256")
        {
            error = "Unsupported signature algorithm";
            return false;
        }

        if (!isValidSignatureFormat(metadata.signature))
        {
            error = "Signed metadata uses unsupported signature format";
            return false;
        }

        if (metadata.signature.size() != 64)
        {
            error = "Unsupported signature length";
            return false;
        }

        if (!isValidHexDigest(metadata.signature))
        {
            error = "Signature contains non-hex characters";
            return false;
        }

        if (signatureAlgorithm == "sha256")
        {
            if (!metadata.checksum.empty() && !metadata.checksumAlgorithm.empty() &&
                toLowerAscii(metadata.checksumAlgorithm) != "sha256")
            {
                error = "SHA256 signature requires checksum algorithm sha256";
                return false;
            }
        }
    }

    if (metadata.signature.empty() && metadata.signatureAlgorithm.size() > 0)
    {
        error = "Signature algorithm set without signature";
        return false;
    }

    return true;
}

bool verifySignedMetadataImpl(const UpdateMetadata& metadata, std::string& error)
{
    if (metadata.signature.empty())
        return true;

    if (metadata.signatureAlgorithm.empty())
    {
        error = "Signature algorithm is required";
        return false;
    }

    if (metadata.signatureAlgorithm == "sha256")
    {
        if (metadata.signaturePublicKey.empty() && metadata.checksum.empty())
        {
            error = "SHA256 metadata signature requires checksum";
            return false;
        }

        if (metadata.checksum.empty())
        {
            error = "SHA256 signature requires checksum to validate";
            return false;
        }

        std::string signature = metadata.signature;
        for (auto& c : signature)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        std::string expected = metadata.checksum;
        for (auto& c : expected)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (signature != expected)
        {
            error = "SHA256 signature does not match checksum metadata";
            return false;
        }

        return true;
    }

    error = "Signature verification requires runtime trust-chain support that is not configured in this build";
    return false;
}

class SHA256
{
private:
    uint32_t m_state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab,
                           0x5be0cd19};
    uint64_t m_bitCount = 0;
    uint8_t  m_buffer[64]{};
    uint32_t m_bufferSize = 0;

    static uint32_t rotr(uint32_t value, uint32_t shift)
    {
        return (value >> shift) | (value << (32u - shift));
    }

    static uint32_t ch(uint32_t x, uint32_t y, uint32_t z)
    {
        return (x & y) ^ (~x & z);
    }

    static uint32_t maj(uint32_t x, uint32_t y, uint32_t z)
    {
        return (x & y) ^ (x & z) ^ (y & z);
    }

    static uint32_t bigSig0(uint32_t x)
    {
        return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
    }

    static uint32_t bigSig1(uint32_t x)
    {
        return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
    }

    static uint32_t smallSig0(uint32_t x)
    {
        return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
    }

    static uint32_t smallSig1(uint32_t x)
    {
        return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
    }

    static void pack32(const uint8_t src[4], uint32_t& dst)
    {
        dst = (static_cast<uint32_t>(src[0]) << 24) | (static_cast<uint32_t>(src[1]) << 16) |
              (static_cast<uint32_t>(src[2]) << 8) | (static_cast<uint32_t>(src[3]));
    }

    static void unpack32(uint8_t dst[4], uint32_t src)
    {
        dst[0] = static_cast<uint8_t>((src >> 24) & 0xff);
        dst[1] = static_cast<uint8_t>((src >> 16) & 0xff);
        dst[2] = static_cast<uint8_t>((src >> 8) & 0xff);
        dst[3] = static_cast<uint8_t>(src & 0xff);
    }

    void transform(const uint8_t block[64])
    {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

        uint32_t w[64] = {0};
        for (int i = 0; i < 16; ++i)
            pack32(&block[i * 4], w[i]);

        for (int i = 16; i < 64; ++i)
        {
            const uint32_t t1 = w[i - 16] + smallSig0(w[i - 15]) + w[i - 7] + smallSig1(w[i - 2]);
            w[i]            = t1;
        }

        uint32_t a = m_state[0];
        uint32_t b = m_state[1];
        uint32_t c = m_state[2];
        uint32_t d = m_state[3];
        uint32_t e = m_state[4];
        uint32_t f = m_state[5];
        uint32_t g = m_state[6];
        uint32_t h = m_state[7];

        for (int i = 0; i < 64; ++i)
        {
            const uint32_t t1 = h + bigSig1(e) + ch(e, f, g) + k[i] + w[i];
            const uint32_t t2 = bigSig0(a) + maj(a, b, c);

            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
        m_state[4] += e;
        m_state[5] += f;
        m_state[6] += g;
        m_state[7] += h;
    }

public:
    void update(const uint8_t* data, std::size_t length)
    {
        if (!data || length == 0)
            return;

        m_bitCount += length * 8ULL;

        while (length > 0)
        {
            const std::size_t needed = 64 - m_bufferSize;
            const std::size_t fill   = std::min(needed, length);

            memcpy(m_buffer + m_bufferSize, data, fill);
            data += fill;
            m_bufferSize += static_cast<uint32_t>(fill);
            length -= fill;

            if (m_bufferSize == 64)
            {
                transform(m_buffer);
                m_bufferSize = 0;
            }
        }
    }

    std::string final()
    {
        uint8_t pad[128] = {0};
        pad[0] = 0x80;

        const auto padding = (m_bufferSize < 56) ? (56 - m_bufferSize) : (56 + 64 - m_bufferSize);
        update(pad, padding);

        uint8_t lengthBytes[8];
        const uint64_t bits = m_bitCount;
        lengthBytes[7]      = static_cast<uint8_t>(bits);
        lengthBytes[6]      = static_cast<uint8_t>(bits >> 8);
        lengthBytes[5]      = static_cast<uint8_t>(bits >> 16);
        lengthBytes[4]      = static_cast<uint8_t>(bits >> 24);
        lengthBytes[3]      = static_cast<uint8_t>(bits >> 32);
        lengthBytes[2]      = static_cast<uint8_t>(bits >> 40);
        lengthBytes[1]      = static_cast<uint8_t>(bits >> 48);
        lengthBytes[0]      = static_cast<uint8_t>(bits >> 56);
        update(lengthBytes, 8);

        uint8_t hash[32];
        for (int i = 0; i < 8; ++i)
            unpack32(&hash[i * 4], m_state[i]);

        std::ostringstream out;
        out << std::hex << std::setfill('0') << std::nouppercase;
        for (uint8_t byte : hash)
            out << std::setw(2) << static_cast<int>(byte);

        m_state[0] = 0x6a09e667;
        m_state[1] = 0xbb67ae85;
        m_state[2] = 0x3c6ef372;
        m_state[3] = 0xa54ff53a;
        m_state[4] = 0x510e527f;
        m_state[5] = 0x9b05688c;
        m_state[6] = 0x1f83d9ab;
        m_state[7] = 0x5be0cd19;
        m_bitCount = 0;
        m_bufferSize = 0;

        return out.str();
    }
};

bool verifySha256Checksum(const std::filesystem::path& filePath, const std::string& expectedHash)
{
    std::ifstream input(filePath, std::ios::binary);
    if (!input)
        return false;

    SHA256 hasher;
    std::array<uint8_t, 1 << 15> buffer;
    while (input.good())
    {
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        const auto count = static_cast<std::size_t>(input.gcount());
        if (count > 0)
            hasher.update(buffer.data(), count);
    }

    const auto actual = hasher.final();
    auto normalizedExpected = expectedHash;
    for (auto& c : normalizedExpected)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    return actual == normalizedExpected;
}

bool isChecksumValid(const std::filesystem::path& stagedFile, const UpdateMetadata& metadata)
{
    if (metadata.checksum.empty())
        return false;

    const std::string checksumAlgorithm = toLowerAscii(metadata.checksumAlgorithm);
    if (checksumAlgorithm != "sha256")
    {
        logf("Unsupported checksum algorithm '%s'\n", metadata.checksumAlgorithm.c_str());
        return false;
    }

    if (!verifySha256Checksum(stagedFile, metadata.checksum))
    {
        log("Update checksum verification failed\n");
        return false;
    }

    return true;
}

} // namespace

bool Updater::validateMetadataEnvelope(const UpdateMetadata& metadata, std::string& error) const
{
    return validateMetadataEnvelopeImpl(metadata, error);
}

bool Updater::verifySignedMetadata(const UpdateMetadata& metadata, std::string& error) const
{
    return verifySignedMetadataImpl(metadata, error);
}

bool Updater::isVersionAvailable(const std::string& currentTag, const std::string& remoteTag) const
{
    return isGreaterVersion(currentTag, remoteTag);
}

bool Updater::fetchReleaseMetadata(UpdateMetadata& metadata, std::string& error)
{
    std::string releaseJson;
    if (!downloadToString(kReleaseApiEndpoint, releaseJson, error))
        return false;

    if (!parseJsonStringField(releaseJson, "tag_name", metadata.tag))
    {
        error = "Could not parse release tag";
        return false;
    }

    std::string releaseNotes;
    parseJsonStringField(releaseJson, "body", releaseNotes);
    if (releaseNotes.size() > kMaxReleaseNotesBytes)
        releaseNotes.resize(kMaxReleaseNotesBytes);
    metadata.releaseNotes = releaseNotes;

    parseAssetList(releaseJson, metadata.assets);
    if (metadata.assets.empty())
    {
        error = "No release assets found";
        return false;
    }

    // Optional manifest asset can provide checksum and signature.
    bool manifestResolved = false;
    for (const auto& asset : metadata.assets)
    {
        std::string candidateName = asset.name;
        std::string lowerName = candidateName;
        for (auto& c : lowerName)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (lowerName.find("manifest") != std::string::npos && (lowerName.find(".json") != std::string::npos ||
                                                                lowerName.find(".txt") != std::string::npos))
        {
            std::string manifestText;
            if (downloadToString(asset.downloadUrl, manifestText, error))
            {
                parseManifestFromText(manifestText, metadata);
                manifestResolved = true;
            }
        }
    }

    if (manifestResolved)
    {
        metadata.packageName = sanitizeFileName(metadata.packageName.empty() ? extractFilenameFromUrl(metadata.packageUrl) : metadata.packageName);
    }
    else if (!metadata.packageUrl.empty())
    {
        metadata.packageName = sanitizeFileName(extractFilenameFromUrl(metadata.packageUrl));
    }

    if (!metadata.packageUrl.empty() && metadata.packageName.empty())
        metadata.packageName = sanitizeFileName(extractFilenameFromUrl(metadata.packageUrl));

    if (!validateMetadataEnvelope(metadata, error))
        return false;

    if (!verifySignedMetadata(metadata, error))
        return false;

    return true;
}

bool Updater::resolvePlatformPackage(const UpdateMetadata& metadata, UpdateMetadata& resolved, std::string& error) const
{
    if (!metadata.packageUrl.empty())
    {
        resolved           = metadata;
        resolved.packageUrl = metadata.packageUrl;
        resolved.packageName = sanitizeFileName(!metadata.packageName.empty() ? metadata.packageName : extractFilenameFromUrl(metadata.packageUrl));
        if (containsPathTraversal(resolved.packageName))
        {
            error = "Manifest package name has unsafe path characters";
            return false;
        }

        if (!isHttpsUrl(resolved.packageUrl))
        {
            error = "Manifest package URL failed trust validation";
            return false;
        }

        if (metadata.packageSize > 0 && metadata.packageSize > kMaxDownloadBytes)
        {
            error = "Manifest package exceeds download limit";
            return false;
        }

        if (resolved.packageUrl.empty())
        {
            error = "Manifest package URL is empty";
            return false;
        }
        return true;
    }

    std::vector<UpdateAssetMetadata> candidates;
    for (const auto& asset : metadata.assets)
    {
        std::string lower = asset.name;
        for (auto& c : lower)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (lower.find(".exe") == std::string::npos)
            continue;

        if (asset.sizeBytes > kMaxDownloadBytes)
            continue;

        const auto safeName = sanitizeFileName(asset.name);
        if (containsPathTraversal(safeName) || safeName.empty())
            continue;

        if (!isHttpsUrl(asset.downloadUrl))
            continue;

        UpdateAssetMetadata safeAsset = asset;
        safeAsset.name = safeName;
        candidates.emplace_back(std::move(safeAsset));
    }

    if (candidates.empty())
    {
        error = "No matching release asset for this platform";
        return false;
    }

    std::vector<UpdateAssetMetadata> preferred;
    for (const auto& candidate : candidates)
    {
        std::string lower = candidate.name;
        for (auto& c : lower)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (lower.find("windows") != std::string::npos || lower.find("win") != std::string::npos)
            preferred.push_back(candidate);
    }

    const UpdateAssetMetadata* selected = nullptr;
    if (!preferred.empty())
        selected = &preferred.front();
    else
        selected = &candidates.front();

    if (!selected)
    {
        error = "No candidate package was selected";
        return false;
    }

    if (!isHttpsUrl(selected->downloadUrl))
    {
        error = "Selected asset URL is not trusted";
        return false;
    }

    if (selected->sizeBytes > kMaxDownloadBytes)
    {
        error = "Selected asset exceeds download limit";
        return false;
    }

    resolved = metadata;
    resolved.packageUrl = selected->downloadUrl;
    resolved.packageName = sanitizeFileName(selected->name);
    resolved.packageSize = selected->sizeBytes;

    if (resolved.packageUrl.empty())
    {
        error = "Selected asset missing download URL";
        return false;
    }

    return true;
}

bool Updater::downloadAndStageUpdate(const UpdateMetadata& metadata, std::filesystem::path& stagedFile,
                                    std::string& error) const
{
    const auto stageDir = std::filesystem::temp_directory_path();

    if (metadata.packageUrl.empty() || !isHttpsUrl(metadata.packageUrl))
    {
        error = "Package URL is missing or untrusted";
        return false;
    }

    if (metadata.packageName.empty())
    {
        error = "Package name is empty";
        return false;
    }

    const std::string fileName = metadata.packageName.empty() ? std::string("PetForDesktop-Update.bin") : metadata.packageName;
    const auto        safeName = sanitizeFileName(fileName);
    const std::filesystem::path stagedPath = stageDir / safeName;
    const std::filesystem::path stagedBackup = stagedPath.string() + ".bak";

    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    const std::filesystem::path stagedPartPath = stageDir / (safeName + ".part." + std::to_string(nowMs));

    auto removeQuietly = [&](const std::filesystem::path& path) {
        std::error_code cleanupError;
        std::filesystem::remove(path, cleanupError);
    };

    if (!downloadToFile(metadata.packageUrl, stagedPartPath, error))
    {
        removeQuietly(stagedPartPath);
        return false;
    }

    if (!metadata.packageUrl.empty())
    {
        std::error_code fileSizeError;
        const auto fileSize = std::filesystem::file_size(stagedPartPath, fileSizeError);
        if (!fileSizeError && metadata.packageSize > 0 && fileSize != metadata.packageSize)
        {
            removeQuietly(stagedPartPath);
            error = "Downloaded package size mismatch";
            return false;
        }
    }

    std::error_code renameError;
    const bool hadExisting = std::filesystem::exists(stagedPath);

    if (hadExisting)
    {
        std::error_code backupError;
        std::filesystem::rename(stagedPath, stagedBackup, backupError);
        if (backupError)
        {
            removeQuietly(stagedPartPath);
            error = backupError.message();
            return false;
        }

        std::filesystem::rename(stagedPartPath, stagedPath, renameError);
        if (renameError)
        {
            removeQuietly(stagedPartPath);
            std::error_code restoreError;
            std::filesystem::rename(stagedBackup, stagedPath, restoreError);
            if (restoreError)
                error = restoreError.message() + " | " + renameError.message();
            else
                error = renameError.message();
            return false;
        }

        removeQuietly(stagedBackup);
    }
    else
    {
        std::filesystem::rename(stagedPartPath, stagedPath, renameError);
        if (renameError)
        {
            removeQuietly(stagedPartPath);
            error = renameError.message();
            return false;
        }
    }

    stagedFile = stagedPath;
    logf("Update payload staged in %s\n", stagedFile.string().c_str());
    return true;
}

bool Updater::verifyDownloadedPackage(const std::filesystem::path& stagedFile, const UpdateMetadata& metadata,
                                     std::string& error) const
{
    if (!std::filesystem::exists(stagedFile))
    {
        error = "Staged update file not found";
        return false;
    }

    if (!isChecksumValid(stagedFile, metadata))
    {
        error = "Checksum verification failed";
        return false;
    }

    if (!metadata.signature.empty() && !verifySignedMetadata(metadata, error))
        return false;

    return true;
}

bool Updater::applyPackage(const std::filesystem::path& stagedFile, const UpdateMetadata& metadata,
                          std::string& error) const
{
    (void)metadata;
    if (!std::filesystem::exists(stagedFile))
    {
        error = "Staged update file not found";
        return false;
    }

    SystemOpen(stagedFile.string());
    return true;
}

bool Updater::checkForUpdate(GameData& datas)
{
#ifdef PET_P2_TESTS
    (void)datas;
    return false;
#else
    UpdateMetadata metadata;
    std::string error;

    if (!fetchReleaseMetadata(metadata, error))
    {
        log(("Update check failed: " + error + "\n").c_str());
        return false;
    }

    if (!isVersionAvailable(PROJECT_VERSION, metadata.tag))
    {
        logf("Version %s is up to date\n", PROJECT_VERSION);
        return false;
    }

    if (datas.updateMenu)
        return false;

    UpdateMetadata chosenMetadata;
    if (!resolvePlatformPackage(metadata, chosenMetadata, error))
    {
        log(("Update package selection failed: " + error + "\n").c_str());
        return false;
    }

    Vec2i mainMonitorPosition;
    Vec2i mainMonitorSize;
    datas.monitors.getMainMonitorWorkingArea(mainMonitorPosition, mainMonitorSize);

    Vec2 menuPosition = mainMonitorPosition + mainMonitorSize / 2;
    datas.updateMenu = std::make_unique<UpdateMenu>(
        datas, menuPosition, chosenMetadata,
        [this](GameData& gameData, const UpdateMetadata& packageMetadata) {
            std::string localError;
            std::filesystem::path stagedFile;

            if (!downloadAndStageUpdate(packageMetadata, stagedFile, localError))
            {
                log(("Download staging failed: " + localError + "\n").c_str());
                return false;
            }

            if (!verifyDownloadedPackage(stagedFile, packageMetadata, localError))
            {
                log(("Verification failed: " + localError + "\n").c_str());
                return false;
            }

            if (!applyPackage(stagedFile, packageMetadata, localError))
            {
                log(("Apply update failed: " + localError + "\n").c_str());
                return false;
            }

            return true;
        });

    return true;
#endif
}

bool Updater::parseManifestForTest(const std::string& manifestText, UpdateMetadata& metadata)
{
    return parseManifestFromText(manifestText, metadata);
}

bool Updater::validateMetadataEnvelopeForTest(const UpdateMetadata& metadata, std::string& error)
{
    return validateMetadataEnvelopeImpl(metadata, error);
}

bool Updater::verifySignedMetadataForTest(const UpdateMetadata& metadata, std::string& error)
{
    return verifySignedMetadataImpl(metadata, error);
}
