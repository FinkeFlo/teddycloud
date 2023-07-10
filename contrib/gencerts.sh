#!/bin/bash

CA_KEY="certs/server/ca-key.pem"
CA_CRT="certs/server/ca-root.pem"
CA_CRT_DER="certs/server/ca.der"
SRV_CSR="certs/server/teddy-key.csr"
SRV_KEY="certs/server/teddy-key.pem"
SRV_CRT="certs/server/teddy-cert.pem"
DAYS="9000"

#TODO faketime / libfaketime!
#faketime '2015-11-04 00:00:00' 

mkdir -p certs/server
openssl genrsa -out ${CA_KEY} 2048
openssl req -x509 -new -nodes -extensions v3_ca -key ${CA_KEY} -days ${DAYS} -out ${CA_CRT} -sha256 -subj '/C=DE/CN=Teddy CA'
openssl genrsa -out ${SRV_KEY} 2048
openssl req -new -key ${SRV_KEY} -out ${SRV_CSR} -sha256 -subj '/C=DE/CN=TeddyCloud'
openssl x509 -req -in ${SRV_CSR} -CA ${CA_CRT} -CAkey ${CA_KEY} -CAcreateserial -out ${SRV_CRT} -days ${DAYS} -sha256
openssl x509 -inform PEM -outform DER -in ${CA_CRT} -out ${CA_CRT_DER}