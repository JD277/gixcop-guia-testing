import requests

def descargar_audio(url, nombre_archivo="audio_descargado.wav"):
    """
    Descarga un archivo de audio desde una URL y lo guarda localmente.
    
    Args:
        url (str): La URL del archivo de audio.
        nombre_archivo (str): El nombre con el que se guardará el archivo localmente.
    """
    try:
        # Realizar la petición GET a la URL
        respuesta = requests.get(url)
        
        # Verificar si la petición fue exitosa (código 200)
        if respuesta.status_code == 200:
            # Guardar el contenido en un archivo
            with open(nombre_archivo, 'wb') as archivo:
                archivo.write(respuesta.content)
            print(f"✅ Audio descargado exitosamente como '{nombre_archivo}'")
        else:
            print(f"❌ Error al descargar el audio. Código de estado: {respuesta.status_code}")
            
    except requests.exceptions.RequestException as e:
        print(f"❌ Ocurrió un error durante la petición: {e}")
    except IOError as e:
        print(f"❌ Error al guardar el archivo: {e}")

# URL del archivo de audio
url_audio = "http://192.168.137.247/download/sound.wav"

# Llamar a la función para descargar el audio
descargar_audio(url_audio, "pepe.wav")