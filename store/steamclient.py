

class SLException(BaseException):
	pass


# empty transport packet
class B_typ_0:
	sfmt = '<BBBL'

	def __init__(self, logger, sl_id=None, pkt=None):
		self.logger = logger
		self.sl_id = sl_id
		self.timestamp = time.time()
		self._ts = time.strftime("%Y-%m-%d %H:%M:%S",time.localtime(self.timestamp))
		self.payload = pkt
		self.epayload = None
		self.n_ver = N_TYP_VER
		self.b_ver = B_TYP_VER
		self.rssi = -10
		self.mesh = None
		self.bkey = None
		self.node_id = None
		self.bkey = None


	def setfields(self, node_id, mesh, bkey):
		if self.mesh and mesh != self.mesh:
			raise SLException("B_typ_0: node %s from %s, should be from %s " % \
				(self.sl_id, self.mesh, mesh))
		self.mesh = mesh
		self.node_id = node_id
		self.bkey = bkey
		if self.epayload:		# we have encrypted payload, decrypt
			decrypted = self.AES128_decrypt(self.epayload, self.bkey)
			try:
				self.payload = decrypted.decode().rstrip('\0')
			except:
				raise SLException("B_typ_0: node sl_id %s: wrong crypto key??" % self.sl_id)

	def unpack(self, epkt):
		dlen = len(epkt) - struct.calcsize(B_typ_0.sfmt)
		sfmt = "%s%is" % (B_typ_0.sfmt, dlen)
		nb_ver, self.node_id, rssi, self.sl_id, self.epayload = \
				 struct.unpack(sfmt, epkt)
		self.n_ver = (nb_ver & 0xF0) >> 4
		self.b_ver = nb_ver & 0x0F
		if self.n_ver != N_TYP_VER or self.b_ver != B_TYP_VER:
			raise SLException("B_typ_0: B/N type version mismatch: %s %s" % (self.n_ver, self.b_ver))
		self.rssi = rssi - 256
		if DBG >= 3: self.logger.debug("pkt self.n_ver %s self.b_ver %s self.rssi %s self.node_id %s self.sl_id %s", \
			self.n_ver, self.b_ver, self.rssi, self.node_id, self.sl_id)


	def pack(self):
		""" return a binary on-wire packet """
		if not self.node_id:
			raise SLException("B_typ_0: no node_id!")

		header = struct.pack(B_typ_0.sfmt, (self.n_ver << 4) | self.b_ver, self.node_id, self.rssi + 256, self.sl_id)
		self.epayload = self.AES128_encrypt(self.payload, self.bkey)
		return header + self.epayload


	def AES128_decrypt(self, pkt, bkey):
		if len(pkt) % 16 != 0:
			self.logger.error( "decrypt pkt len (%s)error: %s" % \
				(len(pkt)), " ".join("{:02x}".format(ord(c)) for c in pkt))
			return ""
		decryptor = AES.new(bkey, AES.MODE_ECB)
		return decryptor.decrypt(pkt)


	def AES128_encrypt(self, msg, bkey):
		if len(msg) % 16 != 0:
			pmsg = msg + "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"[len(msg) % 16:]
		else:
			pmsg = msg
		encryptor = AES.new(bkey, AES.MODE_ECB)
		return bytearray(encryptor.encrypt(pmsg))


	def dict(self):
		return {'_mesh': self.mesh, '_node_id': self.node_id, '_rssi': self.rssi, \
			'_sl_id': self.sl_id, '_ts': self._ts, 'payload': self.payload }


	def __str__(self):
		return str(self.dict())



