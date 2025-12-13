# ============================
# Stage 1 — Build native APM engine
# ============================
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    libfftw3-dev \
    libfftw3-single3 \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DBUILD_BENCHMARKS=OFF \
    && cmake --build build -j$(nproc)

# ============================
# Stage 2 — Runtime (FastAPI + APM)
# ============================
FROM ubuntu:22.04 AS runtime

# ---- System deps ----
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    libfftw3-dev \
    libfftw3-single3 \
    && rm -rf /var/lib/apt/lists/*

# ---- Python deps ----
RUN pip3 install --no-cache-dir \
    fastapi \
    uvicorn[standard]

WORKDIR /app

# ---- Copy native engine ----
COPY --from=builder /app/build/apm /app/apm

# ---- Copy backend (FastAPI) ----
COPY backend /app/backend

# ---- Expose HTTP/WebSocket port ----
EXPOSE 8080

# ---- Run FastAPI ----
CMD ["uvicorn", "backend.app:app", "--host", "0.0.0.0", "--port", "8080"]
