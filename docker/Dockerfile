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

# ---- Show what we're working with ----
RUN echo "===== CMakeLists.txt locations =====" && \
    find . -name "CMakeLists.txt" -type f

# ---- Configure (core-only, zero optional features) ----
RUN cmake -S . -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_BENCHMARKS=OFF \
    -DENABLE_LOCAL_TRANSLATION=OFF \
    -DENABLE_ENCRYPTION=OFF \
    -DENABLE_TFLITE=OFF 2>&1 | tee /tmp/cmake-config.log \
    || (echo "===== CMake configure failed =====" && \
        cat /tmp/cmake-config.log && \
        exit 1)

# ---- Show available build targets ----
RUN echo "===== Available build targets =====" && \
    cmake --build build --target help 2>&1 | head -50

# ---- Try building with error capture ----
RUN cmake --build build --target apm_backend --verbose 2>&1 | tee /tmp/build.log; \
    BUILD_EXIT_CODE=$?; \
    if [ $BUILD_EXIT_CODE -ne 0 ]; then \
        echo "===== BUILD FAILED (exit code $BUILD_EXIT_CODE) ====="; \
        echo "===== Last 200 lines of build output ====="; \
        tail -200 /tmp/build.log; \
        echo "===== First compilation error ====="; \
        grep -A 10 "error:" /tmp/build.log | head -30 || true; \
        exit 1; \
    fi

# ---- DEBUG: Find where the binary actually is ----
RUN echo "===== Searching for binaries =====" && \
    find /app/build -type f -executable 2>/dev/null | head -20 || true

# ============================
# Stage 2 — Runtime (FastAPI + APM)
# ============================
FROM ubuntu:22.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    python3-venv \
    ca-certificates \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install --no-cache-dir --upgrade pip

RUN pip install --no-cache-dir \
    fastapi \
    "uvicorn[standard]"

WORKDIR /app

# Copy entire build dir temporarily
COPY --from=builder /app/build /tmp/build

# Find and copy the binary
RUN find /tmp/build -type f -executable | head -1 | xargs -I {} cp {} /app/apm_backend \
    && chmod +x /app/apm_backend \
    && rm -rf /tmp/build

COPY backend /app/backend

EXPOSE 8080

CMD ["uvicorn", "backend.app:app", "--host", "0.0.0.0", "--port", "8080"]
