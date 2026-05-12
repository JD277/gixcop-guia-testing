import wave
from piper import PiperVoice
from piper.voice import SynthesisConfig

voice = PiperVoice.load("es_AR-daniela-high.onnx")
syn_config = SynthesisConfig(
    volume=0.5,  # half as loud
    length_scale=0.75,  # twice as slow
    noise_scale=1.0,  # more audio variation
    noise_w_scale=1.0,  # more speaking variation
    normalize_audio=False, # use raw audio from voice
)
def speak(text):
    with wave.open("test.wav", "wb") as wav_file:
        voice.synthesize_wav(text, wav_file, syn_config=syn_config)

if __name__ == "__main__":
    speak("Lo primero que atrapa es ese contraste tan fuerte: estamos refugiados en una especie de cueva cálida y rojiza, pero desde ahí contemplamos un mundo exterior que brilla, lleno de aire y libertad. Lo que parece una espada o unas tijeras en el suelo es, en realidad, la clave de toda la historia. No son herramientas para cortar papel o tela, sino el símbolo de un conflicto que ya terminó. Es una espada abandonada.")
