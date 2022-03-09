import socket

vIdP_IP = '127.0.0.1'
vIdP_SERVER_PORT = 11000

fog_IP = '127.0.0.1'
fog_SERVER_PORT = 10000

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as fog_serverSocket:
    fog_serverSocket.bind((fog_IP, fog_SERVER_PORT))
    fog_serverSocket.listen()

    # Waiting for connection from proxy1/User
    while True:
        print('Waiting to receive IMSI...')
        conn, addr = fog_serverSocket.accept()
        with conn:
            print('Connected by', addr)
            while True:
                data = conn.recv(20000)
                if not data:
                    break
                else:
                    data = data.decode('utf-8')
                    print('Received data: ' + data)
                    # Connect to vIdP of proxy2
                    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as fog_clientSocket:
                        fog_clientSocket.connect((vIdP_IP, vIdP_SERVER_PORT))
                        fog_clientSocket.sendall(bytes(data, 'utf-8'))
                        fog_clientSocket.close()
