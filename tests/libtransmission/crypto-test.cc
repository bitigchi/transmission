/*
 * This file Copyright (C) 2013-2014 Mnemosyne LLC
 *
 * It may be used under the GNU GPL versions 2 or 3
 * or any future license endorsed by Mnemosyne LLC.
 *
 */

#include <array>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_set>

#include "transmission.h"

#include "crypto.h"
#include "crypto-utils.h"
#include "utils.h"

#include "crypto-test-ref.h"

#include "gtest/gtest.h"

using namespace std::literals;

TEST(Crypto, torrentHash)
{
    auto a = tr_crypto{};

    auto hash = std::array<uint8_t, SHA_DIGEST_LENGTH>{};
    for (size_t i = 0; i < hash.size(); ++i)
    {
        hash[i] = uint8_t(i);
    }

    tr_cryptoConstruct(&a, nullptr, true);

    EXPECT_FALSE(tr_cryptoHasTorrentHash(&a));
    EXPECT_EQ(nullptr, tr_cryptoGetTorrentHash(&a));

    tr_cryptoSetTorrentHash(&a, hash.data());
    EXPECT_TRUE(tr_cryptoHasTorrentHash(&a));
    EXPECT_NE(nullptr, tr_cryptoGetTorrentHash(&a));
    EXPECT_EQ(0, memcmp(tr_cryptoGetTorrentHash(&a), hash.data(), hash.size()));

    tr_cryptoDestruct(&a);

    for (size_t i = 0; i < hash.size(); ++i)
    {
        hash[i] = uint8_t(i + 1);
    }

    tr_cryptoConstruct(&a, hash.data(), false);

    EXPECT_TRUE(tr_cryptoHasTorrentHash(&a));
    EXPECT_NE(nullptr, tr_cryptoGetTorrentHash(&a));
    EXPECT_EQ(0, memcmp(tr_cryptoGetTorrentHash(&a), hash.data(), hash.size()));

    tr_cryptoSetTorrentHash(&a, nullptr);
    EXPECT_FALSE(tr_cryptoHasTorrentHash(&a));
    EXPECT_EQ(nullptr, tr_cryptoGetTorrentHash(&a));

    tr_cryptoDestruct(&a);
}

TEST(Crypto, encryptDecrypt)
{
    auto hash = std::array<uint8_t, SHA_DIGEST_LENGTH>{};
    for (size_t i = 0; i < hash.size(); ++i)
    {
        hash[i] = uint8_t(i);
    }

    auto a = tr_crypto{};
    tr_cryptoConstruct(&a, hash.data(), false);
    auto b = tr_crypto_{};
    tr_cryptoConstruct_(&b, hash.data(), true);
    auto public_key_length = int{};
    EXPECT_TRUE(tr_cryptoComputeSecret(&a, tr_cryptoGetMyPublicKey_(&b, &public_key_length)));
    EXPECT_TRUE(tr_cryptoComputeSecret_(&b, tr_cryptoGetMyPublicKey(&a, &public_key_length)));

    auto const input1 = std::string{ "test1" };
    auto encrypted1 = std::array<char, 128>{};
    auto decrypted1 = std::array<char, 128>{};

    tr_cryptoEncryptInit(&a);
    tr_cryptoEncrypt(&a, input1.size(), input1.data(), encrypted1.data());
    tr_cryptoDecryptInit_(&b);
    tr_cryptoDecrypt_(&b, input1.size(), encrypted1.data(), decrypted1.data());
    EXPECT_EQ(input1, std::string(decrypted1.data(), input1.size()));

    auto const input2 = std::string{ "@#)C$@)#(*%bvkdjfhwbc039bc4603756VB3)" };
    auto encrypted2 = std::array<char, 128>{};
    auto decrypted2 = std::array<char, 128>{};

    tr_cryptoEncryptInit_(&b);
    tr_cryptoEncrypt_(&b, input2.size(), input2.data(), encrypted2.data());
    tr_cryptoDecryptInit(&a);
    tr_cryptoDecrypt(&a, input2.size(), encrypted2.data(), decrypted2.data());
    EXPECT_EQ(input2, std::string(decrypted2.data(), input2.size()));

    tr_cryptoDestruct_(&b);
    tr_cryptoDestruct(&a);
}

