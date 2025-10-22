import socket
import subprocess
import argparse
import player_position_pb2

def main():
    parser = argparse.ArgumentParser(description='Launch dhewm3 and listen for player position updates.')
    parser.add_argument('executable_path', help='Path to the dhewm3 executable')
    parser.add_argument('--port', type=int, default=12345, help='UDP port to listen on')
    args = parser.parse_args()

    # Launch dhewm3
    command = [
        args.executable_path,
        '--udp-port', str(args.port),
        '--udp-dest', f'127.0.0.1:{args.port}'
    ]
    print(f"Launching: {' '.join(command)}")
    game_process = subprocess.Popen(command)

    # Start UDP server
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', args.port))

    print(f"Listening for player position updates on port {args.port}...")

    try:
        while True:
            data, addr = sock.recvfrom(1024)
            pos = player_position_pb2.PlayerPosition()
            pos.ParseFromString(data)
            print(f"Player position: x={pos.x}, y={pos.y}, z={pos.z}")
    except KeyboardInterrupt:
        print("Stopping server...")
    finally:
        game_process.terminate()
        sock.close()

if __name__ == '__main__':
    main()
