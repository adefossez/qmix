# -*- coding: utf-8 -*-

import math
from PIL import Image, ImageDraw
import cairocffi as cairo

SIZE = 1024

BLACK = (0, 0, 0)
WHITE = (1, 1, 1)

class Context:
    def __init__(self, size, width):
        self.center = [size / 2, size / 2]
        self.size = size
        self.width = width
        self.surface = cairo.ImageSurface(cairo.FORMAT_RGB24, size, size)
        self.context = cairo.Context(self.surface)
        self.context.set_source_rgb(*WHITE)
        self.context.paint()

        self.last_rec = True
        self.last_black = False

    def triangle(self):
        if self.last_rec:
            side = self.size - 2 * self.width * 0.7
            height = math.sqrt(3) * side / 2
            delta = (self.size - height) / 2
            center = [self.center[0], self.center[1] - self.size / 2 + delta + height / 3]
        else:
            center = self.center
            side = self.size - 2 * self.width
            height = math.sqrt(3) * side / 2

        up = (center[0], center[1] + 2 * height / 3)
        down_left = (center[0] - side / 2  , center[1] - height / 3)
        down_right = (center[0] + side / 2  , center[1] - height / 3)

        self.last_rec = False
        if self.last_black:
            self.context.set_source_rgb(*WHITE)
        else:
            self.context.set_source_rgb(*BLACK)
        self.last_black = not self.last_black

        self.context.move_to(*up)
        self.context.line_to(*down_right)
        self.context.line_to(*down_left)
        self.context.close_path()
        self.context.fill()

        self.center = center
        self.size = height

    def rectangle(self):
        if self.last_rec:
            side = self.size - 2 * self.width
            center = self.center
        else:
            diag = 0.8 * (self.size - 2 * self.width)
            side = diag / math.sqrt(2)
            center = self.center
            center[1] += 0.1 * side
            self.context.translate(*center)
            self.context.rotate(math.pi / 4)
            self.context.translate(-center[0], -center[1])
        points = []
        for (i, j) in [(1, 1), (1, -1), (-1, -1), (-1, 1)]:
            points.append((center[0] + i * side / 2, center[1] + j * side / 2))

        self.last_rec = True
        if self.last_black:
            self.context.set_source_rgb(*WHITE)
        else:
            self.context.set_source_rgb(*BLACK)
        self.last_black = not self.last_black

        self.context.move_to(*points[0])
        for i in points[1:]:
            self.context.line_to(*i)
        self.context.close_path()
        self.context.fill()

        self.center = center
        self.size = side

    def save(self, path):
        self.surface.write_to_png(path)


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
    r = size / math.sqrt(2)
    for (i, j) in [(1, 1), (1, -1), (-1, -1), (-1, 1)]:
        points.append((center[0] + i * size / 2, center[1] + j * size / 2))
    
    points = []
    for (i, j) in [(1, 0), (0, -1), (-1, 0), (0, 1)]:
        points.append((center[0] + i * r, center[1] + j * r))
    draw.polygon(points, fill=fill)

def draw_rectangle(draw, center, size, width):
    draw_rectangle_fill(draw, center, size, 0)
    draw_rectangle_fill(draw, center, size - width, 255)

def generate_0(size=SIZE):
    image = Image.new('L', (size, size), 255)
    draw = ImageDraw.Draw(image)
    border = 20
    initial_side = size - 2 * border
    initial_size = math.sqrt(3) * initial_side / 2

    h_border = (size - initial_size) / 2
    width = 150

    center = (size / 2, h_border + initial_size / 3)
    draw_triangle_fill(draw, center, initial_size, 0)
    rectangle_space = initial_size * 0.4
    draw_rectangle_fill(draw, (center[0], center[1] - 0.2 * width), rectangle_space, 255)
    space = rectangle_space * 0.6
    draw_rectangle_fill(draw, (center[0], center[1] - 0.2 * width), space, 0)
    image.save('img/0.png')

def generate_1(size=SIZE):
    image = Image.new('L', (size, size), 255)
    draw = ImageDraw.Draw(image)
    border = 20
    initial_side = size - 2 * border
    initial_size = math.sqrt(3) * initial_side / 2

    h_border = (size - initial_size) / 2
    width = 100

    center = (size / 2, h_border + initial_size / 3)
    draw_triangle_fill(draw, center, initial_size, 0)

    delta = width 
    rectangle_space = initial_size / (1 + math.sqrt(3) / 2) - width
    draw_rectangle_fill(draw, (center[0], center[1] - initial_size/3 + width + rectangle_space / 2), rectangle_space, 255)
    #space = rectangle_space * 0.6
    #draw_triangle_fill(draw, (center[0], center[1] - 0.3 * width), space, 0)
    image.save('img/1.png')

def generate_2():
    ctx = Context(1024, 100)
    ctx.rectangle()
    ctx.triangle()
    ctx.rectangle()
    ctx.save('img/2.png')


#generate_0()
#generate_1()

mapping = {
    0: 'rectangle',
    1: 'triangle'
}

for i in range(7):
    ctx = Context(512, 50)
    n = i
    for j in range(3):
        action = n % 2
        n = n // 2
        getattr(ctx, mapping[action])()
    ctx.save('img/{}.png'.format(i))
