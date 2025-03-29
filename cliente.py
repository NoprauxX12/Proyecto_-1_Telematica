import socket
import sys

HOST = "localhost"
PORT = 8080
BOARD_SIZE = 10



class BattleshipClient:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.name = ""
        self.email = ""

    def connect(self):
        try:
            self.sock.connect((HOST, PORT))
            print(f"Conectado al servidor {HOST}:{PORT}")
        except Exception as e:
            print(f"Error al conectar: {e}")
            sys.exit(1)

    def login(self):
        self.name = input("Ingresa tu nombre: ")
        self.email = input("Ingresa tu email: ")
        self.send_message(f"LOGIN|{self.name}|{self.email}")
        response = self.receive_message()
        if response != "LOGIN|OK":
            print("Error en el login")
            sys.exit(1)

    def send_message(self, message):
        try:
            self.sock.sendall(message.encode())
        except Exception as e:
            print(f"Error enviando mensaje: {e}")
            sys.exit(1)

    def receive_message(self):
        try:
            data = self.sock.recv(1024).decode()
            if not data:
                raise ConnectionError("Conexión cerrada por el servidor")
            return data
        except Exception as e:
            print(f"Error recibiendo mensaje: {e}")
            sys.exit(1)



if __name__ == "__main__":
    client = BattleshipClient()
    client.connect()
    client.login()
