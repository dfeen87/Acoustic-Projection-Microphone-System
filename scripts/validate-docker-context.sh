#!/usr/bin/env bash
# Prove that Docker ignores local credentials and state at every context depth.
# The fixture uses scratch and exported files, so no package downloads or
# hardware access are needed.
set -euo pipefail

repo=$(cd "$(dirname "$0")/.." && pwd)
scratch=$(mktemp -d)
image="apm-docker-context-validation:$(basename "$scratch")"
container=""
cleanup() {
    if [[ -n "$container" ]]; then docker rm "$container" >/dev/null 2>&1 || true; fi
    docker image rm "$image" >/dev/null 2>&1 || true
    rm -rf "$scratch"
}
trap cleanup EXIT

mkdir -p "$scratch/context/backend/nested"
cp "$repo/.dockerignore" "$scratch/context/.dockerignore"
cat > "$scratch/context/Dockerfile" <<'DOCKER'
FROM scratch
COPY . /probe
DOCKER
for directory in "$scratch/context" "$scratch/context/backend" "$scratch/context/backend/nested"; do
    printf 'test-only credential marker\n' > "$directory/.env"
    printf 'test-only credential marker\n' > "$directory/.env.production"
done
printf 'test-only database marker\n' > "$scratch/context/backend/data.sqlite"
printf 'test-only database marker\n' > "$scratch/context/backend/nested/session.db"
printf 'required application source\n' > "$scratch/context/backend/app.py"

docker build --network=none --tag "$image" "$scratch/context"
container=$(docker create "$image" /not-executed)
docker export --output "$scratch/context.tar" "$container"
tar -tf "$scratch/context.tar" > "$scratch/files.txt"
if grep -E '(^|/)\.env(\.|$)|(^|/)(data\.sqlite|session\.db)$' "$scratch/files.txt"; then
    echo "ERROR: Docker context contains local credentials or database state" >&2
    exit 1
fi
if ! grep -Fx 'probe/backend/app.py' "$scratch/files.txt" >/dev/null; then
    echo "ERROR: Docker context excluded required backend source" >&2
    exit 1
fi
echo "Docker context excludes nested credentials and database state."
