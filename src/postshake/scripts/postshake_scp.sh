#! /bin/bash
#
# Postshake additional program:
#   Using the internal scp program to upload the result image file
#   to the FTP storage server.
#

if [ $# -ge 4 ]; then
	echo "$0: Start to upload the image file..."
	shift 5
	for filename in $@
	do
		echo "$0: Start to upload $filename..."
		scp $filename YOUR_USER_NAME@YOUR_SERVER_IP:YOUR_STORED_PATH
	done
else
	echo "Usage: \"./postshake_scp.sh start_time end_time report_time max_magnitude trig_stations result_filename_1 [result_filename_2]...\""
	exit
fi
