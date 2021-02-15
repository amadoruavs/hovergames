from PIL import Image
import datetime
import numpy as np

colors = { 'GREY': (128, 128, 128),
           'PINK': (255, 0, 255),
           'PURPLE': (127, 0, 255),
           'DARK BLUE': (0, 0, 255),
           'LIGHT BLUE': (0, 128, 255),
           'BRIGHT BLUE': (0, 255, 255),
           'LIME': (0, 255, 0),
           'YELLOW': (255, 255, 0),
           'ORANGE': (255, 128, 0),
           'RED': (255, 0, 0) }
color_list = list(colors.keys())

class heatmap:
    def __init__(self, width, height):
        self.height = height
        self.width = width
        #2D array counting the number of visits for every pixel
        self.pixel_count_grid = np.zeros((self.height, self.width))

        self.lowest_color_value = 0
        #We'll use this to calculate the relative visits of each pixel
        self.highest_color_value = 0

    """
    Function to find cartesian lines
    """
    def find_line(self, point_1, point_2):
        x1, y1 = point_1[0], point_1[1] * -1
        x2, y2 = point_2[0], point_2[1] * -1

        if y1 == y2:
            return ('horizontal', y1)
        elif x1 == x2:
            return ('vertical', x1)

        slope = ( y2 - y1 ) / ( x2 - x1 )
        y_intercept = y1 - slope * x1

        return ( 'neither', slope, y_intercept )

    """
    Verify if the point is to the right of the leftmost line
    """
    def verify_right(self, x, y, line):
        if line[0] == 'vertical':
            if x >= line[1]:
                return True
            else:
                return False
        else:
            if line[1] < 0:
                if y >= ( x * line[1] + line[2] ):
                    return True
                else:
                    return False
            else:
                if y <= ( x * line[1] + line[2] ):
                    return True
                else:
                    return False

    """
    Verify if the point is to the right of the rightmost line
    """
    def verify_left(self, x, y, line):
        if line[0] == 'vertical':
            if x <= line[1]:
                return True
            else:
                return False
        else:
            if line[1] < 0:
                if y <= ( x * line[1] + line[2] ):
                    return True
                else:
                    return False
            else:
                if y >= ( x * line[1] + line[2] ):
                    return True
                else:
                    return False

    """
    Verify if the point is above the lowest line
    """
    def verify_above(self, x, y, line):
        if line[0] == 'horizontal':
            if y >= line[1]:
                return True
            else:
                return False
        else:
            if y >= ( x * line[1] + line[2] ):
                return True
            else:
                return False

    """
    Verify if the point is below the highest line
    """
    def verify_below(self, x, y, line):
        if line[0] == 'horizontal':
            if y <= line[1]:
                return True
            else:
                return False
        else:
            if y <= ( x * line[1] + line[2] ):
                return True
            else:
                return False

    def add_bounding_box(self, bounding_box):
        top_left_x, top_left_y = bounding_box[0]
        top_right_x, top_right_y = bounding_box[1]
        bottom_right_x, bottom_right_y = bounding_box[2]
        bottom_left_x, bottom_left_y = bounding_box[3]

        x_list = [top_left_x, top_right_x, bottom_right_x, bottom_left_x]
        y_list = [top_left_y, top_right_y, bottom_right_y, bottom_left_y]

        min_x, max_x = min(x_list), max(x_list)
        min_y, max_y = min(y_list), max(y_list)
        #Used for bounding-box iteration

        upper_line = self.find_line(bounding_box[0], bounding_box[1])
        lower_line = self.find_line(bounding_box[3], bounding_box[2])
        leftmost_line = self.find_line(bounding_box[0], bounding_box[3])
        rightmost_line = self.find_line(bounding_box[1], bounding_box[2])
        #Find the 4 lines of the convex polygon

        #Iterate through all bounding box points
        for y in range(min_y, max_y + 1):
            for x in range(min_x, max_x + 1):
                #We think of these lines as being in the 4th quadrant
                real_y = -1 * y

                #We then check if the point is within all 4 lines
                if not self.verify_below(x, real_y, upper_line):
                    continue
                if not self.verify_above(x, real_y, lower_line):
                    continue
                if not self.verify_right(x, real_y, leftmost_line):
                    continue
                if not self.verify_left(x, real_y, rightmost_line):
                    continue

                """
                If the point is within all 4 lines, we increment
                the number of times it's been visited
                """
                self.pixel_count_grid[y, x] += 1
                if self.pixel_count_grid[y, x] > self.highest_color_value:
                    self.highest_color_value = self.pixel_count_grid[y, x]

    def generate_heatmap(self, source_imagepath):
        """
        We construct a temporary image with all the colors
        We then overlap this colored image onto the base image
        """
        temporary = Image.new("RGBA", (self.width, self.height), (255, 255, 255))
        base_image = Image.open(source_imagepath)

        for y in range(self.height):
            for x in range(self.width):
                #We find the relative number of visits (think of it like a rank)
                value = self.pixel_count_grid[y, x] / self.highest_color_value
                #breakpoint()
                try:
                    hue = colors[self.find_color(value)]
                except KeyError as k:
                    breakpoint()
                temporary.putpixel((x, y), hue)
        tempmask = temporary.convert("L").point(lambda x: min(x, 50))

        """
        1. Making the colored image more transparent
        2. Pasting the colored image onto the base
        3. Saving the new heatmap w/a timestamp
        """
        temporary.putalpha(tempmask)
        base_image.paste(temporary, None, temporary)
        self.heatmap_filename = 'Heatmap at ' + str(datetime.datetime.now()) + '.png'
        base_image.save(self.heatmap_filename, 'PNG')

    def find_color(self, value):
        if 0 <= value < 1 / 10:
            return color_list[0]
        elif 1 / 10 <= value < 2 / 10:
            return color_list[1]
        elif 2 / 10 <= value < 3 / 10:
            return color_list[2]
        elif 3 / 10 <= value < 4 / 10:
            return color_list[3]
        elif 4 / 10 <= value < 5 / 10:
            return color_list[4]
        elif 5 / 10 <= value < 6 / 10:
            return color_list[5]
        elif 6 / 10 <= value < 7 / 10:
            return color_list[6]
        elif 7 / 10 <= value < 8 / 10:
            return color_list[7]
        elif 8 / 10 <= value < 9 / 10:
            return color_list[8]
        elif 9 / 10 <= value <= 10 / 10:
            return color_list[9]
