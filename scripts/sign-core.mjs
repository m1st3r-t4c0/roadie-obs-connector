// Signs one core for the one-click update (roadie docs/specs/obs-plugin.md §24.4, §24.9): writes the
// three files a GitHub release tagged v<version> carries for one platform:
//   roadie-signal-core-<platform>.<ext>, manifest-<platform>.json, manifest-<platform>.sig
// The key is ROADIE_SIGNAL_SIGNING_KEY from the KB's .secrets (base64 PKCS#8 PEM, ECDSA P-256),
// passed in the environment and never printed. The script checks its own signature against the
// public key built into the plugin (src/update-key.hpp) before it writes anything.
//
// Usage: ROADIE_SIGNAL_SIGNING_KEY=... node scripts/sign-core.mjs <version> <platform> <core file> <out dir>
import { createHash, createPrivateKey, createPublicKey, sign, verify } from 'node:crypto';
import { copyFileSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, extname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const [version, platform, core, out] = process.argv.slice(2);
const PLATFORMS = ['windows-x64', 'macos-arm64', 'macos-x64'];
if (!/^\d+\.\d+\.\d+$/.test(version ?? '') || !PLATFORMS.includes(platform) || !core || !out) {
  console.error('usage: node scripts/sign-core.mjs <x.y.z> <windows-x64|macos-arm64|macos-x64> <core file> <out dir>');
  process.exit(2);
}
const encoded = process.env.ROADIE_SIGNAL_SIGNING_KEY;
if (!encoded) {
  console.error('ROADIE_SIGNAL_SIGNING_KEY is not set (it lives in the KB .secrets)');
  process.exit(2);
}

const repo = join(dirname(fileURLToPath(import.meta.url)), '..');
const lock = JSON.parse(readFileSync(join(repo, 'scripts', 'sdk-lock.json'), 'utf8'));
const bytes = readFileSync(core);
const file = `roadie-signal-core-${platform}${extname(core)}`;
// Key order is fixed, and the plugin checks the signature over these exact bytes.
const manifest = Buffer.from(
  `${JSON.stringify(
    {
      version,
      platform,
      file,
      sha256: createHash('sha256').update(bytes).digest('hex'),
      bytes: bytes.length,
      minObs: lock.obsVersion,
    },
    null,
    2,
  )}\n`,
);
const key = createPrivateKey(Buffer.from(encoded, 'base64').toString('utf8'));
const signature = sign('sha256', manifest, { key, dsaEncoding: 'ieee-p1363' });

// The public key the plugin will check against, read from its own source.
const header = readFileSync(join(repo, 'src', 'update-key.hpp'), 'utf8');
const point = Buffer.from((header.match(/0x[0-9a-f]{2}/gi) ?? []).map((h) => Number.parseInt(h, 16)));
if (point.length !== 64) throw new Error('src/update-key.hpp does not hold a 64-byte key');
const builtIn = createPublicKey({
  key: {
    kty: 'EC',
    crv: 'P-256',
    x: point.subarray(0, 32).toString('base64url'),
    y: point.subarray(32).toString('base64url'),
  },
  format: 'jwk',
});
if (!verify('sha256', manifest, { key: builtIn, dsaEncoding: 'ieee-p1363' }, signature)) {
  console.error('The signing key does not match the key built into the plugin. Nothing written.');
  process.exit(1);
}

mkdirSync(out, { recursive: true });
copyFileSync(core, join(out, file));
writeFileSync(join(out, `manifest-${platform}.json`), manifest);
writeFileSync(join(out, `manifest-${platform}.sig`), `${signature.toString('base64')}\n`);
console.log(`signed ${file} (${String(bytes.length)} bytes) for v${version} into ${out}`);
