#!/usr/bin/env bash
# Generate self-signed CA + server cert (SAN = public IPv4) for watch-diy Caddy.
# Run on Linux (cloud server) or WSL/Git Bash. Requires openssl.
#
# Usage:
#   chmod +x scripts/gen-certs.sh
#   ./scripts/gen-certs.sh 120.55.12.34
#   PUBLIC_IP=120.55.12.34 ./scripts/gen-certs.sh
#
# Outputs under server/caddy/certs/:
#   ca.crt ca.key — back up ca.key offline; do not commit keys.
#   server.crt server.key — used by Caddy

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CERT_DIR="${SERVER_DIR}/caddy/certs"

IP="${1:-${PUBLIC_IP:-}}"
if [[ -z "${IP}" ]]; then
  echo "Usage: $0 <PUBLIC_IPV4>"
  echo "Or set PUBLIC_IP=1.2.3.4"
  exit 1
fi

mkdir -p "${CERT_DIR}"
cd "${CERT_DIR}"

if [[ -f ca.key || -f server.key ]]; then
  echo "WARN: ca.key or server.key already exists in ${CERT_DIR}"
  if [[ "${OVERWRITE:-0}" != "1" ]]; then
    read -r -p "Overwrite? [y/N] " ans
    if [[ "${ans}" != "y" && "${ans}" != "Y" ]]; then
      exit 1
    fi
  fi
fi

echo "=== Generating CA (10y) ==="
openssl genrsa -out ca.key 4096
openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.crt \
  -subj "/C=CN/O=Watch DIY/CN=Watch DIY Root CA"

echo "=== Server key + cert (SAN: IP:${IP}, ~5y) ==="
openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr \
  -subj "/C=CN/O=Watch DIY/CN=watch-diy-edge"

cat > server.ext <<EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage=digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=IP:${IP}
EOF

openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out server.crt -days 1825 -sha256 -extfile server.ext

chmod 600 ca.key server.key
chmod 644 ca.crt server.crt

rm -f server.csr server.ext

echo "=== Done ==="
echo "  CA (give to clients):  ${CERT_DIR}/ca.crt"
echo "  Private (Caddy only): ${CERT_DIR}/server.key"
echo ""
echo "Copy ca.crt into firmware + Flutter for TLS pinning."
echo "Test: curl -k https://${IP}:18443/health"
echo "      curl --cacert ca.crt https://${IP}:18443/health"
