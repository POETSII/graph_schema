
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
  config.vm.box = "ubuntu/bionic64"

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
  hcpu = Etc.nprocessors
  vcpu = hcpu > 4 ? 4 : ( hcpu > 1 ? hcpu - 1 : 1 )
  vb.cpus = vcpu
  
   # Customize the amount of memory on the VM:
   vb.memory = 3000     
  
   # If clock drifts more than 500ms, then force it (instead of smooth adjust)
   vb.customize ["guestproperty","set", :id, "/VirtualBox/GuestAdd/VBoxService/--timesync-set-threshold", "500"]

    # Turn off audio, so that VM doesn't keep machine awake
    vb.customize ["modifyvm", :id, "--audio", "none"]
  end



  # Enable provisioning with a shell script. Additional provisioners such as
  # Puppet, Chef, Ansible, Salt, and Docker are also available. Please see the
  # documentation for more information about their specific syntax and use.
  config.vm.provision "shell", path: "provision_ubuntu18.sh"

end
