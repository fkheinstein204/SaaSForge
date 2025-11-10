#!/bin/bash
# Generate self-signed certificates for mTLS (development only)

set -e

CERTS_DIR="./certs"
mkdir -p "$CERTS_DIR"

echo "Generating mTLS certificates for development..."

# Generate CA key and certificate
openssl genrsa -out "$CERTS_DIR/ca.key" 4096
openssl req -new -x509 -days 365 -key "$CERTS_DIR/ca.key" -out "$CERTS_DIR/ca.crt" \
  -subj "/C=US/ST=CA/L=SF/O=SaaSForge/CN=SaaSForge-CA"

# Generate server certificates for each service
for service in auth upload payment notification; do
  echo "Generating certificate for $service-service..."

  openssl genrsa -out "$CERTS_DIR/$service-service.key" 4096

  openssl req -new -key "$CERTS_DIR/$service-service.key" \
    -out "$CERTS_DIR/$service-service.csr" \
    -subj "/C=US/ST=CA/L=SF/O=SaaSForge/CN=$service-service"

  openssl x509 -req -days 365 \
    -in "$CERTS_DIR/$service-service.csr" \
    -CA "$CERTS_DIR/ca.crt" \
    -CAkey "$CERTS_DIR/ca.key" \
    -CAcreateserial \
    -out "$CERTS_DIR/$service-service.crt"

  rm "$CERTS_DIR/$service-service.csr"
done

# Generate client certificate (for API to call services)
echo "Generating client certificate..."
openssl genrsa -out "$CERTS_DIR/client.key" 4096
openssl req -new -key "$CERTS_DIR/client.key" \
  -out "$CERTS_DIR/client.csr" \
  -subj "/C=US/ST=CA/L=SF/O=SaaSForge/CN=api-client"
openssl x509 -req -days 365 \
  -in "$CERTS_DIR/client.csr" \
  -CA "$CERTS_DIR/ca.crt" \
  -CAkey "$CERTS_DIR/ca.key" \
  -CAcreateserial \
  -out "$CERTS_DIR/client.crt"
rm "$CERTS_DIR/client.csr"

chmod 600 "$CERTS_DIR"/*.key

echo "Certificate generation complete!"
echo "Certificates stored in: $CERTS_DIR"
echo ""
echo "WARNING: These are self-signed certificates for DEVELOPMENT ONLY."
echo "Use proper CA-signed certificates in production."
