// The loader's update and rollback rules (roadie docs/specs/obs-plugin.md §24.3), against the real
// loader DLL on a headless libobs, with stand-in cores. Each OBS "start" is one harness process.
// Usage: node run.mjs <harness.exe> <fake-core.dll> <roadie-signal.dll> <OBS bin/64bit>
import { spawnSync } from 'node:child_process';
import {
  copyFileSync,
  existsSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  realpathSync,
  rmSync,
  writeFileSync,
} from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const [harness, fakeCore, loader, obsBin] = process.argv.slice(2);
if (!obsBin) {
  console.error('usage: node run.mjs <harness.exe> <fake-core.dll> <roadie-signal.dll> <OBS bin/64bit>');
  process.exit(2);
}
// The long form: Windows' 8.3 short temp path is not one OBS builds folders under.
const root = realpathSync.native(mkdtempSync(join(tmpdir(), 'roadie-loader-')));
const config = join(root, 'config');
const own = join(config, 'roadie-signal');
const data = join(root, 'data');
const plugin = join(root, 'plugin', 'roadie-signal.dll');
mkdirSync(join(data, 'core'), { recursive: true });
mkdirSync(join(root, 'plugin'), { recursive: true });
copyFileSync(loader, plugin);
copyFileSync(fakeCore, join(data, 'core', 'roadie-signal-core.dll'));
const addCore = (version) => {
  mkdirSync(join(own, 'cores', version), { recursive: true });
  copyFileSync(fakeCore, join(own, 'cores', version, 'roadie-signal-core.dll'));
};
const state = () => JSON.parse(readFileSync(join(own, 'update.json'), 'utf8'));
const setState = (change) => {
  const next = existsSync(join(own, 'update.json')) ? state() : {};
  change(next);
  mkdirSync(own, { recursive: true });
  writeFileSync(join(own, 'update.json'), JSON.stringify(next));
};
const start = (how, crashIn = '') => {
  // Forward slashes, as OBS itself passes its paths: libobs builds folders on them.
  const slash = (p) => p.split('\\').join('/');
  const r = spawnSync(harness, [slash(config), slash(plugin), slash(data), how], {
    encoding: 'utf8',
    env: { ...process.env, PATH: `${obsBin};${process.env.PATH}`, FAKE_CORE_CRASH: crashIn },
    cwd: obsBin,
  });
  const loaded = /core loaded: (.*)/.exec(r.stdout ?? '')?.[1] ?? '';
  return { exit: r.status, core: loaded.includes('cores') ? loaded.split(/[\\/]/).at(-2) : loaded ? 'bundled' : '' };
};

let failed = 0;
const check = (name, ok, detail = '') => {
  if (!ok) failed += 1;
  console.log(`${ok ? 'PASS' : 'FAIL'} ${name}${ok ? '' : ` — ${detail}`}`);
};

try {
  // A fresh install runs the installer's core, and a clean close makes it the last good one.
  let r = start('clean');
  check('a fresh install runs the installer core', r.exit === 0 && r.core === 'bundled', JSON.stringify(r));
  let s = state();
  check('a clean close marks it good', s.lastGood === 'bundled' && !s.starting, JSON.stringify(s));

  // Update pressed: the downloaded core runs on the next start.
  addCore('1.0.1');
  setState((x) => (x.pending = '1.0.1'));
  r = start('clean');
  s = state();
  check('a pending core runs on the next start', r.core === '1.0.1', JSON.stringify(r));
  check('it becomes current and last good', s.current === '1.0.1' && s.lastGood === '1.0.1' && !s.pending, JSON.stringify(s));

  // A core that stops OBS on start: two strikes, then the last good one, and it is put aside.
  addCore('1.0.2');
  setState((x) => (x.pending = '1.0.2'));
  r = start('clean', '1.0.2');
  check('a crashing core takes OBS down on its first start', r.exit === 7, JSON.stringify(r));
  s = state();
  check('that start is left marked as starting', s.starting?.version === '1.0.2' && s.starting.strikes === 0, JSON.stringify(s));
  r = start('clean', '1.0.2');
  check('the second start tries it again', r.exit === 7 && state().starting?.strikes === 1, JSON.stringify(state()));
  r = start('clean', '1.0.2');
  s = state();
  check('the third start runs the last good core', r.exit === 0 && r.core === '1.0.1', JSON.stringify(r));
  check('the crashing core is marked bad', (s.bad ?? []).some((b) => b.version === '1.0.2'), JSON.stringify(s));
  check('the fall-back is noted for the dock', s.rolledBack?.from === '1.0.2' && s.rolledBack?.to === '1.0.1', JSON.stringify(s));
  check('the bad core is never pending again', !s.pending && s.current === '1.0.1', JSON.stringify(s));

  // A bad core named again is skipped.
  setState((x) => (x.pending = '1.0.2'));
  r = start('clean', '1.0.2');
  check('a core marked bad is never run again', r.exit === 0 && r.core === '1.0.1', JSON.stringify(r));

  // A core that crashes OBS after it loaded counts the same way.
  addCore('1.0.3');
  setState((x) => (x.pending = '1.0.3'));
  start('crash');
  start('crash');
  r = start('clean');
  check('a core that crashed OBS later on, twice, is replaced', r.core === '1.0.1', JSON.stringify(r));

  // A core that will not load at all falls back in the same start.
  rmSync(join(own, 'cores', '1.0.1'), { recursive: true, force: true });
  r = start('clean');
  check('a missing core falls back to the installer core at once', r.exit === 0 && r.core === 'bundled', JSON.stringify(r));
} finally {
  rmSync(root, { recursive: true, force: true });
}
process.exit(failed ? 1 : 0);
