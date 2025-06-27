import socket

class SocketServer:
    def __init__(self, host='127.0.0.1', port=8080):
        self.host = host
        self.port = port
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.client_socket = None  # Store the connected client socket

    def start(self):
        try:
            self.server_socket.bind((self.host, self.port))
            self.server_socket.listen(5)  # Listen for up to 5 connections
            print(f"Server started on {self.host}:{self.port}")
            
            while True:
                print("Waiting for a connection...")
                self.client_socket, client_address = self.server_socket.accept()
                print(f"Connection established with {client_address}")
                
                self.handleClient()
        except Exception as e:
            print(f"Server error: {e}")
        finally:
            self.server_socket.close()

    def sendMessage(self, message):
        """
        Sends a message to the connected client using the stored client socket.
        """
        if self.client_socket is None:
            print("No client is connected.")
            return

        try:
            self.client_socket.sendall(message.encode())
            print(f"Sent to client: {message}")
        except Exception as e:
            print(f"Failed to send message: {e}")

    def handleClient(self):
        """
        Handles communication with the connected client in a persistent loop.
        """
        try:
            while True:
                # Receive a message from the client
                message = self.client_socket.recv(1024).decode()
                if not message:  # If the client disconnects
                    print("Client disconnected.")
                    break

                print(f"Received from client: {message}")
                
                #handle message. If client is responding, update GUI. EX, game mode.
        except Exception as e:
            print(f"Error handling client: {e}")
        finally:
            self.client_socket.close()
            self.client_socket = None
            print("Client connection closed")
        
    def closeConnection(self):
        """
        Closes the connection with the client.
        """
        if self.client_socket is not None:
            self.client_socket.close()
            self.client_socket = None
            print("Client connection closed")
        else:
            print("No client is connected.")
