# Proyecto Battleship - Implementación de Juego de Batalla Naval

## Introducción
Este proyecto implementa un juego de Batalla Naval (Battleship) en red, donde dos jugadores pueden competir entre sí. El sistema está compuesto por un servidor en C y un cliente en Python, permitiendo la comunicación entre jugadores a través de sockets TCP/IP. El juego incluye características como colocación de barcos, turnos alternados, registro de disparos y un sistema de logging para seguimiento del juego.

## Desarrollo

### Arquitectura del Sistema
El sistema está dividido en dos componentes principales:

1. **Servidor (C)**
   - Implementado en C utilizando sockets TCP/IP
   - Gestiona múltiples sesiones de juego
   - Maneja la lógica del juego y la comunicación entre jugadores
   - Implementa un sistema de logging para seguimiento

2. **Cliente (Python)**
   - Implementado en Python
   - Proporciona interfaz de usuario para los jugadores
   - Maneja la comunicación con el servidor
   - Implementa visualización del tablero y gestión de turnos

### Componentes Principales

#### Servidor
- `server.c`: Punto de entrada del servidor, maneja conexiones y creación de sesiones
- `session.c`: Gestiona las sesiones de juego y la lógica del juego
- `board.c`: Implementa la lógica del tablero y los barcos
- `logger.c`: Sistema de logging para seguimiento del juego
- `messaging.c`: Manejo de mensajes entre cliente y servidor

#### Cliente
- `client.py`: Implementa la interfaz de usuario y la comunicación con el servidor
- Sistema de logging local para cada jugador

### Protocolo de Comunicación
El sistema utiliza un protocolo de mensajes basado en texto para la comunicación entre cliente y servidor. Los mensajes siguen el formato `TIPO|CONTENIDO`:

- **Mensajes del Cliente al Servidor:**
  - `LOGIN|nombre_jugador|correo_jugador`: Inicia sesión en el servidor
  - `PLACE_SHIPS|ubicacion_barco`: Coloca un barco en el tablero
  - `READY|`: Indica que el cliente está listo para jugar
  - `FIRE|fila,columna`: Realiza un disparo en las coordenadas especificadas
  - `REPLAY|`: Solicita una nueva partida
  - `QUIT|`: Abandona la partida
  - `DISCONNECT|`: Notifica la desconexión del cliente

- **Mensajes del Servidor al Cliente:**
  - `LOGIN|OK`: Confirmación de login exitoso
  - `PLACE_SHIP|nombre_barco|tamaño`: Solicitud para colocar un barco
  - `TURN|`: Indica que es el turno del jugador
  - `WAIT_TURN|mensaje`: Indica que debe esperar su turno
  - `RESULT|HIT/MISS/SUNK`: Resultado de un disparo
  - `ENEMY_HIT|fila,columna`: Notifica un impacto enemigo
  - `VICTORY|`: Indica que el jugador ha ganado
  - `DEFEAT|`: Indica que el jugador ha perdido
  - `GAME_OVER|mensaje`: Indica el fin del juego
  - `TIMEOUT|`: Notifica que el jugador no realizó su movimiento a tiempo
  - `ERROR|mensaje`: Notifica un error
  - `DISCONNECT|`: Notifica la desconexión de un jugador

### Características Implementadas
- Sistema de login para jugadores
- Colocación de barcos con diferentes tamaños
- Turnos alternados entre jugadores
- Sistema de disparos y seguimiento de impactos
- Visualización de tableros (propio y enemigo)
- Sistema de logging para seguimiento del juego
- Manejo de desconexiones y errores
- Soporte para múltiples sesiones de juego simultáneas

## Forma de Uso

### Requisitos Previos
- Sistema operativo Linux o macOS
- Compilador GCC instalado
- Python 3.x instalado
- Acceso a la terminal/consola

### Instalación y Ejecución

#### 1. Compilación del Servidor
1. Abre una terminal
2. Navega hasta el directorio del proyecto
3. Compila el servidor con el siguiente comando:
   ```bash
   gcc -o server server.c session.c board.c logger.c messaging.c -lpthread
   ```

#### 2. Ejecución del Servidor
1. En la terminal, ejecuta el servidor con:
   ```bash
   ./server <ip> <puerto> <ruta_log>
   ```
   Por ejemplo:
   ```bash
   ./server 127.0.0.1 8080 game_log.txt
   ```
   - `ip`: Dirección IP donde escuchará el servidor (127.0.0.1 para local)
   - `puerto`: Puerto de escucha (8080 por defecto)
   - `ruta_log`: Ruta donde se guardará el archivo de log

#### 3. Ejecución del Cliente
1. Abre una nueva terminal
2. Navega hasta el directorio del proyecto
3. Ejecuta el cliente con:
   ```bash
   python3 client.py <ruta_log_cliente>
   ```
   Por ejemplo:
   ```bash
   python3 client.py client_log.txt
   ```
   - `ruta_log_cliente`: Ruta donde se guardará el log del cliente

