#!/usr/bin/perl
#
# Routino translation replacement Perl script
#
# Part of the Routino routing software.
#
# This file Copyright 2014 Andrew M. Bishop
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

# Constants

my @translation_files=(<translation.*.txt>);
my $xml_file="../data/translations.xml";
my $output_dir="../www/routino";

my @html_template_files=(<*.html>);

my %languages=();
my %translations=();


# Read in the translations (HTML)

foreach my $translation_file (@translation_files)
  {
   $translation_file =~ m%translation.([^.]+).txt%;

   # Add to list of languages

   my $language=$1;

   if(! defined $languages{$language})
     {
      $languages{$language}=1;

      $translations{$language}={};
      $translations{$language}->{codes}={};
      $translations{$language}->{html}=0;
      $translations{$language}->{xml}=0;
     }

   # Process the file

   open(FILE,"<$translation_file");

   while(<FILE>)
     {
      s%\r*\n%%;

      next if(m%^#%);
      next if(m%^$%);

      # Single line entries

      if(m%\@\@%)
        {
         my($code,$text)=split("\t");

         if(defined $translations{$language}->{codes}->{$code})
           {
            print STDERR "Language: $language DUPLICATED codeword '$code'\n";
           }
         else
           {
            $translations{$language}->{html}++;
            $translations{$language}->{codes}->{$code}={};
            $translations{$language}->{codes}->{$code}->{text}=$text;
            $translations{$language}->{codes}->{$code}->{used}=0;
           }
        }

      # Multi-line entries

      if(m%\$\$([^\$]+)\$\$%)
        {
         my($code,$text)=("@@".$1."@@","");

         while(<FILE>)
           {
            last if(m%\$\$%);

            $text=$text.$_;
           }

         $text =~ s%\r*\n$%%;

         if(defined $translations{$language}->{codes}->{$code})
           {
            print STDERR "Language: $language DUPLICATED codeword '$code'\n";
           }
         else
           {
            $translations{$language}->{html}++;
            $translations{$language}->{codes}->{$code}={};
            $translations{$language}->{codes}->{$code}->{text}=$text;
            $translations{$language}->{codes}->{$code}->{used}=0;
           }
        }
     }

   close(FILE);
  }


# Read in the translations (XML)

open(XML,"<$xml_file");

my $language="";

while(<XML>)
  {
   if(m%<language +lang=["']([^"']+)["'] *>%)
     {
      # Add to list of languages

      $language=$1;

      if(! defined $languages{$language})
        {
         $languages{$language}=1;

         $translations{$language}={};
         $translations{$language}->{codes}={};
         $translations{$language}->{html}=0;
         $translations{$language}->{xml}=0;
        }

      next;
     }

   $language="" if(m%</language *>%);

   next if(!$language);

   # Count the phrases

   if(m%<[a-z]+% && m% +(text|string)=["']([^"']+)["']%)
     {
      $translations{$language}->{xml}++;
     }
  }

close(XML);


# Sort out the languages

my @languages=();

push(@languages,"en");

foreach my $language (sort (keys %languages))
  {
   push(@languages,$language) if($language ne "en");
  }


# Create the files

foreach my $html_template_file (@html_template_files)
  {
   foreach my $language (@languages)
     {
      next if(!$translations{$language}->{html});

      print "Language: $language File: $html_template_file\n";

      my $language_meta=0;
      my $language_meta_string="";

      open(HTML_IN ,"<$html_template_file");
      open(HTML_OUT,">$output_dir/$html_template_file.$language");

      while(<HTML_IN>)
        {
         my $line=$_;

         # Language selection - special handling

         if($line =~ m%\$\$LANGUAGES-META\$\$%)
           {
            $language_meta=1-$language_meta;

            if($language_meta==0)
              {
               foreach my $language2 (@languages)
                 {
                  my $LANGUAGE2=$language2;
                  $LANGUAGE2 =~ tr%a-z%A-Z%;

                  $line=$language_meta_string;

                  if($language eq $language2)
                    {
                     $line =~ s%CHECKED-XX%checked%g;
                    }
                  else
                    {
                     $line =~ s%CHECKED-XX%%g;
                    }

                  $line =~ s%xx%$language2%g;
                  $line =~ s%XX%$LANGUAGE2%g;

                  if(!$translations{$language2}->{html})
                    {
                     $line =~ s%<a.+</a>%%;
                    }

                  if(!$translations{$language2}->{xml})
                    {
                     $line =~ s%<input .+>%%;
                    }

                  foreach my $code (keys $translations{$language}->{codes})
                    {
                     if($line =~ s%$code%$translations{$language2}->{codes}->{$code}->{text}%g)
                       {$translations{$language2}->{codes}->{$code}->{used} = 1;}
                    }

                  # Remove un-needed spaces

                  $line =~ s%[\t ]+% %g;
                  $line =~ s%\n %\n%g;
                  $line =~ s%^ %%g;

                  print HTML_OUT $line;
                 }
              }

            next;
           }

         if($language_meta)
           {
            $language_meta_string.=$line;
            next;
           }

         # Replace with translated phrases

         foreach my $code (keys $translations{$language}->{codes})
           {
            if($line =~ s%$code%$translations{$language}->{codes}->{$code}->{text}%g)
              {$translations{$language}->{codes}->{$code}->{used} = 1;}
           }

         # Replace what is left with English phrases

         foreach my $code (keys $translations{$languages[0]}->{codes})
           {
            $line =~ s%$code%$translations{$languages[0]}->{codes}->{$code}->{text}%g;
           }

         if($line =~ m%\@\@%)
           {
            print STDERR "   Unmatched codeword in line: $line";
           }

         # Remove un-needed spaces

         $line =~ s%[\t ]+% %g;
         $line =~ s%\n %\n%g;
         $line =~ s%^ %%g;

         print HTML_OUT $line;
        }

      close(HTML_IN);
      close(HTML_OUT);
     }
  }


# Check the languages

foreach my $language (@languages)
  {
   foreach my $code (keys $translations{$language}->{codes})
     {
      if(! $translations{$language}->{codes}->{$code}->{used})
        {
         print STDERR "Language: $language UNUSED codeword: $code\n";
        }
     }
  }


# Print the translation coverage

print "\n";

print "Translation Coverage\n";
print "====================\n";

print "\n";
print "            Number      Fraction\n";
print "Language  HTML   XML   HTML   XML\n";
print "--------  ----   ---   ----   ---\n";

foreach my $language (@languages)
  {
   printf("%-6s     %3d   %3d  %4.0f%% %4.0f%%\n",$language,
                                                  $translations{$language}->{html},
                                                  $translations{$language}->{xml},
                                                  100.0*$translations{$language}->{html}/$translations{$languages[0]}->{html},
                                                  100.0*$translations{$language}->{xml} /$translations{$languages[0]}->{xml});
  }