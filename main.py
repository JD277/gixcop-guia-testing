from grabar_audio import record_prompt
from stt_whisper import transcribe
from send_request import ask_n8n, ask_n8n_with_imagefile
from tts import speak
file = "graba.wav"

def main():
    record_prompt(file, 3)
    text = transcribe(file)
    print("Usuario: ", text)
    resp = ask_n8n_with_imagefile("images-test/test1.jpg",text)
    print("Asistente: ", resp["output"])
    speak(resp["output"])

if __name__ == "__main__":
    main()