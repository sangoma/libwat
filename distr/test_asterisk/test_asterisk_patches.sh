#!/bin/sh

release_dir=$1
home_dir=`pwd`
test_dir="tmp"

asterisk_releases_failed=""
asterisk_releases_passed=""
last_tested_asterisk_release=""




verify_asterisk ()
{
	asterisk_patch=$1
	asterisk_release=$2
	last_tested_asterisk_release=""

	echo "Verifying $asterisk_patch against release:$asterisk_release"

	logfile="$home_dir/$test_dir/$asterisk_release"".log"
	echo "logfile:$logfile"

	echo "Removing old asterisk directories"
	eval "ls | grep -v \".*.log\" | xargs rm -rf"

	echo "Downloading $asterisk_release"
	eval "wget -q downloads.asterisk.org/pub/telephony/asterisk/$asterisk_release.tar.gz"
	if [ $? -ne 0 ];then
		eval "wget -q downloads.asterisk.org/pub/telephony/asterisk/releases/$asterisk_release.tar.gz" 2>> $logfile >> $logfile

		if [ $? -ne 0 ];then
			echo "Failed to download $asterisk_release" 2>> $logfile >> $logfile
			asterisk_releases_failed="$asterisk_releases_failed $asterisk_release"
			return 1
		fi
	fi	

	echo "Untarring $asterisk_release.tar.gz"
	eval "tar xfz $asterisk_release.tar.gz" 2>> $logfile >> $logfile
	if [ $? -ne 0 ];then
		echo "Failed to untar $asterisk_release" 2>> $logfile >> $logfile
		asterisk_releases_failed="$asterisk_releases_failed $asterisk_release"
		return 1
	fi

	eval "echo $asterisk_release |grep \"current\""
	if [ $? -eq 0 ]; then
		#find the real asterisk release name
		asterisk_branch=`echo $asterisk_release | cut -d "-" -f -2`
		asterisk_release=`ls |grep $asterisk_branch | grep -v \"gz\" | grep -v \"log\" | head -n 1`
		echo "Current Asterisk version:$asterisk_release"
	fi

	echo "Changing directory to $asterisk_release"
	eval "cd $asterisk_release" 2>> $logfile >> $logfile
	if [ $? -ne 0 ];then
		echo "Failed to change dir $asterisk_release" 2>> $logfile >> $logfile
		asterisk_releases_failed="$asterisk_releases_failed $asterisk_release"
		return 1
	fi


	echo "Applying patch $home_dir/../../asterisk/$asterisk_patch"
	eval "patch -p 1 < $home_dir/../../asterisk/$asterisk_patch" 2>> $logfile >> $logfile
	if [ $? -ne 0 ];then
		echo "Failed to apply patch to $asterisk_release" 2>> $logfile >> $logfile
		asterisk_releases_failed="$asterisk_releases_failed $asterisk_release"
		return 1
	fi

	echo "Executing bootstrap"
	eval "./bootstrap.sh" 2>> $logfile >> $logfile
	if [ $? -ne 0 ]; then
		echo "Failed to bootstrap $asterisk_release after patch" 2>> $logfile >> $logfile
		asterisk_releases_failed="$asterisk_releases_failed $asterisk_release"
		return 1
	fi

	echo "Executing configure"
	eval "./configure --enable-dev-mode --with-wat=$WORKSPACE/libwat_install" 2>> $logfile >> $logfile
	if [ $? -ne 0 ]; then
		echo "Failed to configure $asterisk_release after patch" 2>> $logfile >> $logfile
		tail -n 20 $logfile
		asterisk_releases_failed="$asterisk_releases_failed $asterisk_release"
		return 1
	fi

	echo "Verifying compilation $asterisk_release"
	eval "make" 2>> $logfile >> $logfile
	if [ $? -ne 0 ]; then
		echo "Failed to compile $asterisk_release after patch" 2>> $logfile >> $logfile
		tail -n 20 $logfile
		asterisk_releases_failed="$asterisk_releases_failed $asterisk_release"
		return 1
	fi

	eval "cd .."
	if [ $? -ne 0 ]; then
		echo "Failed to chdir"
		exit 1
	fi

	echo
	echo 
	echo "==========================================================================="
	echo "$asterisk_patch passed"
	echo "==========================================================================="

	echo "" 2>> $logfile >> $logfile
	echo "" 2>> $logfile >> $logfile
	echo "$asterisk_patch passed" 2>> $logfile >> $logfile
	
	asterisk_releases_passed="$asterisk_releases_passed $asterisk_release"
}


