#!/usr/bin/env python3
"""EBALIA Patreon bridge. Run behind HTTPS; credentials stay on this server."""
import hashlib
import hmac
import html
import json
import os
from pathlib import Path
import secrets
import sqlite3
import threading
import time
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.error import HTTPError
from urllib.parse import urlencode, urlparse, parse_qs
from urllib.request import Request, urlopen

API = 'https://www.patreon.com/api/oauth2/v2'


class Database:
    """SQLite for local tests; external PostgreSQL keeps sessions across free-host restarts."""
    def __init__(self, path, url=''):
        self.url, self.connection = url, None
        if not url:
            Path(path).parent.mkdir(parents=True, exist_ok=True)
            fd = os.open(path, os.O_CREAT | os.O_RDWR, 0o600)
            os.close(fd)
            os.chmod(path, 0o600)
            self.connection = sqlite3.connect(path, check_same_thread=False)

    def execute(self, sql, values=()):
        if self.url:
            import psycopg
            if self.connection is None or self.connection.closed or self.connection.broken:
                self.connection = psycopg.connect(self.url, autocommit=True, connect_timeout=10)
            sql = sql.replace('?', '%s')
        return self.connection.execute(sql, values)

    def commit(self):
        if not self.url:
            self.connection.commit()

    def close(self):
        if self.connection:
            self.connection.close()


def api(path, token):
    req = Request(API + path, headers={'Authorization': 'Bearer ' + token, 'User-Agent': 'EBALIA-Launcher-Bridge/4.0'})
    with urlopen(req, timeout=25) as response:
        return json.load(response)


def oauth(form, config):
    body = urlencode(dict(form, client_id=config['client_id'], client_secret=config['client_secret'])).encode()
    req = Request('https://www.patreon.com/api/oauth2/token', data=body,
                  headers={'User-Agent': 'EBALIA-Launcher-Bridge/4.0'})
    with urlopen(req, timeout=25) as response:
        return json.load(response)


def membership(doc, campaign_id):
    user = doc.get('data', {})
    if user.get('type') != 'user':
        raise ValueError('Invalid identity')
    owned = {m['id'] for m in user.get('relationships', {}).get('memberships', {}).get('data', [])}
    included = doc.get('included', [])
    titles = {t['id']: t.get('attributes', {}).get('title', '') for t in included if t.get('type') == 'tier'}
    result = {'name': user.get('attributes', {}).get('full_name', ''), 'active': False, 'tiers': [], 'tier': ''}
    for member in included:
        rel = member.get('relationships', {})
        if (member.get('type') != 'member' or member.get('id') not in owned or
                rel.get('campaign', {}).get('data', {}).get('id') != campaign_id or
                member.get('attributes', {}).get('patron_status') != 'active_patron' or
                (member.get('attributes', {}).get('currently_entitled_amount_cents') or 0) <= 0):
            continue
        result['active'] = True
        result['tiers'] = [t['id'] for t in rel.get('currently_entitled_tiers', {}).get('data', [])]
        result['tier'] = ' · '.join(titles.get(t, '') for t in result['tiers']).strip(' ·')
    return result


def visible_posts(doc, identity):
    """Fail closed for missing access metadata; never send restricted titles as previews."""
    output = []
    for item in doc.get('data', []):
        if item.get('type') != 'post':
            continue
        a = item.get('attributes', {})
        try:
            published = datetime.fromisoformat(a['published_at'].replace('Z', '+00:00'))
            if published.tzinfo is None or published > datetime.now(timezone.utc):
                continue
        except (KeyError, ValueError, TypeError):
            continue
        allowed = a.get('is_public') is True
        required = a.get('tiers')
        if a.get('is_public') is False and identity.get('active') and isinstance(required, list):
            ids = {str(t.get('id', '')) if isinstance(t, dict) else str(t) for t in required}
            # is_paid means per-post billing, not permission to read the post.
            allowed = bool(ids.intersection(identity.get('tiers', [])))
        url = a.get('url', '')
        if url.startswith('/'):
            url = 'https://www.patreon.com' + url
        parsed = urlparse(url)
        if not allowed or parsed.scheme != 'https' or parsed.hostname not in ('patreon.com', 'www.patreon.com'):
            continue
        output.append({'id': item['id'], 'title': a.get('title') or '', 'content': a.get('content') or '',
                       'url': url, 'date': a['published_at'], 'is_public': a.get('is_public') is True})
    return sorted(output, key=lambda p: p['date'], reverse=True)


