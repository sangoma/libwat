#!/usr/bin/perl
#Generates ChangeLog from smg_fs/smgwebgui/smgwebserv commits
#
#line format: <smg version>|<smg_fs revision>|<smgwebgui revision>|<smgwebserv revision>

use strict;
use warnings;

#default values
my $g_changelog_tag="chlog:";
my $g_changelog_file="Changelog";
my $g_revision_info_file = "releases_info.txt";
my $g_svn_users_file = "svn_users.txt";
my $g_project_name = "LibWAT";


#internal variables
my $g_homedir=`pwd`;
chomp($g_homedir);
my $g_current_version = "";
my @g_project_dirs;
my $g_changelog_str;

#main
read_args();
changelog_header();
process_projects();
update_changelog();


#===================================FUNCTIONS================================#
sub read_args
{
	foreach my $arg_num (0..$#ARGV) {
		$_ = $ARGV[$arg_num];
		
		if ( $_ =~ /--version=(\w+)/) {
			$g_current_version=substr($_, length("--version="));
		} elsif ( $_ =~ /--users_file=(\w+)/) {
			$g_svn_users_file=substr($_, length("--users_file="));
		} elsif ( $_ =~ /--g_changelog_file=(\w+)/) {
			$g_changelog_file=substr($_, length("--g_changelog_file="));
		} elsif ( $_ =~ /--project_dir=.*/) {
			push(@g_project_dirs, substr($_, length("--project_dir=")));
		} elsif ( $_ =~ /--project_name=(\w+).*/) {
			$g_project_name=substr($_, length("--project_name="));
		} else {
			die "Invalid option $_";
		}
	}

	if ( $g_current_version eq "" ) {
			printf("Current version not specified\n");
			printf("usage:gen_changelog.pl --version=<version>\n\n");
			exit(1);
	}
	
	printf("Generating Changelog for SMG version %s\n", $g_current_version);
}

sub process_projects
{
	foreach my $project_dir(@g_project_dirs) {		
		my $scm_type;
		my $prev_revision;
		my $prev_version;
		my $current_revision;
		my $g_changelog_str = "";
		my $tag = "";
		printf("\n\n\nProcessing project:%s\n", $project_dir);

		chdir("$project_dir");
				
		#read releases_info_file to obtain previous revision number
		if (&read_releases_info(\$prev_version, \$prev_revision, \$tag)) {
			printf("Failed to obtain previous release information");
			exit(1);
		}
				
		if (&get_scm_type(\$scm_type)) {
			printf("Failed to get source control type\n");
			exit(1);
		}

		printf("SCM:%s Previous build version:%s,revision:%s\n", $scm_type, $prev_version, $prev_revision);

		if ($scm_type eq "svn") {
			if (&gen_changelog_svn($prev_revision, $tag)) {
				printf("Failed to generate SVN changelog for project %s\n", $project_dir);
				exit(1);
			}
			if (&get_current_revision_svn(\$current_revision)) {
				printf("Failed to obtain current SVN revision for project %s\n", $project_dir);
				exit(1);
		}
		} elsif ($scm_type eq "git") {
			if (&gen_changelog_git($prev_revision, $tag)) {
				printf("Failed to generate GIT changelog for project %s\n", $project_dir);
				exit(1);
			}
			if (&get_current_revision_git(\$current_revision)) {
				printf("Failed to obtain current GIT revision for project %s\n", $project_dir);
				exit(1);
			}
		} else {
			printf("Unsupported SCM type %s\n", $scm_type);
			exit(1);
		}
		
		#write releases_info file to update revision number
		update_releases_info($g_current_version, $current_revision);
		chdir("$g_homedir");
	}
}


#determine the source control type (currently SVN or GIT)
sub get_scm_type
{
	my $scm_type = shift;
	if (-d ".git") {
		$$scm_type = "git";
		return 0;
	} elsif (-d ".svn") {
		$$scm_type = "svn";
		return 0;
	}
	return 1;
}

sub gen_changelog_git
{
	my $prev_revision = shift;
	my $tag = shift;
	my $i;
	my $date;
	my $commit_date;
	my $commit_author;
	my $len;
	my $num_items = 0;
	my $changelog_str.="\n$tag\n";
	#look for the date	
	$date = `git show $prev_revision |grep Date`;
	chomp($date);
	$date = substr($date, 8);

	my $save_to_changelog=0;
	my @log_list= `git log --date=iso --since="$date" --abbrev-commit`;

	foreach my $log_line (@log_list) {
		chomp($log_line);
		if (substr($log_line, 0, 7) eq "commit ") {
			if (substr($log_line, -7) eq $prev_revision) {
				#this revision was included in last commit, ignore it
				last;
			}
			$save_to_changelog=0;
		} elsif (substr($log_line, 0, 7) eq "Author:") {
			$commit_author=$log_line;
		} elsif (substr($log_line, 0, 5) eq "Date:") {
			$commit_date=$log_line;
		} elsif (length($log_line) <= 0) {
 			#ignore empty lines
		} else {
			if ($log_line =~ m/$g_changelog_tag/ ) {
				$log_line =~ s/$g_changelog_tag//g;
				$save_to_changelog=1;
				$changelog_str.="\n* $commit_author\n";
				$changelog_str.="  $commit_date\n";
				$num_items = $num_items+1;
			}

			#remove double-tabs
			$log_line =~ s/\t\t/\t/g;

			if ($save_to_changelog) {
				$changelog_str.=$log_line."\n";
			}
		}
	}

	printf("Number of commits:%d\n", $num_items);
	if ($num_items > 0) {
		$g_changelog_str .= $changelog_str;
	}
	return 0;
#print "==============================================================\n";
# 	print "$changelog_str\n";
# 	print "==============================================================\n";
}

