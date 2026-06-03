# Linux development container for the OS-independent cores and the sanitizer
# matrix (architecture ADR-12/ADR-14). The Windows shipping build uses MSVC /
# clang-cl on the host or CI — not this image.
#
# Base image ships CMake, Ninja, and a vcpkg checkout at $VCPKG_ROOT.
FROM mcr.microsoft.com/devcontainers/cpp:1-debian-12

ARG LLVM_VERSION=18

# Clang toolchain + the sanitizers MSVC lacks (TSan/UBSan), plus the lint tools
# the CI gates use, so the container reproduces CI locally.
RUN apt-get update \
  && export DEBIAN_FRONTEND=noninteractive \
  && apt-get install -y --no-install-recommends \
       clang-${LLVM_VERSION} \
       clang-format-${LLVM_VERSION} \
       clang-tidy-${LLVM_VERSION} \
       clang-tools-${LLVM_VERSION} \
       lld-${LLVM_VERSION} \
       llvm-${LLVM_VERSION} \
       libc++-${LLVM_VERSION}-dev \
       libc++abi-${LLVM_VERSION}-dev \
       python3 \
       pkg-config \
  && for tool in clang clang++ clang-format clang-tidy clang-apply-replacements lld; do \
       update-alternatives --install /usr/bin/$tool $tool /usr/bin/$tool-${LLVM_VERSION} 100; \
     done \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*
