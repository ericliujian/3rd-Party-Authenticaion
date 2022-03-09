import subprocess as sub

coa_packet = '''
Calling-Station-Id='{mac}'
Cisco-AVPair='subscriber:command=reauthenticate'
NAS-IP-Address='10.0.2.1'
'''

class CaptiveAuthenticator(object):
    device_mac = None
    username = None
    password = None
    authenticated = False
    
    def initialized(self, device_mac):
        return device_mac == self.device_mac
    
    def set_mac(self, mac):
    	self.device_mac = mac
    
    def set_credentials(self, username, password):
        self.username = username
        self.password = password
    
    def complete(self):
        self.authenticated = True
    
    def send_coa(self):
        packet_txt = coa_packet.format(mac=self.device_mac)
        sub.run('echo "' + packet_txt + '" | radclient -x 10.0.2.2:3799 coa proxy_secret', shell=True)
    
    
