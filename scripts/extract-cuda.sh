#!/usr/bin/env bash

set -e

CUDA_VERSION="10.2"

cd "$(dirname "$0")"

mkdir -p "../nvidia"
cd "../nvidia"

# Build the docker image that will contain the CUDA toolkit
docker build -t "cuda-${CUDA_VERSION}_amd64" -f "../scripts/Dockerfile.cuda-${CUDA_VERSION}_amd64" .
docker create --name "cuda-${CUDA_VERSION}_amd64-container" "cuda-${CUDA_VERSION}_amd64"

# Extract the CUDA toolkit from the docker image
rm -rf "cuda-${CUDA_VERSION}_amd64"
docker cp "cuda-${CUDA_VERSION}_amd64-container:/usr/local/cuda-${CUDA_VERSION}" "cuda-${CUDA_VERSION}_amd64"
docker rm "cuda-${CUDA_VERSION}_amd64-container"

echo "CUDA toolkit extracted to $(pwd)/cuda-${CUDA_VERSION}_amd64"
