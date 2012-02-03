#!/usr/bin/perl

use Net::FTP;
#use Net::FTP::Throttle;

$file=shift @ARGV;
$dir=shift @ARGV;

if (!defined($ip)){
	$ip="ftp.sangoma.com";
}

if (!defined($file)){
	print "Unknown File";
	exit 1;
}

if (!defined($dir)){
	print "No directory";
	exit 1;
}

$ftp = Net::FTP->new($ip, Debug => 0);
#$ftp = Net::FTP::Throttle->new($ip, MegabitsPerSecond => 0.8);

#$ftp->login("anonymous",'me@here.there' ) || die "\nFailed to login!\n\n";
$ftp->login("sangoma",'Ookech1f') || die "\nFailed to login!\n\n";

$ftp->binary;  

if (defined($dir)){
	$ftp->cwd($dir) || die "\n Failed to change directory $dir";
}

$ftp->put($file) || die "\nFailed to upload $file from $ip@$dir\n\n";
$ftp->quit;               

print "\nFile $dir/$file uploaded to $ip\n\n";

exit (0);


sub usage()
{

print "

Usage:

 sangoma_ftp.pl  <filename> <directory>

"

}
