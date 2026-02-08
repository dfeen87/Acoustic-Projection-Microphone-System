# ============================
# Stage 1 — Build native APM engine
# ============================
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# ---- Toolchain only (NO optional libs) ----
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# ---- Configure (core-only, zero optional features) ----
RUN cmake -S . -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_BENCHMARKS=OFF \
    -DENABLE_LOCAL_TRANSLATION=OFF \
    -DENABLE_ENCRYPTION=OFF \
    -DENABLE_TFLITE=OFF \
    || (echo "===== CMake configure failed =====" && \
        cat build/CMakeFiles/CMakeError.log || true && \
        cat build/CMakeFiles/CMakeOutput.log || true && \
        exit 1)

# ---- Build ONLY the backend binary ----
RUN cmake --build build --target apm_backend --verbose \
    || (echo "===== Build failed =====" && exit 1)

# ============================
# Stage 2 — Runtime (FastAPI + APM)
# ============================
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# ---- Runtime deps ONLY (no build tools) ----
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    python3-venv \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# ---- Upgrade pip ----
RUN python3 -m pip install --no-cache-dir --upgrade pip

# ---- Python deps ----
RUN pip install --no-cache-dir \
    fastapi \
    "uvicorn[standard]"

WORKDIR /app

# ---- Copy native engine ----
COPY --from=builder /app/build/apm_backend /app/apm_backend

# ---- Copy FastAPI backend ----
COPY backend /app/backend

# ---- Expose API port ----
EXPOSE 8080

# ---- Run API ----
CMD ["uvicorn", "backend.app:app", "--host", "0.0.0.0", "--port", "8080"]
