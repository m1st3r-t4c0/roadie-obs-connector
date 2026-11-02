#include "update-verify.hpp"
#include "update-key.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <vector>
#elif defined(__APPLE__)
#include <Security/Security.h>
#endif

// No Qt and no libobs here, so tests/update-verify can run exactly this code.
bool verifyUpdateDigest(const unsigned char *digest, unsigned long digestSize, const unsigned char *signature,
                        unsigned long signatureSize)
{
  if (digestSize != 32 || signatureSize != 64)
    return false;
#ifdef _WIN32
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0)
    return false;
  std::vector<unsigned char> blob(sizeof(BCRYPT_ECCKEY_BLOB) + kUpdateKey.size());
  auto *header = reinterpret_cast<BCRYPT_ECCKEY_BLOB *>(blob.data());
  header->dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
  header->cbKey = 32;
  std::copy(kUpdateKey.begin(), kUpdateKey.end(), blob.begin() + sizeof(BCRYPT_ECCKEY_BLOB));
  BCRYPT_KEY_HANDLE key = nullptr;
  bool ok = BCryptImportKeyPair(algorithm, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key, blob.data(),
                                static_cast<ULONG>(blob.size()), 0) == 0;
  if (ok)
    ok = BCryptVerifySignature(key, nullptr, const_cast<PUCHAR>(digest), digestSize, const_cast<PUCHAR>(signature),
                               signatureSize, 0) == 0;
  if (key)
    BCryptDestroyKey(key);
  BCryptCloseAlgorithmProvider(algorithm, 0);
  return ok;
#elif defined(__APPLE__)
  unsigned char point[65];
  point[0] = 0x04;
  std::copy(kUpdateKey.begin(), kUpdateKey.end(), point + 1);
  CFDataRef keyData = CFDataCreate(nullptr, point, sizeof(point));
  int bits = 256;
  CFNumberRef size = CFNumberCreate(nullptr, kCFNumberIntType, &bits);
  const void *keys[] = {kSecAttrKeyType, kSecAttrKeyClass, kSecAttrKeySizeInBits};
  const void *values[] = {kSecAttrKeyTypeECSECPrimeRandom, kSecAttrKeyClassPublic, size};
  CFDictionaryRef attributes =
      CFDictionaryCreate(nullptr, keys, values, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  SecKeyRef key = SecKeyCreateWithData(keyData, attributes, nullptr);
  bool ok = false;
  if (key) {
    CFDataRef hashed = CFDataCreate(nullptr, digest, static_cast<CFIndex>(digestSize));
    CFDataRef signedBytes = CFDataCreate(nullptr, signature, static_cast<CFIndex>(signatureSize));
    ok = SecKeyVerifySignature(key, kSecKeyAlgorithmECDSASignatureDigestRFC4754, hashed, signedBytes, nullptr);
    CFRelease(hashed);
    CFRelease(signedBytes);
    CFRelease(key);
  }
  CFRelease(attributes);
  CFRelease(size);
  CFRelease(keyData);
  return ok;
#else
  (void)digest;
  (void)signature;
  return false;
#endif
}
