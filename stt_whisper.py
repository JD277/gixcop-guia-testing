from faster_whisper import WhisperModel

# Loading the model
model = WhisperModel("base", device="cpu", compute_type="int8")

# Processing the audio
def transcribe(filename:str, context:bool=False) -> str:
    segments, info = model.transcribe(filename, 
        beam_size=5, language="es", 
        condition_on_previous_text=context,
        vad_filter=True,
        vad_parameters=dict(min_silence_duration_ms=500)
    )
    full_text = " ".join(segment.text for segment in segments).strip()
    return full_text

if __name__ == "__main__":
    print(transcribe("graba.wav"))

