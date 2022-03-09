import ast
import json
from socket import *

# vUser_IP = '127.0.0.1'
vUser_IP = '192.168.0.103'
MME_CLIENT_PORT = 21000  # this port is MME client port for forwarding auth challenge to vUser of proxy2

MME_IP = '127.0.0.1'
MME_SERVER_PORT = 50000  # this port is MME server for receiving auth challenge from HSS

mme_serverSocket = socket(AF_INET, SOCK_STREAM)
mme_serverSocket.bind((MME_IP, MME_SERVER_PORT))
mme_serverSocket.listen()

# Wait for connection from HSS
while True:
    print("Waiting for HSS connection: ")
    conn, addr = mme_serverSocket.accept()
    with conn:
        print("Connected by ", addr)
        while True:
            authChallenge = conn.recv(20000)
            if not authChallenge:
                break
            else:
                authChallenge = authChallenge.decode('utf-8')
                print("Received data: ", authChallenge)
                authChallenge = ast.literal_eval(authChallenge)
                xres = authChallenge.pop("XRES")
                authChallenge.pop("K")
                authChallenge = json.dumps(authChallenge)
                # Connect to proxy2
                mme_clientSocket = socket(AF_INET, SOCK_STREAM)
                mme_clientSocket.connect((vUser_IP, MME_CLIENT_PORT))
                mme_clientSocket.sendall(bytes(authChallenge, 'utf-8'))
                mme_clientSocket.close()
                xresFile = open("xres.txt", "w+")
                xresFile.write(xres)
                xresFile.close()
