#!/usr/bin/env python

#ENVIRONMENT-ARTEFACT: .hooks/git/commit-msg.py
#EXPECTED_EXIT_CODE: 2

import tempfile
import os
import subprocess
import sys

msg = [
		"ViDDK-3. Check a commit msg with to few lines\n",
		"\n"
      ]

with tempfile.NamedTemporaryFile( mode="w+" ) as f:
	for line in msg:
		f.write( line )
	f.flush()
	rc = subprocess.call( [os.environ["ARTEFACT"], f.name] )
	sys.exit(rc)
