from socket import *

clientIP = '127.0.0.1'
clientPort = 10000

serverIP = '127.0.0.1'
serverPort = 39999

with socket(AF_INET, SOCK_STREAM) as serverSocket:
    serverSocket.bind((serverIP, serverPort))
    serverSocket.listen()
    while True:
        if input("Press s to start server:") == 's':
            with socket(AF_INET, SOCK_STREAM) as clientSocket:
                clientSocket.connect((clientIP, clientPort))
                clientSocket.sendall(bytes("111111111111111", 'utf-8'))
                clientSocket.close()
            conn, addr = serverSocket.accept()
            with conn:
                data = conn.recv(20000)
                if not data:
                    break
                else:
                    data = data.decode('utf-8')
                    print(data)
                    if data == '200 OK':
                        print("User authenticated")
                    else:
                        with socket(AF_INET, SOCK_STREAM) as clientSocket:
                            clientSocket.connect((clientIP, clientPort))
                            clientSocket.sendall(bytes("a54211d5e3ba50bf", 'utf-8'))
                            clientSocket.close()