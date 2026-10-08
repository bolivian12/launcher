#!/usr/bin/env bash
# Mirrors main to GitGud and creates the GitGud release of a version already published on GitHub.
# Usage: GGTOKEN=<gitgud token> bash ebalia-launcher/scripts/publish-gitgud.sh 1.1.4
# The token is only read from the environment; it is never written to disk or to the git remote.
set -euo pipefail
version=${1:?version, for example 1.1.4}
tag=v$version
: "${GGTOKEN:?set GGTOKEN to your GitGud token}"
gh=$(command -v gh || echo /nix/store/11bv3f47pgrv9hjclnmd12lbc028gqna-gh-2.101.0/bin/gh)
cd "$(git rev-parse --show-toplevel)"
"$gh" release view "$tag" -R ebalia-real/launcher >/dev/null || { echo "$tag is not published on GitHub yet"; exit 1; }

# GitGud keeps the code without the GitHub workflows, as plain files.
git fetch -q gitgud master
index=$(mktemp)
export GIT_INDEX_FILE=$index
git read-tree main
git rm -r -q --cached .github
for f in ebalia-launcher/packaging/sign-macos.sh ebalia-launcher/resources/mascot.png ebalia-launcher/scripts/build-local.sh ebalia-launcher/silence.pixel.png; do git update-index --chmod=-x "$f"; done
tree=$(git write-tree)
unset GIT_INDEX_FILE; rm -f "$index"
commit=$(GIT_AUTHOR_NAME=castigarse GIT_AUTHOR_EMAIL=96686-castigarse@users.noreply.gitgud.io GIT_COMMITTER_NAME=castigarse GIT_COMMITTER_EMAIL=96686-castigarse@users.noreply.gitgud.io \
  git commit-tree "$tree" -p gitgud/master -m "Mirror EBALIA $version (GitHub $(git rev-parse --short main))")
git -c credential.helper= -c 'credential.helper=!f(){ echo username=castigarse; echo "password=$GGTOKEN"; };f' push -q gitgud "$commit:refs/heads/master"
echo "Code mirrored: $commit"

api=https://gitgud.io/api/v4/projects/51367
curl -sf -o /dev/null -X POST -H "PRIVATE-TOKEN: $GGTOKEN" "$api/repository/tags" -d tag_name="$tag" -d ref="$commit" && echo "Tag $tag created"
"$gh" release view "$tag" -R ebalia-real/launcher --json assets,body | python3 -c "
import json,sys;d=json.load(sys.stdin);v='$version'
print(json.dumps({'name':'EBALIA Launcher '+v,'tag_name':'v'+v,'description':d['body'],'assets':{'links':[{'name':a['name'],'url':a['url'],'direct_asset_path':'/'+a['name'],'link_type':'package'} for a in d['assets']]}}))" \
| curl -sf -o /dev/null -X POST -H "PRIVATE-TOKEN: $GGTOKEN" -H "Content-Type: application/json" "$api/releases" --data @- && echo "GitGud release $tag created"
