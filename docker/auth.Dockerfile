# Multi-stage build for Auth Service
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libssl-dev \
    libpq-dev \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc \
    libhiredis-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy source code
WORKDIR /build
COPY . .

# Build jwt-cpp and redis-plus-plus if in third_party
RUN if [ -d third_party/redis-plus-plus ]; then \
    cd third_party/redis-plus-plus && \
    mkdir -p build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) && \
    make install && \
    ldconfig; \
    fi

# Copy jwt-cpp headers
RUN if [ -d third_party/jwt-cpp ]; then \
    cp -r third_party/jwt-cpp/include/* /usr/local/include/; \
    fi

# Build the service
RUN cd /build && \
    mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc) auth_service

# Runtime stage
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libpq5 \
    libgrpc++1.51 \
    libprotobuf32 \
    libssl3 \
    libhiredis0.14 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy built binary
COPY --from=builder /build/build/auth/auth_service /usr/local/bin/

# Copy redis-plus-plus library if exists
COPY --from=builder /usr/local/lib/libredis++.so* /usr/local/lib/ || true
RUN ldconfig || true

# Create app directory
WORKDIR /app
RUN mkdir -p /app/certs

EXPOSE 50051

CMD ["auth_service"]