#### 4. Jugar una Partida en el mismo ordenador
1. Ejecuta el servidor como se indicó anteriormente
2. Ejecuta dos instancias del cliente (una para cada jugador) en terminales diferentes
3. En cada cliente:
   - Ingresa tu nombre cuando se te solicite
   - Coloca tus barcos siguiendo las instrucciones:
     - Ingresa la fila (0-9)
     - Ingresa la columna (0-9)
     - Selecciona la orientación (H para horizontal, V para vertical)
   - Cuando sea tu turno:
     - Ingresa las coordenadas para disparar en formato "fila,columna"
     - Por ejemplo: "3,4" disparará en la fila 3, columna 4
   - El juego continuará alternando turnos hasta que un jugador gane

### Jugar en dos ordenadores

Para jugar en dos computadores, sigue estos pasos:

1. **En la computadora que actuará como servidor:**
   - Compila y ejecuta el servidor como se indicó anteriormente
   - Usa la IP de esta computadora en lugar de 127.0.0.1
   - Para encontrar la IP de tu computadora:
     - En Linux/macOS: ejecuta `ifconfig` o `ip addr` en la terminal
     - En Windows: ejecuta `ipconfig` en la terminal
   - Ejemplo de ejecución del servidor:
     ```bash
     ./server 192.168.1.100 8080 game_log.txt
     ```
     (Reemplaza 192.168.1.100 con la IP real de tu computadora)

2. **En la computadora que actuará como cliente:**
   - Asegúrate de que el código del cliente (`client.py`) esté disponible
   - Ejecuta el cliente especificando la IP del servidor:
     ```bash
     python3 client.py client_log.txt
     ```
   - Cuando se te solicite la IP del servidor, ingresa la IP de la computadora donde está corriendo el servidor

3. **Consideraciones importantes:**
   - Ambas computadoras deben estar en la misma red local
   - El firewall de la computadora servidor debe permitir conexiones en el puerto especificado (8080 por defecto)
   - Si hay problemas de conexión:
     - Verifica que el servidor esté corriendo
     - Confirma que estás usando la IP correcta del servidor
     - Intenta desactivar temporalmente el firewall para pruebas
   - Si la conexión es lenta:
     - Verifica la calidad de la conexión de red
     - Asegúrate de que no haya otras aplicaciones consumiendo ancho de banda

4. **Solución de problemas comunes:**
   - Si no puedes conectarte:
     - Verifica que el servidor esté corriendo
     - Confirma que estás usando la IP correcta del servidor
     - Intenta desactivar temporalmente el firewall para pruebas
   - Si la conexión es lenta:
     - Verifica la calidad de la conexión de red
     - Asegúrate de que no haya otras aplicaciones consumiendo ancho de banda

#### Notas Importantes
- Asegúrate de que el servidor esté ejecutándose antes de iniciar los clientes
- Los clientes deben conectarse a la misma IP y puerto donde está corriendo el servidor
- Para jugar en la misma computadora, usa la IP 127.0.0.1
- Para jugar en red local, usa la IP de la computadora donde corre el servidor
- Puedes rendirte en cualquier momento escribiendo "R" durante tu turno

## Aspectos Logrados y No Logrados

### Logrados
- ✅ Implementación completa del servidor en C
- ✅ Cliente funcional en Python
- ✅ Sistema de comunicación cliente-servidor
- ✅ Gestión de múltiples sesiones
- ✅ Sistema de logging
- ✅ Lógica completa del juego
- ✅ Interfaz de usuario básica
- ✅ Manejo de errores y desconexiones

### No Logrados
- ❌ Interfaz gráfica de usuario (GUI)
- ❌ Sistema de puntuación y ranking
- ❌ Modo de juego contra IA
- ❌ Sistema de chat entre jugadores
- ❌ Persistencia de datos entre sesiones

## Conclusiones
El proyecto implementa exitosamente un juego de Batalla Naval en red con las características básicas necesarias para una experiencia de juego completa. La arquitectura cliente-servidor permite una comunicación eficiente entre jugadores, mientras que el sistema de logging facilita el seguimiento y depuración del juego.

La implementación en C del servidor y Python del cliente demuestra una buena separación de responsabilidades y aprovecha las fortalezas de cada lenguaje. El sistema de sesiones permite múltiples partidas simultáneas, y el manejo de errores asegura una experiencia robusta para los usuarios.

## Referencias
- [Documentación de Sockets en C](https://man7.org/linux/man-pages/man2/socket.2.html)
- [Documentación de Python Sockets](https://docs.python.org/3/library/socket.html)
- [Reglas del Juego Battleship](https://en.wikipedia.org/wiki/Battleship_(game))
- [Pthreads Programming](https://computing.llnl.gov/tutorials/pthreads/)
- [PDF-Proyecto-N1-BattleShip.pdf](PDF-Proyecto-N1-BattleShip.pdf) - Documento de especificaciones del proyecto 