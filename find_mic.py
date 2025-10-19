import pyaudio
pa = pyaudio.PyAudio()
print("Available Microphones:\n")
for i in range(pa.get_device_count()):
    info = pa.get_device_info_by_index(i)
    if info['maxInputChannels'] > 0:
        print(f"Index {info['index']}: {info['name']}")
pa.terminate()