#!/usr/bin/env python

#ENVIRONMENT-ARTEFACT: .hooks/git/commit-msg.py
#EXPECTED_EXIT_CODE: 0

import tempfile
import os
import subprocess
import sys

msg = [
		"ViDDK-7. Check a commit msg without blank line\n",
		"\n",
		"This validates a correct formamtted commit message.\n"
		"\n"
		"# That happens to a very very very long comment that should not count since it begins with #."
      ]

with tempfile.NamedTemporaryFile( mode="w+" ) as f:
	for line in msg:
		f.write( line )
	f.flush()
	rc = subprocess.call( [os.environ["ARTEFACT"], f.name] )
	sys.exit(rc)
