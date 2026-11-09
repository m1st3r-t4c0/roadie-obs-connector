// One-step Roadie connector release (t4c0, 2026-11-08): the core the dock's Update button downloads.
//
//   node scripts/release.mjs prepare    builds from a clean tree, signs the core, and writes
//                                       releases/v<version>/ (core, manifest, signature, the two
//                                       ZIPs, checksums, build report, notes). Changes nothing
//                                       outside it.
//   node scripts/release.mjs refresh    rewrites an already prepared version's ZIPs, notes and
//                                       checksums from its committed files. Never rebuilds or
//                                       re-signs: the core and its manifest stay byte for byte.
//   node scripts/release.mjs publish    tags the commit holding releases/v<version>/, pushes main and
//                                       the tag, and creates the GitHub release with its files. On a
//                                       release that already exists it replaces the notes and files.
//
// Every release ships the same way (t4c0, 2026-11-09; KB agent-governance › OBS plugin releases):
//   - two ZIPs, one for OBS 33 and newer and one for OBS 30 to 32, each laid out as the folder it is
//     extracted into;
//   - one set of release notes from the template below, carrying both install sets and both update
//     sets, every time. Only "What's new" changes, and it comes from the CHANGELOG section.
//   - nothing in the notes about test status. `prepare` and `refresh` refuse a CHANGELOG section that
//     has any.
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
import { copyFileSync, cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
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
const tag = `v${version}`;
const out = join(repo, 'releases', tag);

/** The two ZIPs. Each is laid out as the folder it is extracted into. */
const zips = {
  obs33: {
    file: `roadie-signal-${version}-${platform}-obs33-and-newer.zip`,
    label: 'OBS 33 and newer',
    into: 'C:\\ProgramData\\obs-studio\\plugins',
    loader: 'roadie-signal\\roadie-signal.dll',
    core: 'roadie-signal\\data\\core\\roadie-signal-core.dll',
    docs: 'roadie-signal',
  },
  obs30: {
    file: `roadie-signal-${version}-${platform}-obs30-to-32.zip`,
    label: 'OBS 30 to 32',
    into: 'C:\\Program Files\\obs-studio',
    loader: 'obs-plugins\\64bit\\roadie-signal.dll',
    core: 'data\\obs-plugins\\roadie-signal\\core\\roadie-signal-core.dll',
    docs: 'data\\obs-plugins\\roadie-signal',
  },
};
const updateFiles = [`roadie-signal-core-${platform}.dll`, `manifest-${platform}.json`, `manifest-${platform}.sig`];
const assets = [zips.obs33.file, zips.obs30.file, ...updateFiles, 'SHA256SUMS.txt', 'build-report.json'];

const sha256 = (file) => createHash('sha256').update(readFileSync(file)).digest('hex');
const folderOf = (path) => path.slice(0, path.lastIndexOf('\\'));

function whatsNew() {
  const changelog = readFileSync(join(repo, 'CHANGELOG.md'), 'utf8').replace(/\r\n/g, '\n');
  const start = changelog.indexOf(`\n## ${version}`);
  if (start < 0) fail(`CHANGELOG.md has no "## ${version}" section. Write it first.`);
  const rest = changelog.slice(start + 1);
  const end = rest.indexOf('\n## ', 3);
  const section = (end < 0 ? rest : rest.slice(0, end)).split('\n').slice(1).join('\n').trim();
  if (/\b(un)?tested\b|\btest(ing)?\b/i.test(section))
    fail(`CHANGELOG.md's ${version} section talks about testing. Release notes never do; say what changed.`);
  return section;
}

/** The steps for one ZIP, used in the release notes and as INSTALL.txt inside it. */
function installSteps(zip) {
  return [
    `1. Close OBS.`,
    zip === zips.obs33
      ? `2. Open \`${zip.into}\` in File Explorer (paste it into the address bar; create the \`plugins\` folder if it is missing).`
      : `2. Open \`${zip.into}\` in File Explorer.`,
    `3. Extract the ZIP there${zip === zips.obs30 ? ' (Windows asks for administrator permission)' : ''}. It puts \`roadie-signal.dll\` in \`${folderOf(zip.loader)}\` and \`roadie-signal-core.dll\` in \`${folderOf(zip.core)}\`.`,
    `4. Start OBS, open **Docks → Roadie Signal** and log in to Roadie.`,
  ];
}

function notes() {
  const section = whatsNew();
  const zip33 = zips.obs33;
  const zip30 = zips.obs30;
  return [
    `# Roadie connector ${version} for Windows`,
    '',
    `Windows x64 · OBS 30 or newer · OBS plugin ${loader} · Roadie connector ${version}`,
    '',
    `## What's new`,
    '',
    section,
    '',
    '## Update',
    '',
    `**From the dock:** press **Update** in the Roadie Signal dock when Roadie offers ${version}, then restart OBS.`,
    `The Update button works from Roadie connector 1.1.1 and newer.`,
    '',
    `**By hand** (Roadie connector 1.0.0 or 1.1.0): close OBS, download the ZIP for your OBS version below,`,
    `copy \`roadie-signal-core.dll\` from it over the one in this folder, and start OBS. The OBS plugin file stays as it is.`,
    '',
    '| OBS | Copy `roadie-signal-core.dll` to |',
    '|---|---|',
    `| ${zip33.label} | \`${zip33.into}\\${folderOf(zip33.core)}\` |`,
    `| ${zip30.label} | \`${zip30.into}\\${folderOf(zip30.core)}\` |`,
    '',
    '## New install',
    '',
    'Check your OBS version in **Help → About**, then download the ZIP for it under **Assets** (not **Source code**).',
    '',
    `### ${zip33.label}: \`${zip33.file}\``,
    '',
    ...installSteps(zip33),
    '',
    `### ${zip30.label}: \`${zip30.file}\``,
    '',
    ...installSteps(zip30),
    '',
    `**Tools → Roadie Signal…** then shows "OBS plugin ${loader} · Roadie connector ${version}". Never put`,
    '`roadie-signal-core.dll` beside `roadie-signal.dll`: OBS would load it as a second plugin. Portable OBS and',
    'other installs: see the [README](../../README.md#install-on-windows).',
    '',
    '## Release files',
    '',
    '| File | For |',
    '|---|---|',
    `| \`${zip33.file}\` | A new install or a hand update on ${zip33.label}. Extract into \`${zip33.into}\`. |`,
    `| \`${zip30.file}\` | A new install or a hand update on ${zip30.label}. Extract into \`${zip30.into}\`. |`,
    `| ${updateFiles.map((file) => `\`${file}\``).join(', ')} | The dock's Update button. The manifest names the core's SHA-256, size and minimum OBS; the signature is ECDSA P-256 by the release key built into the plugin. |`,
    '| `SHA256SUMS.txt`, `build-report.json` | Checksums and build record. |',
    '',
  ].join('\n');
}

function installText(zip) {
  const text = [
    `Roadie connector ${version} for Windows, ${zip.label}`,
    `OBS plugin ${loader} · Roadie connector ${version}`,
    '',
    'New install:',
    ...installSteps(zip),
    '',
    `Update by hand (Roadie connector 1.0.0 or 1.1.0): close OBS, copy roadie-signal-core.dll from this ZIP`,
    `into ${zip.into}\\${folderOf(zip.core)}, and start OBS.`,
    '',
    `Release notes: https://github.com/${owner}/${name}/releases/tag/${tag}`,
    '',
  ].join('\n');
  return text.replace(/\*\*|`/g, '').replace(/\n/g, '\r\n');
}

/** Writes both ZIPs into `out` from its committed loader and core. */
function packageZips() {
  const loaderDll = join(out, 'obs-plugins', '64bit', 'roadie-signal.dll');
  const coreDll = join(out, 'data', 'obs-plugins', 'roadie-signal', 'core', 'roadie-signal-core.dll');
  for (const file of [loaderDll, coreDll]) if (!existsSync(file)) fail(`${file} is missing.`);
  for (const zip of Object.values(zips)) {
    const stage = mkdtempSync(join(tmpdir(), 'roadie-zip-'));
    const place = (from, to) => {
      const target = join(stage, ...to.split('\\'));
      mkdirSync(dirname(target), { recursive: true });
      if (from) copyFileSync(from, target);
      return target;
    };
    place(loaderDll, zip.loader);
    place(coreDll, zip.core);
    writeFileSync(place(null, `${zip.docs}\\INSTALL.txt`), installText(zip));
    place(join(repo, 'LICENSE'), `${zip.docs}\\LICENSE`);
    const target = join(out, zip.file);
    rmSync(target, { force: true });
    // Windows' own bsdtar: forward-slash entries, which every unzip reads.
    execFileSync('C:\\Windows\\System32\\tar.exe', ['-a', '-cf', target, '-C', stage, ...readdirSync(stage)]);
    rmSync(stage, { recursive: true, force: true });
  }
  // Older layouts: one ZIP for every OBS version.
  for (const file of readdirSync(out)) if (file.endsWith('.zip') && !assets.includes(file)) rmSync(join(out, file));
}

function writeSumsAndNotes() {
  const sums = [
    ['roadie-signal.dll', join(out, 'obs-plugins', '64bit', 'roadie-signal.dll')],
    ['roadie-signal-core.dll', join(out, 'data', 'obs-plugins', 'roadie-signal', 'core', 'roadie-signal-core.dll')],
    ...assets.filter((file) => file !== 'SHA256SUMS.txt').map((file) => [file, join(out, file)]),
  ];
  writeFileSync(join(out, 'SHA256SUMS.txt'), sums.map(([label, file]) => `${sha256(file)}  ${label}\r\n`).join(''));
  writeFileSync(join(out, 'README.md'), notes().replace(/\n/g, '\r\n'));
}

/** The build record without the build script's host-test flag, which release notes never carry. */
function writeBuildReport(from) {
  const report = JSON.parse(readFileSync(from, 'utf8').replace(/^\uFEFF/, ''));
  delete report.hostTested;
  writeFileSync(join(out, 'build-report.json'), JSON.stringify(report, null, 2) + '\r\n');
}

function prepare() {
  if (!process.env.ROADIE_SIGNAL_SIGNING_KEY) fail('ROADIE_SIGNAL_SIGNING_KEY is not set (it lives in the KB .secrets).');
  if (git('status', '--porcelain')) fail('The tree is not clean. Commit the change first; a release is built from a commit.');
  if (existsSync(out)) fail(`${out} already exists. A released version is never rebuilt; raise the version.`);
  whatsNew();
  const head = git('rev-parse', 'HEAD');
  execFileSync('powershell', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', join(repo, 'scripts', 'build-windows.ps1')], {
    stdio: 'inherit',
  });
  const lock = JSON.parse(readFileSync(join(repo, 'scripts', 'sdk-lock.json'), 'utf8'));
  const stage = join(repo, 'stage', `obs${lock.obsVersion}`);
  const report = JSON.parse(readFileSync(join(stage, 'build-report.json'), 'utf8').replace(/^\uFEFF/, ''));
  if (report.sourceDirty || report.sourceCommit !== head) fail('The build is not from the clean HEAD commit.');
  const core = join(stage, 'data', 'obs-plugins', 'roadie-signal', 'core', 'roadie-signal-core.dll');
  execFileSync(process.execPath, [join(repo, 'scripts', 'sign-core.mjs'), version, platform, core, out], { stdio: 'inherit' });
  cpSync(join(stage, 'obs-plugins'), join(out, 'obs-plugins'), { recursive: true });
  cpSync(join(stage, 'data'), join(out, 'data'), { recursive: true });
  writeBuildReport(join(stage, 'build-report.json'));
  packageZips();
  writeSumsAndNotes();
  console.log(`Prepared ${tag} in ${out}. Read README.md there, commit releases/${tag}, then run publish.`);
}

function refresh() {
  if (!existsSync(join(out, `manifest-${platform}.json`))) fail(`${out} has no prepared release to refresh.`);
  const core = join(out, `roadie-signal-core-${platform}.dll`);
  const manifest = JSON.parse(readFileSync(join(out, `manifest-${platform}.json`), 'utf8'));
  if (!JSON.stringify(manifest).includes(sha256(core))) fail('The core no longer matches its signed manifest.');
  writeBuildReport(join(out, 'build-report.json'));
  packageZips();
  writeSumsAndNotes();
  console.log(`Refreshed ${tag}: ZIPs, notes and checksums. The core, manifest and signature are unchanged.`);
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
  if (response.status === 404 && method === 'GET') return null;
  if (!response.ok) fail(`GitHub ${method} ${url.replace(/\?.*/, '')} answered ${response.status}: ${text.slice(0, 300)}`);
  return text ? JSON.parse(text) : null;
}

async function publish() {
  if (!process.env.GITHUB_TOKEN) fail('GITHUB_TOKEN is not set (the KB .secrets GITHUB_PERSONAL_ACCESS_TOKEN).');
  if (!existsSync(join(out, 'README.md'))) fail(`Run prepare first: ${out} is missing.`);
  for (const file of assets) if (!existsSync(join(out, file))) fail(`${out} has no ${file}. Run refresh.`);
  if (git('status', '--porcelain')) fail('The tree is not clean. Commit releases/' + tag + ' first.');
  if (git('ls-files', `releases/${tag}`) === '') fail(`releases/${tag} is not committed.`);
  const tags = git('tag', '--list', tag);
  if (!tags) git('tag', '-a', tag, '-m', `Roadie connector ${version} for Windows`);
  // Fast-forward only: a rejected push means main moved; rebase and run publish again.
  git('push', 'origin', 'HEAD:main');
  if (!tags) git('push', 'origin', tag);
  const api = `https://api.github.com/repos/${owner}/${name}`;
  const fields = {
    name: `Roadie connector ${version} for Windows`,
    body: readFileSync(join(out, 'README.md'), 'utf8').replace(/\(\.\.\/\.\.\/README\.md/g, `(https://github.com/${owner}/${name}/blob/main/README.md`),
  };
  const existing = await github('GET', `${api}/releases/tags/${tag}`);
  const release = existing
    ? await github('PATCH', `${api}/releases/${existing.id}`, JSON.stringify(fields))
    : await github('POST', `${api}/releases`, JSON.stringify({ tag_name: tag, ...fields, draft: false, prerelease: false, make_latest: 'true' }));
  // On an existing release, every changed file is replaced and anything no longer shipped is removed.
  // An update file that is already there byte for byte stays, so Update never sees it missing.
  const kept = new Set();
  for (const asset of existing?.assets ?? []) {
    const path = join(out, asset.name);
    if (updateFiles.includes(asset.name) && existsSync(path) && asset.size === readFileSync(path).length) {
      kept.add(asset.name);
      continue;
    }
    await github('DELETE', `${api}/releases/assets/${asset.id}`);
    console.log(`removed ${asset.name}`);
  }
  for (const file of assets.filter((file) => !kept.has(file))) {
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
  for (const file of updateFiles) {
    const response = await fetch(`https://github.com/${owner}/${name}/releases/download/${tag}/${file}`);
    const bytes = Buffer.from(await response.arrayBuffer());
    if (!response.ok || createHash('sha256').update(bytes).digest('hex') !== sha256(join(out, file)))
      fail(`${file} does not download as released.`);
  }
  console.log(`Published ${release.html_url}. Docks on connector 1.1.1 or newer offer Update to ${version}.`);
}

const command = process.argv[2];
if (command === 'prepare') prepare();
else if (command === 'refresh') refresh();
else if (command === 'publish') await publish();
else {
  console.error('usage: node scripts/release.mjs prepare|refresh|publish');
  process.exit(2);
}
