#!/bin/sh
cd "/e/Eternal Darkness Recomp Fork"
python -I -c "import sys;sys.path.insert(0,'tools/ed/facts/checks/skeptic/usemix');exec(open('tools/ed/facts/checks/skeptic/usemix/$1.py').read())" "$@" 2>&1
