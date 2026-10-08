# ============================
# Stage 1 — Build native APM engine
# ============================
# Pin the base image to a specific digest for reproducible, tamper-evident builds.
FROM ubuntu:22.04@sha256:3ba65aa20f86a0fad9df2b2c259c613df006b2e6d0bfcc8a146afb8c525a9751 AS builder
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

# ---- Show what we're working with ----
RUN echo "===== CMakeLists.txt locations =====" && \
    find . -name "CMakeLists.txt" -type f

# Configure and build without pipelines that mask the compiler exit status.
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF -DBUILD_BENCHMARKS=OFF \
    -DENABLE_LOCAL_TRANSLATION=OFF -DENABLE_ENCRYPTION=OFF -DENABLE_TFLITE=OFF
RUN cmake --build build --target apm_backend --parallel 2

# ============================
# Stage 2 — Runtime (FastAPI + APM)
# ============================
# Pin the base image to a specific digest for reproducible, tamper-evident builds.
FROM ubuntu:22.04@sha256:3ba65aa20f86a0fad9df2b2c259c613df006b2e6d0bfcc8a146afb8c525a9751 AS runtime
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    python3-venv \
    ca-certificates \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install --no-cache-dir --upgrade pip

COPY backend/requirements.txt /tmp/apm-requirements.txt
RUN pip install --no-cache-dir -r /tmp/apm-requirements.txt

WORKDIR /app

# Copy entire build dir temporarily
COPY --from=builder /app/build /tmp/build

# Copy and install the binary at an explicit, deterministic path.
RUN cp /tmp/build/apm_backend /app/apm_backend \
    || { echo "ERROR: apm_backend binary not found at /tmp/build/apm_backend"; exit 1; } \
    && chmod +x /app/apm_backend \
    && rm -rf /tmp/build

COPY backend /app/backend

EXPOSE 8080

CMD ["uvicorn", "backend.app:app", "--host", "0.0.0.0", "--port", "8080"]
