import struct
import wave
import pyaudio
import pvporcupine
import whisper
from collections import deque

# --- Configuration ---
ACCESS_KEY = '6bk7NbxSdALUhuB/Yg+47itNE9/gtAefNMzbF6XfT1n7VezqJwTjoA=='  # Get your key from Picovoice Console
KEYWORD_PATHS = [pvporcupine.KEYWORD_PATHS['porcupine']]  # Can change to Pepsi
MODEL_PATH = None
SENSITIVITY = 0.5
INPUT_DEVICE_INDEX = None # Use None for default microphone

# Recording configuration
PRE_RECORD_SECONDS = 1.5   # Buffer audio BEFORE the wake word
POST_RECORD_SECONDS = 3.5  # Record audio AFTER the wake word
WAVE_OUTPUT_FILENAME = "command.wav"
CHANNELS = 1
FORMAT = pyaudio.paInt16

# Whisper model
WHISPER_MODEL = "tiny.en" # Recommended for RPi: tiny.en, base.en

def main():
    porcupine = None
    pa = None
    audio_stream = None

    try:
        # --- Initialize Porcupine ---
        porcupine = pvporcupine.create(
            access_key=ACCESS_KEY,
            keyword_paths=KEYWORD_PATHS,
            sensitivities=[SENSITIVITY] * len(KEYWORD_PATHS)
        )
        
        # --- Initialize Whisper ---
        print("Loading Whisper model...")
        model = whisper.load_model(WHISPER_MODEL)
        print(f"Whisper model '{WHISPER_MODEL}' loaded.")

        # --- Initialize Audio Buffer ---
        num_pre_record_frames = int(PRE_RECORD_SECONDS * porcupine.sample_rate / porcupine.frame_length)
        pre_roll_buffer = deque(maxlen=num_pre_record_frames)
        
        # --- Initialize PyAudio ---
        pa = pyaudio.PyAudio()
        audio_stream = pa.open(
            rate=porcupine.sample_rate,
            channels=CHANNELS,
            format=FORMAT,
            input=True,
            frames_per_buffer=porcupine.frame_length,
            input_device_index=INPUT_DEVICE_INDEX
        )

        print("="*60)
        print(f"Listening for 'Porcupine'... (Press Ctrl+C to exit)")
        print(f"Audio clip will be {PRE_RECORD_SECONDS + POST_RECORD_SECONDS} seconds total.")
        print("="*60)

        # --- Main Loop ---
        while True:
            pcm_packed = audio_stream.read(porcupine.frame_length)
            pcm_unpacked = struct.unpack_from("h" * porcupine.frame_length, pcm_packed)
            
            # Add the latest audio frame to our rolling buffer
            pre_roll_buffer.append(pcm_unpacked)

            keyword_index = porcupine.process(pcm_unpacked)

            if keyword_index >= 0:
                print("Wake word detected! Recording command...")
                
                # --- Start Post-Wake-Word Recording ---
                frames = []
                # Add the entire pre-roll buffer to the start of the recording
                for frame in pre_roll_buffer:
                    frames.extend(frame)
                
                num_post_record_frames = int(POST_RECORD_SECONDS * porcupine.sample_rate / porcupine.frame_length)

                for _ in range(0, num_post_record_frames):
                    data = audio_stream.read(porcupine.frame_length)
                    frames.extend(struct.unpack_from("h" * porcupine.frame_length, data))

                # --- Save the combined audio to a WAV file ---
                with wave.open(WAVE_OUTPUT_FILENAME, 'wb') as wf:
                    wf.setnchannels(CHANNELS)
                    wf.setsampwidth(pa.get_sample_size(FORMAT))
                    wf.setframerate(porcupine.sample_rate)
                    wf.writeframes(struct.pack("h" * len(frames), *frames))
                
                print("Recording finished. Transcribing...")

                # --- Transcribe with Whisper ---
                result = model.transcribe(WAVE_OUTPUT_FILENAME)
                command_text = result['text']
                
                if command_text:
                    print(f">>> Your command: {command_text.strip()}")
                else:
                    print("No speech detected.")

                print("\nListening for 'Porcupine'...")


    except KeyboardInterrupt:
        print("Stopping...")
    finally:
        if porcupine is not None:
            porcupine.delete()
        if audio_stream is not None:
            audio_stream.stop_stream()
            audio_stream.close()
        if pa is not None:
            pa.terminate()

if __name__ == '__main__':
    main()