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
* Install czmq 3.0.0
* Install protobuf 2.6.1
* Install protobuf-c (HEAD of git repo should be okay. In this case, you must install _libtool_, then run `./autogen.sh`)

VHPC uses protobuf for communication means. Therefore, the .proto files must be compiled beforehand.

```bash
cd utils
protoc-c *.proto --c_out=.
cd ..
```

Then, to compile the source code.

```bash
cmake .
make
```

Now there are two executables available each for organizer and executer. Please keep in mind that it's better to run executer first. You could run each of them like this:

```bash
executer/executer
organizer/organizer
```
