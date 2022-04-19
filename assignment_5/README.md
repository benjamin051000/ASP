# Assignment 5

## Build
Run `make` in this directory.

## Initialize Module
```bash
$ sudo insmod char_driver.ko [NUM_DEVICES=num]
```

## Run tests
```bash
$ make app
$ sudo ./userapp <device minor number>
```

## Unload module
```bash
$ sudo rmmod char_driver
```
# VSCode Intellisense Config
Use the following `.vscode/c_cpp_properties.json` for proper intellisense:

https://gist.github.com/benjamin051000/11b53c96baa11d9d51329d1b98648749

