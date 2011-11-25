# Run make and save it's output to make.out
function Make() {
	nice make $* 2>&1 | tee make.out
}

# Perform an svn checkout of uri ($1) on directory ($2)
# assumes the directory is empty or non-existant
#   $1 = URI
#   $2 = destination directory
function svn_checkout() {
	echo "Performing svn checkout of $1 on $2..."
	echo "SVN_CHECKOUT_ARGS value: $SVN_CHECKOUT_ARGS"
	svn co $SVN_CHECKOUT_ARGS $1 $2 || exit 1
	echo "done."
}

# Perform an svn update on the working copy on directory
#   $1 = working copy's directory
function svn_update() {
	echo "Performing svn update on $1..."
	cd $1 && svn up && cd .. || exit 1
	echo "done."
}

# Clone a git repository
#   $1 = repository name
#   $2 = destination directory
#   $3 = branch name - optional and defaults to 'master'
function git_clone() {
	echo "Performing git clone of $1 on $2..."
	sngclone $1 $2 || exit 1

	if [ "x$3" != "x" ]; then
		echo "Switcthing to branch $3"
		oldpwd=$(pwd)
		cd $2
		git checkout -b $3 origin/$3
		cd $oldpwd
	fi
	echo "done."
}

# Update a git repository
#   $1 = local path to repository
#   $2 = branch to update
function git_update() {
	echo "Performing git update of $1 ..."
	oldpwd=$(pwd)
	cd $1
	git fetch origin || exit 1
	git checkout $2 || exit 1
	git merge origin/$2 || exit 1
	cd $oldpwd
	echo "done."
}

# Pull from a git repository
#   $1 = local path to repository
#   $2 = branch name - optional and defaults to 'master'
function git_pull() {
    echo "Pulling changes on $1..."
    oldpwd=$(pwd)
    cd $1
    if [ "x$2" != "x" ]; then
        echo "Changes will be merged on $2 branch"
        git pull origin $2
    else
        echo "Changes will be merged on master branch"
        git pull
    fi
    echo "done."
}

# download and decompress a tarball
#   $1 = prefix_url, such as ftp://ftp.sangoma.com/foo/bar
#   $2 = package name, such as libsng_isdn-7.0.0.x86_64
function download() {
	wget $1/$2.tgz
	if [ $? = 0 ]
	then
		tardir=$(tar -tf $2.tgz | head -n1 | sed 's,\/,,g')
		tar -xvzf $2.tgz || echo "FAILED to decompress $2.tgz"
		echo "$tardir" > $tardir/.version
		echo "$tardir" >> $PACKAGE_VERSION_LOG
		if [ "$tardir" != "$2" ]
		then
			mv $tardir $2 || echo "FAILED to move $tardir to $2"
		fi
		echo "SUCCESSFULLY downloaded $2"
	else
		echo "FAILED to download $1/$2.tgz"
	fi
}

# Generate a md5 hash of a file, saving it was filename.md5sum
#   $1 = filename
function hash() {
    echo -ne "Generating md5sum of $1... "
    md5sum $1 | cut -d\  -f1 > $1.md5sum
    echo "done."
}

# Uploads a file and it's .md5sum file to sangoma ftp
#   $1 = local file
#   $2 = remote directory
#   $3 = ftp server hostname
function upload() {
	hash $1
	echo -ne "Uploading $1 to $2... "
	sangoma_ftp.pl $1 $2 $3
	echo "done."

	echo -ne "Uploading $1.md5sum to $2... "
	sangoma_ftp.pl $1.md5sum $2 $3
	echo "done."
}