class Bridge:
    def __init__(self, config, db_path):
        self.config = config
        self.db = Database(db_path, config.get('database_url', ''))
        self.db.execute('CREATE TABLE IF NOT EXISTS sessions (token TEXT PRIMARY KEY, state TEXT UNIQUE, expires DOUBLE PRECISION, access TEXT, refresh TEXT)')
        self.db.execute('CREATE TABLE IF NOT EXISTS creator (id INTEGER PRIMARY KEY, access TEXT, refresh TEXT)')
        self.db.commit()
        self.lock = threading.RLock()
        self.feed_lock = threading.Lock()
        self.cached_posts = None
        self.cached_at = 0
        self.events = threading.Condition()
        self.epoch = secrets.token_hex(8)
        self.revision = 0

    def version(self):
        with self.events:
            return self.epoch + ':' + str(self.revision)

    def wait_for_update(self, after, timeout=25):
        with self.events:
            self.events.wait_for(lambda: after != self.version(), timeout)
            return {'revision': self.version()}

    def webhook(self, body, signature, event):
        secret = self.config.get('webhook_secret', '')
        expected = hmac.new(secret.encode(), body, hashlib.md5).hexdigest()
        if not secret or not hmac.compare_digest(expected, signature):
            raise PermissionError('Invalid webhook signature')
        if event not in ('posts:publish', 'posts:update', 'posts:delete', 'members:create', 'members:update',
                         'members:delete', 'members:pledge:create', 'members:pledge:update', 'members:pledge:delete'):
            raise ValueError('Unsupported event')
        doc = json.loads(body)
        campaign = doc.get('data', {}).get('relationships', {}).get('campaign', {}).get('data', {}).get('id')
        if campaign is not None and campaign != self.config['campaign_id']:
            raise PermissionError('Wrong campaign')
        with self.events:
            self.cached_at = 0
            self.revision += 1
            self.events.notify_all()
        return {'ok': True}

    @staticmethod
    def digest(value):
        return hashlib.sha256(value.encode()).hexdigest()

    def start(self):
        token, state = secrets.token_urlsafe(32), secrets.token_urlsafe(32)
        with self.lock:
            self.db.execute('DELETE FROM sessions WHERE expires < ?', (time.time(),))
            self.db.execute('INSERT INTO sessions VALUES (?, ?, ?, ?, ?)',
                            (self.digest(token), self.digest(state), time.time() + 300, '', ''))
            self.db.commit()
        params = dict(response_type='code', client_id=self.config['client_id'],
                      redirect_uri=self.config['public_url'] + '/callback', scope='identity identity.memberships', state=state)
        return {'session': token, 'auth_url': 'https://www.patreon.com/oauth2/authorize?' + urlencode(params)}

    def callback(self, state, code):
        with self.lock:
            row = self.db.execute('SELECT token FROM sessions WHERE state=? AND expires>?',
                                  (self.digest(state), time.time())).fetchone()
            if not row or not code:
                raise PermissionError('Invalid or expired callback')
            # Consume state before the network call. A replay cannot exchange the same authorization.
            self.db.execute('UPDATE sessions SET state=NULL WHERE token=?', (row[0],))
            self.db.commit()
        tokens = oauth({'grant_type': 'authorization_code', 'code': code,
                        'redirect_uri': self.config['public_url'] + '/callback'}, self.config)
        if not tokens.get('access_token'):
            raise PermissionError('No access token')
        with self.lock:
            self.db.execute('UPDATE sessions SET access=?, refresh=?, expires=? WHERE token=?',
                            (tokens['access_token'], tokens.get('refresh_token', ''), time.time() + 30 * 86400, row[0]))
            self.db.commit()

    def identity(self, token):
        if not token:
            return {'active': False, 'tiers': [], 'name': '', 'tier': ''}
        with self.lock:
            row = self.db.execute('SELECT access, refresh FROM sessions WHERE token=? AND expires>?',
                                  (self.digest(token), time.time())).fetchone()
        if not row:
            raise PermissionError('Session expired')
        if not row[0]:
            return {'pending': True}
        path = '/identity?' + urlencode({'include': 'memberships.currently_entitled_tiers,memberships.campaign',
                                        'fields[user]': 'full_name', 'fields[member]': 'patron_status,currently_entitled_amount_cents', 'fields[tier]': 'title'})
        try:
            doc = api(path, row[0])
        except HTTPError as error:
            if error.code != 401 or not row[1]:
                raise
            tokens = oauth({'grant_type': 'refresh_token', 'refresh_token': row[1]}, self.config)
            access = tokens['access_token']
            with self.lock:
                self.db.execute('UPDATE sessions SET access=?, refresh=? WHERE token=?',
                                (access, tokens.get('refresh_token', row[1]), self.digest(token)))
                self.db.commit()
            doc = api(path, access)
        return membership(doc, self.config['campaign_id'])

    def posts(self):
        with self.feed_lock:
            revision = self.version()
            if self.cached_posts is not None and time.time() - self.cached_at < 30:
                return self.cached_posts
            with self.lock:
                row = self.db.execute('SELECT access, refresh FROM creator WHERE id=1').fetchone()
            access, refresh = row or (self.config['creator_token'], self.config.get('creator_refresh', ''))
            query = urlencode({'fields[post]': 'title,content,url,published_at,is_public,is_paid,tiers', 'sort': '-published_at', 'page[count]': '100'})
            path = '/campaigns/' + self.config['campaign_id'] + '/posts?' + query
            try:
                doc = api(path, access)
            except HTTPError as error:
                if error.code != 401 or not refresh:
                    raise
                tokens = oauth({'grant_type': 'refresh_token', 'refresh_token': refresh}, self.config)
                access = tokens['access_token']
                with self.lock:
                    self.db.execute('INSERT INTO creator VALUES (1, ?, ?) ON CONFLICT (id) DO UPDATE SET access=excluded.access, refresh=excluded.refresh',
                                    (access, tokens.get('refresh_token', refresh)))
                    self.db.commit()
                doc = api(path, access)
            if not isinstance(doc.get('data'), list):
                raise ValueError('Invalid posts response')
            with self.events:
                self.cached_posts, self.cached_at = doc, time.time() if revision == self.version() else 0
            return doc

    def feed(self, token):
        revision = self.version()
        identity = self.identity(token)
        if identity.get('pending'):
            return {'pending': True, 'posts': []}
        return {'identity': identity, 'posts': visible_posts(self.posts(), identity), 'revision': revision,
                'updated_at': datetime.now(timezone.utc).isoformat()}

    def logout(self, token):
        with self.lock:
            self.db.execute('DELETE FROM sessions WHERE token=?', (self.digest(token),))
            self.db.commit()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass  # OAuth codes and session IDs must never enter request logs.

    def send(self, code, value):
        data = json.dumps(value, ensure_ascii=False).encode()
        self.send_response(code)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(data)

    def dispatch(self):
        path = urlparse(self.path)
        token = self.headers.get('Authorization', '').removeprefix('Bearer ')
        bridge = self.server.bridge
        try:
            if self.command == 'GET' and path.path == '/health':
                return self.send(200, {'ok': True})
            if self.command == 'GET' and path.path == '/v1/events':
                return self.send(200, bridge.wait_for_update(parse_qs(path.query).get('after', [''])[0]))
            if self.command == 'POST' and path.path == '/v1/webhook':
                length = int(self.headers.get('Content-Length', '0'))
                if length < 1 or length > 1024 * 1024:
                    return self.send(413, {'error': 'Invalid payload size'})
                return self.send(200, bridge.webhook(self.rfile.read(length), self.headers.get('X-Patreon-Signature', ''),
                                                     self.headers.get('X-Patreon-Event', '')))
            if self.command == 'POST' and path.path == '/v1/login':
                return self.send(200, bridge.start())
            if self.command == 'GET' and path.path == '/callback':
                query = parse_qs(path.query)
                bridge.callback(query.get('state', [''])[0], query.get('code', [''])[0])
                return self.send(200, {'message': 'Return to EBALIA Launcher. Membership will be verified there.'})
            if self.command == 'GET' and path.path == '/v1/feed':
                return self.send(200, bridge.feed(token))
            if self.command == 'POST' and path.path == '/v1/logout':
                bridge.logout(token)
                return self.send(200, {'ok': True})
            self.send(404, {'error': 'Not found'})
        except PermissionError:
            self.send(401, {'error': 'Session expired or callback invalid'})
        except (HTTPError, OSError, ValueError, KeyError, TypeError, sqlite3.Error):
            self.send(502, {'error': 'Patreon is unavailable or the integration needs configuration'})
        except Exception:
            # Database drivers can contain connection credentials in their diagnostics.
            self.send(503, {'error': 'Service temporarily unavailable'})

    do_GET = dispatch
    do_POST = dispatch


