# ============================
# Stage 1 — Build native APM engine
# ============================
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    libfftw3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# 🔒 Explicitly disable optional features for container build
RUN cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_BENCHMARKS=OFF \
    -DENABLE_LOCAL_TRANSLATION=OFF \
    -DENABLE_ENCRYPTION=OFF \
    -DENABLE_TFLITE=OFF \
 && cmake --build build --verbose -- -v

# ============================
# Stage 2 — Runtime (FastAPI + APM)
# ============================
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# ---- System runtime deps only ----
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    python3-venv \
    libfftw3-3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# ---- Upgrade pip safely ----
RUN python3 -m pip install --no-cache-dir --upgrade pip

# ---- Python deps ----
RUN pip install --no-cache-dir \
    fastapi \
    "uvicorn[standard]"

WORKDIR /app

# ---- Copy native engine ----
COPY --from=builder /app/build/apm_backend /app/apm_backend

# ---- Copy backend (FastAPI) ----
COPY backend /app/backend

# ---- Expose HTTP/WebSocket port ----
EXPOSE 8080

# ---- Run FastAPI ----
CMD ["uvicorn", "backend.app:app", "--host", "0.0.0.0", "--port", "8080"]
