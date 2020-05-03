#!/bin/bash

set -e
set -x

kvm-ok
service apt-cacher-ng start
ls -l /dev/kvm

cd /gitian-builder
./bin/make-base-vm --suite precise --arch i386
./bin/make-base-vm --suite precise --arch amd64

./bin/gbuild ../bcexchange/contrib/gitian-descriptors/boost-linux.yml </dev/null
cp build/out/boost-linux*-1.55.0-gitian-r1.zip inputs/
./bin/gbuild ../bcexchange/contrib/gitian-descriptors/deps-linux.yml </dev/null
cp build/out/bcexchange-deps-linux*-gitian-r5.zip inputs/
./bin/gbuild ../bcexchange/contrib/gitian-descriptors/boost-win.yml </dev/null
cp build/out/boost-win*-1.55.0-gitian-r6.zip inputs/
./bin/gbuild ../bcexchange/contrib/gitian-descriptors/qt-win.yml </dev/null
cp build/out/qt-win*-4.8.5-gitian-r3.zip inputs/
./bin/gbuild ../bcexchange/contrib/gitian-descriptors/deps-win.yml </dev/null
cp build/out/bcexchange-deps-win*-gitian-r12.zip inputs/

