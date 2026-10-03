import hashlib
import hmac
import json
import tempfile
import threading
import time
import unittest
from http.server import ThreadingHTTPServer
from urllib.request import Request, urlopen
from unittest.mock import patch

import server


def post(identifier, **access):
    return {'type': 'post', 'id': identifier, 'attributes': {
        'title': identifier, 'content': 'Contents ' + identifier,
        'published_at': '2026-01-01T00:00:00Z', 'url': '/posts/' + identifier, **access}}


def identity(status='active_patron', amount=500, campaign='42'):
    return {'data': {'type': 'user', 'attributes': {'full_name': 'Test'},
                     'relationships': {'memberships': {'data': [{'id': 'member'}]}}},
            'included': [{'type': 'member', 'id': 'member',
                          'attributes': {'patron_status': status, 'currently_entitled_amount_cents': amount},
                          'relationships': {'campaign': {'data': {'id': campaign}},
                                            'currently_entitled_tiers': {'data': [{'id': 'gold'}]}}},
                         {'type': 'tier', 'id': 'gold', 'attributes': {'title': 'Gold'}}]}


class AccessTests(unittest.TestCase):
    def test_paid_membership_requires_this_campaign_and_current_entitlement(self):
        self.assertTrue(server.membership(identity(), '42')['active'])
        for doc in (identity(amount=0), identity(amount=None), identity(status='declined_patron'),
                    identity(status='former_patron'), identity(campaign='99')):
            self.assertFalse(server.membership(doc, '42')['active'])

    def test_public_and_tier_posts_are_separated_without_private_previews(self):
        doc = {'data': [post('public', is_public=True), post('gold', is_public=False, tiers=['gold']),
                        post('silver', is_public=False, tiers=['silver']), post('unknown'),
                        post('all-paid', is_public=False, is_paid=True, tiers=[])]}
        guest = server.visible_posts(doc, {'active': False})
        self.assertEqual([p['id'] for p in guest], ['public'])
        self.assertTrue(guest[0]['is_public'])
        paid = server.visible_posts(doc, {'active': True, 'tiers': ['gold']})
        self.assertEqual([p['id'] for p in paid], ['public', 'gold'])
        self.assertFalse(paid[1]['is_public'])
        future = post('future', is_public=True)
        future['attributes']['published_at'] = '2999-01-01T00:00:00Z'
        self.assertEqual(server.visible_posts({'data': [future]}, {}), [])


class BridgeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.bridge = server.Bridge({'campaign_id': '42', 'client_id': 'test', 'client_secret': 'local-test-only',
                                     'public_url': 'https://example.test', 'creator_token': 'test',
                                     'webhook_secret': 'webhook-test'}, self.temp.name + '/sessions.sqlite')

    def tearDown(self):
        self.bridge.db.close()
        self.temp.cleanup()

    def test_webhook_wakes_waiting_clients_and_invalidates_cached_posts(self):
        before = self.bridge.version()
        self.bridge.cached_at = time.time()
        result = []
        waiter = threading.Thread(target=lambda: result.append(self.bridge.wait_for_update(before, 2)))
        waiter.start()
        body = json.dumps({'data': {'type': 'post', 'relationships': {'campaign': {'data': {'id': '42'}}}}}).encode()
        digest = hmac.new(b'webhook-test', body, hashlib.md5).hexdigest()
        self.bridge.webhook(body, digest, 'posts:publish')
        waiter.join(1)
        self.assertFalse(waiter.is_alive())
        self.assertNotEqual(result[0]['revision'], before)
        self.assertEqual(self.bridge.cached_at, 0)
        with self.assertRaises(PermissionError):
            self.bridge.webhook(body, 'invalid', 'posts:publish')
        self.assertEqual(self.bridge.version(), result[0]['revision'])

    def test_oauth_callback_is_single_use_and_logout_removes_access(self):
        from urllib.parse import urlparse, parse_qs
        login = self.bridge.start()
        state = parse_qs(urlparse(login['auth_url']).query)['state'][0]
        with patch.object(server, 'oauth', return_value={'access_token': 'access', 'refresh_token': 'refresh'}):
            self.bridge.callback(state, 'code')
            with self.assertRaises(PermissionError):
                self.bridge.callback(state, 'code')
        with patch.object(server, 'api', return_value=identity()):
            self.assertTrue(self.bridge.identity(login['session'])['active'])
        self.bridge.logout(login['session'])
        with self.assertRaises(PermissionError):
            self.bridge.identity(login['session'])

    def test_membership_is_rechecked_before_every_private_feed(self):
        with patch.object(self.bridge, 'identity', side_effect=[{'active': True, 'tiers': ['gold']}, {'active': False}]), \
             patch.object(self.bridge, 'posts', return_value={'data': [post('exclusive', is_public=False, tiers=['gold'])]}):
            self.assertEqual(len(self.bridge.feed('session')['posts']), 1)
            self.assertEqual(self.bridge.feed('session')['posts'], [])

    def test_http_feed_and_live_webhook_notification(self):
        http = ThreadingHTTPServer(('127.0.0.1', 0), server.Handler)
        http.bridge = self.bridge
        runner = threading.Thread(target=http.serve_forever, daemon=True)
        runner.start()
        base = 'http://127.0.0.1:' + str(http.server_port)
        try:
            with patch.object(self.bridge, 'posts', return_value={'data': [post('public', is_public=True), post('private', tiers=['gold'])]}):
                with urlopen(base + '/v1/feed') as response:
                    feed = json.load(response)
                self.assertEqual([p['id'] for p in feed['posts']], ['public'])
            result = []
            def receive():
                with urlopen(base + '/v1/events?after=' + feed['revision'], timeout=3) as response:
                    result.append(json.load(response))
            listener = threading.Thread(target=receive)
            listener.start()
            body = b'{"data":{"type":"post"}}'
            digest = hmac.new(b'webhook-test', body, hashlib.md5).hexdigest()
            req = Request(base + '/v1/webhook', data=body, headers={'X-Patreon-Signature': digest, 'X-Patreon-Event': 'posts:update'})
            with urlopen(req) as response:
                self.assertTrue(json.load(response)['ok'])
            listener.join(2)
            self.assertFalse(listener.is_alive())
            self.assertNotEqual(result[0]['revision'], feed['revision'])
            self.assertEqual(set(result[0]), {'revision'})
        finally:
            http.shutdown()
            http.server_close()
            runner.join()


if __name__ == '__main__':
    unittest.main()
