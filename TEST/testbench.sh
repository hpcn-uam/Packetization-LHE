#!/bin/bash
# LHE - VMAF video quality testbench for Racing Drones Project
#Author: Angel Lopez

#############
#SETUP PATHS#
#############
#program paths
	#Directory where packetizer (sender/receiver) program is located
	PACKETIZER_PATH="/home/hpcn/Repos/Packetization-LHE"
	#Directory where player program is located
	PLAYER_PATH="/home/hpcn/Repos/Packetization-LHE/LHE_Player"
	#Directory where mockvid generator program is located
	MOCKGEN_PATH="/home/hpcn/Repos/Packetization-LHE/data/LHE_mockvid_generator"
	#Directory where VMAF program is located
	VMAF_PATH="/home/hpcn/Repos/vmaf-hpcn"

#directory/file paths (They should all be different!!)
	#-INPUT-#
	#Path of original mp4 videos
	ORIGINAL_PATH="/home/hpcn/Repos/Packetization-LHE/TEST/original_path"
	#Directory where input .lhe frames are to be stored
	IN_LHE_PATH="/mnt/ramdisk/in_lhe_path/"
	#Directory where input .png frames are to be stored
	IN_PNG_PATH="/home/hpcn/Repos/Packetization-LHE/TEST/in_png_path"
	#Path of input .mp4 video 
	IN_MP4_PATH="/home/hpcn/Repos/Packetization-LHE/TEST/in_mp4_path/in_mp4.mp4"
	IN_MP4_PATH2="/home/hpcn/Repos/Packetization-LHE/TEST/in_mp4_path2/in_mp4.mp4"
	#Path of input .yuv video
	IN_RAW_PATH="/home/hpcn/Repos/Packetization-LHE/TEST/in_raw_path/in_raw.yuv"
	#-RECEIVED-#
    #Directory where received .lhe frames are to be stored
    LHE_PATH="/mnt/ramdisk/lhe_path"
	#Directory where received .png frames are to be stored
	PNG_PATH="/home/hpcn/Repos/Packetization-LHE/TEST/png_path"
	#Directory where received .mp4 videos are to be stored
	MP4_PATH="/home/hpcn/Repos/Packetization-LHE/TEST/mp4_path"
	MP4_PATH2="/home/hpcn/Repos/Packetization-LHE/TEST/mp4_path2"

	#Directory where received .yuv videos are to be stored
	RAW_PATH="/home/hpcn/Repos/Packetization-LHE/TEST/raw_path"
	#-VMAF-#
	#Directory where batch file () is to be stored
	BATCH_PATH="/home/hpcn/Repos/Packetization-LHE/TEST/batch_path/batchfile"
	#Directory where VMAF scores file () is to be stored
	QSCORES_PATH="/home/hpcn/Repos/Packetization-LHE/TEST/qscores_path/qscores.csv"
	#Directory where final results file () is to be stored
	RESULTS_PATH="/home/hpcn/Repos/Packetization-LHE/TEST/results_path"

	FFMPEG_LOG="/home/hpcn/Repos/Packetization-LHE/TEST/ffmpeg_log.txt"

	PARTIALR_PATH="/home/hpcn/Repos/Packetization-LHE/TEST/partial_results.csv"

#################
#SETUP VARIABLES#
#################
#sweep parameters
	declare -a VIDEO_LIST=("fpv_youtube_cut.mp4") #"racing_drones_1.mp4" "racing_drones_2.mp4"
	
	REPS=1 #Repetitions of the experiment (optimal ~30, reasonable ~15)
	 
	declare -a SWEEP_RESOLUTION=("360p" "480p" "720p")
	
	FPS_MIN=45
	FPS_MAX=45
	FPS_STEP=15

	declare -a SWEEP_PROFILES=("yuv422p" "yuv420p" "gray")
	declare -a SWEEP_FPS=(15 30 60)
	#block size
	PIX_MAX=640 #1024 #max amount of pixels in a block
	PIX_MIN=640 #min amount of pixels in a block

	IMAGE_BSIZE_MAX=2880 #max amount of blocks per image (defined in LHE_receiver.h)

	BLOCK_L_MIN=640 #min block width (length)
	BLOCK_H_MIN=1 #min block height
	BLOCK_H_MAX=1 #max block height 16

	#net loss, fine step
	NET_F_MIN=10
	NET_F_MAX=20
	NET_F_STEP=5
	#net loss, coarse step
	SWEEP_COARSE=0 #sweeps if 1
	NET_C_MIN=15
	NET_C_MAX=25
	NET_C_STEP=5

	#packet size
	PACK_MIN=1480
	PACK_MAX=1480
	PACK_STEP=340
	#PACK_MIN=800
	#PACK_MAX=800
	#PACK_STEP=0

#network variables
	LOCAL_IP="127.0.0.1"
	RECEIVER_IP="150.244.58.163"
	SENDER_IP="" #optional
	PORT=6660

