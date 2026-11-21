import argparse, http.server, threading, subprocess, os, pathlib, json, time
root = pathlib.Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description='Isolated OBS Browser lifecycle and synthetic-cookie probe')
parser.add_argument('--host-root', type=pathlib.Path, default=root/'build/compatibility/32.0.4/obs-host')
parser.add_argument('--expect-parent-crash', action='store_true')
parser.add_argument('--skip-gate', action='store_true', help='skip the #304 script-gate case')
parser.add_argument('--skip-hidden', action='store_true', help='skip the closed-dock case')
args = parser.parse_args()
host = args.host_root.resolve()
build = root / 'build/browser-probe-verified'
exe = build / 'Release/browser-probe.exe'
seen = []
class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        cookie = self.headers.get('Cookie', '')
        seen.append({'path': self.path, 'session': 'roadie_probe_session=ok' in cookie,
                     'persistent': 'roadie_probe_persistent=ok' in cookie})
        self.send_response(200)
        if self.path == '/seed':
            self.send_header('Set-Cookie', 'roadie_probe_session=ok; Path=/; HttpOnly; SameSite=Lax')
            self.send_header('Set-Cookie', 'roadie_probe_persistent=ok; Path=/; Max-Age=86400; HttpOnly; SameSite=Lax')
        self.send_header('Content-Type', 'text/html')
        self.end_headers()
        self.wfile.write(b'<html><head><title>Roadie probe</title></head><body>Roadie isolated browser probe</body></html>')
    def log_message(self, *args): pass
server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
env = dict(os.environ)
env['PATH'] = str(host/'bin/64bit') + os.pathsep + str(host/'obs-plugins/64bit') + os.pathsep + env['PATH']
env['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(host/'bin/64bit/platforms')
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
result = []
for mode in ['preclose', 'parent-delete']:
    profile = build / ('profile-' + mode + '-' + str(time.time_ns()))
    for action in ['seed', 'probe']:
        seen.clear()
        process = subprocess.Popen([str(exe), str(host), str(profile),
            f'http://127.0.0.1:{server.server_port}/{action}', mode], env=env,
            cwd=host/'bin/64bit', stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            startupinfo=startup)
        try:
            output, _ = process.communicate(timeout=25)
        except subprocess.TimeoutExpired:
            subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], capture_output=True)
            output, _ = process.communicate()
        text = output.decode('utf-8', errors='replace')
        log = build / f'{mode}-{action}.log'
        log.write_text(text, encoding='utf-8')
        item = {'mode': mode, 'action': action, 'exitCode': process.returncode,
                'requests': list(seen), 'probeLog': [l for l in text.splitlines() if 'PROBE' in l],
                'log': str(log)}
        result.append(item)
        print(json.dumps(item), flush=True)
# A dock closed when OBS starts. "hidden-control" is the old plugin: no show, so obs-browser never
# makes the browser and the page never loads. "hidden" sends HiddenStart: the page must load and run
# a script while the dock stays closed, and opening the dock afterwards must not load it again.
hidden = {}
for mode in ([] if args.skip_hidden else ['hidden', 'hidden-control']):
    profile = build / ('profile-' + mode + '-' + str(time.time_ns()))
    seen.clear()
    process = subprocess.Popen([str(exe), str(host), str(profile),
        f'http://127.0.0.1:{server.server_port}/hidden', mode], env=env,
        cwd=host/'bin/64bit', stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        startupinfo=startup)
    try:
        output, _ = process.communicate(timeout=30)
    except subprocess.TimeoutExpired:
        subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], capture_output=True)
        output, _ = process.communicate()
    text = output.decode('utf-8', errors='replace')
    log = build / f'{mode}.log'
    log.write_text(text, encoding='utf-8')
    states = {}
    for line in text.splitlines():
        if line.startswith('PROBE hidden closed') or line.startswith('PROBE hidden opened'):
            words = line.split()
            states[words[2]] = {k: int(v) for k, v in (w.split('=') for w in words[3:])}
    item = {'mode': mode, 'exitCode': process.returncode, 'requests': [r['path'] for r in seen],
            'states': states, 'log': str(log)}
    hidden[mode] = item
    result.append(item)
    print(json.dumps(item), flush=True)
