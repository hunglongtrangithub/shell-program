# Use Ubuntu 22.04 as the base image
FROM ubuntu:22.04

# Install dependencies and GCC 11
RUN apt-get update && \
  apt-get install -y build-essential software-properties-common && \
  add-apt-repository ppa:ubuntu-toolchain-r/test && \
  apt-get update && \
  apt-get install -y gcc-11 g++-11

# Set gcc-11 as the default gcc and g++
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 100 && \
  update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 100

# Verify GCC version
RUN gcc --version

# Set a working directory
WORKDIR /workspace

# Set default entry point (optional)
CMD ["/bin/bash"]

