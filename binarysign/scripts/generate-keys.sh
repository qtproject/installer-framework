#!/bin/bash

if [ -z "$1" ]; then
  echo "Usage: generate-keys.sh certname"
  exit 1
fi

NAME=$1

echo Creating RSA key...

openssl genrsa -out $NAME-rsaprivkey.pem 1024
openssl rsa -in $NAME-rsaprivkey.pem -pubout -outform DER -out $NAME-rsapubkey.der
openssl pkcs8 -topk8 -inform PEM -outform DER -in $NAME-rsaprivkey.pem -out $NAME-rsaprivkey.der -nocrypt

echo Creating DSA key...

openssl dsaparam -out $NAME-dsaparam.pem 1024
openssl gendsa -out $NAME-dsaprivkey.pem $NAME-dsaparam.pem

openssl dsa -in $NAME-dsaprivkey.pem -outform DER -pubout -out $NAME-dsapubkey.der
openssl pkcs8 -topk8 -inform PEM -outform DER -in $NAME-dsaprivkey.pem -out $NAME-dsaprivkey.der -nocrypt

echo Creating X.509 Certificate...

openssl genrsa -des3 -out $NAME-ca.key 4096
openssl req -new -x509 -days 365 -key $NAME-ca.key -outform DER -out $NAME-ca.cer
openssl req -new -x509 -days 365 -key $NAME-ca.key -out $NAME-ca.crt
openssl crl2pkcs7 -nocrl -certfile $NAME-ca.cer -out $NAME-ca.p7b -certfile $NAME-ca.crt
