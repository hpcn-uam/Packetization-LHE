#!/bin/bash

#hace 10 capturas de pantalla lo más rápido posible y despés las guarda en png 

	sudo mount -t tmpfs -o size=512m tmpfs /mnt/ramdisk/capturas/
xwd -root >> "/mnt/ramdisk/capturas/1"
convert xwd:"/mnt/ramdisk/capturas/1" "/mnt/ramdisk/capturas/1.png"
rm -rf "/mnt/ramdisk/capturas/1"
	#taskset -c 0 xwd -root >> "/mnt/ramdisk/capturas/1"
	#taskset -c 2 xwd -root >> "/mnt/ramdisk/capturas/2"
	#taskset -c 4 xwd -root >> "/mnt/ramdisk/capturas/3"
	#taskset -c 6 xwd -root >> "/mnt/ramdisk/capturas/4"
	#taskset -c 1 xwd -root >> "/mnt/ramdisk/capturas/5"
	#taskset -c 3 xwd -root >> "/mnt/ramdisk/capturas/6"
	#taskset -c 5 xwd -root >> "/mnt/ramdisk/capturas/7"
	#taskset -c 7 xwd -root >> "/mnt/ramdisk/capturas/8"

	#taskset -c 0 convert xwd:"/mnt/ramdisk/capturas/1" "/mnt/ramdisk/capturas/1.png"
	#taskset -c 2 convert xwd:"/mnt/ramdisk/capturas/2" "/mnt/ramdisk/capturas/2.png"
	#taskset -c 4 convert xwd:"/mnt/ramdisk/capturas/3" "/mnt/ramdisk/capturas/3.png"
	#taskset -c 6 convert xwd:"/mnt/ramdisk/capturas/4" "/mnt/ramdisk/capturas/4.png"
	#taskset -c 1 convert xwd:"/mnt/ramdisk/capturas/5" "/mnt/ramdisk/capturas/5.png"
	#taskset -c 3 convert xwd:"/mnt/ramdisk/capturas/6" "/mnt/ramdisk/capturas/6.png"
	#taskset -c 5 convert xwd:"/mnt/ramdisk/capturas/7" "/mnt/ramdisk/capturas/7.png"
	#taskset -c 7 convert xwd:"/mnt/ramdisk/capturas/8" "/mnt/ramdisk/capturas/8.png"
	#
	#taskset -c 0 rm -rf "/mnt/ramdisk/capturas/1"
	#taskset -c 2 rm -rf "/mnt/ramdisk/capturas/2"
	#taskset -c 4 rm -rf "/mnt/ramdisk/capturas/3"
	#taskset -c 6 rm -rf "/mnt/ramdisk/capturas/4"
	#taskset -c 1 rm -rf "/mnt/ramdisk/capturas/5"
	#taskset -c 3 rm -rf "/mnt/ramdisk/capturas/6"
	#taskset -c 5 rm -rf "/mnt/ramdisk/capturas/7"
	#taskset -c 7 rm -rf "/mnt/ramdisk/capturas/8"
