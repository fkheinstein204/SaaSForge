#!/bin/bash
# Generate gRPC stubs from proto files

set -e

PROTO_DIR="./proto"
CPP_OUT="./services/cpp/generated"
PYTHON_OUT="./api/generated"

echo "Generating C++ gRPC stubs..."
mkdir -p "$CPP_OUT"
protoc \
  --proto_path="$PROTO_DIR" \
  --cpp_out="$CPP_OUT" \
  --grpc_out="$CPP_OUT" \
  --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) \
  "$PROTO_DIR"/*.proto

echo "Generating Python gRPC stubs..."
mkdir -p "$PYTHON_OUT"
python3 -m grpc_tools.protoc \
  --proto_path="$PROTO_DIR" \
  --python_out="$PYTHON_OUT" \
  --grpc_python_out="$PYTHON_OUT" \
  "$PROTO_DIR"/*.proto

echo "Proto generation complete!"
