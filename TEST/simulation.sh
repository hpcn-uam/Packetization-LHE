#!/bin/bash
# LHE - VMAF video quality testbench for Racing Drones Project
#Author: Angel Lopez
#calcula el numero de tamaños de bloque válidos para la versíon actual, para un determinado rango de píxeles por bloque

#IMAGE_W=1280
#IMAGE_H=720
#IMAGE_W=854
#IMAGE_H=480
IMAGE_W=640
IMAGE_H=360

PIX_MAX=1024
PIX_MIN=320

IMAGE_BSIZE_MAX=2880

BLOCK_L_MIN=32
BLOCK_H_MIN=1
BLOCK_H_MAX=16

block_length=$BLOCK_L_MIN
block_l_max=$IMAGE_W
I=0

clear

echo -ne \\n\\t"B_W"\\t"B_H"\\t"#Pix"\\t"P-Area_Rat"\\n\\n

while [ $block_length -le $block_l_max ]
do
	if [[ ($(($IMAGE_W%$block_length)) -eq 0) && ($(($block_length%16)) -eq 0) ]]
	then
		block_height=$BLOCK_H_MIN
		while [ $block_height -le  $BLOCK_H_MAX ]
		do
			if [[ ($block_height -le $block_length) && ($(($IMAGE_H%$block_height)) -eq 0) && (($(($block_height%2)) -eq 0) && ($block_height -le 16) || ($block_height -eq 1)) && ($(($block_length*$block_height)) -le $PIX_MAX) && ($(($block_length*$block_height)) -ge $PIX_MIN) && ($((($IMAGE_W*$IMAGE_H)/($block_length*$block_height))) -le $IMAGE_BSIZE_MAX) ]]		
			then
				ratio=$( bc <<< "scale=4; ((( 2 * $block_length + 2 * $block_height) / ($block_length * $block_height )))") 
				echo -e \\t   $block_length\\t   $block_height\\t   $(($block_length*$block_height))\\t   $ratio\\n 
				let I++
			fi
			let block_height++
		done
	fi
	let block_length++
done

echo -ne \\n Cantidad total de tamaños de bloque válidos: $I\\n\\n