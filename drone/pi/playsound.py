from pydub import AudioSegment
from pydub.playback import play

song = AudioSegment.from_wav("audio.mp3")
play(song)
