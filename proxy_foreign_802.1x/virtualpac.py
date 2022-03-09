from lxml import etree
import subprocess
import time
import signal

class VirtualPaC(object):
    def __init__(self):
        self.username = None
        self.password = None
        self.authenticated = False
        self.endpoint = None
    
    # sets the credentials.
    def set_credentials(self, username, password):
        self.username = username
        self.password = password
        config_root = etree.parse('/etc/openpana/config.xml')
        username_node = config_root.xpath('/CONFIG/PAC/USER')
        password_node = config_root.xpath('/CONFIG/PAC/PASSWORD')
        username_node[0].text = self.username
        password_node[0].text = self.password
        etree.ElementTree(config_root.getroot()).write('/etc/openpana/config.xml', pretty_print=True)

    # sets the endpoint
    def set_endpoint(self, endpoint):
        self.endpoint = endpoint
        config_root = etree.parse('/etc/openpana/config.xml')
        endpoint_node = config_root.xpath('/CONFIG/PAC/IP_PAA')
        endpoint_node[0].text = self.endpoint
        etree.ElementTree(config_root.getroot()).write('/etc/openpana/config.xml', pretty_print=True)
    
    # resets the stored credentials.
    def reset_credentials(self):
        self.username = None
        self.password = None
        self.authenticated = False
    
    def login(self):
        if self.authenticated:
            return True
        print("vpac: trying to log in...")
        proc = subprocess.Popen(['stdbuf', '-o0', 'openpac'], stdout=subprocess.PIPE)
        while True:
            line = proc.stdout.readline()
            if not line:
                break
            print(str(line))
            if 'EAP: Received EAP-Success' in str(line):
                print("vpac: received success from openpac!")
                self.authenticated = True
                break
            elif 'EAP: Received EAP-Failure' in str(line):
                print("vpac: received failure from openpac.")
                self.authenticated = False
        proc.kill()
        return self.authenticated
