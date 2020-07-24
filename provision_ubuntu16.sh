#!/bin/bash

sudo apt-get update

sudo apt-get install -y libxml2-dev gdb g++ git make libxml++2.6-dev libboost-dev libboost-filesystem-dev zip python3 zip default-jre-headless python3-lxml curl mpich rapidjson-dev

# RISC-V toolchain (not sure exactly how much is needed)
sudo apt-get install -y autoconf automake autotools-dev curl libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev

# Graph partitioning
sudo apt-get install -y metis

# Visualisation
sudo apt-get install -y graphviz imagemagick ffmpeg

# Editors
sudo apt-get install -y emacs-nox screen

# Algebraic multigrid, plus others
sudo apt-get install -y python3-pip python3-numpy python3-scipy python3-ujson
sudo pip3 install pyamg
sudo pip3 install svgwrite

# Creating meshes
sudo apt-get install -y octave octave-msh octave-geometry hdf5-tools
# Fix a bug in geometry package for svg.
# Note that sed is _not_ using extended regular expressions (no "-r")
sudo sed -i -e 's/,{0},/,"{0}",/g' -e 's/,{1},/,"{1}",/g'  /usr/share/octave/packages/geometry-2.1.0/io/@svg/parseSVGData.py

# Used to support generation of documentation from schema
sudo apt-get install -y xsltproc ant libsaxon-java docbook docbook-xsl-ns pandoc