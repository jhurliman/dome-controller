#!/usr/bin/env bash

set -e

CUDA_VERSION="11.4"
CUDA_DOCKER_TAG="11.4.3-devel-ubuntu20.04"

cd "$(dirname "$0")"

mkdir -p "../nvidia"
cd "../nvidia"

# Create the docker image that will contain the CUDA toolkit
docker create --name "cuda-${CUDA_VERSION}_amd64-container" "nvidia/cuda:${CUDA_DOCKER_TAG}"

# Extract the CUDA toolkit from the docker image
rm -rf "cuda-${CUDA_VERSION}_amd64"
docker cp "cuda-${CUDA_VERSION}_amd64-container:/usr/local/cuda-${CUDA_VERSION}" "cuda-${CUDA_VERSION}_amd64"
docker rm "cuda-${CUDA_VERSION}_amd64-container"

echo "CUDA toolkit extracted to $(pwd)/cuda-${CUDA_VERSION}_amd64"
