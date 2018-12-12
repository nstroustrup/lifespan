# Lifespan Machine on a Raspberry Pi

This document was written by [Olivier Martin](mailto:oliviermfmartin@gmail.com) in 2018 using Raspberry Pi Model B+, Raspbian 4.14 and the Lifespan Machine 2.0.3.

## Introduction

The following items are described in this document.

1. Installation;
2. Running an experiment;
3. Setting-up scanners;
4. Cloning an SD card.

Please note that this requires a second computer (Linux or Windows) with an internet connection, a microSD card, a microSD-to-USB adapter and obviously a Raspberry Pi with adapted Epson scanners.

This requires using the Raspberry Pi terminal which can be opened using `Ctrl-Alt-T`. A basic knowledge of `shell` and of some text-editor (e.g. `nano` or `vi`) can help a lot. Please do not copy-paste all the code blocks directly in the terminal window. Some commands require user input.

**The Raspberry Pi has been shown to run the image acquisition with no more than 3 scanners. Above this number, memory allocation errors may arise.**

## Installation

Here we describe the steps necessary to get from a Raspberry Pi with an empty SD card to a functional image acquiring machine. 

There are two ways to proceed: 
​    1. use the cloned microSD image; 
​    2. install everything from scratch.

### From the cloned microSD image

Here we describe how to install everything using a cloned microSD card (`raspberry_lifespan.img`). Please note that instruction differ for Linux and Windows and that **your microSD card must be have at least 32GB.**

To learn how to clone the Pi's microSD card into an `img` file, see the corresponding section.

#### Step 1: Clone the `img` file to the microSD card

##### Linux

Insert the card and check the name of the device (usually something like `sda`). 

```
sudo fdisk -l
```

In this tutorial we will call our SD card `sda`, but do change the name so that it matches yours. **Be aware that this name can change if you unplug your SD card and plug it back in.**

You should now unmount the card.

```
sudo umount /dev/sda
```

Now create the image using `dd`. This operation is quite slow.

```
sudo dd if=lifespan_raspberry.img of=/dev/sda bs=100M
```

You can monitor progress in Ubuntu 16.04 and above using

```
sudo dd if=lifespan_raspberry.img of=/dev/sda status=progress bs=100M
```

#### Windows