TEST(Crypto, sha1)
{
    auto hash1 = std::array<uint8_t, SHA_DIGEST_LENGTH>{};
    auto hash2 = std::array<uint8_t, SHA_DIGEST_LENGTH>{};

    EXPECT_TRUE(tr_sha1(hash1.data(), "test", 4, nullptr));
    EXPECT_TRUE(tr_sha1_(hash2.data(), "test", 4, nullptr));
    EXPECT_EQ(
        0,
        memcmp(hash1.data(), "\xa9\x4a\x8f\xe5\xcc\xb1\x9b\xa6\x1c\x4c\x08\x73\xd3\x91\xe9\x87\x98\x2f\xbb\xd3", hash1.size()));
    EXPECT_EQ(0, memcmp(hash1.data(), hash2.data(), hash2.size()));

    EXPECT_TRUE(tr_sha1(hash1.data(), "1", 1, "22", 2, "333", 3, nullptr));
    EXPECT_TRUE(tr_sha1_(hash2.data(), "1", 1, "22", 2, "333", 3, nullptr));
    EXPECT_EQ(
        0,
        memcmp(hash1.data(), "\x1f\x74\x64\x8e\x50\xa6\xa6\x70\x8e\xc5\x4a\xb3\x27\xa1\x63\xd5\x53\x6b\x7c\xed", hash1.size()));
    EXPECT_EQ(0, memcmp(hash1.data(), hash2.data(), hash2.size()));

    EXPECT_TRUE(tr_sha1(hash1.data(), "test", 4, nullptr));

    auto const hash3 = tr_makeSha1("test"sv);
    EXPECT_TRUE(hash3);
    EXPECT_EQ("a94a8fe5ccb19ba61c4c0873d391e987982fbbd3"sv, tr_sha1_to_hex(*hash3));

    auto const hash4 = tr_makeSha1("te"sv, "st"sv);
    EXPECT_TRUE(hash4);
    EXPECT_EQ("a94a8fe5ccb19ba61c4c0873d391e987982fbbd3"sv, tr_sha1_to_hex(*hash4));

    auto const hash5 = tr_makeSha1("t"sv, "e"sv, std::string{ "s" }, std::array<char, 1>{ { 't' } });
    EXPECT_TRUE(hash5);
    EXPECT_EQ("a94a8fe5ccb19ba61c4c0873d391e987982fbbd3"sv, tr_sha1_to_hex(*hash5));
}

TEST(Crypto, ssha1)
{
    struct LocalTest
    {
        std::string_view plain_text;
        std::string_view ssha1;
    };

    auto constexpr Tests = std::array<LocalTest, 2>{ {
        { "test"sv, "{15ad0621b259a84d24dcd4e75b09004e98a3627bAMbyRHJy"sv },
        { "QNY)(*#$B)!_X$B !_B#($^!)*&$%CV!#)&$C!@$(P*)"sv, "{10e2d7acbb104d970514a147cd16d51dfa40fb3c0OSwJtOL"sv },
    } };

    auto constexpr HashCount = size_t{ 4 * 1024 };

    for (auto const& [plain_text, ssha1] : Tests)
    {
        auto hashes = std::unordered_set<std::string>{};
        hashes.reserve(HashCount);

        EXPECT_TRUE(tr_ssha1_matches(ssha1, plain_text));
        EXPECT_TRUE(tr_ssha1_matches_(ssha1, plain_text));

        for (size_t j = 0; j < HashCount; ++j)
        {
            auto const hash = (j % 2 == 0) ? tr_ssha1(plain_text) : tr_ssha1_(plain_text);

            // phrase matches each of generated hashes
            EXPECT_TRUE(tr_ssha1_matches(hash, plain_text));
            EXPECT_TRUE(tr_ssha1_matches_(hash, plain_text));

            hashes.insert(hash);
        }

        // confirm all hashes are different
        EXPECT_EQ(HashCount, hashes.size());

        /* exchange two first chars */
        auto phrase = std::string{ plain_text };
        phrase[0] ^= phrase[1];
        phrase[1] ^= phrase[0];
        phrase[0] ^= phrase[1];

        for (auto const& hash : hashes)
        {
            /* changed phrase doesn't match the hashes */
            EXPECT_FALSE(tr_ssha1_matches(hash, phrase));
            EXPECT_FALSE(tr_ssha1_matches_(hash, phrase));
        }
    }

    /* should work with different salt lengths as well */
    EXPECT_TRUE(tr_ssha1_matches("{a94a8fe5ccb19ba61c4c0873d391e987982fbbd3", "test"));
    EXPECT_TRUE(tr_ssha1_matches("{d209a21d3bc4f8fc4f8faf347e69f3def597eb170pySy4ai1ZPMjeU1", "test"));
}

