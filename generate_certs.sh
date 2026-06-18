#!/bin/bash
openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 \
    -subj "/C=US/ST=State/L=City/O=Organization/CN=localhost" \
    -keyout server.key -out server.crt

echo "Generated server.key and server.crt"
