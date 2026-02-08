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
    -DENABLE_TFLITE=OFF

# ---- Build ONLY the backend binary ----
RUN cmake --build build --target apm_backend --verbose

# ---- DEBUG: Find where the binary actually is ----
RUN echo "===== Searching for apm_backend binary =====" && \
    find /app/build -type f -name "apm_backend*" -o -name "*apm_backend*" | head -20 && \
    echo "===== All executables in build directory =====" && \
    find /app/build -type f -executable | head -20 && \
    echo "===== Contents of build directory =====" && \
    ls -la /app/build/

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
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# ---- Upgrade pip ----
RUN python3 -m pip install --no-cache-dir --upgrade pip

# ---- Python deps ----
RUN pip install --no-cache-dir \
    fastapi \
    "uvicorn[standard]"

WORKDIR /app

# ---- Copy native engine (adjust path based on debug output) ----
# Try copying the entire build directory first to find it
COPY --from=builder /app/build /app/build

# Find and move the binary to expected location
RUN find /app/build -type f -executable -name "*apm*" | head -1 | xargs -I {} cp {} /app/apm_backend \
    && chmod +x /app/apm_backend \
    && ls -la /app/apm_backend

# ---- Copy FastAPI backend ----
COPY backend /app/backend

# ---- Expose API port ----
EXPOSE 8080

# ---- Run API ----
CMD ["uvicorn", "backend.app:app", "--host", "0.0.0.0", "--port", "8080"]
