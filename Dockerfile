FROM ubuntu:18.04

RUN apt-get update && \
    apt-get install -y \
    bc \
    cmake \
    curl \
    g++ \
    git \
    python3-dev \
    xz-utils

COPY . /workspace/AccelSeeker

ENV LLVM_SRC_TREE=/workspace/AccelSeeker/llvm-8.0.0/llvm-8.0.0.src
ENV LLVM_BLD_TREE=/workspace/AccelSeeker/llvm-8.0.0/build
RUN cd /workspace/AccelSeeker && \
    ./bootstrap.8.0.sh && \
    ./bootstrap_AS_passes.sh && \
    cd ${LLVM_BLD_TREE} && \
    make
