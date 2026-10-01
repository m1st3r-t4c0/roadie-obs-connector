import argparse, http.server, threading, subprocess, os, pathlib, json, time
root = pathlib.Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description='Isolated OBS Browser lifecycle and synthetic-cookie probe')
parser.add_argument('--host-root', type=pathlib.Path, default=root/'build/compatibility/32.0.4/obs-host')
parser.add_argument('--expect-parent-crash', action='store_true')
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
        self.wfile.write(b'<html><body>Roadie isolated browser probe</body></html>')
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
