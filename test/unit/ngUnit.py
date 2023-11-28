#! /usr/bin/python

#IS-INTEGRATION-KIT yes

import getopt
import sys
import os.path
import os
import subprocess
import re
import shutil
import tempfile
import signal

class InterruptHandler:
	def __init__( self, signal=signal.SIGINT, handler=None ):
		self.signal = signal
		self.handler = handler

	def __enter__( self ):
		self.interrupted = False
		self.released = False

		self.original_handler = signal.getsignal(self.signal)

		def handler(signum, frame):
			if None is not self.handler:
				self.handler( signum )
			self.release()
			self.interrupted = True

		signal.signal(self.signal, handler)
		return self

	def __exit__(self, type, value, tb):
		if (False is self.interrupted) and (None is not self.handler):
			self.handler( None )
		self.release()

	def release(self):
		if self.released:
			return False

		signal.signal(self.signal, self.original_handler)
		self.released = True
		return True

class Repo:
	def __init__( self ):
		self.pwd = os.path.split( os.path.abspath(__file__) )[0]
		self.root = os.path.split( self.pwd )[0]
		self.root = os.path.split( self.root )[0]
		self.tmpPath = tempfile.mkdtemp()

class Application:
	def __init__(self, app, stdout=None):
		self.app = app
		self.stdout = stdout

	def run( self, args=None ):
		cmd = [ self.app ]
		if None is not args:
			cmd.extend( args )
		with open(os.devnull, "w") as fp:
			if None is not self.stdout:
				r = subprocess.call( cmd, stdout=self.stdout, stderr=fp )
			else:
				r = subprocess.call( cmd, stdout=fp, stderr=fp )
		if r is not 0:
			return False
		else:
			return True

class Test:
	UNKNOWN_TEST = 0
	C_TEST = 1
	PY_TEST = 2

	def __init__( self, name, type ):
		self.name = name
		if type == "c":
			self.type = self.C_TEST
		elif type == "py":
			self.type = self.PY_TEST
		else:
			self.type = self.UNKNOWN_TEST
		self.desc = { "EXPECTED_EXIT_CODE": 0 }

	def isPythonTest( self ):
		return self.type == self.PY_TEST

	def isCTest( self ):
		return self.type == self.C_TEST

	def compile( self, target ):
		if not self.isCTest():
			return False
		app = Application( "gcc" )
		return app.run( ["-I"+repo.tmpPath, "-o", target, repo.pwd + "/" + self.name] )

	def readSelfDescription( self ):
		with open( repo.pwd + "/" + self.name, "r" ) as f:
			for line in f:
				match = re.match( r"^#EXPECTED_EXIT_CODE: (\d+)", line )
				if None != match:
					self.desc["EXPECTED_EXIT_CODE"] = match.group(1)
					continue
				match= re.match( r"^#([a-zA-Z0-9\-_]+):\s*(.+)", line )
				if None != match:
					if match.group(1) in self.desc:
						self.desc[match.group(1)].append( match.group(2))
					else:
						self.desc[match.group(1)] = [match.group(2)]
					continue;

test_set = []
repo = Repo()

def run_test( test, verbose ):
	file = os.path.join( repo.pwd, test.name )
	test.readSelfDescription()
	env = test.desc
	cmd = [ "env" ]
	for key in env:
		if key.startswith("ENVIRONMENT-"):
			cmd.append( key[12:] + "=" + repo.root + "/" + str(env[key][0]))

	if test.isPythonTest():
		cmd.append( file )
	elif test.isCTest():
		if "CODE-EXTRACT" in test.desc:
			with open( repo.tmpPath + "/extracted-code", "w" ) as f:
				app = Application( repo.root + "/tools/bin/code-extract", stdout=f )
				args = [ "-extra-arg=-I"+repo.root+"/driver"]
				srcs = set()
				extracts = set()
				for line in test.desc["CODE-EXTRACT"]:
					match= re.match( r"^([a-zA-Z0-9/\.\-_]+)::(.+)", line )
					if None is not match:
						srcs.add( match.group(1) )
						extracts.add( match.group(2) )

				for name in extracts:
					args.append( "-extract=" + name )
				for src in srcs:
					args.append( repo.root + "/" + src )
				app.run(args)
		if os.path.isfile( repo.tmpPath + "/artefact"):
			os.remove( repo.tmpPath + "/artefact" )
		test.compile( repo.tmpPath + "/artefact" )
		cmd.append( repo.tmpPath + "/artefact" )

	if verbose:
		cmd.append( "-v" )
	with open(os.devnull, 'w') as fp:
		r = subprocess.call( cmd, stdout=fp, stderr=fp );
	if r is not int(env["EXPECTED_EXIT_CODE"]):
		if test.isCTest():
			print test.name + "  failed"
		else:
			print test.name + " failed"
		return 1
	else:
		if test.isCTest():
			print test.name + "  ok"
		else:
			print test.name + " ok"
		return 0

def find_all_tests():
	pattern = re.compile( "utest\.(\d{3})\.(\d{4})\.(py|c)" )
	for entry in os.listdir( repo.pwd ):
		match = re.match( pattern, entry )
		if match is not None:
			test_set.append( Test(entry, match.group(3)) )

def exit_handler( signum ):
	if None is not repo.tmpPath:
		shutil.rmtree( repo.tmpPath )

def main(argv):
	rc = 0
	with InterruptHandler( handler = exit_handler ) as h:
		path = os.path.split( repo.pwd )[0]
		path = os.path.join( path, "modules", "python")
		if "PYTHONPATH" in os.environ.keys():
			os.environ["PYTHONPATH"] += os.pathsep + path
		else:
			os.environ["PYTHONPATH"] = path

		v = False
		try:
			opts, args = getopt.getopt(argv, "v", ["verbose"])
			for opt, arg in opts:
				if opt in ("-v","--verbose"):
					v = True
		except getopt.GetoptError:
			return 2

		# This will update test_set
		find_all_tests()

		for test in test_set:
			if False is h.interrupted:
				r = run_test( test, v )
				if r is not 0:
					rc = 1

		print
		if rc is not 0 or True is h.interrupted:
			print "Completed, with failures"
		else:
			print "Completed, successfully"
	return rc

if __name__ == "__main__":
    sys.exit( main(sys.argv[1:]) )
