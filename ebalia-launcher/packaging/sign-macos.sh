#!/usr/bin/env bash
set -euo pipefail
: "${EBALIA_SIGN_IDENTITY:?Set a Developer ID Application identity}"
: "${EBALIA_NOTARY_PROFILE:?Set a notarytool keychain profile}"
app_path=${1:?Usage: sign-macos.sh path/to/ebalia-launcher.app}
# macdeployqt signs each bundled dependency as well as the application.
macdeployqt "$app_path" -always-overwrite "-sign-for-notarization=$EBALIA_SIGN_IDENTITY"
archive_path="${app_path%.app}-notarize.zip"
ditto -c -k --keepParent "$app_path" "$archive_path"
xcrun notarytool submit "$archive_path" --keychain-profile "$EBALIA_NOTARY_PROFILE" --wait
xcrun stapler staple "$app_path"
codesign --verify --deep --strict --verbose=2 "$app_path"
spctl --assess --type execute --verbose=2 "$app_path"
