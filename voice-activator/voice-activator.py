#!/usr/bin/env python3

from pedalboard import load_plugin
from pedalboard.io import AudioStream

effect = load_plugin("/Library/Audio/Plug-Ins/VST3/Voxless.vst3")
effect.track_mute = 1
effect.coreml = 1

input_device_name = AudioStream.default_input_device_name
output_device_name = AudioStream.default_output_device_name
print(f"Recording from {input_device_name} and playing back on {output_device_name}")
# with AudioStream(input_device_name, output_device_name) as stream:
#   stream.plugins.append(effect)

stream = AudioStream(input_device_name, output_device_name)
stream.plugins.append(effect)
stream.run()

# channels = 0
# sample_rate = 48000.0
# with AudioFile("test.wav").resampled_to(sample_rate) as f:
#     audio = f.read(f.frames)
#     channels = f.channels

# processed = effect(audio, sample_rate)

# with AudioFile("test_processed.wav", "w", sample_rate, processed.shape[0]) as f:
#     f.write(processed)
# print(f"Wrote {len(processed)} samples to test_processed.wav")
