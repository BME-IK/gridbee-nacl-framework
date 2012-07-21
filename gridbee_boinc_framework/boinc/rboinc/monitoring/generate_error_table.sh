#!/bin/bash

# $Id: generate_error_table.sh 23083 2011-02-22 21:28:13Z tonig $
# Regenerate the error table hash. To be run upon BOINC update

table=monitoring_errors_table.pm
input=$HOME/BOINC/boinc/lib/error_numbers.h

echo "our %error_table=();" > $table
grep ERR_ $input | 
   awk '{print "$error_table{" $3 "}=\"" $2 "\";"}' >> $table



