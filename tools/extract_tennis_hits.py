#!/usr/bin/env python3
"""Extract three clean one-shots from the CC0 jacklilley tennis recording.

This is an asset-preparation tool, not a runtime dependency. It expects
miniaudio and numpy and writes 48 kHz mono PCM16 WAV files.
"""

from pathlib import Path
import argparse
import wave

import miniaudio
import numpy as np


SAMPLE_TIMES_SECONDS = (4.054, 7.857, 11.740)
OUTPUT_RATE = 48_000


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    decoded = miniaudio.decode_file(
        str(args.source), output_format=miniaudio.SampleFormat.FLOAT32
    )
    stereo = np.asarray(decoded.samples, dtype=np.float32).reshape(
        -1, decoded.nchannels
    )
    mono = stereo.mean(axis=1)
    args.output.mkdir(parents=True, exist_ok=True)

    for index, center_seconds in enumerate(SAMPLE_TIMES_SECONDS, start=1):
        center = int(center_seconds * decoded.sample_rate)
        start = max(0, center - int(0.012 * decoded.sample_rate))
        end = min(len(mono), center + int(0.082 * decoded.sample_rate))
        clip = mono[start:end].astype(np.float64)

        # Remove recording DC/very-low rumble without erasing the racket body.
        filtered = np.zeros_like(clip)
        for sample_index in range(1, len(clip)):
            filtered[sample_index] = (
                clip[sample_index]
                - clip[sample_index - 1]
                + 0.992 * filtered[sample_index - 1]
            )

        source_positions = np.arange(len(filtered), dtype=np.float64)
        output_count = round(len(filtered) * OUTPUT_RATE / decoded.sample_rate)
        output_positions = np.linspace(0.0, len(filtered) - 1.0, output_count)
        clip = np.interp(output_positions, source_positions, filtered)

        fade_in = min(len(clip), int(0.001 * OUTPUT_RATE))
        fade_out = min(len(clip), int(0.010 * OUTPUT_RATE))
        clip[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
        clip[-fade_out:] *= np.linspace(1.0, 0.0, fade_out)
        peak = max(1.0e-9, float(np.max(np.abs(clip))))
        pcm = np.round(np.clip(clip * (0.88 / peak), -1.0, 1.0) * 32767.0)
        pcm = pcm.astype("<i2")

        path = args.output / f"tennis_hit_{index}.wav"
        with wave.open(str(path), "wb") as output:
            output.setnchannels(1)
            output.setsampwidth(2)
            output.setframerate(OUTPUT_RATE)
            output.writeframes(pcm.tobytes())


if __name__ == "__main__":
    main()
