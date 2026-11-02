// Runs the plugin's own update-signature check (src/update-verify.cpp) on a digest and signature
// given as hex: exit 0 when it verifies, 1 when it does not, 2 on bad input. run.mjs signs a test
// manifest with the release key and expects a good signature to pass and every altered one to fail.
#include "../../src/update-verify.hpp"
#include <cstdio>
#include <string>
#include <vector>

static bool hex(const char *text, std::vector<unsigned char> &out)
{
  const std::string s(text);
  if (s.size() % 2)
    return false;
  const auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  };
  for (size_t i = 0; i < s.size(); i += 2) {
    const int high = nibble(s[i]);
    const int low = nibble(s[i + 1]);
    if (high < 0 || low < 0)
      return false;
    out.push_back(static_cast<unsigned char>(high * 16 + low));
  }
  return true;
}

int main(int argc, char **argv)
{
  std::vector<unsigned char> digest, signature;
  if (argc != 3 || !hex(argv[1], digest) || !hex(argv[2], signature))
    return 2;
  const bool ok = verifyUpdateDigest(digest.data(), static_cast<unsigned long>(digest.size()), signature.data(),
                                     static_cast<unsigned long>(signature.size()));
  std::puts(ok ? "verified" : "rejected");
  return ok ? 0 : 1;
}