TEST(Crypto, hex)
{
    auto constexpr Hex = std::array<std::string_view, 2>{
        "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3"sv,
        "d209a21d3bc4f8fc4f8faf347e69f3def597eb17"sv,
    };

    for (auto const& hex : Hex)
    {
        auto digest = tr_sha1_digest_t{};
        tr_hex_to_sha1(std::data(digest), std::data(hex));
        auto hex2 = std::array<char, TR_SHA1_DIGEST_LEN * 2 + 1>{};
        tr_sha1_to_hex(std::data(hex2), std::data(digest));
        EXPECT_EQ(hex, std::data(hex2));
    }
}

TEST(Crypto, random)
{
    /* test that tr_rand_int() stays in-bounds */
    for (int i = 0; i < 100000; ++i)
    {
        int const val = tr_rand_int(100);
        EXPECT_LE(0, val);
        EXPECT_LT(val, 100);
    }
}

static bool base64Eq(char const* a, char const* b)
{
    for (;; ++a, ++b)
    {
        while (*a == '\r' || *a == '\n')
        {
            ++a;
        }

        while (*b == '\r' || *b == '\n')
        {
            ++b;
        }

        if (*a == '\0' || *b == '\0' || *a != *b)
        {
            break;
        }
    }

    return *a == *b;
}

TEST(Crypto, base64)
{
    auto len = size_t{};
    auto* out = static_cast<char*>(tr_base64_encode_str("YOYO!", &len));
    EXPECT_EQ(strlen(out), len);
    EXPECT_TRUE(base64Eq("WU9ZTyE=", out));
    auto* in = static_cast<char*>(tr_base64_decode_str(out, &len));
    EXPECT_EQ(decltype(len){ 5 }, len);
    EXPECT_STREQ("YOYO!", in);
    tr_free(in);
    tr_free(out);

    out = static_cast<char*>(tr_base64_encode("", 0, &len));
    EXPECT_EQ(size_t{}, len);
    EXPECT_STREQ("", out);
    tr_free(out);
    out = static_cast<char*>(tr_base64_decode("", 0, &len));
    EXPECT_EQ(0, len);
    EXPECT_STREQ("", out);
    tr_free(out);

    out = static_cast<char*>(tr_base64_encode(nullptr, 0, &len));
    EXPECT_EQ(0, len);
    EXPECT_EQ(nullptr, out);
    out = static_cast<char*>(tr_base64_decode(nullptr, 0, &len));
    EXPECT_EQ(0, len);
    EXPECT_EQ(nullptr, out);

    static auto constexpr MaxBufSize = size_t{ 1024 };
    for (size_t i = 1; i <= MaxBufSize; ++i)
    {
        auto buf = std::array<char, MaxBufSize + 1>{};

        for (size_t j = 0; j < i; ++j)
        {
            buf[j] = char(tr_rand_int_weak(256));
        }

        out = static_cast<char*>(tr_base64_encode(buf.data(), i, &len));
        EXPECT_EQ(strlen(out), len);
        in = static_cast<char*>(tr_base64_decode(out, len, &len));
        EXPECT_EQ(i, len);
        EXPECT_EQ(0, memcmp(in, buf.data(), len));
        tr_free(in);
        tr_free(out);

        for (size_t j = 0; j < i; ++j)
        {
            buf[j] = char(1 + tr_rand_int_weak(255));
        }

        buf[i] = '\0';

        out = static_cast<char*>(tr_base64_encode_str(buf.data(), &len));
        EXPECT_EQ(strlen(out), len);
        in = static_cast<char*>(tr_base64_decode_str(out, &len));
        EXPECT_EQ(i, len);
        EXPECT_STREQ(buf.data(), in);
        tr_free(in);
        tr_free(out);
    }
}
