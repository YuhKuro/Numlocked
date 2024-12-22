import socket

class SocketClient:
    def __init__(self, host='127.0.0.1', port=8080):
        self.host = host
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    def connect(self):
        try:
            self.sock.connect((self.host, self.port))
            print(f"Connected to server at {self.host}:{self.port}")
        except Exception as e:
            print(f"Failed to connect: {e}")
            self.sock.close()

    def sendMessage(self, message):
        try:
            self.sock.sendall(message.encode())
            response = self.sock.recv(1024).decode()
            print(f"Server response: {response}")
        except Exception as e:
            print(f"Failed to send/receive message: {e}")

    def close(self):
        self.sock.close()
        print("Connection closed")