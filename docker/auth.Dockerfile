FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg for dependencies
RUN git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg && \
    /opt/vcpkg/bootstrap-vcpkg.sh

# Install C++ dependencies
RUN /opt/vcpkg/vcpkg install \
    grpc \
    jwt-cpp \
    redis-plus-plus \
    libpqxx

# Copy source code
WORKDIR /app
COPY services/cpp/common ./common
COPY services/cpp/auth ./auth
COPY proto ./proto

# Build
RUN mkdir build && cd build && \
    cmake -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake .. && \
    make -j$(nproc)

# Runtime stage
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libssl3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/auth/auth_service .

EXPOSE 50051

CMD ["./auth_service"]
