#!/bin/bash
#This file addes a signed smime message to notes for the HEAD commit
#NOTE: you must have a cert name mycert.pem in ./.
HASH=$(git log -n1 | cut -d ' ' -f 2 | head -n1)
openssl smime -sign -signer mycert.pem -in ".git/objects/${HASH:0:2}/${HASH:2}" > cryptoSig.txt
git notes --ref=crypto add -F cryptoSig.txt HEAD
rm cryptoSig.txt
