import socket
import sys

HOST = "localhost"
PORT = 8080

class BattleshipClient:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.name = ""
        self.email = ""
        self.my_turn = False

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
        print("Login exitoso. Esperando oponente...")

    def send_message(self, message):
        try:
            self.sock.sendall(message.encode())
        except Exception as e:
            print(f"Error enviando mensaje: {e}")
            sys.exit(1)

    def receive_message(self):
        try:
            data = self.sock.recv(1024).decode().strip()
            if not data:
                raise ConnectionError("Conexión cerrada por el servidor")
            return data
        except Exception as e:
            print(f"Error recibiendo mensaje: {e}")
            sys.exit(1)

    def handle_game_loop(self):
        """Maneja el bucle principal del juego"""
        while True:
            message = self.receive_message()
            msg_type, _, content = message.partition('|')
            
            if msg_type == "YOUR_TURN":
                self.my_turn = True
                print("\n--- ES TU TURNO ---")
                print(content)  # Mensaje adicional del servidor
                self.handle_turn()
                
            elif msg_type == "WAIT_TURN":
                self.my_turn = False
                print(f"\nEsperando turno... {content}")
                
            elif msg_type == "GAME_START":
                print("\n¡El juego ha comenzado!")
                
            elif msg_type == "GAME_OVER":
                print(f"\n¡Juego terminado! {content}")
                break
                
            elif msg_type == "ERROR":
                print(f"\nError: {content}")
                
            else:
                print(f"\nMensaje no reconocido: {message}")

    def handle_turn(self):
        """Maneja las acciones durante el turno del jugador"""
        while self.my_turn:
            print("\nOpciones:")
            print("1. Terminar turno")
            print("2. Rendirse")
            
            choice = input("Selecciona una opción: ")
            
            if choice == "1":
                self.send_message("END_TURN|")
                self.my_turn = False
            elif choice == "2":
                self.send_message("QUIT|")
                self.my_turn = False
                return  # Salir del juego
            else:
                print("Opción no válida")

    def run(self):
        """Ejecuta el flujo principal del cliente"""
        self.connect()
        self.login()
        self.handle_game_loop()
        self.sock.close()

if __name__ == "__main__":
    client = BattleshipClient()
    client.run()