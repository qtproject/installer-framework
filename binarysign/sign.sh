  osslsigncode -spc TestPublisher.spc -key TestPublisher.der \
        -n "The Installer" -i http://www.yourwebsite.com/ \
        -in $1.exe -out $1-signed.exe


