# SimGrid

SimGrid is a Versatile Simulation of Distributed Systems.
To Download latest version of it use this [link][Download-simgrid].

### Installation
The installation Documnt could be found [here][Install-simgrid].
### Some common errors and solutions
Error 1:
FL_LIBRARY(ADVANCE) NOTFOUND
```sh
$ sudo yum install flex-devel
```
Error 2:
! LaTeX Error: File `titlesec.sty' not found.
```sh
$ sudo yum install texlive
$ sudo yum install texlive-titlesec
```
Error 3:
```sh
#include <PajeComponnet.h> , no such file or directory"
```
```sh
$ export PAJENG_PATH /pajeng-install-path
```
for example
```sh
$ export PAJENG_PATH /home/os932/Simgrid/simgrid/pajeng/pajeng/
```
and then rerun cmake command!

Error 4:
```sh
#include <PajeComponnet.h> , no such file or directory"
```
```sh
$ sudo yum install texlive
$ sudo yum install texlive-titlesec
```

#### tip
To install paje use the script  **paje_installer.sh**
That could be find here: https://github.com/dosimont/paje_installer

##License


[Download-simgrid]:http://simgrid.gforge.inria.fr/download.html
[Install-simgrid]:http://simgrid.gforge.inria.fr/simgrid/latest/doc/install.html