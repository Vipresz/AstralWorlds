# Multi-stage Dockerfile for building the game engine
# Builds engine, platform, and game executable from root directory

# Build stage
FROM ubuntu:22.04 AS builder

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /workspace

# Copy source files
COPY . /workspace/

# Build the entire project (engine + platform + game)
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --config Release -j$(nproc)

# Runtime stage - for running the game executable
FROM ubuntu:22.04 AS runtime

# Copy built game executable (Linux binary)
COPY --from=builder /workspace/build/bin/game /app/game

# Make executable
RUN chmod +x /app/game

# Set working directory
WORKDIR /app

# Default command runs the game
CMD ["./game"]