#main
echo "Compiling libwat"

echo "==========================================================================="
echo "Installing DAHDI"
echo "==========================================================================="

eval "wget http://downloads.asterisk.org/pub/telephony/dahdi-linux/dahdi-linux-current.tar.gz"
if [ $? -ne 0 ];then
	echo "Failed to download latest DAHDI"
	exit 1
fi

eval "tar xfz dahdi-linux-current.tar.gz"
if [ $? -ne 0 ]; then
	echo "Failed to untar dahdi"
	exit 1
fi

dahdi_dir=`find maxdepth 1 -type d |grep dahdi`
echo "Dahdi dir:$dahdi_dir"
eval "cd $dahdi_dir`
if [ $? -ne 0 ]; then
	echo "Failed to change directory to $dahdi_dir"
	exit 1
fi

eval "


echo "==========================================================================="
echo "Testing Asterisk patches"
echo "==========================================================================="

asterisk_patches=`ls $home_dir/../../asterisk`

if [ -e $test_dir ]; then
	rm -rf $test_dir
fi
eval "mkdir $test_dir"
if [ $? -ne 0 ];then
	echo "Failed to create $test_dir directory"
	exit 1
fi

eval "cd $test_dir"
if [ $? -ne 0 ];then
	echo "Failed to change directory to $test_dir"
	exit 1
fi


echo "List of Asterisk patches:"
echo "$asterisk_patches"
echo

for asterisk_patch in $asterisk_patches
do
	asterisk_release=`echo $asterisk_patch | sed s/\.patch//`	
	echo
	echo
	echo "==========================================================================="
	echo "Verifying $asterisk_patch"
	echo

	verify_asterisk $asterisk_patch $asterisk_release
done

echo "Verifying last Asterisk 1.8 patch vs Asterisk-1.8-current"
last_patch=`ls $home_dir/../../asterisk | grep asterisk-1.8 |tail -n 1`
verify_asterisk $last_patch "asterisk-1.8-current"
if [ $? -eq 0 ]; then
	if [ ! -e  $home_dir/../../asterisk/$last_tested_asterisk_release.patch ]; then
		echo "Copying $last_patch to $last_tested_asterisk_release.patch"
		cp $home_dir/../../asterisk/$last_patch $home_dir/../../asterisk/$last_tested_asterisk_release.patch
	fi
fi

echo "Verifying last Asterisk 10.1 patch vs Asterisk-10-current"
last_patch=`ls $home_dir/../../asterisk | grep asterisk-10 |tail -n 1`
verify_asterisk $last_patch "asterisk-10-current"
if [ $? -eq 0 ]; then
	if [ ! -e  $home_dir/../../asterisk/$last_tested_asterisk_release.patch ]; then
		echo "Copying $last_patch to $last_tested_asterisk_release.patch"
		cp $home_dir/../../asterisk/$last_patch $home_dir/../../asterisk/$last_tested_asterisk_release.patch
	fi
fi

echo "==========================================================================="
echo "Asterisk releases passed:$asterisk_releases_passed"
echo "Asterisk releases failed:$asterisk_releases_failed"

if test "x$asterisk_releases_failed" = "x" ; then
	echo "All patches were successful"
	echo "==========================================================================="
	exit 0
else
	echo "Some patches failed"
	echo "==========================================================================="
	exit 1
fi