# #304: executeJavaScript every 2 ms across a page reload, a URL change, a nested dialog loop and
# closeBrowser. "gate" goes through the plugin's ScriptGate and must exit normally and drop the
# calls while the page has none; "ungated" is the old Windows path, recorded for comparison only
# (the empty-frame instant is a race, so it is not expected to crash on every run).
gate = {}
for mode in ([] if args.skip_gate else ['gate', 'ungated']):
    profile = build / ('profile-' + mode + '-' + str(time.time_ns()))
    seen.clear()
    process = subprocess.Popen([str(exe), str(host), str(profile),
        f'http://127.0.0.1:{server.server_port}/gate', mode], env=env,
        cwd=host/'bin/64bit', stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        startupinfo=startup)
    try:
        output, _ = process.communicate(timeout=40)
    except subprocess.TimeoutExpired:
        subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], capture_output=True)
        output, _ = process.communicate()
    text = output.decode('utf-8', errors='replace')
    log = build / f'{mode}.log'
    log.write_text(text, encoding='utf-8')
    phases = {}
    for line in text.splitlines():
        if line.startswith('PROBE gate phase='):
            fields = dict(part.split('=') for part in line.split()[2:])
            name = fields.pop('phase')
            phases[name] = {k: int(v) for k, v in fields.items()}
    item = {'mode': mode, 'exitCode': process.returncode, 'requests': [r['path'] for r in seen],
            'phases': phases, 'log': str(log)}
    gate[mode] = item
    result.append(item)
    print(json.dumps(item), flush=True)
server.shutdown()
(build/f'results-{time.time_ns()}.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
preclose = [r for r in result if r['mode'] == 'preclose']
assert all(r['exitCode'] == 0 for r in preclose), 'Pre-close failed; inspect the diagnostic logs'
assert any(r['path'] == '/seed' for r in preclose[0]['requests']), 'Seed page did not load'
assert any(r['path'] == '/probe' and r['persistent'] and not r['session']
           for r in preclose[1]['requests']), 'Cookie persistence did not match the expected result'
if args.expect_parent_crash:
    assert all(r['exitCode'] == 0xC0000005 for r in result if r['mode'] == 'parent-delete'), 'Old order did not reproduce the expected access violation'
print('PASS: pre-close completed and only the persistent cookie survived restart')
if 'hidden' in hidden:
    h = hidden['hidden']
    c = hidden['hidden-control']
    assert h['exitCode'] == 0, f"Closed-dock run exited {h['exitCode']:#x}; inspect {h['log']}"
    assert h['states'].get('closed', {}).get('visible') == 0, 'The dock did not stay closed'
    assert h['states']['closed']['titles'] >= 1, 'The page did not load while the dock was closed'
    assert '/hidden-script' in h['requests'], 'A script did not run in the page while the dock was closed'
    assert h['requests'].count('/hidden') == 1, 'Opening the dock loaded the page a second time'
    assert h['states'].get('opened', {}).get('visible') == 1, 'The dock did not open'
    assert c['states'].get('closed', {}).get('titles') == 0 and '/hidden-script' not in c['requests'], \
        'Control: the old plugin loaded a page in a closed dock, so the case no longer shows the bug'
    print('PASS: a closed dock loads its page and runs scripts in it; the old plugin did not')
if 'gate' in gate:
    g = gate['gate']
    p = g['phases']
    assert g['exitCode'] == 0, f"Gated run exited {g['exitCode']:#x}; inspect {g['log']}"
    assert g['requests'].count('/gate') >= 2 and '/gate-next' in g['requests'], 'The page did not reload and navigate'
    for name in ['reload', 'navigate']:
        assert p[name]['urls'] >= 1, f'No urlChanged during {name}'
        assert p[name]['dropped'] > 0, f'No call was dropped while the page loaded ({name})'
        assert p[name]['sent'] > 0, f'Calls did not resume after the page loaded ({name})'
    assert p['dialog']['dropped'] > 0, 'No call was dropped under the nested dialog loop'
    assert p['close']['sent'] == 0 and p['close']['dropped'] > 0, 'A call reached the page after it closed'
    u = gate['ungated']
    print(f"PASS: #304 gate dropped calls while the page had none and OBS's browser kept running "
          f"(ungated control exit code {u['exitCode']:#x})")
