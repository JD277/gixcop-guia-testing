import pyaudio
import wave 
FORMAT = pyaudio.paInt16 # Format of the recording 
CHANNELS = 1 # Set the audio stereo  
RATE = 44100 # Establish the framerate 
CHUNK = 1024 
duration = 10 # s
file = "graba.wav"

audio = pyaudio.PyAudio() # start the module
def record_prompt(filename:str, duration: int) -> None:
    """
    Record an audio prompt for the given duration.
    Args:
        filename (str): Name of the file to save the audio
        duration (int): Duration of the audio in seconds
    """
    # Start the streaming
    stream = audio.open(format=FORMAT, channels=CHANNELS, rate=RATE, input= True, frames_per_buffer=CHUNK)

    print("grabando...")
    frames = [] # List to save the parts of the audio

    # The record start 
    for i in range(0,int(RATE/CHUNK*duration)):
        data=stream.read(CHUNK) # Get the audio from the microphone 
        frames.append(data) # save a pice of audio in the list
    stream.stop_stream() 
    stream.close()
    audio.terminate() 
    print("Audio grabado")

    waveFile = wave.open(filename, 'wb') # Create the file
    waveFile.setnchannels(CHANNELS) # set the stereo channel
    waveFile.setsampwidth(audio.get_sample_size(FORMAT)) # set the format
    waveFile.setframerate(RATE) # set the framerate
    waveFile.writeframes(b''.join(frames)) # join the frames in one audio file 
    waveFile.close() # close the file and save it

if __name__ == "__main__":
    record_prompt(file, 10)