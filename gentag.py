# -*- coding: utf-8 -*-

import math
from PIL import Image, ImageDraw

SIZE = 1024

def draw_triangle_fill(draw, center, size, fill):
    up = (center[0], center[1] + 2 * size / 3)
    side = 2 * size / math.sqrt(3)
    down_left = (center[0] - side / 2  , center[1] - size / 3)
    down_right = (center[0] + side / 2  , center[1] - size / 3)
    draw.polygon([up, down_right, down_left], fill=fill)


def draw_triangle(draw, center, size, width):
    draw_triangle_fill(draw, center, size, 0)
    draw_triangle_fill(draw, center, size - width, 255)

def draw_rectangle_fill(draw, center, size, fill):
    points = []
    for (i, j) in [(1, 1), (1, -1), (-1, -1), (-1, 1)]:
        points.append((center[0] + i * size / 2, center[1] + j * size / 2))
    draw.polygon(points, fill=fill)

def draw_rectangle(draw, center, size, width):
    draw_rectangle_fill(draw, center, size, 0)
    draw_rectangle_fill(draw, center, size - width, 255)

def generate(seq, size=SIZE):
    image = Image.new('L', (size, size), 255)
    draw = ImageDraw.Draw(image)
    border = 20
    initial_side = size - 2 * border
    initial_size = math.sqrt(3) * initial_side / 2

    h_border = (size - initial_size) / 2
    width = 150

    center = (size / 2, h_border + initial_size / 3)
    draw_triangle(draw, center, initial_size, width)
    rectangle_space = 2 * (initial_size - 2 * width) / 3
    draw_rectangle(draw, (center[0], center[1] - 0.2 * width), rectangle_space, width / 2)
    image.save('tmp.png')

generate([])

