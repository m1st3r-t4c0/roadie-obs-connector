// Signs a test manifest with the release key and checks the plugin's own verifier (probe.exe) accepts
// it and rejects an altered manifest, an altered signature and a signature by another key.
// Usage: ROADIE_SIGNAL_SIGNING_KEY=<base64 PKCS#8 PEM> node run.mjs <probe.exe>
import { createHash, createPrivateKey, generateKeyPairSync, sign } from 'node:crypto';
import { spawnSync } from 'node:child_process';

const probe = process.argv[2];
const encoded = process.env.ROADIE_SIGNAL_SIGNING_KEY;
if (!probe || !encoded) {
  console.error('usage: ROADIE_SIGNAL_SIGNING_KEY=... node run.mjs <probe.exe>');
  process.exit(2);
}
const key = createPrivateKey(Buffer.from(encoded, 'base64').toString('utf8'));
const manifest = Buffer.from(
  JSON.stringify({ version: '1.0.0', platform: 'windows-x64', file: 'roadie-signal-core-windows-x64.dll' }),
);
const digest = (bytes) => createHash('sha256').update(bytes).digest('hex');
const signed = (bytes, k = key) => sign('sha256', bytes, { key: k, dsaEncoding: 'ieee-p1363' }).toString('hex');
const run = (d, s) => spawnSync(probe, [d, s], { encoding: 'utf8' }).status;

const good = signed(manifest);
const other = generateKeyPairSync('ec', { namedCurve: 'prime256v1' }).privateKey;
const flipped = (good[0] === '0' ? '1' : '0') + good.slice(1);
const cases = [
  ['a manifest signed with the release key', run(digest(manifest), good), 0],
  ['an altered manifest', run(digest(Buffer.concat([manifest, Buffer.from(' ')])), good), 1],
  ['an altered signature', run(digest(manifest), flipped), 1],
  ['a signature by another key', run(digest(manifest), signed(manifest, other)), 1],
];
let failed = 0;
for (const [name, got, want] of cases) {
  const pass = got === want;
  if (!pass) failed += 1;
  console.log(`${pass ? 'PASS' : 'FAIL'} ${name} (exit ${String(got)}, want ${String(want)})`);
}
process.exit(failed ? 1 : 0);
