#!/usr/bin/env python3
"""Prepare natural match, bounce and net one-shots from CC0 previews."""

from pathlib import Path
import argparse
import wave

import miniaudio
import numpy as np


RATE = 48_000


def decode(path: Path) -> np.ndarray:
    sound = miniaudio.decode_file(
        str(path), output_format=miniaudio.SampleFormat.FLOAT32,
        nchannels=1, sample_rate=RATE
    )
    return np.asarray(sound.samples, dtype=np.float64)


def prepare(source: np.ndarray, center: float, pre: float, post: float,
            peak_level: float = 0.88, tone: float = 1.0) -> np.ndarray:
    center_sample = round(center * RATE)
    start = max(0, center_sample - round(pre * RATE))
    end = min(len(source), center_sample + round(post * RATE))
    clip = source[start:end].copy()

    # Remove DC/handling rumble while keeping the 100-200 Hz racket body.
    high_passed = np.zeros_like(clip)
    for index in range(1, len(clip)):
        high_passed[index] = (
            clip[index] - clip[index - 1] + 0.994 * high_passed[index - 1]
        )
    clip = high_passed

    # tone < 1 produces a duller clay-like derivative without inventing a
    # resonant pitch. This is a one-pole low-pass applied to the real impact.
    if tone < 0.999:
        filtered = np.zeros_like(clip)
        for index in range(1, len(clip)):
            filtered[index] = filtered[index - 1] + tone * (
                clip[index] - filtered[index - 1]
            )
        clip = filtered

    fade_in = min(len(clip), round(0.001 * RATE))
    fade_out = min(len(clip), round(0.012 * RATE))
    clip[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
    clip[-fade_out:] *= np.linspace(1.0, 0.0, fade_out)
    peak = max(1.0e-9, float(np.max(np.abs(clip))))
    return clip * (peak_level / peak)


def save(path: Path, clip: np.ndarray) -> None:
    pcm = np.round(np.clip(clip, -1.0, 1.0) * 32767.0).astype("<i2")
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(pcm.tobytes())


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--forehand", type=Path, required=True)
    parser.add_argument("--serve", type=Path, required=True)
    parser.add_argument("--grass", type=Path, required=True)
    parser.add_argument("--net", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    forehand = decode(args.forehand)
    serve = decode(args.serve)
    grass = decode(args.grass)
    net = decode(args.net)

    for index, center in enumerate((0.133, 1.343, 2.788), start=1):
        save(args.output / f"match_forehand_{index}.wav",
             prepare(forehand, center, 0.012, 0.105))
    for index, center in enumerate((0.110, 5.749, 10.924), start=1):
        save(args.output / f"match_serve_{index}.wav",
             prepare(serve, center, 0.012, 0.120))
    for index, center in enumerate((0.286, 13.537, 31.277), start=1):
        save(args.output / f"bounce_grass_{index}.wav",
             prepare(grass, center, 0.010, 0.090, peak_level=0.78))

    # The net recording contains the initial racket strike, the actual net
    # impact, then several hard-court bounces. Keep those events separate.
    save(args.output / "net_impact.wav",
         prepare(net, 0.662, 0.014, 0.150, peak_level=0.82))
    # These two bounces have the cleanest single transient in the recording.
    # Keep the one-shots short so court echo cannot sound like a second bounce;
    # the third variation intentionally reuses a clean event and is varied at
    # runtime by playback rate and impact gain.
    for index, center in enumerate((1.581, 3.557, 1.581), start=1):
        hard = prepare(net, center, 0.008, 0.044, peak_level=0.72)
        save(args.output / f"bounce_hard_{index}.wav", hard)
        clay = prepare(net, center, 0.008, 0.050,
                       peak_level=0.68, tone=0.43)
        save(args.output / f"bounce_clay_{index}.wav", clay)


if __name__ == "__main__":
    main()
