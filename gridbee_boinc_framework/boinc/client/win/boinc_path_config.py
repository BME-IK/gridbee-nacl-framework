# $Id: boinc_path_config.py.in 18375 2009-06-11 16:01:36Z davea $

# This module prepends boinc/py/ to the python system path

# configure generates boinc/*/boinc_path_config.py from
# boinc/py/Boinc/boinc_path_config.py.in (in all directories that contain
# python scripts, so that they can directly import path_config.py)

import sys, os

TOP_BUILD_DIR = "/home/kalmi/boinc"
sys.path.insert(0, os.path.join(TOP_BUILD_DIR, 'py'))
