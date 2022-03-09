from socket import *

MME_IP = '192.168.0.108'
MME_SERVER_PORT = 37000

vUser_IP = '192.168.0.103'
MME_CLIENT_PORT = 21000

with socket(AF_INET, SOCK_STREAM) as mme_serverSocket:
    mme_serverSocket.bind((MME_IP, MME_SERVER_PORT))
    mme_serverSocket.listen()
    while True:
        conn, addr = mme_serverSocket.accept()
        with conn:
            res = conn.recv(20000)
            xresFile = open('xres.txt', 'r')
            line = str(xresFile.read())
            if res.decode('utf-8') == line:
                print("User authenticated")
                with socket(AF_INET, SOCK_STREAM) as mme_clientSocket:
                    mme_clientSocket.connect((vUser_IP, MME_CLIENT_PORT))
                    mme_clientSocket.sendall(bytes('200 OK', 'utf-8'))
                    mme_clientSocket.close()