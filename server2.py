import socket
import sys

# Configuración del cliente
HOST = 'localhost'  # Cambia por la IP del servidor si es necesario
PORT = 8080
BOARD_SIZE = 10

# Símbolos del tablero
WATER = '~'
HIT = 'X'
SUNK = '#'
SHIP = 'O'

class BattleshipClient:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.name = ""
        self.my_board = [[WATER for _ in range(BOARD_SIZE)] for _ in range(BOARD_SIZE)]
        self.opponent_board = [[WATER for _ in range(BOARD_SIZE)] for _ in range(BOARD_SIZE)]
        self.ships = [
            {"name": "Portaaviones", "size": 5},
            {"name": "Buque", "size": 4},
            {"name": "Crucero", "size": 3},
            {"name": "Destructor", "size": 2},
            {"name": "Submarino", "size": 1}
        ]

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

    def place_ships(self):
        print("\n=== COLOCA TUS BARCOS ===")
        for ship in self.ships:
            while True:
                self.print_board(self.my_board, show_ships=True)
                print(f"\nColoca tu {ship['name']} (tamaño: {ship['size']})")
                try:
                    row = int(input("Fila (0-9): "))
                    col = int(input("Columna (0-9): "))
                    orientation = input("Orientación (H/V): ").upper()
                    
                    if orientation not in ['H', 'V']:
                        print("Orientación debe ser H o V")
                        continue
                    
                    # Verificar posición válida
                    if self.is_valid_ship_position(row, col, ship['size'], orientation):
                        # Colocar barco
                        self.place_ship_on_board(row, col, ship['size'], orientation)
                        # Enviar al servidor
                        self.send_message(f"PLACE_SHIPS|{ship['name']},{row},{col},{orientation},{ship['size']}")
                        response = self.receive_message()
                        if response == "RESULT|SHIP_PLACED":
                            break
                        else:
                            print("Error del servidor al colocar barco")
                    else:
                        print("Posición inválida, intenta de nuevo")
                except ValueError:
                    print("Entrada inválida, usa números")

    def is_valid_ship_position(self, row, col, size, orientation):
        if orientation == 'H':
            if col + size > BOARD_SIZE:
                return False
            for i in range(size):
                if self.my_board[row][col + i] != WATER:
                    return False
        else:  # 'V'
            if row + size > BOARD_SIZE:
                return False
            for i in range(size):
                if self.my_board[row + i][col] != WATER:
                    return False
        return True

    def place_ship_on_board(self, row, col, size, orientation):
        if orientation == 'H':
            for i in range(size):
                self.my_board[row][col + i] = SHIP
        else:  # 'V'
            for i in range(size):
                self.my_board[row + i][col] = SHIP

    def print_board(self, board, show_ships=False):
        print("\n  0 1 2 3 4 5 6 7 8 9")
        for i, row in enumerate(board):
            display_row = []
            for cell in row:
                if cell == SHIP and not show_ships:
                    display_row.append(WATER)
                else:
                    display_row.append(cell)
            print(f"{i} {' '.join(display_row)}")

    def play_game(self):
        print("\n=== JUEGO INICIADO ===")
        while True:
            message = self.receive_message()
            if not message:
                break

            print(f"\nMensaje recibido: {message}")
            parts = message.split('|')
            command = parts[0]
            data = parts[1] if len(parts) > 1 else ""

            if command == "TURN":
                self.handle_turn()
            elif command == "RESULT":
                self.handle_result(data)
            elif command == "VICTORY":
                print("\n¡GANASTE LA PARTIDA!")
                break
            elif command == "DEFEAT":
                print("\n¡Perdiste la partida!")
                break
            elif command == "ERROR":
                print(f"Error: {data}")
            else:
                print(f"Comando no reconocido: {message}")

    def handle_turn(self):
        print("\nEs tu turno de atacar!")
        self.print_board(self.opponent_board)
        while True:
            try:
                row = int(input("Fila para disparar (0-9): "))
                col = int(input("Columna para disparar (0-9): "))
                if 0 <= row < BOARD_SIZE and 0 <= col < BOARD_SIZE:
                    self.send_message(f"FIRE|{row},{col}")
                    break
                else:
                    print("Coordenadas fuera de rango")
            except ValueError:
                print("Entrada inválida, usa números")

    def handle_result(self, data):
        if data.startswith("SHIP_PLACED"):
            return  # Respuesta durante colocación de barcos

        # Procesar resultado de disparo: "row,col,result"
        parts = data.split(',')
        row = int(parts[0])
        col = int(parts[1])
        result = parts[2]

        if result == 'A':
            print("¡Agua!")
            self.opponent_board[row][col] = 'A'
        elif result == 'T':
            print("¡Tocado!")
            self.opponent_board[row][col] = HIT
        elif result == 'S':
            print("¡Hundido!")
            self.opponent_board[row][col] = SUNK

        self.print_board(self.opponent_board)

    def run(self):
        self.connect()
        self.login()
        self.place_ships()
        
        # Esperar confirmación READY del servidor
        response = self.receive_message()
        if response != "READY|":
            print("Error al iniciar juego")
            return
        
        self.play_game()
        self.sock.close()
        print("Juego terminado")

if __name__ == "__main__":
    client = BattleshipClient()
    client.run()