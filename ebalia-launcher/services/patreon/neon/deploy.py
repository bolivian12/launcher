#!/usr/bin/env python3
"""Deploy with private JSON credentials, never in command-line arguments."""
import argparse
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--credentials', required=True, type=Path)
parser.add_argument('--config-dir', required=True)
args = parser.parse_args()
config = json.loads(args.credentials.read_text())
env = os.environ.copy()
for key in ('client_id', 'client_secret', 'campaign_id', 'creator_token', 'creator_refresh', 'webhook_secret', 'public_url'):
    value = config.get(key, '')
    if not value:
        raise SystemExit('Missing configuration: ' + key)
    env['EBALIA_PATREON_' + key.upper()] = str(value)
folder = Path(__file__).resolve().parent
subprocess.run([str(folder / 'node_modules/.bin/neon'), 'deploy', '--project-id', 'damp-darkness-58836387', '--branch', 'br-frosty-lake-b5hsqiwp', '--config-dir', args.config_dir, '--no-analytics', '--no-env-pull', '--update-existing'], cwd=folder, env=env, check=True)
