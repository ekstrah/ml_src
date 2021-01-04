import logging
from cassandra import ConsistencyLevel
from cassandra.cluster import Cluster, BatchStatement
from cassandra.query import SimpleStatement



class cassandraSampleText:
	def __init__(self):
		self.cluster = None
		self.session = None
		self.keyspaces = "beta"
		self.log = None


	def __del__(self):
		self.cluster.shutdown()

	def create_session(self):
		self.cluster = Cluster(['localhost'])
		self.session = self.cluster.connect(self.keyspaces)

	def getsession(self):
		return self.session

	def setlogger(self):
		log = logging.getLogger()
		log.setLevel('INFO')
		handler = logging.StreamHandler()
		handler.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(name)s: %(message)s"))
		log.addHandler(handler)
		self.log = log

	def select_data(self):
		rows = self.session.execute('select * from openstack;')
		for row in rows:
			print("id = "+str(row.id)+ "  " + "proc_name = " + row.proc_name + "   "+"thread_id = "+ row.thread_id)

if __name__ == '__main__':
	example1 = cassandraSampleText()
	example1.create_session()
	example1.setlogger()
	example1.select_data()
