// One-step Roadie connector release (t4c0, 2026-11-08): the core the dock's Update button downloads.
//
//   node scripts/release.mjs prepare    builds from a clean tree, signs the core, and writes
//                                       releases/v<version>/ (core, manifest, signature, ZIP,
//                                       checksums, build report, notes). Changes nothing outside it.
//   node scripts/release.mjs publish    tags the commit holding releases/v<version>/, pushes main and
//                                       the tag, and creates the GitHub release with its files.
//
// The version is CMakeLists.txt's project version: the Roadie connector. The OBS plugin (the loader)
// has its own number there and changes only with a reinstall. Roadie reads the newest release from
// GitHub, so once `publish` finishes, docks offer Update with no Roadie deploy.
//
// prepare needs ROADIE_SIGNAL_SIGNING_KEY (the KB's .secrets), publish needs GITHUB_TOKEN (the KB's
// GITHUB_PERSONAL_ACCESS_TOKEN). Both are read from the environment and never printed or passed as
// arguments. Windows only for now; the Mac release is still made by hand.
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { copyFileSync, cpSync, existsSync, readFileSync, writeFileSync } from 'node:fs';
import { basename, dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const repo = join(dirname(fileURLToPath(import.meta.url)), '..');
const owner = 'm1st3r-t4c0';
const name = 'roadie-obs-connector';
const platform = 'windows-x64';

const git = (...args) => execFileSync('git', ['-C', repo, ...args], { encoding: 'utf8' }).trim();
const fail = (message) => {
  console.error(message);
  process.exit(1);
};

const cmake = readFileSync(join(repo, 'CMakeLists.txt'), 'utf8');
const version = cmake.match(/project\(roadie-signal VERSION (\d+\.\d+\.\d+)/)?.[1];
if (!version) fail('Could not read the connector version from CMakeLists.txt.');
const loader = cmake.match(/set\(ROADIE_SIGNAL_LOADER_VERSION "(\d+\.\d+\.\d+)"\)/)?.[1] ?? 'unknown';
const lock = JSON.parse(readFileSync(join(repo, 'scripts', 'sdk-lock.json'), 'utf8'));
const tag = `v${version}`;
const out = join(repo, 'releases', tag);
const zipName = `roadie-signal-${version}-obs${lock.obsVersion}-${platform}.zip`;

const sha256 = (file) => createHash('sha256').update(readFileSync(file)).digest('hex');

function notesFromChangelog() {
  const changelog = readFileSync(join(repo, 'CHANGELOG.md'), 'utf8').replace(/\r\n/g, '\n');
  const start = changelog.indexOf(`\n## ${version}`);
  if (start < 0) fail(`CHANGELOG.md has no "## ${version}" section. Write it first.`);
  const rest = changelog.slice(start + 1);
  const end = rest.indexOf('\n## ', 3);
  const section = (end < 0 ? rest : rest.slice(0, end)).split('\n').slice(1).join('\n').trim();
  return [
    `# Roadie connector ${version} for Windows`,
    '',
    `Tag: \`${tag}\`. Windows x64, minimum OBS ${lock.obsVersion}. OBS plugin (loader) ${loader}.`,
    '',
    `**On OBS plugin 1.0.0 or newer?** Press **Update** in the Roadie Signal dock when Roadie offers`,
    `${version}, then restart OBS. Nothing to download by hand.`,
    '',
    '## What is new',
    '',
    section,
    '',
    '## Release files',
    '',
    '| File | For |',
    '|---|---|',
    `| \`roadie-signal-core-${platform}.dll\`, \`manifest-${platform}.json\`, \`manifest-${platform}.sig\` | The dock's Update button. The manifest names the core's SHA-256, size and minimum OBS; the signature is ECDSA P-256 by the release key built into the plugin. |`,
    `| \`${zipName}\` | A new install by hand: both DLLs, README, LICENSE, build report. See the [README](../../README.md#install-on-windows). |`,
    '| `SHA256SUMS.txt`, `build-report.json` | Checksums and build record. |',
    '',
  ].join('\n');
}

function prepare() {
  if (!process.env.ROADIE_SIGNAL_SIGNING_KEY) fail('ROADIE_SIGNAL_SIGNING_KEY is not set (it lives in the KB .secrets).');
  if (git('status', '--porcelain')) fail('The tree is not clean. Commit the change first; a release is built from a commit.');
  if (existsSync(out)) fail(`${out} already exists. A released version is never rebuilt; raise the version.`);
  const head = git('rev-parse', 'HEAD');
  execFileSync('powershell', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', join(repo, 'scripts', 'build-windows.ps1')], {
    stdio: 'inherit',
  });
  const stage = join(repo, 'stage', `obs${lock.obsVersion}`);
  const report = JSON.parse(readFileSync(join(stage, 'build-report.json'), 'utf8'));
  if (report.sourceDirty || report.sourceCommit !== head) fail('The build is not from the clean HEAD commit.');
  const core = join(stage, 'data', 'obs-plugins', 'roadie-signal', 'core', 'roadie-signal-core.dll');
  execFileSync(process.execPath, [join(repo, 'scripts', 'sign-core.mjs'), version, platform, core, out], { stdio: 'inherit' });
  cpSync(join(stage, 'obs-plugins'), join(out, 'obs-plugins'), { recursive: true });
  cpSync(join(stage, 'data'), join(out, 'data'), { recursive: true });
  copyFileSync(join(stage, 'build-report.json'), join(out, 'build-report.json'));
  copyFileSync(join(repo, 'build', `roadie-signal-${version}-obs${lock.obsVersion}-minimum-${platform}.zip`), join(out, zipName));
  const sums = [
    ['roadie-signal.dll', join(out, 'obs-plugins', '64bit', 'roadie-signal.dll')],
    ['roadie-signal-core.dll', join(out, 'data', 'obs-plugins', 'roadie-signal', 'core', 'roadie-signal-core.dll')],
    ...[zipName, `roadie-signal-core-${platform}.dll`, `manifest-${platform}.json`, `manifest-${platform}.sig`, 'build-report.json'].map(
      (file) => [file, join(out, file)],
    ),
  ];
  writeFileSync(join(out, 'SHA256SUMS.txt'), sums.map(([label, file]) => `${sha256(file)}  ${label}\r\n`).join(''));
  writeFileSync(join(out, 'README.md'), notesFromChangelog().replace(/\n/g, '\r\n'));
  console.log(`Prepared ${tag} in ${out}. Read README.md there, commit releases/${tag}, then run publish.`);
}

async function github(method, url, body, type = 'application/json') {
  const response = await fetch(url, {
    method,
    headers: {
      Authorization: `Bearer ${process.env.GITHUB_TOKEN}`,
      Accept: 'application/vnd.github+json',
      'User-Agent': 'roadie-connector-release',
      'Content-Type': type,
    },
    body,
  });
  const text = await response.text();
  if (!response.ok) fail(`GitHub ${method} ${url.replace(/\?.*/, '')} answered ${response.status}: ${text.slice(0, 300)}`);
  return text ? JSON.parse(text) : null;
}

async function publish() {
  if (!process.env.GITHUB_TOKEN) fail('GITHUB_TOKEN is not set (the KB .secrets GITHUB_PERSONAL_ACCESS_TOKEN).');
  if (!existsSync(join(out, 'README.md'))) fail(`Run prepare first: ${out} is missing.`);
  if (git('status', '--porcelain')) fail('The tree is not clean. Commit releases/' + tag + ' first.');
  if (git('ls-files', `releases/${tag}`) === '') fail(`releases/${tag} is not committed.`);
  const tags = git('tag', '--list', tag);
  if (!tags) git('tag', '-a', tag, '-m', `Roadie connector ${version} for Windows`);
  // Fast-forward only: a rejected push means main moved; rebase and run publish again.
  git('push', 'origin', 'HEAD:main');
  git('push', 'origin', tag);
  const api = `https://api.github.com/repos/${owner}/${name}`;
  const release = await github(
    'POST',
    `${api}/releases`,
    JSON.stringify({
      tag_name: tag,
      name: `Roadie connector ${version} for Windows`,
      body: readFileSync(join(out, 'README.md'), 'utf8').replace(/\(\.\.\/\.\.\/README\.md/g, `(https://github.com/${owner}/${name}/blob/main/README.md`),
      draft: false,
      prerelease: false,
      make_latest: 'true',
    }),
  );
  const assets = [zipName, `roadie-signal-core-${platform}.dll`, `manifest-${platform}.json`, `manifest-${platform}.sig`, 'SHA256SUMS.txt', 'build-report.json'];
  for (const file of assets) {
    const path = join(out, file);
    const uploaded = await github(
      'POST',
      release.upload_url.replace(/\{.*\}$/, `?name=${encodeURIComponent(basename(file))}`),
      readFileSync(path),
      'application/octet-stream',
    );
    if (uploaded.size !== readFileSync(path).length) fail(`${file} uploaded with the wrong size.`);
    console.log(`uploaded ${file}`);
  }
  // The update itself fetches these three; check them as a dock would.
  for (const file of [`manifest-${platform}.json`, `manifest-${platform}.sig`, `roadie-signal-core-${platform}.dll`]) {
    const response = await fetch(`https://github.com/${owner}/${name}/releases/download/${tag}/${file}`);
    const bytes = Buffer.from(await response.arrayBuffer());
    if (!response.ok || createHash('sha256').update(bytes).digest('hex') !== sha256(join(out, file)))
      fail(`${file} does not download as released.`);
  }
  console.log(`Published ${release.html_url}. Docks on 1.0.0 or newer now offer Update to ${version}.`);
}

const command = process.argv[2];
if (command === 'prepare') prepare();
else if (command === 'publish') await publish();
else {
  console.error('usage: node scripts/release.mjs prepare|publish');
  process.exit(2);
}
