import socket
import pygame
from pygame.locals import *

SERVER_IP = "192.168.1.10"
SERVER_PORT = 7
BUFSIZE = 4096

FRAME_WIDTH = 640
FRAME_HEIGHT = 480
FRAME_SIZE = FRAME_WIDTH * FRAME_HEIGHT * 4

client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client_socket.connect((SERVER_IP, SERVER_PORT))

pygame.init()
screen = pygame.display.set_mode((FRAME_WIDTH, FRAME_HEIGHT))
pygame.display.set_caption('Video')

running = True
while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
    
    frame_data = bytearray()
    while len(frame_data) < FRAME_SIZE:
        frame_data += client_socket.recv(BUFSIZE)

    image_surface = pygame.image.frombuffer(frame_data,
                                            (FRAME_WIDTH, FRAME_HEIGHT),
                                            'RGBA')
    screen.blit(image_surface, (0, 0))
    pygame.display.flip()

pygame.quit()
