import time

import music76489


def play(song: str) -> None:
    m = music76489.Music76489()
    m.load_song(song)
    while True:
        m.play()
        time.sleep(1 / 60)


play("data/anime.vgm")
