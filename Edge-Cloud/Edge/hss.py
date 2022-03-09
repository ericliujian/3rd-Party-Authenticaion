from socket import *
import json

HSS_IP = '127.0.0.1'
HSS_SERVER_PORT = 42000  # this port is HSS server for receiving IMSI from MME

MME_IP = '127.0.0.1'
HSS_CLIENT_PORT = 50000  # this port is HSS client for forwarding auth challenge to MME

hss_serverSocket = socket(AF_INET, SOCK_STREAM)
hss_serverSocket.bind((HSS_IP, HSS_SERVER_PORT))
hss_serverSocket.listen()

# Waiting for connection from MME
while True:
    print("Waiting from MME connection: ")
    conn, addr = hss_serverSocket.accept()
    with conn:
        print("Connected by ", addr)
        while True:
            IMSI = conn.recv(20000)
            if not IMSI:
                break
            else:
                IMSI = IMSI.decode('utf-8')
                print("Received data ", IMSI)
                jsonFile = open('userinfo.json', 'r')
                userInfo = json.load(jsonFile)
                jsonFile.close()
                try:
                    userInfo = userInfo[IMSI]
                    hss_clientSocket = socket(AF_INET, SOCK_STREAM)
                    hss_clientSocket.connect((MME_IP, HSS_CLIENT_PORT))
                    authChallenge = {"K": userInfo["K"], "RAND": userInfo["RAND"], "XRES": userInfo["XRES"],
                                   "AUTN": userInfo["AUTN"]}
                    authChallenge = json.dumps(authChallenge)
                    hss_clientSocket.sendall(bytes(authChallenge, 'utf-8'))
                    hss_clientSocket.close()
                except KeyError:
                    print(IMSI + ' has no registered user')
