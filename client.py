import socket
import sys
 
HOST = "localhost"
PORT = 8080
 
class BattleshipClient:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.name = ""
        self.my_turn = False
        self.my_board = [['~'] * 10 for _ in range(10)]  # barcos propios
        self.enemy_board = [['~'] * 10 for _ in range(10)]  # disparos al enemigo
        self.last_shot = (-1, -1)
 
    def connect(self):
        try:
            self.sock.connect((HOST, PORT))
            print(f"Conectado al servidor {HOST}:{PORT}")
        except Exception as e:
            print(f"Error al conectar: {e}")
            sys.exit(1)
 
    def login(self):
        self.name = input("Ingresa tu nombre: ")
        self.send_message(f"LOGIN|{self.name}")
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
            print(f"Mensaje recibido: {data}")
            return data
        except Exception as e:
            print(f"Error recibiendo mensaje: {e}")
            sys.exit(1)
 
    def print_board(self):
        print("\nTu tablero:")
        print("  " + " ".join(map(str, range(10))))
        for i, row in enumerate(self.my_board):
            print(f"{i} " + " ".join(row))
 
        print("\nTablero enemigo:")
        print("  " + " ".join(map(str, range(10))))
        for i, row in enumerate(self.enemy_board):
            print(f"{i} " + " ".join(row))
 
    def handle_game_loop(self):
        while True:
            message = self.receive_message()
            msg_type, _, content = message.partition('|')
 
            if msg_type == "PLACE_SHIP":
                ship_name, size = content.split('|')[1:]
                print(f"Ubica tu barco: {ship_name} (tamaño {size})")
                while True:
                    try:
                        row = int(input("Fila inicial: "))
                        col = int(input("Columna inicial: "))
                        orientation = input("Orientación (H/V): ").upper()
                        if orientation not in ['H', 'V']:
                            raise ValueError("La orientación debe ser 'H' o 'V'.")
 
                        for i in range(int(size)):
                            r = row + i if orientation == 'V' else row
                            c = col + i if orientation == 'H' else col
                            self.my_board[r][c] = 'O'
 
                        self.print_board()
                        self.send_message(f"SHIP_POS|{row},{col},{orientation}")
                        break
                    except Exception as e:
                        print(f"Entrada inválida: {e}. Intenta de nuevo.")
 
            elif msg_type == "YOUR_TURN":
                self.my_turn = True
                print("\n--- ES TU TURNO ---")
                print(content)
                self.handle_turn()
 
            elif msg_type == "WAIT_TURN":
                self.my_turn = False
                print(f"\nEsperando turno... {content}")
 
            elif msg_type == "GAME_OVER":
                print(f"\n¡Juego terminado! {content}")
                break
 
            elif msg_type == "RESULT":
                print(f"\nResultado del disparo: {content}")
                row, col = self.last_shot
                if content in ["HIT", "SUNK"] and 0 <= row < 10 and 0 <= col < 10:
                    self.enemy_board[row][col] = 'X'
                elif content == "MISS" and 0 <= row < 10 and 0 <= col < 10:
                    self.enemy_board[row][col] = '*'
                self.print_board()
 
            elif msg_type == "ERROR":
                print(f"\nError: {content}")
 
            else:
                print(f"\nMensaje no reconocido: {message}")
 
    def handle_turn(self):
        while self.my_turn:
            coords = input("Ingresa fila,columna para disparar o 'R' para rendirte: ")
            if coords.strip().upper() == "R":
                self.send_message("QUIT|")
                self.my_turn = False
                break
            else:
                try:
                    row, col = map(int, coords.strip().split(","))
                    self.last_shot = (row, col)
                except ValueError:
                    print("Formato inválido. Usa fila,columna. Ej: 3,4")
                    continue
 
                self.send_message(coords.strip())
                self.my_turn = False
 
    def run(self):
        self.connect()
        self.login()
        self.handle_game_loop()
        self.sock.close()
 
if __name__ == "__main__":
    client = BattleshipClient()
    client.run()