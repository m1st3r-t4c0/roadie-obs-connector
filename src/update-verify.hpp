#pragma once

// 1.0.0 (roadie docs/specs/obs-plugin.md §24.5): checks an ECDSA P-256 signature, raw r||s (64
// bytes), over a SHA-256 digest (32 bytes), against the built-in update key, with the operating
// system's own crypto (Windows CNG, macOS Security). False for anything that does not verify.
bool verifyUpdateDigest(const unsigned char *digest, unsigned long digestSize, const unsigned char *signature,
                        unsigned long signatureSize);
