#!/usr/bin/env bash
set -euo pipefail

branch=automation/moonlight-upstream
git fetch --no-tags https://github.com/moonlight-stream/moonlight-android.git \
  master:refs/remotes/upstream/master
upstream_sha="$(git rev-parse refs/remotes/upstream/master)"
base="$(git rev-parse HEAD)"
if git merge-base --is-ancestor "$upstream_sha" "$base"; then
  echo "Iris already contains Moonlight $upstream_sha." >> "$GITHUB_STEP_SUMMARY"
  exit 0
fi

# This branch belongs to automation. A lease protects against concurrent edits.
previous="$(git ls-remote --heads origin "refs/heads/$branch" | cut -f1)"
git config user.name 'github-actions[bot]'
git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
clean=true
conflicts=''
if ! git merge --no-ff --no-edit "$upstream_sha"; then
  conflicts="$(git diff --name-only --diff-filter=U)"
  git merge --abort
  if [[ -z "$conflicts" ]]; then
    echo '::error::Upstream merge failed without conflicts.'
    exit 1
  fi
  # Publish the upstream head so even a conflicting update has a reviewable PR.
  candidate="$upstream_sha"
  clean=false
else
  candidate="$(git rev-parse HEAD)"
fi

# Keep the PR head stable on unchanged daily runs, including manual resolutions.
if [[ -n "$previous" ]]; then
  git fetch --no-tags origin "refs/heads/$branch"
  if git merge-base --is-ancestor "$upstream_sha" "$previous" &&
     git merge-base --is-ancestor "$base" "$previous"; then
    candidate="$previous"
    clean=true
  fi
fi

git push --force-with-lease="refs/heads/$branch:$previous" \
  origin "$candidate:refs/heads/$branch"

body="$(mktemp)"
trap 'rm -f "$body"' EXIT
{
  echo 'Bring missing Moonlight Android upstream commits into Iris.'
  echo
  echo "Upstream: https://github.com/moonlight-stream/moonlight-android/commit/$upstream_sha"
  echo "Iris base: $base"
  echo
  if [[ "$clean" == true ]]; then
    echo 'The merge is clean. See the sync run for validation results.'
  else
    echo 'Manual conflict resolution is required. This PR currently points to the upstream head.'
    echo 'Merge master into this branch and resolve these paths before merging the PR:'
    echo '```'
    printf '%s\n' "$conflicts"
    echo '```'
  fi
  echo
  echo "Validation run: $GITHUB_SERVER_URL/$GITHUB_REPOSITORY/actions/runs/$GITHUB_RUN_ID"
  echo
  echo 'Review Iris branding, controller behavior, build settings, and workflow changes before merging.'
  echo 'Use a merge commit (not squash or rebase) to preserve upstream ancestry.'
  echo 'This branch is managed by automation and may be rebuilt when master or upstream changes.'
} > "$body"

pr="$(gh pr list --base master --head "$branch" --state open --json number --jq '.[0].number // empty')"
if [[ -n "$pr" ]]; then
  gh pr edit "$pr" --title 'Merge Moonlight upstream updates' --body-file "$body"
else
  gh pr create --base master --head "$branch" \
    --title 'Merge Moonlight upstream updates' --body-file "$body"
fi
cat "$body" >> "$GITHUB_STEP_SUMMARY"

# Changed workflows need review before we execute upstream-controlled automation.
if [[ "$clean" == true ]] && git diff --quiet "$base" "$candidate" -- .github/workflows; then
  echo "sha=$candidate" >> "$GITHUB_OUTPUT"
else
  echo 'Automatic validation skipped: resolve conflicts or review workflow changes, then run PR checks.' \
    >> "$GITHUB_STEP_SUMMARY"
fi
