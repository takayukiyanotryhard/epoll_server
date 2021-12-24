#!/bin/bash
# オレオレ証明書作成スクリプトを兼ねたREADME


## key.pem
openssl genrsa 2048 > key.pem

## openssl.cnf file
cat << EOT >> server_ssl.cnf
[ req ]
default_bits       = 2048
distinguished_name = req_distinguished_name
req_extensions     = req_ext
[ req_distinguished_name ]
countryName                 = Country Name (2 letter code)
countryName_default         = JP
stateOrProvinceName         = State or Province Name (full name)
stateOrProvinceName_default = Tokyo
localityName                = Locality Name (e.g, city)
localityName_default        = Minato-ku
organizationName            = Organization Name (e.g, company)
organizationName_default    = Try Hard Inc.
commonName                  = *.try-hard.co.jp
[ req_ext ]
subjectAltName = @alt_names
[ alt_names ]
DNS.1 = try-hard.co.jp
EOT

## server.csr
openssl req -new -key key.pem -config server_ssl.cnf -out server.csr -subj "/C=JP/ST=Tokyo/L=Minato-ku/O=Try Hard Inc./CN=*.try-hard.co.jp"

## confirm
openssl req -text -noout -in server.csr

## cert.pem
openssl x509 -in ./server.csr -req -signkey key.pem -out ./cert.pem

## confirm
openssl x509 -text -noout -in cert.pem


##### server connect sample
##### openssl s_client -connect tls.try-hard.co.jp:4433

##### ローカルでテストする場合はhostsファイルを改変するなどして、名前を合わせること
##### ex.)
#####  127.0.0.1 tls.try-hard.co.jp
#####  ::1 tls.try-hard.co.jp



