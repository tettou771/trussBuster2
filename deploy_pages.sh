#!/bin/bash
# Deploy the web build to gh-pages with mobile-fullscreen patches.
#
# The generated shell (bin/trussBuster2.html) is overwritten on every web
# build, so fullscreen/PWA tweaks are injected here at deploy time:
#   - apple-mobile-web-app-capable etc.: "Add to Home Screen" on iPhone runs
#     the game standalone (no Safari chrome) — iPhone has no Fullscreen API
#   - viewport-fit=cover: extend into the notch area
#   - a small fullscreen button for Android / desktop browsers
set -euo pipefail
cd "$(dirname "$0")"

PAGES_DIR=${1:-/tmp/tb2_pages}

[ -f bin/trussBuster2.html ] || { echo "run a web build first (trusscli build --web)"; exit 1; }

cp bin/trussBuster2.js bin/trussBuster2.wasm bin/trussBuster2.data "$PAGES_DIR/"
# Cache-bust: asset filenames are fixed (trussBuster2.js/.wasm/.data), so without
# a per-deploy version query the browser / Pages CDN serve the stale build.
export TB_VER=$(date +%Y%m%d%H%M%S)
python3 - "$PAGES_DIR/index.html" <<'EOF'
import sys, os
ver = os.environ['TB_VER']
src = open('bin/trussBuster2.html').read()

# Browser tab title (emscripten leaves the {{{ TITLE }}} placeholder unfilled).
src = src.replace('{{{ TITLE }}}', 'TRUSS BUSTER')

# Version the loader script and the wasm/data it fetches (locateFile).
src = src.replace('src=trussBuster2.js', 'src=trussBuster2.js?v=' + ver, 1)
src = src.replace('var Module={canvas:canvasElement,',
                  'var Module={locateFile:function(p){return p+"?v=%s"},canvas:canvasElement,' % ver, 1)

# PWA / notch metas (iPhone standalone fullscreen via Add to Home Screen)
metas = ('<meta name=apple-mobile-web-app-capable content=yes>'
         '<meta name=mobile-web-app-capable content=yes>'
         '<meta name=apple-mobile-web-app-status-bar-style content=black-translucent>'
         '<meta name=theme-color content="#0b0a17">')
src = src.replace('name=viewport>', 'name=viewport>' + metas, 1)
src = src.replace('width=device-width,initial-scale=1,user-scalable=no',
                  'width=device-width,initial-scale=1,user-scalable=no,viewport-fit=cover', 1)

# Fullscreen button (Fullscreen API: Android / desktop; auto-hidden on iPhone
# where the API doesn't exist, and while already fullscreen / standalone)
button = (
  '<div id=fsbtn style="position:absolute;top:10px;right:10px;width:34px;height:34px;'
  'border-radius:9px;background:rgba(255,255,255,.12);color:rgba(255,255,255,.65);'
  'font:20px/34px sans-serif;text-align:center;cursor:pointer;z-index:10;'
  '-webkit-user-select:none;user-select:none">&#x26F6;</div>'
  '<script>(function(){var b=document.getElementById("fsbtn"),d=document.documentElement;'
  'var standalone=navigator.standalone||matchMedia("(display-mode: standalone)").matches;'
  'if(!d.requestFullscreen&&!d.webkitRequestFullscreen||standalone){b.style.display="none";return;}'
  'function sync(){b.style.display=(document.fullscreenElement||document.webkitFullscreenElement)?"none":"block";}'
  'b.addEventListener("click",function(){(d.requestFullscreen||d.webkitRequestFullscreen).call(d);});'
  'document.addEventListener("fullscreenchange",sync);document.addEventListener("webkitfullscreenchange",sync);'
  '})();</script>')
src = src.replace('</body>', button + '</body>', 1)

open(sys.argv[1], 'w').write(src)
print('patched ->', sys.argv[1])
EOF

cd "$PAGES_DIR"
git add -A
git -c commit.gpgsign=false commit -m "deploy: $(date +%Y-%m-%d_%H%M)"
git push -f origin gh-pages
echo "deployed: https://tettou771.github.io/demo-trussBuster2/"
