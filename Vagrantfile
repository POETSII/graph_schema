# -*- mode: ruby -*-
# vi: set ft=ruby :

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
  config.vm.box = "debian/contrib-jessie64"

  # Create a forwarded port mapping which allows access to a specific port
  # within the machine from a port on the host machine. In the example below,
  # accessing "localhost:8010" will access port 8000 on the guest machine.
  # Allow external access to a web-site hosted internally on 8000 (eg. python3 http.server)
  config.vm.network "forwarded_port", guest: 8000, host: 8010, host_ip: "127.0.0.1"

  # Share an additional folder to the guest VM. The first argument is
  # the path on the host to the actual folder. The second argument is
  # the path on the guest to mount the folder. And the optional third
  # argument is a set of non-required options.
  # config.vm.synced_folder "../data", "/vagrant_data"

  config.vm.provider "virtualbox" do |vb|
     # Display the VirtualBox GUI when booting the machine
     # vb.gui = true
  
     # Customize the amount of memory on the VM:
     vb.memory = "4096"
     
     vb.cpus=3
  end

  config.ssh.forward_agent = true
  config.ssh.forward_x11 = true
  
  config.vm.provision "shell", inline: <<-SHELL
      sudo apt install -y curl zip graphviz emacs g++ make git default-jre-headless build-essential
      
     sudo apt install -y libxml2-dev libxml++2.6-dev libboost-dev 
     
     sudo apt install -y python3.4 python3-scipy python3-lxml python3-matplotlib
     
     sudo apt install -y openmpi-doc openmpi-bin openmpi-common  libopenmpi-dev
     
     # For web visualisation stuff
     # This is modern "best practises" for you...
     curl -sL https://deb.nodesource.com/setup_7.x | sudo -E bash -
     sudo apt install -y nodejs npm
     sudo npm update -g
     # https://github.com/nodejs/node-v0.x-archive/issues/3911
     sudo ln -s /usr/bin/nodejs /usr/bin/node
     sudo npm install -g typescript
     sudo npm install -g grunt
     sudo npm install -g source-map-support
     sudo npm install -g require
     sudo npm install -g http-server
  SHELL
end
