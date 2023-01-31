# Developer Notes

## Creating a new release

* update `src/version.txt` and `uni_version.h`
* update `CHANGELOG.md`
* update `AUTHORS`
* merge `main` into `develop`... and solve possible conflicts on `develop` first
* then merge `develop` into `main`

  ```sh
  git merge main
  # Solve possible conflicts
  git checkout main
  git merge develop
  ```

* generate a new tag

  ```sh
  git tag -a release_v2.4.0
  ```

* push changes both to gitlab and github:

  ```sh
  git push gitlab
  git push gitlab --tags
  git push github
  git push github --tags
  ```

* Generate binaries

  ```sh
  cd tools/fw
  ./build.py --set-version 2.4.0 all
  ```
  
* And generate the release both in gitlab and github, and upload the already generated binaries

## Analizing a core dump

Since v2.4.0, core dumps are stored in flash. And they can be retreived using:

 ```sh
 # Using just one command
 espcoredump.py --port /dev/ttyUSB0 --baud 921600 info_corefile build/bluepad32-app.elf
 ```

```sh
# Or it can be done in two parts
esptool.py --port /dev/ttyUSB0 read_flash 0x110000 0x10000 /tmp/core.bin
espcoredump.py info_corefile --core /tmp/core.bin --core-format raw build/bluepad32-app.elf 
 ```

## Analyzing Bluetooth packets

Use the "pc_debug" platform:

```sh
cd tools/pc_debug
make
sudo ./bluepad32
```

Let it run... stop it... and open the logs using:

```sh
wireshark /tmp/hci_dump.pklg
```
