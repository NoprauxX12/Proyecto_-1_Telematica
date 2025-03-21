TAMAÑO_TABLERO = 10

BARCOS = {
    "Portaaviones": 5,
    "Buque de Guerra": 4,
    "Crucero": 3,
    "Destructor": 2,
    "Submarino": 1,
}

AGUA = "~"
TOCADO = "X"
HUNDIDO = "#"
BARCO = "O"

class Tablero:
    def __init__(self):
        self.tablero = [[AGUA for _ in range(TAMAÑO_TABLERO)] for _ in range(TAMAÑO_TABLERO)]
        self.barcos = []

    def colocarBarco(self, nombre, tamano, fila, columna, orientacion):
        if orientacion == "H":
            if columna + tamano > TAMAÑO_TABLERO:
                return False
            for i in range(tamano):
                if self.tablero[fila][columna + i] != AGUA:
                    return False
            for i in range(tamano):
                self.tablero[fila][columna + i] = BARCO
        elif orientacion == "V":
            if fila + tamano > TAMAÑO_TABLERO:
                return False
            for i in range(tamano):
                if self.tablero[fila + i][columna] != AGUA:
                    return False
            for i in range(tamano):
                self.tablero[fila + i][columna] = BARCO
        self.barcos.append({"nombre": nombre, "tamaño": tamano, "posiciones": [(fila, columna + i) if orientacion == "H" else (fila + i, columna) for i in range(tamano)]})
        return True
    
    def disparar(self, fila, columna):
        if self.tablero[fila][columna] == BARCO:
            self.tablero[fila][columna] = TOCADO
            for barco in self.barcos:
                if (fila, columna) in barco["posiciones"]:
                    barco["posiciones"].remove((fila, columna))
                    if not barco["posiciones"]:
                        print(f"¡Hundiste el {barco['nombre']}!")
                        self.barcos.remove(barco)
                        for pos in barco["posiciones"]:
                            self.tablero[pos[0]][pos[1]] = HUNDIDO
                    else:
                        print("¡Tocado!")
                    return True
        else:
            self.tablero[fila][columna] = AGUA
            print("¡Agua!")
            return False
    
    def mostrarTablero(self, ocultarBarcos=True):
        print(' 0 1 2 3 4 5 6 7 8 9')
        for i, fila in enumerate(self.tablero):
                print(f"{i}"+" ".join([AGUA if ocultarBarcos and casilla == BARCO else casilla for casilla in fila]))
                i = i + 1

def jugar():
    nombre_jugador1 = input("Selecciona tu nombre jugador1: ")
    nombre_jugador2 = input("Selecciona tu nombre jugador2: ")
    jugador1 = Tablero()
    jugador2 = Tablero()

    print(f"{nombre_jugador1}, coloca tus barcos:")
    for nombre, tamano in BARCOS.items():
        while True:
            jugador1.mostrarTablero(ocultarBarcos=False)
            try:
                fila = int(input(f"Fila para el {nombre} ({tamano} casillas): "))
                columna = int(input(f"Columna para el {nombre} ({tamano} casillas): "))
                orientacion = input("Orientación H para horizontal y V para vertical: ").upper()

                if jugador1.colocarBarco(nombre, tamano, fila, columna, orientacion):
                    break
                else:
                    print("Posición inválida, intenta de nuevo")
            except (ValueError, IndexError):
                print("Posición inválida, intenta de nuevo")

    print(f"{nombre_jugador2}, coloca tus barcos:")
    for nombre, tamano in BARCOS.items():
        while True:
            jugador2.mostrarTablero(ocultarBarcos=False)
            try:
                fila = int(input(f"Fila para el {nombre} ({tamano} casillas): "))
                columna = int(input(f"Columna para el {nombre} ({tamano} casillas): "))
                orientacion = input("Orientación H para horizontal y V para vertical: ").upper()

                if jugador2.colocarBarco(nombre, tamano, fila, columna, orientacion):
                    break
                else:
                    print("Posición inválida, intenta de nuevo")
            except (ValueError, IndexError):
                print("Posición inválida, intenta de nuevo")

    turno = 1
    while jugador1.barcos and jugador2.barcos:
        if turno == 1:
            print(f"{nombre_jugador1}, es tu turno:")
            jugador2.mostrarTablero()
            fila = int(input("Fila para disparar: "))
            columna = int(input("Columna para disparar: "))
            if jugador2.disparar(fila, columna):
                if not jugador2.barcos:
                    print(f"¡{nombre_jugador1} ganó!")
                    break
            turno = 2
        else:
            print(f"{nombre_jugador2}, es tu turno:")
            jugador1.mostrarTablero()
            fila = int(input("Fila para disparar: "))
            columna = int(input("Columna para disparar: "))
            if jugador1.disparar(fila, columna):
                if not jugador1.barcos:
                    print(f"¡{nombre_jugador2} ganó!")
                    break
            turno = 1