#DEFAULTS
	VIDEO="fpv_youtube_cut.mp4"
	WIDTH=1280 #Frame width
	HEIGHT=720 #Frame Height
	YUV_PROFILE="yuv422p" # yuv444p | yuv422p | yuv420p
	FPS_RATE=60 #Video's framerate in Frames Per Second
	FRAME_N=1800 #Number of frames in the video (= video duration in S x FPS_rate)

##################
#GLOBAL VARIABLES#
##################
	NETEM_ON=0 
	NETEM_LOCAL_ON=0
	RECEIVER_ON=0
	PLAYER_ON=0

	NUMVIDS=0

###########
#FUNCTIONS#
###########

	trap 'kill -2 $pid_player $pid_receiver $pid_sender; wait $pid_player $pid_receiver $pid_sender 2>/dev/null; sudo tc qdisc del dev ifb0 root netem 2>/dev/null; sudo tc qdisc del dev eth0 ingress 2>/dev/null; sudo tc qdisc del dev lo ingress 2>/dev/null; delete_file temp_s >/dev/null; delete_file temp_r >/dev/null; echo -ne \\n\\nQUIT!\\n\\n; exit' SIGINT

	function delete_path(){
    #Deletes everything inside path $1
    	echo -ne Deleting files in $1...\\t 

		rm -rf $1/* #-f to ingnore if directory is empty
		wait

		echo -ne [OK!]\\n
    }

    function delete_file(){
    #Deletes everything inside path $1
    	echo -ne Deleting file $1...\\t 

		rm -f $1 #-f to ingnore if nonexistent
		wait

		echo -ne [OK!]\\n
    }

	function init_net_loss(){
	#netem setup of a bridge
		if [ $NETEM_ON = 0 ]
    	then
	    	echo -ne Init net loss ...\\t

	    	sudo modprobe ifb
			sudo ip link set dev ifb0 up
			sudo tc qdisc add dev eth0 ingress
			sudo tc filter add dev eth0 parent ffff: protocol ip prio 1 u32 match ip src 0.0.0.0/0 match ip dport $PORT 0xffff flowid 1:1 action mirred egress redirect dev ifb0
			sudo tc qdisc add dev ifb0 root netem loss 0%
			wait

			NETEM_ON=1

			echo -ne [OK!]\\n
		fi
    }
    function set_net_loss(){
    #Sets netem bridge loss 
    	if [ $NETEM_ON = 1 ]
    	then
    		echo -ne Setting net loss to $1% ...\\t
    		
    		sudo tc qdisc change dev ifb0 root netem loss $1%
    		#from man: 
    		#It is also possible to add a  corre‐lation, but this option is
    		#now deprecated due to the noticed bad behavior.
    		wait
    		
    		echo -ne [OK!]\\n
    	fi
    }
    function finish_net_loss(){
    #Deactivates netem bridge
    	if [ $NETEM_ON = 1 ]
    	then
    		echo -ne Finishing net loss ...\\t
    		sudo tc qdisc del dev ifb0 root netem
    		sudo tc qdisc del dev eth0 ingress
    		wait

    		NETEM_ON=0
    		echo -ne [OK!]\\n
    	fi
    }
	function init_local_loss(){
	#netem setup on loopback
		if [ $NETEM_LOCAL_ON = 0 ]
    	then
	    	echo -ne Init net loss ...\\t
			#sudo tc qdisc add dev lo ingress
			#sudo tc qdisc add dev lo root handle 1:0 netem loss 0%
			#sudo tc filter add dev lo parent ffff: protocol ip prio 1 u32 match ip src 0.0.0.0/0 match ip dport 6660 0xffff
			
			sudo modprobe ifb
			sudo ip link set dev ifb0 up
			sudo tc qdisc add dev lo ingress
			sudo tc filter add dev lo parent ffff: protocol ip prio 1 u32 match ip src 0.0.0.0/0 match ip dport $PORT 0xffff flowid 1:1 action mirred egress redirect dev ifb0
			sudo tc qdisc add dev ifb0 root netem loss 0%
			wait

			NETEM_LOCAL_ON=1

			echo -ne [OK!]\\n
		fi
	}
	function set_local_loss(){
    #Sets netem loopback loss 
    	if [ $NETEM_LOCAL_ON = 1 ]
    	then
    		#sudo tc qdisc change dev lo root handle 1:0 netem loss $1%

    		sudo tc qdisc change dev ifb0 root netem loss $1%

    		#from man: 
    		#It is also possible to add a  corre‐lation, but this option is
    		#now deprecated due to the noticed bad behavior.
    		wait #WARNING: if loss rate is too high, communication with netem will be slow
    		
    		echo Net loss set to $1%
    	fi
    }
    function finish_local_loss(){
    #Deactivates netem on loopback
    	if [ $NETEM_LOCAL_ON = 1 ]
    	then
    		echo -ne Finishing net loss ...\\t
    		#sudo tc qdisc del dev lo root

			sudo tc qdisc del dev ifb0 root netem
    		sudo tc qdisc del dev lo ingress

    		wait #WARNING: if loss rate is too high, communication with netem will be slow
    		
    		NETEM_LOCAL_ON=0
    		echo -ne [OK!]\\n
    	fi
    }

   	function set_player(){
   	#Setups player on play mode (no png files)
		if [ $PLAYER_ON = 0 ]
    	then
	   		cd $PLAYER_PATH

	   		taskset -c 7 ./LHEplayer --decode $LHE_PATH --play --keepopen &
	   		pid_player=$! 
	   		PLAYER_ON=1  		
	   		
	   		echo Player set on $LHE_PATH
	   	fi
   	}
   	function kill_player(){
   	#Kills player.
		if [ $PLAYER_ON = 1 ]
    	then
			echo -ne Killing player ...\\t
			
			kill $pid_player
			wait $pid_player 2>/dev/null #this removes annoying kill output
			PLAYER_ON=0

			echo -ne [OK!]\\n
		fi
   	}
    function set_receiver(){
	#Setups receiver and begins listening. 
    #Receiver program is an infinite loop -> kill it.
   		echo setting receiver...
		if [ $RECEIVER_ON = 0 ]
    	then
	    	cd $PACKETIZER_PATH
	    	taskset -c 2 ./LHEPacketizer.out --directory $LHE_PATH --receiver2 $SENDER_IP --port $PORT > temp_r 2>/dev/null &
	    	pid_receiver=$!
	    	sleep 2 #can't wait, so sleep a bit to prevent running the sender before the receiver (it happens)

	    	RECEIVER_ON=1

			echo Receiver set on $LHE_PATH
		fi
    }
    function kill_receiver(){
    #Kills receiver. receiver returns measured losses
		if [ $RECEIVER_ON = 1 ]
    	then
			echo -ne Killing receiver ...\\t

			sleep 2 #to catch late packets
			kill -2 $pid_receiver
			wait $pid_receiver 2>/dev/null #this removes annoying kill output

			#packets_received=$(<$PACKETIZER_PATH/temp_r)

			while IFS=", " read -r packets_received block_loss
			do
				echo #not executed
			done < $PACKETIZER_PATH/temp_r
			wait

			RECEIVER_ON=0

			echo -ne [OK!]\\n
		fi
    }
    function set_local_sender(){
	#Setups sender @local and begins transmission. 
    #Sender program finishes when all frames have been sent.    	

		    echo -ne Sending frames in $IN_LHE_PATH with max $1 B packets...\\t
		 
			taskset -c 5 ./LHEPacketizer.out --directory $IN_LHE_PATH --sender $LOCAL_IP --port $PORT --max-chunk-size $1 > temp_s &
		    pid_sender=$!
		    wait $pid_sender

			packets_sent=$(<$PACKETIZER_PATH/temp_s)

		    echo -ne [OK!]\\n
    }
    function set_sender(){
    #Setups sender @ZynqBerry and begins transmission. 
    #Sender program finishes when all frames have been sent.
	    echo -ne Sending frames from ZynqBerry ...\\t

		#ssh -p 8888 hpcn@zend.ii.uam.es &
		#pid_sender=$!
		#wait $pid_sender
		#ssh -p 8888 hpcn@zend.ii.uam.es 'cd /home/hpcn/Packetization-LHE/; ./LHEPacketizer.out --directory /home/hpcn/Packetization-LHE/data/mockvid_youtube_LHE_360p_30s_148x8 --sender '$RECEIVER_IP' --port '$PORT''
		#pid_sender=$!
		#wait $pid_sender
		
		echo -ne [NOT IMPLEMENTED!]\\n
    }

	function get_in_video(){
	#$1 = name of original video
	#$2 = width
	#$3 = height
	#$4 = framerate

		echo -ne Getting input video $IN_MP4_PATH: W:$2 H:$3 FPS:$4...\\t
		num_fr=$((30*$4))
		ffmpeg -i $ORIGINAL_PATH/$1 -vf scale=$2:$3,fps=$4 -frames:v $num_fr $IN_MP4_PATH &>> $FFMPEG_LOG
		wait

		echo -ne [OK!]\\n
	} 
	function get_in_video2(){
	#$1 = name of original video
	#$2 = width
	#$3 = height
	#$4 = framerate

		echo -ne Getting input video $IN_MP4_PATH2: W:$2 H:$3 FPS:$4...\\t

		ffmpeg -i $ORIGINAL_PATH/$1 -vf scale=$2:$3,fps=$4 -frames:v $FRAME_N $IN_MP4_PATH2 &>> $FFMPEG_LOG
		wait

		echo -ne [OK!]\\n
	} 

	function get_in_raw(){
	#Converts input mp4video to raw video using ffmpeg

		echo -ne Converting input mp4 $IN_MP4_PATH2 video to raw video $IN_RAW_PATH...\\t

		ffmpeg -i $IN_MP4_PATH2 -c:v rawvideo -pix_fmt $YUV_PROFILE $IN_RAW_PATH &>> $FFMPEG_LOG
		

		echo -ne [OK!]\\n
	} 

    function mp4_to_raw(){
    #Converts mp4 video to YUV (YUV_PROFILE) raw video using ffmpeg
    #$1 = input .mp4 
    #$1 = output .yuv (same name)

    	echo -ne Converting mp4 video $MP4_PATH/$1.mp4 to raw yuv video $RAW_PATH/$1.yuv...\\t

		ffmpeg -i $MP4_PATH2/$1.mp4 -c:v rawvideo $RAW_PATH/$1.yuv &>> $FFMPEG_LOG
		wait

		echo -ne [OK!]\\n
    }


    function mp4_to_png(){
    #Extracts png frames from input mp4 using ffmpeg

    	echo -ne Extracting png frames from $IN_MP4_PATH in $IN_PNG_PATH ...\\t

		ffmpeg -i $IN_MP4_PATH -frames:v $FRAME_N $IN_PNG_PATH/%05d.png &>> $FFMPEG_LOG
		wait

		echo -ne [OK!]\\n
    }

    function png_to_mp4(){
    #Joins all png frames in an mp4 file using ffmpeg
    	echo -ne Converting png frames in $PNG_PATH to mp4 video $MP4_PATH/$1.mp4 $FPS_RATE FPS, $FRAME_N frames ...\\t
    	
    	cd $PNG_PATH
    	num_fr=$((30*$2))
    	ffmpeg -framerate $2 -i %010d.png -vframes $num_fr -pix_fmt $YUV_PROFILE $MP4_PATH/$1.mp4 &>> $FFMPEG_LOG
    	wait
    	ffmpeg -i $MP4_PATH/$1.mp4 -vf scale=$WIDTH:$HEIGHT,fps=$FPS_RATE -frames:v $FRAME_N $MP4_PATH2/$1.mp4 &>> $FFMPEG_LOG
    	wait

    	echo -ne [OK!]\\n
    }

    function png_to_lhe(){
    #Decodes lhe frames from png frames of the original video
    #using YUV profile $1, and a block size of $2 x $3
		echo -ne Compressing original frames at $IN_PNG_PATH in $IN_LHE_PATH with a block size: $2 x $3, YUV: $1...\\t

		cd $MOCKGEN_PATH
		
		case $1 in
			"yuv420p")
				taskset -c 7 ./mock_gen.out $IN_PNG_PATH $IN_LHE_PATH $2 $3 1
				;;
			"yuv422p")
				taskset -c 7 ./mock_gen.out $IN_PNG_PATH $IN_LHE_PATH $2 $3 2
    			;;
			"yuv444p")
				taskset -c 7 ./mock_gen.out $IN_PNG_PATH $IN_LHE_PATH $2 $3 3
    			;;
    		"gray")
				taskset -c 7 ./mock_gen.out $IN_PNG_PATH $IN_LHE_PATH $2 $3 0
    			;;
			*) 
    			tput clear
    			echo -ne Invalid YUV profile!\\n\\n
    			;;
		esac
    	wait

		echo -ne [OK!]\\n
    }

    function lhe_to_png(){
    #Decodes lhe frames to png files using Player program
    	echo -ne Decoding lhe frames in $LHE_PATH to png frames in $PNG_PATH ...\\t

    	cd $PLAYER_PATH

    	taskset -c 7 ./LHEplayer --decode $LHE_PATH  $PNG_PATH
		wait

		echo -ne [OK!]\\n
    }

    function write_batch_file(){
    #Adds a line to the batch file for VMAF
    #With this, all videos will ve evaluated in parallel
		echo -ne Adding video to batchfile $BATCH_PATH ...\\t

    	echo $YUV_PROFILE $WIDTH $HEIGHT $IN_RAW_PATH $RAW_PATH/${resolution}_${framerate}_${yuv_profile}_${block_length}_${block_height}_${packet_size}_${net_loss}_${rep}.yuv >> $BATCH_PATH
		wait

		echo -ne [OK!]\\n
    }
    function get_vmaf_scores(){
    #Computes VMAF scores for all raw videos, against original video
   	#in parallel, using the batch file

   	#WARNING: Depending on the amount of videos, this may take VERY long
   		tput bold 
   		echo -ne Getting vmaf scores of batch $BATCH_PATH in file $QSCORES_PATH ...\\n

   		let "progress=($NUMVIDS/8)*5+(3/$NUMVIDS)+1" #not important
   		cd $VMAF_PATH
   		source ~/.bashrc
   		./run_vmaf_in_batch $BATCH_PATH --out-fmt text --parallelize > $QSCORES_PATH & PID=$!

		printf "["
		# While process is running, "progress" bar
		while kill -0 $PID 2> /dev/null; do 
		    printf  "▓"
		    sleep $progress
		done
		printf "]"
		wait
    	echo -ne [OK!]\\n
		tput sgr0

		#FROM FAQ:
		#VMAF was designed with HTTP adaptive streaming in mind. Correspondingly, in terms of the types of video artifacts, 
		#it only considers compression artifact and scaling artifact (read [this](http://techblog.netflix.com/2016/06/toward-practical-perceptual-video.html) 
		#tech blog post for more details). The perceptual quality of other artifacts 
		#(for example, artifacts due to packet losses or transmission errors) may be predicted inaccurately.

    }
    function write_results_file(){
    #Writes all the input info + results in the uotput file to be analyzed 

    	echo -ne Writing benchmark results data in file $RESULTS_PATH/results.csv ...\\t

    	awk -F ', ' '{print " "$7}' $QSCORES_PATH | paste -d, $PARTIALR_PATH - >> $RESULTS_PATH/results.csv
		#deletes all columns but the one with the VMAF scores, then appends that last column to the 
		#partial results and copies it to the results file

    	echo -ne [OK!]\\n
    }

    function write_partial_results(){
    #Writes partial results in PARTIALR_PATH

    	echo -ne Writing partial results in  $PARTIALR_PATH...\\t

    	#File structure:
		#         Video,  Resolution,  Framerate,  Frames,   YUV_Profile,  Block_W,       Block_H,       Video_Size,  Packet_Size, Packets_Sent,   Loss_Set, Loss_Measured,   Block_Loss
		echo -ne $video, $resolution, $framerate, $FRAME_N, $yuv_profile, $block_length, $block_height, $video_size, $packet_size, $packets_sent, $net_loss, $measured_loss, $block_loss\\n >> $PARTIALR_PATH

		echo -ne [OK!]\\n
    }

    function print_partial_results(){
    #Prints partial results in between transmissions.

		tput bold 
		echo -ne \\n\\nPARTIAL RESULT:\\n\\n
		echo -ne \\tBLOCK SIZE\\tPACKET SIZE\\tPACKETS SENT\\tPACKETS RECEIVED\\tNET LOSS SET\\tNET LOSS MEASURED\\tBLOCK LOSS MEASURED\\n
		tput sgr0
		echo -ne \\t$block_length x $block_height\\t\\t$packet_size B\\t\\t$packets_sent\\t\\t$packets_received\\t\\t\\t$net_loss%\\t\\t$measured_loss%\\t\\t\\t$block_loss%\\n\\n\\n
    }

    function do_transmission(){
    #Performs all the transmission process and loss measurement
    #$1 = net_loss
    #$2 = packet_size
    #$3 == 1 -> player, else no player
    	
		packets_sent=0 
		packets_received=0

    	while [[ ( "$packets_sent" = 0 ) || ( -z "$packets_sent" ) || ( "$packets_received" = 0 ) || ( -z "$packets_received" ) ]] #sometimes transmission fails. 
		do
			set_local_loss $1
			if [ $3 = 1 ]
			then
				set_player
			fi
			set_receiver
			set_local_sender $2
			kill_player #ignored if not set
			kill_receiver

			measured_loss=$( bc -l <<< "((( $packets_sent - $packets_received) / $packets_sent ) * 100 )")
			measured_loss="$(printf '%.3f\n' ${measured_loss})"

			#echo "sent: $packets_sent  received: $packets_received (packet loss: $measured_loss%, block loss: $block_loss%)" #debug

		done
    }

	function sweep_packet_size(){
	#Sweeps the maximum size of packet
    #$1 = net_loss
    #$2 = yuv profile
    #$4  = framerate

		for packet_size in $(eval echo "{$PACK_MIN..$PACK_MAX..$PACK_STEP}")
		do
			for rep in $(eval echo "{1..$REPS..1}")
    		do
				do_transmission $1 $packet_size 0
			done

			#process received files
			lhe_to_png
			png_to_mp4 ${resolution}_${framerate}_${yuv_profile}_${block_length}_${block_height}_${packet_size}_${net_loss}_${rep}  $framerate
			mp4_to_raw ${resolution}_${framerate}_${yuv_profile}_${block_length}_${block_height}_${packet_size}_${net_loss}_${rep}
			
			#FREE RESOURCES
			delete_path $LHE_PATH
			#delete_path $PNG_PATH
			#delete_path $MP4_PATH

			tput clear
			tput bold 
		    echo -e \\nRepetition $rep\/$REPS of video $video \($resolution $framerateFPS $yuv_profile\) ...
			tput sgr0
			print_partial_results
			write_partial_results

			write_batch_file
			echo -ne \\n\\n

			let "NUMVIDS++"
		done
    }

    function sweep_loss(){
    #Sweeps network loss parameters
    #$1 = min
    #$2 = max
    #$3 = step
    #$4 = yuv profile
	#$5 = framerate

    	for net_loss in $(eval echo "{$1..$2..$3}")
		do
			sweep_packet_size $net_loss	$4	$5		
		done
    }

	function sweep_block(){
	#Runs the testbench for the sizes of blocks described by the input parameters. Sweeps network losses in fine and coarse steps.
	#$1 = yuv profile
	#$2 = image width
	#$3 = image height
	#$4 = framerate

		block_length=$BLOCK_L_MIN
		block_l_max=$2

		while [ $block_length -le $block_l_max ]
		do
			if [[ ($(($2%$block_length)) -eq 0) && ($(($block_length%16)) -eq 0) ]]
			then
				block_height=$BLOCK_H_MIN
				while [ $block_height -le $BLOCK_H_MAX ]
				do
					if [[ ($block_height -le $block_length) && ($(($3%$block_height)) -eq 0) && (($(($block_height%2)) -eq 0) && ($block_height -le 16) || ($block_height -eq 1)) && ($(($block_length*$block_height)) -le $PIX_MAX) && ($(($block_length*$block_height)) -ge $PIX_MIN)  && ($((($2*$3)/($block_length*$block_height))) -le $IMAGE_BSIZE_MAX) ]]		
					then
						echo $block_length x $block_height
						png_to_lhe $1 $block_length $block_height #encode with new block size
						sleep 3
						video_size=$(du -b $IN_LHE_PATH | cut -f1) #size of all the .lhe files, in bytes
						
						#fine step
						sweep_loss $NET_F_MIN $NET_F_MAX $NET_F_STEP $1 $4
						
						#coarse step 
						if [ $SWEEP_COARSE = 1 ]
	    				then
							sweep_loss $NET_C_MIN $NET_C_MAX $NET_C_STEP $1 $4
						fi
						#GENERATE RESULTS
						get_vmaf_scores
						echo
						write_results_file
						echo
						echo
						
						#FREE RESOURCES
						NUMVIDS=0
						delete_path $RAW_PATH
						delete_file $BATCH_PATH
						#delete_file $QSCORES_PATH
						delete_file $PARTIALR_PATH
						delete_path $IN_LHE_PATH #free resources for next encoding
						#demos' .lhe files are not replenished! run generate input before running a demo!
					fi
					let block_height++
				done
			fi
			let block_length++
		done	
    }


    function sweep_yuv_profile(){
    #Runs the testbench for each chroma subsampling profile in the list SWEEP_PROFILES. Can run with different types (shapes) of block for comparison.
    #$1 = image width
	#$2 = image height
	#$3 = framerate
    	for yuv_profile in "${SWEEP_PROFILES[@]}"
    	do
    		echo
			#get_in_raw
			
	    	sweep_block $yuv_profile $1 $2 $3
		
			#FREE RESOURCES
			echo
	    	#delete_file $IN_RAW_PATH
	    done
    }

    function sweep_framerate(){
    #Runs the testbench for the framerates described by FPS_MIN, FPS_MAX, FPS_STEP.
	#$1 = name of original video
	#$2 = video width
	#$3 = video height

		for framerate in "${SWEEP_FPS[@]}"
		do
			echo
			
			#get_in_video2 $1 $WIDTH $HEIGHT $framerate #get video with desired resolution and framerate
			#get_in_raw

			sweep_resolution $1 $framerate
			#sweep_yuv_profile $2 $3

			echo
			#delete_file $IN_MP4_PATH2 #prepare for next video
			#delete_path $IN_PNG_PATH #prepare for next video's frames

		done
	}
	
	function sweep_resolution(){
	#Runs the testbench for each resolution in the list SWEEP_RESOLUTION. 
	#$1 = original video name
	#$2 = framerate
		
		for resolution in "${SWEEP_RESOLUTION[@]}"
    	do
			case $resolution in
				"360p")
					get_in_video $1 640 360 $2 #get video with desired resolution and framerate
					mp4_to_png #get png frames of current video
					sweep_yuv_profile 640 360 $2
					
    				#sweep_framerate $1 640 360
    			;;
				"480p")
					get_in_video $1 640 480 $2 #get video with desired resolution and framerate
					mp4_to_png #get png frames of current vide
					sweep_yuv_profile 640 480 $2
					o
    				#sweep_framerate $1 640 480
    			;;
    			"720p")
					get_in_video $1 1280 720 $2 #get video with desired resolution and framerate
					mp4_to_png #get png frames of current video
					sweep_yuv_profile 1280 720 $2
					
    				#sweep_framerate $1 1280 720
    			;;
			esac
			delete_file $IN_MP4_PATH #prepare for next video
			delete_file $IN_MP4_PATH2 #prepare for next video
			delete_path $IN_PNG_PATH #prepare for next video's frames
    	done

	}

    function sweep_videos(){
    #Runs the testbench REPS times for each video (at ORIGINAL_PATH) in the list VIDEO_LIST 

    	for video in "${VIDEO_LIST[@]}"
    	do
    		get_in_video2 $video $WIDTH $HEIGHT $FPS_RATE #get video with desired resolution and framerate
			get_in_raw
			sweep_framerate $video
   			delete_file $IN_MP4_PATH2
   			delete_file $IN_RAW_PATH
       	done
    }

############
#MAIN MODES#
############

    function generate_input_from_original(){
    #Gets input lhe frames from original video
    #using YUV_PROFILE, WIDTH, HEIGHT, FPS_RATE, FRAME_N, ... 
		tput clear
    	tput bold 
	    echo -e \\nGenerating input lhe frames from original video ... \\n
		tput sgr0

		echo "Please input:   block_length block_height yuv_profile (yuv444p / yuv422p / yuv420p)"
    	read block_length block_height yuv_profile

		delete_file $IN_MP4_PATH
		delete_file $IN_RAW_PATH
		delete_path $IN_LHE_PATH 
		delete_path $IN_PNG_PATH
		
		get_in_video $VIDEO $WIDTH $HEIGHT $FPS_RATE
		mp4_to_png 
		png_to_lhe $yuv_profile $block_length $block_height
		delete_file $IN_MP4_PATH
		delete_path $IN_PNG_PATH

		tput bold 
	    echo -e \\n[OK!]
		tput sgr0
		echo -e \\n\\n
    } 

	function run_local_demo(){
    #Sets up player, receiver and sender in local machine
    	tput clear
    	tput bold 
	    echo -e \\nRunning local demo ... \\n
	    tput sgr0

	    init_local_loss
		delete_path $LHE_PATH
		delete_file temp_s
		delete_file temp_r

    	echo Please input   net_loss% 
    	read net_loss 
    	set_local_loss $net_loss

    	packet_size=1480

	    do_transmission $net_loss $packet_size 1

	    finish_local_loss
	    delete_path $LHE_PATH

		#print partial results
		tput clear
		print_partial_results

	    tput bold 
	    echo -e \\n[OK!]
		tput sgr0
		echo -e \\n\\n
    }

    function run_zynqberry_demo(){
    #Sets up losses, player and receiver in local machine, 
    #and sender in ZynqBerry
    	tput clear
    	tput bold 
    	echo -ne \\nRunning ZynqBerry demo ... \\t

    	#¿?

	    echo -e [NOT IMPLEMENTED!]
		tput sgr0
		echo -e \\n\\n
    }

    function run_testbench(){
    #Runs testbench to get data for training optimization model
    	tput clear
    	tput bold 
    	echo -e \\nWARNING: This may take up to several days depending on the configuration!!\\n
    	echo -e \\nWARNING: This will delete all data generated on previous testbench!!\\n
    	tput sgr0

    	valid=0
		while [ $valid = 0 ]
		do
			echo "Continue? (y/n): "
	    	read option
			if [ "$option" = y ]
			then
				valid=1
		    	tput clear
		    	tput bold 
		    	echo -e \\nRunning testbench ... \\n
		    	tput sgr0
		    	
				#INIT
		    	init_local_loss
		    	#delete everything except original video
		    	delete_path $RESULTS_PATH #WARNING!!
		    	delete_file $QSCORES_PATH #WARNING!!
		    	delete_file $PARTIALR_PATH
		    	delete_file $FFMPEG_LOG
		    	delete_file $BATCH_PATH 
		    	delete_file $IN_MP4_PATH
		    	delete_file $IN_MP4_PATH2

		    	delete_file $IN_RAW_PATH
		    	delete_path $IN_LHE_PATH 
		    	delete_path $IN_PNG_PATH
		    	delete_path $MP4_PATH #WARNING!!
		    	delete_path $MP4_PATH2

		    	delete_path $RAW_PATH
		    	delete_path $PNG_PATH
		    	delete_path $LHE_PATH
		    	delete_file temp_s
				delete_file temp_r
		    	
		    	#only one results file for all repetitions:
    			echo -ne Creating results file $RESULTS_PATH/results.csv...\\t
    			echo "Video, Resolution, Framerate, Frames, YUV_Profile, Block_W, Block_H, Video_Size, Packet_Size, Packets_Sent, Loss_Set, Loss_Measured, Block_Loss, VMAF_Score" > $RESULTS_PATH/results.csv
				echo -ne [OK!]\\n

		    	#TESTBENCH		    	
		    	sweep_videos

				#FREE RESOURCES
				finish_local_loss
				delete_file temp_s
				delete_file temp_r


		    	tput bold 
			    echo -e \\n\\n[OK!]
				tput sgr0
				echo -e \\n\\n

			elif [ "$option" = n ]
			then
				valid=1
				tput clear
				break
			else
				echo Type y or n
				echo
			fi
		done
    }

    function estimate_runtime(){
		#Estimates the time it will take to run the full testbench with the actual configuration
    	tput clear
    	tput bold 
	    echo -ne Estimating testbench runtime ... \\t
	    tput sgr0
		
		VIDEO_TIME=60 #seconds
		VMAF_TIME=300 #seconds
		i=0
		j=0
		total_time=0

		for video in "${VIDEO_LIST[@]}"
		do
			#echo -ne VIDEO $video\\n
			for rep in $(eval echo "{1..$REPS..1}")
			do
				#echo -ne \\t REP $rep\\n
				for resolution in "${SWEEP_RESOLUTION[@]}"
				do
					#echo -ne \\t\\t RESOLUTION $resolution\\n
					case $resolution in
						"360p")
							IMAGE_W=640 
							IMAGE_H=360
						;;
						"480p")
							IMAGE_W=640 
							IMAGE_H=480
						;;
						"720p")
							IMAGE_W=1280 
							IMAGE_H=720
						;;
					esac

					for framerate in $(eval echo "{$FPS_MIN..$FPS_MAX..$FPS_STEP}")
					do
						#echo -ne \\t\\t\\t FRAMERATE $framerate\\n
						for yuv_profile in "${SWEEP_PROFILES[@]}"
						do
							#echo -ne \\t\\t\\t\\t YUV PROFILE $yuv_profile\\n
							block_length=$BLOCK_L_MIN
							block_l_max=$IMAGE_W
							
							while [ $block_length -le $block_l_max ]
							do
								if [[ ($(($IMAGE_W%$block_length)) -eq 0) && ($(($block_length%16)) -eq 0) ]]
								then
									block_height=$BLOCK_H_MIN
									while [ $block_height -le  $BLOCK_H_MAX ]
									do
										if [[ ($block_height -le $block_length) && ($(($IMAGE_H%$block_height)) -eq 0) && (($(($block_height%2)) -eq 0) && ($block_height -le 16) || ($block_height -eq 1)) && ($(($block_length*$block_height)) -le $PIX_MAX) && ($(($block_length*$block_height)) -ge $PIX_MIN)  && ($((($IMAGE_W*$IMAGE_H)/($block_length*$block_height))) -le $IMAGE_BSIZE_MAX) ]]		
										then
											#echo -ne \\t\\t\\t\\t\\t BLOCK SIZE $block_length x $block_height\\n

											#fine step
											for net_loss in $(eval echo "{$NET_F_MIN..$NET_F_MAX..$NET_F_STEP}")
											do
												#echo -ne \\t\\t\\t\\t\\t\\t NET LOSS $net_loss\\n
												for packet_size in $(eval echo "{$PACK_MIN..$PACK_MAX..$PACK_STEP}")
												do
													#echo -ne \\t\\t\\t\\t\\t\\t\\t PACKET SIZE $packet_size\\n
													let i++
												done				
											done
								
											#coarse step 
											if [ $SWEEP_COARSE = 1 ]
			    							then
												for net_loss in $(eval echo "{$NET_C_MIN..$NET_C_MAX..$NET_C_STEP}")
												do
													#echo -ne \\t\\t\\t\\t\\t\\t NET LOSS $net_loss\\n
													for packet_size in $(eval echo "{$PACK_MIN..$PACK_MAX..$PACK_STEP}")
													do
														#echo -ne \\t\\t\\t\\t\\t\\t\\t PACKET SIZE $packet_size\\n
														let i++
													done	
												done
											fi	
											let j++
										fi
										let block_height++
									done
								fi
								let block_length++
							done
		    			done
					done
		    	done
			done
		done

		tput bold 
		echo -ne [OK!]\\n\\n 
		echo -ne $i videos will be generated, transmited and analyzed... \\n
		echo -ne Estimated testbench runtime: $(((($i*$VIDEO_TIME)+($j*$VMAF_TIME))/60)) minutes "("about $(((($i*$VIDEO_TIME)+($j*$VMAF_TIME))/3600)) hours")"!
		tput sgr0
		echo
		echo -e \\n\\n
    }


######
#MAIN#
######
	#[ `whoami` = root ] || exec sudo su -c $0 root #this gets sudo access for later commands ("built-in root wrapper")
		
	sudo tput clear #this gets sudo access for later commands (sudo ./run_vma_in_batch -> over 9000 problems)

	delete_file $FFMPEG_LOG #reset
	export LC_NUMERIC="en_US.UTF-8" #for printing floats
	wait $!
	tput clear

	#if not existing (-p), create paths for LHE in/out files on RAM 
	sudo mkdir -p $IN_LHE_PATH 
    sudo mount -t tmpfs -o size=512m tmpfs $IN_LHE_PATH
    sudo mkdir -p $LHE_PATH
    sudo mount -t tmpfs -o size=512m tmpfs $LHE_PATH

	tput bold 
	echo -e \\nLHE VIDEO QUALITY BENCHMARK ~VMAF\\n
	tput sgr0
	
	option=""
	while [ "$option" != Quit ]
	do
		tput bold 
		echo Select runnning mode:
		tput sgr0
		options=("Generate input lhe frames" "Run local demo" "Run ZynqBerry demo" "Run video quality testbench" "Estimate testbench runtime" "Quit")
		PS3='Select mode: '
		select option in "${options[@]}"
		do
			case $option in
				"Generate input lhe frames")
	    			generate_input_from_original
	    			;;
				"Run local demo")
	    			run_local_demo
	    			;;
				"Run ZynqBerry demo")
	    			run_zynqberry_demo
	    			;;
				"Run video quality testbench")
	    			run_testbench
	    			;;
	    		"Estimate testbench runtime")
	    			estimate_runtime
	    			;;
	    		"Quit")
    				tput clear
    				tput bold 
	    			echo -ne \\nQuitting ...
	    			tput sgr0

	    			#just to make sure & clean from previous failed runs
	    			sudo tc qdisc del dev ifb0 root netem 2>/dev/null
		    		sudo tc qdisc del dev eth0 ingress 2>/dev/null
					sudo tc qdisc del dev lo ingress 2>/dev/null
					delete_file temp_s >/dev/null
					delete_file temp_r >/dev/null

					tput bold 
					echo -e \\n\\nHave fun!\\n
					tput sgr0
					break
	    			;;
	    		*) 
	    			tput clear
	    			echo -ne Invalid option!\\n\\n
	    			;;
			esac
			break
		done
	done