def main():
    os.umask(0o077)
    config = {key: os.environ['EBALIA_PATREON_' + key.upper()] for key in
              ('client_id', 'client_secret', 'campaign_id', 'creator_token')}
    config['public_url'] = os.getenv('EBALIA_PATREON_PUBLIC_URL') or os.environ['RENDER_EXTERNAL_URL']
    config['public_url'] = config['public_url'].rstrip('/')
    config['creator_refresh'] = os.getenv('EBALIA_PATREON_CREATOR_REFRESH', '')
    config['webhook_secret'] = os.getenv('EBALIA_PATREON_WEBHOOK_SECRET', '')
    config['database_url'] = os.getenv('DATABASE_URL', '')
    if not config['public_url'].startswith('https://') or not config['campaign_id'].isdigit():
        raise SystemExit('Use an HTTPS public URL and a numeric campaign ID.')
    server = ThreadingHTTPServer((os.getenv('HOST', '127.0.0.1'), int(os.getenv('PORT', '8787'))), Handler)
    server.bridge = Bridge(config, os.getenv('EBALIA_PATREON_DB', './private/sessions.sqlite'))
    print('EBALIA Patreon bridge listening on loopback. Expose through an HTTPS reverse proxy.')
    server.serve_forever()


if __name__ == '__main__':
    main()
