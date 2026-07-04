FROM debian:forky

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt update && apt install -y \
    build-essential \
    cmake \
    intltool \
    libgtkmm-4.0-dev \
    libwebkitgtk-6.0-dev \
    pkgconf \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Create build directory and build
RUN mkdir -p build/release && cd build/release && \
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr ../.. && \
    make -j$(nproc)

# Default command (note: this is a GUI app and requires X11 to run)
CMD ["/app/build/release/src/wasistlos"]
