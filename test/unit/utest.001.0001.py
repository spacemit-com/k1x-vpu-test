#!/usr/bin/env python

#ENVIRONMENT-ARTEFACT: .hooks/git/commit-msg.py

import tempfile
import os
import subprocess
import sys

msg = [
		"ViDDK-1. Check a correctly formatted commit msg\n",
		"\n",
		"This validates a correct formamtted commit message.\n"
      ]

with tempfile.NamedTemporaryFile( mode="w+" ) as f:
	for line in msg:
		f.write( line )
	f.flush()
	rc = subprocess.call( [os.environ["ARTEFACT"], f.name] )
	sys.exit(rc)
