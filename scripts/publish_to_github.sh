
#!/usr/bin/env bash
set -euo pipefail

OWNER="${FDGR_GITHUB_OWNER:-Dicklesworthstone}"
REPO="${FDGR_GITHUB_REPO:-franken_drone_geometry_reconstruction}"
VISIBILITY="${FDGR_GITHUB_VISIBILITY:-public}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

command -v git >/dev/null 2>&1 || { printf 'git is required\n' >&2; exit 1; }
command -v gh >/dev/null 2>&1 || { printf 'GitHub CLI (gh) is required\n' >&2; exit 1; }

git rev-parse --is-inside-work-tree >/dev/null 2>&1 || git init -b main
if ! git config user.name >/dev/null 2>&1; then git config user.name "Jeffrey Emanuel"; fi
if ! git config user.email >/dev/null 2>&1; then git config user.email "35050222+Dicklesworthstone@users.noreply.github.com"; fi

git add --all
if ! git diff --cached --quiet; then
  git commit -m "Initial evidence-grade FDGR architecture and scaffold"
fi

if gh repo view "$OWNER/$REPO" >/dev/null 2>&1; then
  if ! git remote get-url origin >/dev/null 2>&1; then
    git remote add origin "https://github.com/$OWNER/$REPO.git"
  fi
  git push -u origin main
else
  gh repo create "$OWNER/$REPO" "--$VISIBILITY" --source=. --remote=origin --push \
    --description "Agent-native, evidence-grade operating substrate for turning owner-authorized drone video into metrically honest semantic digital twins"
fi

gh repo edit "$OWNER/$REPO" \
  --description "Agent-native, evidence-grade operating substrate for turning owner-authorized drone video into metrically honest semantic digital twins" \
  --add-topic 3d-reconstruction \
  --add-topic computer-vision \
  --add-topic digital-twin \
  --add-topic drone \
  --add-topic photogrammetry \
  --add-topic rust \
  --add-topic spatial-computing

printf 'Published https://github.com/%s/%s\n' "$OWNER" "$REPO"
