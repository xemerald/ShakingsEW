#! /bin/bash
#
# P-Alert modules for Earthworm Installation script:
#
# Revision history:
#   Revision 1.0  2019/08/28 16:46:50  Benjamin Yang
#   Initial revision
#
MODULES="cf2trace \
	dif2trace \
	peak2trig \
	respectra \
	shake2redis \
	shake2ws \
	trace2peak"

#	shakemap
#	postshake

if [ $EW_VERSION ]; then
	echo "Earthworm version is $EW_VERSION."
else
	echo "Need to install the Earthworm first!"
	exit
fi

echo "---------------------------------"
echo "Cleaning the pre-compiled file..."
echo "---------------------------------"
make clean_unix
echo "----------------------------"
echo "Compiling all the modules..."
echo "----------------------------"
make unix

if [ $EW_PARAMS ]; then
	echo "----------------------------------------"
	echo "Start to copy the configuration files..."
	echo "----------------------------------------"
	for m in $MODULES
	do
		echo "Entering the $m..."
		cd $m
		cp *.d $EW_PARAMS
		if [ -d data ]; then
			cd data
			cp * $EW_DATA_DIR
			cd ..
		fi
		cd ..
	done

	echo "----------------------------------------"
	echo "Start to copy the environment files..."
	echo "----------------------------------------"
	echo "Entering the environment..."
	cd environment
	cp * $EW_PARAMS
	cd ..
else
	echo "Can't find the Earthworm's params directory!"
	exit
fi

echo "---------------------"
echo "Installation success!"
echo "---------------------"