1. Download [Win32 Disk Imager](https://sourceforge.net/projects/win32diskimager/)

2. Use Win32 Disk Imager to clone the `img` file to your microSD card.

#### Step 2: Expand the partition

Using a software like `gparted`, expand the rootfs partition so it fills up the whole SD card. You can now plug in the SD card in the Pi and start it.

#### Step 3: Configuration

Follow steps 3 and 9 as if you were installing the Pi from scratch. The default password is `lifespan2018`.

If you want to SSH in the Rasberry Pi without having to use a computer screen to figure out the IP address, please note that the IP address written to a file located in the home directory (`/home/pi/ip.txt`) every minute. Just plug-in the Pi, wait a little and get the IP address by plugging it the SD card to another compute.

Finally, think of updating the lifespan machine code before you run it the first time and then from time to time. Instructions are provided in the corresponding section.

### From scratch

#### Step 1: Installing the OS on the Raspberry Pi

- Download [Raspbian](https://www.raspberrypi.org/downloads/raspbian/) and unzip the archive.
- Download [Etcher](https://etcher.io/)
- Write the Raspbian image to the microSD card using Etcher.
- More information about the installation process [here](https://www.raspberrypi.org/documentation/installation/installing-images/README.md).

#### Step 2: Set passwords

The default password for the default user `pi` is `raspberry`.
Change it using the `passwd` command.

You should also set the root user password using `sudo passwd`.

#### Step 3: Tell git who you are

```
git config --global user.email "you@example.com"
git config --global user.name "Your Name"
```

#### Step 4: Open the ports!

Click on the Raspberry on the top left corner of the screen, go to Preference/Raspberry Pi Configuration/Interfaces and enable SSH.

#### Step 5: Get the Lifespan Machine source code from the Github repository 

```
git clone https://github.com/nstroustrup/lifespan # Run the following command to clone directory
cd lifespan 
git checkout flow_devel # Switch to the flow dev branch
```

#### Step 6: Install Lifespan Machine dependencies using apt-get


Using `apt-get` is sufficient to install mode dependencies.

```
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install \
    cmake \
    vim \
    apache2 \
    apache2-utils \
    php \
    php-mysql \
    mariadb-server \
    mysql-server \
    mysql-client \
    mysql-common \
    libmysql++-dev \
    libopenjpeg-dev \
    default-libmysqlclient-dev \
    libjpeg-dev \
    libdmtx-dev \
    zlib1g-dev \
    libusb-1.0-0 \
    libusb-1.0-0-dev \
    cifs-utils \
    libsane-dev
```

#### Step 7: Install Lifespan Machine dependencies using the provided source code

Some software had to be modified to make the lifespan machine code work. 
This source code can be found in `external_compile_libraries`.

```
cd external_compile_libraries/libtiff-4.0.9
cmake .
sudo make install

cd ../openjpeg-2.3.0/
cmake .
sudo make install

cd ../sane-backends-1.0.27
./configure
sudo make
sudo make install
cd ../..
```

#### Step 8: Actually installing the Lifespan Machine

```
cd build 
cmake . -DONLY_IMAGE_ACQUISITION=1
sudo make install 
cd ..
```

#### Step 9: Setup long-term storage

Raw data generated from the lifespan machine requires a reasonable amount of free hard disk space. You can use a large external hard drive or use the network accessible storage if your institute provides one. 

##### Option 1: Use an external hard-drive

1: Get the external hard-drive name

Insert the hard-drive and check the name of the device (usually something like `sda1`). 

```
sudo fdisk -l
```

In this tutorial we will call our hardrive `sda1`, but do change the name so that it matches yours.

2. Add mount instructions.

Add the following line to `/etc/fstab` You need to `sudo` to get in there. Replace `ext4` by the hard-drive format.

```
/dev/sda1    /mnt/isis2    ext4    noauto    0    0
```

Be wary that if you mess up this file, you won't be able to boot into the pi again. Don't hesitate to create a backup of the file (_e.g._ `/etc/fstab.bak`) that you can also use in emergency mode.

3. Mount the hard-drive

```
sudo mkdir /mnt/isis2
sudo mount /mnt/isis2
sudo mkdir -p /mnt/lifespan_machine
```

##### Option 2: Mount a network accessible storage (NAS) for long-term storage

1. Add mount instructions.

Add the following line to `/etc/fstab` You need to `sudo` to get in there.

```
//isis2/users/username/directoryname /mnt/isis2 cifs credentials=/root/.cifspass 0 0
```

2. Create a file that automatically provides credentials to the server. 

The file should be `/root/.cifspass` and contain the following lines.

```
username=my=username
password=my-password
domain=my-domain-name
```

Again, don't forget to use `sudo`.

3. Mount the network folder

```
sudo mkdir /mnt/isis2
sudo mount /mnt/isis2
sudo mkdir /mnt/isis2/lifespan_machine
```

#### Step 10: Configure the image server software

Place the `ns_image_server.ini` file in `/usr/local/etc`. 
An example file is provided in `./doc/ns_image_server.ini.example.pi`
Feel free to modify passwords to your needs.

```
cp ./doc/ns_image_server.ini.example.pi /usr/local/etc/ns_image_server.ini 
```

#### Step 11: Change mySQL memory usage default settings

Create/modify the file /etc/mysql/mariadb.cnf so that it contains the following lines.

```
# The MariaDB configuration file
#
# The MariaDB/MySQL tools read configuration files in the following order:
# 1. "/etc/mysql/mariadb.cnf" (this file) to set global defaults,
# 2. "/etc/mysql/conf.d/*.cnf" to set global options.
# 3. "/etc/mysql/mariadb.conf.d/*.cnf" to set MariaDB-only options.
# 4. "~/.my.cnf" to set user-specific options.
#
# If the same option is defined multiple times, the last one will apply.
#
# One can use all long options that the program supports.
# Run program with --help to get a list of available options and with
# --print-defaults to see which it would actually understand and use.

#
# This group is read both by the client and the server
# use it for options that affect everything
#
[client-server]

# Import all .cnf files from configuration directory
!includedir /etc/mysql/conf.d/
!includedir /etc/mysql/mariadb.conf.d/

[mysqld]
key_buffer              = 16M  
read_buffer             = 60K  
sort_buffer             = 1M  
innodb_buffer_pool_size = 64M  
tmp_table               = 8M  
max_allowed_packet      = 16M  
thread_stack            = 192K  
thread_cache_size       = 8  

# This replaces the startup script and checks MyISAM tables if needed
# the first time they are touched

myisam-recover         = BACKUP  
max_connections        = 10

```

#### Step 12: Start the mySQL database software

Set-up MariaDB

```
sudo systemctl enable mariadb # Set the database to always run 
sudo systemctl start mariadb # Run the database 
sudo mysqladmin -u root password [password] # Set the mysql root password 
```
#### Step 13: Create and configure the lifespan MySQL databases

Create and configure databases by typing the following command. The username should be root and the password is the one you specified above.

```
sudo ns_image_server create_and_configure_sql_db
```

You should now type in the following command to update the schema.

```
sudo ns_image_server update_sql
```

#### Step 14: Setup the lifespan web interface

The web interface consists of a set of PHP scripts. These are located in the `web_interface` subdirectory. They need to be copied to the Linux directory used by the web browser. This is located at `/var/www/html`.

```
sudo mkdir /var/www/
sudo mkdir /var/www/html
sudo cp -r ./web_interface/* /var/www/html
```

This web interface has a configuration file called ns_image_server_website.ini . An example configuration file is included in the git repository. In the previous step, you copied it to `/var/www/html/image_server_web/`

You can use this template to create a configuration file specific to system. Typing the command 

```
sudo cp /var/www/html/image_server_web/ns_image_server_website_template.ini /var/www/html/image_server_web/ns_image_server_website.ini
```

You may also have to grant permission to the files to access the website.

```
sudo chmod 755 /var/www/
```

You'll need to change the file so it contains the SQL database. Feel free to change it to your liking. If after changing the file, the website stops working, check the apache2 logs with the command.

```
sudo tail /var/log/apache2/error.log
```

You can also try to restart the apache2 server.

```
sudo systemctl restart apache2
```

You can now access the web interface on the Raspberry Pi by typing `http://localhost` in a web browser. Within the network, the web interface is accessible using the Raspberry Pi IP address.

Type the following command to get that IP address.

```
ifconfig | awk '$1=="inet" && $2!="127.0.0.1"{print $2}'
```

#### Step 15: Script that automatically outputs IP address to file

The can come in handy if you want to SSH in the Rasberry Pi without having to use a computer screen to figure out the IP address. Using `cron`, a time-based job scheduler, you can have the IP address written to a file located in the home directory every minute.

In the terminal, open `cron` by typing

```
crontab -e
```

Now add a line to that file

```
* * * * * /sbin/ifconfig | /usr/bin/awk '$1 == "inet" && $2 != "127.0.0.1" {print $2}' > /home/pi/ip.txt
```

#### Step 16: Run the image 

If everything went smoothly, you should be able to run the image server running the following command without getting an error message.

```
sudo ns_image_server
```

If you are running your Rapsberry Pi using SSH. Think of using `screen` to not have the image server dependant on your computer being turned on. 

## Update

Pull the updated source code using `git pull` in the command line. You may need to type `git stash` before. Finally, build the software using `cmake` as specified below.

```
cd build 
cmake . -DONLY_IMAGE_ACQUISITION=1
sudo make install 
cd ..
```

## Setting-up scanners

Now that you have installed the lifespan machine, you should connect the adapted scanners. Each scanner needs to be assigned a unique name. Scanners names should be three or more characters long. These names are given to the Pi by generating barcodes and placing them inside surface of the bottom scanner glass.

Barcodes are generated using the following command.

```
ns_image_server_barcodes –c mybarcodes.tif scannername1 scannername2
```

## Running an experiment

Experiments can be run from the command line using the following command.

```
ns_image_server submit experiment experiment.xml u
```

An example experiment XML file can be found in `./doc/example_experiment.xml`.

## Clone a microSD Card

This explains how we got the Lifespan Machine Pi image. It's also useful to know how to do this if you wish to backup your Pi for whatever reason. The explanation here holds for Linux but probably also works for MacOSX.

The idea is to use `dd` to simply create an image the microSD card by copying information byte-by-byte. 

**Please be aware that mistakes can lead to data loss! Be sure of what you are doing!**

#### Step 1: Get the card name

Insert the card and check the name of the device (usually something like `sda`). 

```
sudo fdisk -l
```

In this tutorial we will call our SD card `sda`, but do change the name so that it matches yours. **Be aware that this name can change if you unplug your SD card and plug it back in.**

#### Step 2: Create the image

Next unmount the microSDcard (change the name of microSD card to match yours).

```
sudo umount /dev/sda
```

Now create the image using `dd`. This operation is quite slow.

```
sudo dd if=/dev/sda of=lifespan_raspberry.img bs=100M
```
You can monitor progress in Ubuntu 16.04 and above using

```
sudo dd if=/dev/sda of=lifespan_raspberry.img status=progress bs=100M
```
