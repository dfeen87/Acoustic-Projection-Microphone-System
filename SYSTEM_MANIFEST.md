# SYSTEM MANIFEST

This document defines the architectural layers, ownership boundaries, and
build responsibilities of the Acoustic Projection Microphone (APM) system.

It is documentation-only and establishes the build and integration contract
for contributors, CI, Docker, and automated tooling.

---

## Architectural Layers

### Core Engine (C++)
**Responsibilities**
- Implements core DSP, capture, and signal-processing logic.
- Exposes the stable public API used by other layers.
- Owns native build configuration and compiled artifacts.

**Directories**
- `include/apm/` (public API headers)
- `src/`
- `cmake/`
- `config/`
- `CMakeLists.txt`

**Required For**
- Local development: Yes
- CI: Yes
- Docker runtime: Yes (engine binary + minimal runtime artifacts)

---

### Backend / Orchestration (Python)
**Responsibilities**
- Provides control, signaling, and orchestration services.
- Manages data flow between the engine and external clients.

**Directories**
- `backend/`
- `signaling/`
- `peer/`
- `control/`

**Required For**
- Local development: Yes
- CI: Yes
- Docker runtime: No (unless explicitly packaging backend services)

---

### Frontend / UI (JS)
**Responsibilities**
- Implements user interfaces and operator workflows.
- Packages web or desktop UI assets.

**Directories**
- `frontend/`
- `ui/`
- `launcher/`

**Required For**
- Local development: Yes (when working on UI/UX)
- CI: Yes
- Docker runtime: No

---

### Delivery / Ops (CI, Docker, installers)
**Responsibilities**
- Defines build, packaging, and deployment workflows.
- Captures automation for CI validation and release artifacts.

**Directories**
- `.github/`
- `docker/`
- `installers/`
- `scripts/`
- `tools/`
- `docs/`

**Required For**
- Local development: No (unless packaging or release tasks are needed)
- CI: Yes
- Docker runtime: Yes (Docker build context and packaging assets)

---

## Build-Critical Directories

- **Native engine builds**
  - `include/apm/`
  - `src/`
  - `cmake/`
  - `config/`
  - `CMakeLists.txt`

- **CI validation**
  - All Core Engine directories
  - `tests/`
  - `.github/`

- **Docker builds**
  - Core Engine directories only
  - `docker/`
  - Explicitly excludes UI, frontend, and test sources

---

## Build Contract

- Docker builds depend **only** on the Core Engine and minimal runtime artifacts.
- UI and frontend assets are explicitly excluded from Docker engine builds.
- Tests are required for CI but are not part of runtime or deployment images.
- CI validates each layer independently to prevent cross-layer regressions.

---

## Tooling & Automation Guidance

- Automated tools should not refactor across architectural layers without
  explicit instruction.
- Headers under `include/apm/` are treated as **public API** and should remain
  stable.
- Changes affecting Docker, CI, or build behavior must respect this manifest.
