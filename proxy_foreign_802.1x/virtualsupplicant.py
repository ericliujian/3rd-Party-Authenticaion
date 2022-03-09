import subprocess
import time

config_file_template = '''
ctrl_interface=/tmp/wpa_supplicant
ctrl_interface_group=0
ap_scan=0
network={{
key_mgmt=IEEE8021X
eap=MD5
identity="{username}"
password="{password}"
}}'''

cmd = 'stdbuf -o0 wpa_supplicant -c/tmp/wpasupplicant/wired-md5.conf -ieno2 -Dwired'

class VirtualSupplicant(object):
    def __init__(self, iface):
        self.username = None
        self.password = None
        self.authenticated = False
        self.wsproc = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE)
    
    # sets the credentials.
    def set_credentials(self, username, password):
        self.username = username
        self.password = password
    
    # resets the stored credentials.
    def reset_credentials(self):
        self.username = None
        self.password = None
        self.authenticated = False
    
    def login(self):
        if self.authenticated:
            return True
        print("vsupplicant: trying to log in...", self.username, self.password)
        # then, run the authentication procedure.
        subprocess.run(['wpa_cli', 'identity', '0', self.username], stdout=subprocess.PIPE)
        subprocess.run(['wpa_cli', 'password', '0', self.password], stdout=subprocess.PIPE)
        while True:
            r = self.wsproc.stdout.readline()
            if not r:
                self.authenticated = False
                break
            print(str(r))
            if 'CTRL-EVENT-EAP-SUCCESS' in str(r):
                print("vsupplicant: received success from wpa_cli!")
                self.authenticated = True
                break
            elif 'CTRL-EVENT-EAP-FAILURE' in str(r):
                print("vsupplicant: received failure from wpa_cli.")
                self.authenticated = False
                break
        self.wsproc.kill()
        self.wsproc = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE)
        return self.authenticated
