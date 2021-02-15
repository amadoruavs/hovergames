import subprocess, os, heatmap, time
from PIL import Image

start = time.time()

current_directory = os.getcwd() + '/'
os.system(f'ffmpeg -i {current_directory + "../testvideos/test2.mp4"} -r 4 data/images/output_%04d.png')

output = subprocess.check_output('python3 detect.py --save-txt', shell=True).decode().rstrip('\n').split('\n')
for line in output:
    if 'Path' in line:
        break

directory = current_directory + line[6:] + '/'
images = os.listdir(directory)
images.remove('labels')
images.sort()
label_dir = directory + 'labels/'
label_files = os.listdir(label_dir)

last_image = images[-1]
handle = Image.open(directory + last_image)
width, height = handle.size
handle.close()
map = heatmap.heatmap(width, height)

for image in images:
    textfile = image[:-3] + 'txt'
    h = open(label_dir + textfile)
    l = h.readlines()
    h.close()

    for pl in l:
        #breakpoint()
        pli = pl.rstrip('\n').split(' ')
        x1, y1, width, height = int(pli[0]) - 1, int(pli[1]) - 1, int(pli[2]), int(pli[3])
        top_left = (x1 - width // 2, y1 - height // 2)
        top_right = (x1 + width // 2, y1 - height // 2)
        bottom_right = (x1 + width // 2, y1 + height // 2)
        bottom_left = (x1 - width // 2, y1 + height // 2)
        box = [ top_left, top_right, bottom_right, bottom_left ]
        map.add_bounding_box(box)

map.generate_heatmap(directory + last_image)
print('File:', map.heatmap_filename)

end = time.time()
print(end - start, 'seconds.')
