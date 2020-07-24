#!/usr/bin/env bash

# This script is intended to to setup a pypy3 environment, as it
# can provide a modest speed boost for a number of the tools, e.g.
# around 2x faster when generating large streaming graphs in python.
#
# If you want to use this env, then run this script to set it up,
# and then source pypy3-virtualenv/bin/activate to use it. You
# can use `deactivate` in bash to return to normal python.


# I think this is idempotent?
sudo add-apt-repository -y ppa:pypy/ppa
sudo apt update

# Atlas + gfortran is needed to compile scipy
sudo apt install pypy3 libatlas-base-dev gfortran

if [[ ! -d pypy-virtualenv ]] ; then

    virtualenv -p $(which pypy3) pypy3-virtualenv

    source pypy3-virtualenv/bin/activate
    python -m ensurepip

    pip3 install lxml
    pip3 install numpy
    pip3 install scipy
    pip3 install pyamg
    pip3 install h5py

fi