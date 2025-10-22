import socket
import player_position_pb2

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', 12345))

    print("Listening for player position updates...")

    while True:
        data, addr = sock.recvfrom(1024)
        pos = player_position_pb2.PlayerPosition()
        pos.ParseFromString(data)
        print(f"Player position: x={pos.x}, y={pos.y}, z={pos.z}")

if __name__ == '__main__':
    main()
