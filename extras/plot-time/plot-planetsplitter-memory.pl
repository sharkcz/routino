#!/usr/bin/perl
#
# Routino execution log plotter.
#
# Part of the Routino routing software.
#
# This file Copyright 2013-2014, 2022 Andrew M. Bishop
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

use strict;

# Check the command line

my $type="RAM";

$type="Disk" if(length(@ARGV)>0 && $ARGV[0] eq "--disk");

# Read the planetsplitter log file

open(SECTION   ,">gnuplot.section.tmp");
open(SUBSECTION,">gnuplot.subsection.tmp");

my $count=1;
my $startcount=0;
my $sectionmemory=0;
my $totalmemory=0;

while(<STDIN>)
  {
   s%\r*\n%%;

   next if(! $_);

   next if(m%^=%);

   if( m%^(\[[ 0-9:.]+\] *)?\[ *([0-9]+), *([0-9]+) MB\] *([^:]+)% && ! m%Finish Program$% )
     {
      my $memory;
      my $description=$4;

      if($type eq "RAM")
        { $memory=$2; }
      else
        { $memory=$3; }

      print SUBSECTION "$count $memory \"$description\"\n";

      $sectionmemory=$memory if($memory>$sectionmemory);
      $totalmemory=$memory if($memory>$totalmemory);
     }
   else
     {
      if($startcount>0)
        {
         my $boxcentre=($count+$startcount+0.5)/2;
         my $boxwidth=$count-$startcount-1;

         print SECTION "$boxcentre $sectionmemory $boxwidth\n";
        }

      $startcount=$count-0.5;
      $sectionmemory=0;
     }

   $count++;
  }

close(SECTION);
close(SUBSECTION);

# Plot using gnuplot

open(GNUPLOT,"|gnuplot");

print GNUPLOT <<EOF

set title "Planetsplitter Execution Memory (Maximum $type = $totalmemory MB)"

set noxtics

set ylabel "Sub-section Memory ($type MB)"
set logscale y
set yrange [1:]

set style fill solid 1.0
set boxwidth 0.8

set nokey

set style line 1 lt rgb "#FFC0C0" lw 1
set style line 2 lt rgb "#FF0000" lw 1

set term png size 1000,750
set output "planetsplitter.png"

plot "gnuplot.section.tmp" using 1:2:3 with boxes linestyle 1, \\
     "gnuplot.section.tmp" using 1:(\$2*1.1):(\$2) with labels font "Sans,9" center textcolor rgbcolor "#000000", \\
     "gnuplot.subsection.tmp" using 1:2 with boxes linestyle 2, \\
     "gnuplot.subsection.tmp" using (\$1+0.1):(1.1):3 with labels font "Sans,8" left rotate textcolor rgbcolor "#000000"

exit
EOF
;

close(GNUPLOT);

unlink "gnuplot.section.tmp";
unlink "gnuplot.subsection.tmp";