sub gen_changelog_svn
{
	my $prev_revision = shift;
	my $tag = shift;	
	my $save_to_changelog=0;
	my $user;
	my $date;
	my $num_items = 0;

	my $changelog_str.="\n$tag\n";
	
	my @log_list= `svn log -r HEAD:$prev_revision`;

	foreach my $log_line (@log_list) {
		chomp($log_line);		
		if ($log_line =~ /r(\d+).*\| (\w+) \| (\w+)-(\w+)-(\w+) (\d+):(\d+):(\d+) -(\w+).* \|.*/) {
#			print "DEBUG revision:$1 user:$2 year:$3 month:$4 day:$5 hour:$6 min:$7 sec:$8 offset:$9\n";
			my $revision=$1;
			$user=$2;
			$date=$3."-".$4."-".$5." ".$6.":".$7.":".$8." -".$9;
			$save_to_changelog=0;
		} elsif (length($log_line) <= 0) {
			#ignore empty line
		} elsif (substr($log_line, 0, 4) eq "----") {
			#ignore delimiter line
		} else {
			if ($log_line =~ m/$g_changelog_tag/ ) {
				my $svn_user = &lookup_svn_user($user);
				$log_line =~ s/$g_changelog_tag//g;
				$save_to_changelog=1;
				$changelog_str.="\n* Author: $svn_user\n";
				$changelog_str.="  Date:   $date\n\n";
				$num_items = $num_items+1;
			}
			#remove double-tabs
			$log_line =~ s/\t\t/\t/g;

			if ($save_to_changelog) {
				$changelog_str.="     ".$log_line."\n";
			}
		}
	}
	printf("Number of commits:%d\n", $num_items);
	if ($num_items > 0) {
		$g_changelog_str .= $changelog_str;
	}
	return 0;
#	print "==============================================================\n";
#	print "$changelog_str\n";
#	print "==============================================================\n";
}


sub get_current_revision_svn
{
	my $revision = shift;
	$$revision = `svn info |grep Revision | cut -d ' ' -f2`;
	return 0;
}

sub get_current_revision_git
{
	my $revision = shift;
	
	$$revision=`git log --abbrev-commit -n 1 |grep commit | cut -d' ' -f 2`;
	return 0;
}



sub read_releases_info
{
	my $version = shift;
	my $revision = shift;
	my $tag = shift;
	my $revision_info_line = "";
	
	$revision_info_line=`tail -n 1 $g_revision_info_file`;
	chomp($revision_info_line);

	if ($revision_info_line =~ /(.*)\|(.*)/) {
		$$version=$1;
		$$revision=$2;
	} else {
		print "Failed to parse info line";
		return 1;
	}

	#if there is a tag, save it
	if (`grep -c \"tag:\" $g_revision_info_file` > 0) {
		my $tag_info_line=`grep \"tag:\" $g_revision_info_file |tail -n 1`;
		
		while(chomp($tag_info_line)) {};
		
		$$tag = substr($tag_info_line, length("tag:"));
	}
	return 0;
}

sub update_releases_info
{
	my $version = shift;
	my $revision = shift;

	while(chomp($version)) {};
	while(chomp($revision)) {};
	
	my $revision_line=$version."|".$revision;
	system("echo \"$revision_line\" >> $g_revision_info_file");
}


#generate changelog header
sub changelog_header
{
	my $i;
	my $today_date = `date +"%Y-%m-%d"`;
	$g_changelog_str.="\n\n";

	chomp($today_date);
	my $header="$today_date $g_project_name-$g_current_version\n";
	$g_changelog_str.=$header;
	for $i (2..length($header)) {		
		$g_changelog_str.="=";
	}
	$g_changelog_str.="\n";
}


sub update_changelog
{
	my $prev_changelog_str="";
	if (! -e $g_changelog_file) {
		system("touch \"$g_changelog_file\"");
	}
	
	open(FH, "<$g_changelog_file") or die "Can't open $g_changelog_file";
	while(<FH>) {
		$prev_changelog_str .= $_;	
	}
	close(FH);

	open(FH, ">$g_changelog_file") or die "Can't open $g_changelog_file";
	print FH $g_changelog_str;
	print FH $prev_changelog_str;
	close(FH);	
}


#look up email address for user
sub lookup_svn_user
{
	my $svn_user = shift;
	my @svn_users_string=`cat $g_homedir/$g_svn_users_file`;

	foreach my $svn_users_line (@svn_users_string) {
		if ( substr($svn_users_line, 0, 1) eq "#" || 
		     length($svn_users_line) <= 0 ) {

			#ignore comment, or empty lines
		} else {
			my $pos;
			my $user;
			my $email;
			$pos=index($svn_users_line, ":");
			
			$user=substr($svn_users_line, 0, $pos);
			$email=substr($svn_users_line, $pos+1);

			$user =~ s/^\s+//; #remove leading spaces
			chomp($user);
			$email =~ s/^\s+//; #remove leading spaces
			chomp($email);

#printf("DEBUG user:[%s] email:[%s]\n", $user, $email);
			if ($user eq $svn_user) {
				return $email;
			}
		}
	}
	die "Failed to find email for user $svn_user\n";	
}
