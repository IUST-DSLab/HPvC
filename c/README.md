C Implementation
================

Running
-------

* Install CMake
* Download the virtualbox SDK with respect to your VirtualBox version. Extract the zip file under the name `sdk/` (which is the default).
* Add these two lines to your `.bashrc` file

```bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig
. ~/.bashrc
```

* Install zeromq 4.1.0

```bash
cd ~/Downloads
wget http://download.zeromq.org/zeromq-4.1.0.tar.gz
tar xzf zeromq-4.1.0.tar.gz
cd zermq-4.1.0
./configure
make
sudo make install
```

* Install czmq 3.0.0

```bash
cd ~/Downloads
wget http://download.zeromq.org/czmq-3.0.0-rc1.tar.gz
tar xzf czmq-3.0.0-rc1.tar.gz
cd czmq-3.0.0
./configure
make
sudo make install
```

* Install protobuf 2.6.1

```bash
wget https://github.com/google/protobuf/releases/download/v2.6.1/protobuf-2.6.1.tar.gz
tar xzf protobuf-2.6.1.tar.gz
cd protobuf-2.6.1
./configure
make
sudo make install
```

* Install protobuf-c (HEAD of git repo should be okay. In this case, you must install _libtool_, then run `./autogen.sh`)

```bash
wget https://github.com/protobuf-c/protobuf-c/archive/master.zip
unzip master.zip
cd master
./autogen.sh
./configure
make
sudo make install
```

HPvC uses protobuf for communication means. Therefore, the .proto files must be compiled beforehand.

```bash
cd utils
protoc-c *.proto --c_out=.
cd ..
```

Then, compile the source code.

```bash
cmake .
make
```

Now there are two executables available each for organizer and executer. Please keep in mind that it's better to run executer first. You could run each of them like this:

```bash
executer/executer
organizer/organizer
```
