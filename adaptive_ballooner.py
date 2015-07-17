import subprocess

# path to ballooning input pipe
PIPE = '/tmp/JavaBalloonSizeBytesInput'

# number or bytes in one kilobyte
KB = 2**10
# number or bytes in one megabyte
MB = 2**20
# number or bytes in one gigabyte
GB = 2**30


class Ballooner():
	"""This class saves the pid of the ballooned java program, 
	and changes the balloon size with update_balloon_in_bytes(size_in_bytes).

	Example:

	>>> from adaptive_ballooner import *
	>>> b = Ballooner(9999) # control the balloon of process 9999
	>>> b.upate_balloon_in_bytes(100 * MB) # changes the balloon size to 100M. MB defined in adaptive_ballooner.py
	"""

	def __init__(self, pid):
		self._pid = pid

	def pid(self):
		"""Returns the pid of the controlled java program.
		"""
		return self._pid

	def gc(self):
		"""Runs System.GC() using jcmd, a tool introduced in OpenJDK 7.
		"""
		if subprocess.check_output(['jcmd', str(self.pid()), 'GC.run']) != '{}:\n'.format(self.pid()):
			raise Exception("GC not successful")

	def update_balloon_in_bytes(self, size_in_bytes):
		"""Writes desired balloon size in bytes, then forces GC.
		"""
		# write desired balloon size and flush.
		input_pipe = open(PIPE, 'w+')
		input_pipe.write('{}\n'.format(size_in_bytes))
		input_pipe.close()
		# force (hopefully full) gc so that the old gen ballooning can take effect immediately.
		# Otherwise we may have to wait long time.
		self.gc()
