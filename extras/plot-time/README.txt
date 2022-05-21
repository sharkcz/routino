               Planetsplitter Execution Time or Memory Analysis
               ================================================

A Perl script that uses Gnuplot to plot a graph of the time taken or
memory used by the planetsplitter program to run.


plot-planetsplitter-time.pl
---------------------------

Example usage:

planetsplitter --loggable --logtime ... > planetsplitter.log

plot-planetsplitter-time.pl < planetsplitter.log

This will generate a file called planetsplitter.png that contains the graph of
the execution time.


plot-planetsplitter-memory.pl
-----------------------------

Example usage:

planetsplitter --loggable --logmemory ... > planetsplitter.log

plot-planetsplitter-memory.pl --ram  < planetsplitter.log

OR

plot-planetsplitter-memory.pl --disk < planetsplitter.log

This will generate a file called planetsplitter.png that contains the graph of
the RAM or memory mapped disk usage.
