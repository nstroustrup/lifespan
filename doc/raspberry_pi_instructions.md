# Lifespan Machine on a Raspberry Pi

This document was written by [Olivier Martin](mailto:oliviermfmartin@gmail.com) in 2018 using Raspberry Pi Model B+, Raspbian 4.14 and the Lifespan Machine 2.0.3.

## Introduction

The following items are described in this document.

1. Installation;
2. Running an experiment;
3. Setting-up scanners;
4. Cloning an SD card.


Please note that this requires a second computer (Linux or Windows) with an internet connection, a microSD card, a microSD-to-USB adapter and obviously a Raspberry Pi with adapted Epson scanners.

This requires using the Raspberry Pi terminal which can be opened using `Ctrl-Alt-T`. A basic knowledge of `shell` and of some text-editor (e.g. `nano` or `vi`) can help a lot.

Please do not copy-paste all the code blocks directly in the terminal window. Some commands require user input. Please do copy them one-by-one.

## Installation

Here we describe the steps necessary to get from a Raspberry Pi with an empty SD card to a functional image acquiring machine. 

There are two ways to proceed: 
    1. use the cloned microSD image; 
    2. install everything from scratch.

### From the cloned microSD image

Here we describe how to install everything using a cloned microSD card (`raspberry_lifespan.img`). Please note that instruction differ for Linux and Windows and that **your microSD card must be have at least 32GB.**

To learn how to clone the Pi's microSD card into an `img` file, see the corresponding section.

#### Step 1: Clone the `img` file to the microSD card

##### Linux

Insert the card and check the name of the device (usually something like `sdb1`). 

```
sudo fdisk -l
```

In this tutorial we will call our SD card `mysd`, but do change the name so that it matches yours. **Be aware that this name can change if you unplug your SD card and plug it back in.**

You should now unmount the card.

```
sudo umount /dev/mysd
```

Now create the image using `dd`. This operation is quite slow.

```
sudo dd if=raspberry_lifespan.img of=/dev/mysd
```

You can monitor progress in Ubuntu 16.04 and above using

```
sudo dd if=raspberry_lifespan.img of=/dev/mysd status=progress
```

#### Windows

1. Download [Win32 Disk Imager](https://sourceforge.net/projects/win32diskimager/)

2. Use Win32 Disk Imager to clone the `img` file to your microSD card.

#### Step 2: Expand the partition

Using a software like `gparted`, expand the rootfs partition so it fills up the whole SD card. You can now plug in the SD card in the Pi and start it.

#### Step 3: Configuration

Follow steps 2, 3 and 9-12 as if you were installing the Pi from scratch.

#### Step 4: Configure the web interface

Change the `/var/www/html/image_server_web/ns_image_server_website.ini` file to meet your needs.

### From scratch

#### Step 1: Installing the OS on the Raspberry Pi

- Download [Raspbian](https://www.raspberrypi.org/downloads/raspbian/) and unzip the archive.
- Download [Etcher](https://etcher.io/)
- Write the Raspian image to the microSD card using Etcher.
- More information about the installation process [here] (https://www.raspberrypi.org/documentation/installation/installing-images/README.md)

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

Click on the Raspberry on the top left corner of the screen, 
go to Preference/Raspberry Pi Configuration/Interfaces
and enable SSH and VNC.

#### Step 5: Get the Lifespan Machine source code from the Github repository 

```
git clone https://github.com/nstroustrup/lifespan # Run the following command to clone directory
cd lifespan 
git checkout flow # Switch to the flow branch
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
    libjpeg-dev \ 
    libdmtx-dev \
    zlib1g-dev \
    libusb-1.0-0 \
    libusb-1.0-0-dev \
    cifs-utils
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

In this tutorial we will call our hardrive `myhd`, but do change the name so that it matches yours.

2. Add mount instructions.

Add the following line to `/etc/fstab` You need to `sudo` to get in there. Replace `ext4` by the hard-drive format.

```
/dev/myhd    /mnt/isis2    ext4    0    0
```

Be wary that if you mess up this file, you won't be able to boot into the pi again. 

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
sudo mkdir -p /mnt/lifespan_machine
```

#### Step 9: Configure the image server software

1. Replace fields in the `ns_image_server.ini` file on desktop. If you wish to run the Central SQL database on the Pi, you should replace `central_sql_hostname` to `localhost`.

2. Place the `ns_image_server.ini` file in `/usr/local/etc`

```
sudo mv ~/Desktop/ns_image_server.ini /usr/local/etc
```

#### Step 11: Start the mySQL database software

Set-up MariaDB

```
sudo systemctl enable mariadb # Set the database to always run 
sudo systemctl start mariadb # Run the database 
sudo mysqladmin -u root password [password] # Set the mysql root password 
```
#### Step 12: Create and configure the lifespan mySQL databases

Create and configure databases by typing the following command. The username should be root and the password is the one you specified above.

```
sudo ns_image_server create_and_configure_sql_db
```

You should now type in the following command to update the schema.

```
sudo ns_image_server update_sql
```

#### Step 13: Setup the lifespan web interface

The web interface consists of a set of PHP scripts. These are located in the `web_interface` subdirectory. They need to be copied to the linux directory used by the web browser. This is located at `/var/www/html`.

```
sudo mkdir /var/www/
sudo mkdir /var/www/html
cp –r ~/lifespan/web_interface/* /var/www/html
```

This web interface has a configuration file called ns_image_server_website.ini . An example configuration file is included in the git repository. In the previous step, you copied it to `/var/www/html/image_server_web/`

You can use this template to create a configuration file specific to system. Typing the command 

```
cp /var/www/html/image_server_web/ns_image_server_website_template.ini /var/www/html/image_server_web/ns_image_server_website.ini
```

You can now access the web interface on the Raspberry Pi by typing `http://localhost` in a web browser. Within the network, the web interface is accessible using the Raspberry Pi IP address.

Type `ifconfig | awk '$1=="inet" && $2!="127.0.0.1"{print $2}'` to get that IP address.

#### Step 14: Run the image If everything went smoothly, you should be able to run the image server running the following command without getting an error message.

```
sudo ns_image_server
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

An example experiment XML file can be found on the desktop.

## Clone a microSD Card

This explains how we got the Lifespan Machine Pi image. It's also useful to know how to do this if you wish to backup your Pi for whatever reason. The explanation here holds for Linux but probably also works for MacOSX.

The idea is to _use_ `dd` to simply create an image the microSD card by copying information byte-by-byte. 

**Please be aware that mistakes can lead to data loss! Be sure of what you are doing!**

#### Step 1: Get the card name

Insert the card and check the name of the device (usually something like `sdb1`). 

```
sudo fdisk -l
```

In this tutorial we will call our SD card `mysd`, but do change the name so that it matches yours. **Be aware that this name can change if you unplug your SD card and plug it back in.**

#### Step 2: Create the image

Next unmount the microSDcard (change the name of microSD card to match yours).

```
sudo umount /dev/mysd
```

Now create the image using `dd`. This operation is quite slow.

```
sudo dd if=/dev/mysd of=~/sd-card-copy.img
```
You can monitor progress in Ubuntu 16.04 and above using

```
sudo dd if=/dev/mysd of=~/sd-card-copy.img status=progress
```
