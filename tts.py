import pyttsx3

def speak(text:str) -> None:
    """
    Convert text to speech
    Args:
        text (str): Text to convert to speech
    Returns:
        None
    """
    engine = pyttsx3.init() # Initialize the engine
    engine.setProperty('rate', 140) # Set the speech rate
    engine.say(text) # Set the text to speech
    engine.runAndWait() # Wait for the text to speech

if __name__ == "__main__":
    speak("Lo primero que atrapa es ese contraste tan fuerte: estamos refugiados en una especie de cueva cálida y rojiza, pero desde ahí contemplamos un mundo exterior que brilla, lleno de aire y libertad.\n\nLo que parece una espada o unas tijeras en el suelo es, en realidad, la clave de toda la historia. No son herramientas para cortar papel o tela, sino el símbolo de un conflicto que ya terminó. Es una espada abandonada. El artista nos está diciendo que la batalla ha cesado; el arma ya no es necesaria y ha sido dejada atrás para que pueda nacer algo nuevo.\n\nFíjate en las dos figuras: una está sumida en la reflexión y la otra parece bailar de alegría. Es ese instante mágico donde el dolor se convierte en alivio. El árbol solitario que domina la escena es el puente entre esos dos mundos, recordándonos que, tras cualquier tormenta o lucha, siempre hay un espacio para volver a florecer y respirar.")