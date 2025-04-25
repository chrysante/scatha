#!/usr/bin/perl

use warnings;
use strict;
use Getopt::Std;
use File::Spec;

sub parse_options {
    my %options = ();
    getopts("r:i", \%options);
    if (not defined $options{r}) {
        print "Missing option -r\n";
        exit;
    }
    if (not -d $options{r}) {
        print "$options{r} is not a directory\n";
        exit;
    }
    return %options;
}

# Computes an include guard name from a relative file path
sub make_include_guard {
    my ($relpath) = @_;
    my $newstring = $relpath =~ s/(\/|\.)/_/gr;
    return uc($newstring) . "_";
}

# Opens the file and reads it line by line into an array and returns that
sub read_file_to_array {
    my ($filepath) = @_;
    open my $contents, $filepath or die $!;
    my @array = ();
    while (my $line = <$contents>) {   
        push @array, $line; 
    }
    return @array;
}

# Returns true if the given string is only white space symbols
sub is_blank {
    return shift =~ /^\s*$/;
}

# Returns true if the given string is only white space symbols or C or C++ style comments
sub is_blank_or_comment {
    my ($arg) = @_;
    return is_blank($arg) || $arg =~ /(^\s*\/\*(.*?)\*\/$)|(^\s*\/\/.*$)/;
}

our $alpha = qr/[a-zA-Z_]/;
our $alnum = qr/[a-zA-Z0-9_]/;

# Returns true if the file already has include guards
sub has_include_guard {
    my ($lines) = @_;
    my $have_ifndef = 0;
    my $have_define = 0;
    foreach my $line (@$lines) {
        if (is_blank_or_comment($line)) {
            next;
        }
        if ($line =~ /\s*#\s*ifndef\s+${alpha}${alnum}*\s*/) {
            $have_ifndef = 1;
            next;
        }
        if ($line =~ /\s*#\s*define\s+${alpha}${alnum}*\s*/) {
            if (not $have_ifndef) {
                return 0;
            }
            $have_define = 1;
            last;
        }
        return 0;
    }
    if (not $have_define) {
        return 0;
    }
    foreach my $line (reverse @$lines) {
        if (is_blank_or_comment($line)) {
            next;
        }
        if ($line =~ /\s*#\s*endif.*/) {
            return 1;
        }
    }
    return 0;
}

# Checks if the first non-blank, non-comment line is "// NO-INCLUDE-GUARD"
sub has_no_include_guard_directive {
    my ($lines) = @_;
    foreach my $line (@$lines) {
        if ($line =~ /^\s*\/\/\s*NO-INCLUDE-GUARD\s*$/) {
            return 1;
        }
        if (is_blank_or_comment($line)) {
            next;
        }
        last; 
    }
    return 0;
}

# Fixes existing include guards, i.e. sets the name to $include_guard and fixes
# #endif comment
# Must only be called if `has_include_guard()` returns `true`
sub fix_existing_include_guard {
    my ($lines, $include_guard) = @_;
    foreach my $line (@$lines) {
        if (is_blank_or_comment($line)) {
            next;
        }
        if ($line =~ s/\s*#\s*ifndef\K\s*(${alpha}${alnum}*)\s*/ ${include_guard}\n/g) {
            next;
        }
        if ($line =~ s/\s*#\s*define\K\s*(${alpha}${alnum}*)\s*/ ${include_guard}\n/g) {
            last;
        }
    }
    foreach my $line (reverse @$lines) {
        if (is_blank_or_comment($line)) {
            next;
        }
        if ($line =~ s/\s*#\s*endif\K(.*)/ \/\/ ${include_guard}/g) {
            last;
        }
    }
}

# Removes the first occurrence of "#pragma once"
sub remove_pragma_once {
    my ($lines) = @_;
    for my $i (0 .. $#$lines) {
        my $line = @$lines[$i];
        if (is_blank_or_comment($line)) {
            next;
        }
        if ($line =~ /^\s*#\s*pragma\s+once\s*$/) {
            splice(@$lines, $i, 1);
            return;
        }
        $i++;
    }
}

# Fixes existing or adds missing include guard
sub fix_include_guard {
    my ($lines, $relpath) = @_;
    my $include_guard = make_include_guard($relpath);
    if (has_include_guard($lines)) {
        fix_existing_include_guard($lines, $include_guard);
        return;
    }
    remove_pragma_once($lines);
    if (!is_blank(@$lines[0])) {
        unshift @$lines, "\n";
    }
    unshift @$lines, "#ifndef $include_guard\n",
                    "#define $include_guard\n";
    if (!is_blank(@$lines[-1])) {
        push @$lines, "\n";
    }
    push @$lines, "#endif // $include_guard\n";
}

#
sub write_lines {
    my ($fh, $lines) = @_;
    foreach my $line (@$lines) {
        print $fh $line;
    }
}

sub main {
    my %options = parse_options();
    my $rootdir = $options{r};
    foreach my $filepath (@ARGV) {
        my $relpath = File::Spec->abs2rel($filepath, $rootdir);
        my @lines = read_file_to_array($filepath);
        if (has_no_include_guard_directive(\@lines)) {
            next;
        }
        fix_include_guard(\@lines, $relpath);
        if ($options{i}) {
            open(my $fh, '>', $filepath) or die $!;
            write_lines($fh, \@lines);
        }
        else {
            write_lines(*STDOUT, \@lines);
        }
    }   
}

main();
