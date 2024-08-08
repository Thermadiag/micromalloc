The *mp* command line tool forces an executable to link with the *micro_proxy* library, either with LD_PRELOAD-like tricks or using DLL injection on Windows. 
It provides a simple way to test foreign applications with the micro allocator, and/or to output some allocation statistics.

Usage
-----

Simple usage on Linux (open a file with gedit):
```console
mp gedit my_file.txt
```
This will launch gedit tool and pass the *my_file.txt* argument to the software.

It is possible to pass arguments to *mp* before the actual command line to launch. These arguments MUST be on the form *MICRO_...* and use the same names as environment variables used to configure micro library.

Another example that will print some memory statistics at exit of gedit:

```console
mp MICRO_PRINT_STATS=stdout MICRO_PRINT_STATS_TRIGGER=1 gedit my_file.txt
```

The *mp* tool can launch programs using a file as memory provider. In the below example, we start the Kate text editor in this mode, and specify the folder in which page files are created:

```console
mp MICRO_PRINT_STATS=stdout MICRO_PRINT_STATS_TRIGGER=1 MICRO_PROVIDER_TYPE=3 MICRO_PAGE_FILE_PROVIDER_DIR=~/pages MICRO_PAGE_FILE_FLAGS=1 kate
```

The page provider type is set to 3 (file provider) using the MICRO_PROVIDER_TYPE environment variable.
We use a MICRO_PAGE_FILE_FLAGS of 1 to allow file growing on memory demand.
All created page files are stored in the folder '~/pages' that must already exist.