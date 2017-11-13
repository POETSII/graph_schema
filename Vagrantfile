
# -*- mode: ruby -*-
# vi: set ft=ruby :

require 'etc'

# All Vagrant configuration is done below. The "2" in Vagrant.configure
# configures the configuration version (we support older styles for
# backwards compatibility). Please don't change it unless you know what
# you're doing.
Vagrant.configure(2) do |config|
  # The most common configuration options are documented and commented below.
  # For a complete reference, please see the online documentation at
  # https://docs.vagrantup.com.

  # Every Vagrant development environment requires a box. You can search for
  # boxes at https://atlas.hashicorp.com/search.
  config.vm.box = "ubuntu/xenial64"

  # Disable automatic box update checking. If you disable this, then
  # boxes will only be checked for updates when the user runs
  # `vagrant box outdated`. This is not recommended.
  # config.vm.box_check_update = false

  # Create a forwarded port mapping which allows access to a specific port
  # within the machine from a port on the host machine. In the example below,
  # accessing "localhost:8080" will access port 8000 on the guest machine.
  config.vm.network "forwarded_port", guest: 80, host: 8080
  
  config.ssh.forward_agent = true
  
  config.ssh.forward_x11 = true

  # Create a private network, which allows host-only access to the machine
  # using a specific IP.
  # config.vm.network "private_network", ip: "192.168.33.10"

  # Create a public network, which generally matched to bridged network.
  # Bridged networks make the machine appear as another physical device on
  # your network.
  # config.vm.network "public_network"

  # Share an additional folder to the guest VM. The first argument is
  # the path on the host to the actual folder. The second argument is
  # the path on the guest to mount the folder. And the optional third
  # argument is a set of non-required options.
  # config.vm.synced_folder "../data", "/vagrant_data"

  # If the POETS repos are in the parent folder, this gives access to them all
  config.vm.synced_folder "..", "/POETS"

  # Provider-specific configuration so you can fine-tune various
  # backing providers for Vagrant. These expose provider-specific options.
  # Example for VirtualBox:
  #
  config.vm.provider "virtualbox" do |vb|
  #   # Display the VirtualBox GUI when booting the machine
  #   vb.gui = true
  #
  #  
  
    vcpu = Etc.nprocessors > 4 ? 4 : ( Etc.nprocessors > 1 ? Etc.nprocessors - 1 : 1 )
    
    vb.cpus = vcpu
  
   # Customize the amount of memory on the VM:
   # 1  hcpu -> 1 vcpu : 6GB
   # 2  hcpu -> 1 vcpu : 6GB
   # 3  hcpu -> 2 vcpu : 8GB
   # 4  hcpu -> 3 vcpu : 10GB
   # 5+ hcpu -> 4 vcpu : 12GB
    vb.memory = 4000 + vcpu * 2000     
  
    # If clock drifts more than 500ms, then force it (instead of smooth adjust)
    vb.customize ["guestproperty","set", :id, "/VirtualBox/GuestAdd/VBoxService/--timesync-set-threshold", "500"]

    # Turn off audio, so that VM doesn't keep machine awake
    vb.customize ["modifyvm", :id, "--audio", "none"]
  end



  # Enable provisioning with a shell script. Additional provisioners such as
  # Puppet, Chef, Ansible, Salt, and Docker are also available. Please see the
  # documentation for more information about their specific syntax and use.
  config.vm.provision "shell", inline: <<-SHELL
	sudo apt-get update

     sudo apt-get install -y libxml2-dev gdb g++ git make libxml++2.6-dev libboost-dev python3.4 zip default-jre-headless python3-lxml curl mpich rapidjson-dev
     sudo apt-get install -y libxml2-dev gdb g++ git make libxml++2.6-dev libboost-dev libboost-filesystem-dev python3 zip default-jre-headless python3-lxml curl mpich rapidjson-dev

     # RISC-V toolchain (not sure exactly how much is needed)
      sudo apt-get install -y autoconf automake autotools-dev curl libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev

     # Graph partitioning
     sudo apt-get install -y metis

     # Visualisation
     sudo apt-get install -y graphviz imagemagick ffmpeg

     # Editors
     sudo apt-get install -y emacs-nox screen

     # Algebraic multigrid, plus others
     sudo apt-get install -y python3-pip python3-numpy python3-scipy ujson svgwrite
     sudo pip3 install pyamg

     # Creating meshes
     sudo apt-get install -y octave octave-msh octave-geometry hdf5-tools
     # Fix a bug in geometry package for svg.
     # Note that sed is _not_ using extended regular expressions (no "-r")
     sudo sed -i -e 's/,{0},/,"{0}",/g' -e 's/,{1},/,"{1}",/g'  /usr/share/octave/packages/geometry-2.1.0/io/@svg/parseSVGData.py
     
     # Used to support generation of documentation from schema
     sudo apt-get install -y xsltproc ant libsaxon-java docbook docbook-xsl-ns pandoc

  SHELL


end
