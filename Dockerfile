# Multi-stage Dockerfile for APM System
# Stage 1: Build environment
FROM ubuntu:22.04 AS builder

# Prevent interactive prompts
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

# Install TensorFlow Lite (optional)
RUN pip3 install tensorflow && \
    mkdir -p /opt/tensorflow && \
    cp -r $(python3 -c "import tensorflow as tf; print(tf.__path__[0])")/lite /opt/tensorflow/ || true

# Create workspace
WORKDIR /workspace/apm

# Copy source files
COPY CMakeLists.txt .
COPY include/ include/
COPY src/ src/
COPY tests/ tests/
COPY examples/ examples/
COPY benchmarks/ benchmarks/

# Build the project
RUN mkdir build && cd build && \
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=ON \
        -DBUILD_BENCHMARKS=ON && \
    cmake --build . -j$(nproc) && \
    ctest --output-on-failure

# Install to /usr/local
RUN cd build && cmake --install .

# Stage 2: Runtime environment
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libfftw3-single3 \
    libgomp1 \
    && rm -rf /var/lib/apt/lists/*

# Copy installed libraries and headers
COPY --from=builder /usr/local/lib/libapm.a /usr/local/lib/
COPY --from=builder /usr/local/include/apm /usr/local/include/apm
COPY --from=builder /usr/local/lib/cmake/apm /usr/local/lib/cmake/apm

# Copy example binary
COPY --from=builder /workspace/apm/build/apm_example /usr/local/bin/

# Set library path
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Default command
CMD ["/usr/local/bin/apm_example"]

# Stage 3: Development environment (optional)
FROM builder AS development

# Install additional development tools
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

# Install Python tools for analysis
RUN pip3 install \
    matplotlib \
    numpy \
    scipy

WORKDIR /workspace/apm

# Keep the build directory for development
CMD ["/bin/bash"]
