# Hovergames

Necessary steps to reproduce mosaic and detection results (on prerecorded video):
1. Download [the test videos](https://drive.google.com/drive/folders/1FDwAICmS5KsaoBLwbqo3QC0LNczx2Y0i?usp=sharing) to `testvideos/`.
2. Run `server/detect.py --source <video> --conf 0.25 --save-txt` to detect on a prerecorded video.
3. Run `server/main.py` to generate mosaic.

