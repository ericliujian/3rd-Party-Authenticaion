from socket import *

HSS_IP = '127.0.0.1'
MME_CLIENT_PORT = 42000  # this port is MME client for forwarding IMSI to HSS

# MME_IP = '127.0.0.1'
MME_IP = '192.168.0.108'
MME_SERVER_PORT = 12000  # this port is MME server for receiving IMSI from proxy2

mme_serverSocket = socket(AF_INET, SOCK_STREAM)
mme_serverSocket.bind((MME_IP, MME_SERVER_PORT))
mme_serverSocket.listen()

# Wait for connection from vUser of proxy2
while True:
    print("Waiting for proxy connection: ")
    conn, addr = mme_serverSocket.accept()
    with conn:
        print("Connected by ", addr)
        while True:
            data = conn.recv(20000)
            if not data:
                break
            else:
                data = data.decode('utf-8')
                print("Received data: ", data)
                # Connect to HSS
                mme_clientSocket = socket(AF_INET, SOCK_STREAM)
                mme_clientSocket.connect((HSS_IP, MME_CLIENT_PORT))
                mme_clientSocket.sendall(bytes(data, 'utf-8'))
                mme_clientSocket.close()
