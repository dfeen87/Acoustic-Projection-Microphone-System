# ============================
# Stage 1: Build environment
# ============================
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    libfftw3-dev \
    libfftw3-single3 \
    pkg-config \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Optional: TensorFlow Lite (Python version)
RUN pip3 install tensorflow || true

WORKDIR /workspace/apm

# Copy only the files you actually have
COPY CMakeLists.txt .
COPY include/ include/
COPY main.cpp .
COPY apm_system.h .
COPY fft_processor.h .
COPY tflite_translation_engine.h .

# Build
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . -j$(nproc)

# Install to /usr/local
RUN cd build && cmake --install .

# ============================
# Stage 2: Runtime environment
# ============================
FROM ubuntu:22.04 AS runtime

RUN apt-get update && apt-get install -y \
    libfftw3-single3 \
    libgomp1 \
    && rm -rf /var/lib/apt/lists/*

# Copy installed library + headers
COPY --from=builder /usr/local/lib /usr/local/lib
COPY --from=builder /usr/local/include /usr/local/include

# Copy the built binary
COPY --from=builder /workspace/apm/build/apm /usr/local/bin/apm

ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

CMD ["/usr/local/bin/apm"]

# ============================
# Stage 3: Dev environment
# ============================
FROM builder AS development

RUN apt-get update && apt-get install -y \
    gdb \
    valgrind \
    clang-format \
    clang-tidy \
    cppcheck \
    lcov \
    doxygen \
    graphviz \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install numpy scipy matplotlib

WORKDIR /workspace/apm
CMD ["/bin/bash"]
